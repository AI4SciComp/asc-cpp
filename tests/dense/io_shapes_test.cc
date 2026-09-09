#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "asc/core/array_io.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/io.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "test_support.h"

namespace {
using asc_dense_test::TestContext;

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}

class Source final : public asc::ByteSource {
 public:
  explicit Source(std::span<const std::byte> bytes) : bytes_(bytes) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    const auto count = std::min({bytes_.size(), output.size(), std::size_t{3}});
    std::copy_n(bytes_.begin(), count, output.begin());
    bytes_ = bytes_.subspan(count);
    return count;
  }

 private:
  std::span<const std::byte> bytes_;
};

class Sink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> input) override {
    if (input.size() > storage_.size() - size_) {
      return asc::Status(asc::ErrorCode::kAllocation);
    }
    std::copy(input.begin(), input.end(), storage_.begin() + size_);
    size_ += input.size();
    return input.size();
  }
  [[nodiscard]] std::span<const std::byte> bytes() const {
    return std::span(storage_).first(size_);
  }
  [[nodiscard]] std::string_view text() const {
    return {reinterpret_cast<const char*>(storage_.data()), size_};
  }

 private:
  std::array<std::byte, 4096> storage_{};
  std::size_t size_ = 0;
};

template <typename T>
T Value(std::size_t ordinal) {
  if constexpr (std::is_same_v<T, std::complex<float>> ||
                std::is_same_v<T, std::complex<double>>) {
    using Real = typename T::value_type;
    return {static_cast<Real>(ordinal + 1), static_cast<Real>(ordinal + 2)};
  } else {
    return static_cast<T>(ordinal + 1);
  }
}

template <typename T, std::size_t Rank>
std::string ExpectedText(const char* code,
                         const std::array<asc::extent_t, Rank>& shape,
                         std::size_t count) {
  std::string text = "ASCARRAY 1\nkind dense\nscalar ";
  text += code;
  text += "\nrank " + std::to_string(Rank) + "\nshape";
  for (const auto extent : shape) {
    text += " " + std::to_string(extent);
  }
  text += "\norder dim0\ncount " + std::to_string(count) + "\ndata\n";
  for (std::size_t i = 0; i < count; ++i) {
    if constexpr (std::is_same_v<T, std::complex<float>> ||
                  std::is_same_v<T, std::complex<double>>) {
      text += "(" + std::to_string(i + 1) + "," + std::to_string(i + 2) + ")\n";
    } else {
      text += std::to_string(i + 1) + "\n";
    }
  }
  return text + "end\n";
}

template <typename T, std::size_t Rank>
void ReadCases(TestContext& test, asc::DenseView<T, Rank> target,
               std::array<T, 256>& storage, const Sink& sink, bool binary,
               const std::array<T, 256>& expected) {
  std::array<T, 16> staging{};
  std::array<asc::extent_t, Rank> metadata{};
  std::array<std::byte, 512> scratch{};
  const auto count = static_cast<std::size_t>(target.logical_size());
  if (count != 0) {
    storage.fill(T{37});
    const auto before = storage;
    Source source(sink.bytes());
    asc::ArrayIoReport report;
    auto reader =
        Take(binary ? asc::DenseArrayReader::PrepareBinary(
                          source, metadata, scratch, {}, report, true)
                    : asc::DenseArrayReader::PrepareText(
                          source, metadata, scratch, {}, report, true));
    const auto consumed = report.input_bytes;
    const auto status = asc::ReadDenseArrayInto(
        reader, target, std::span(staging).first(count - 1));
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kAllocation);
    ASC_DENSE_TEST_CHECK(test, !report.committed);
    ASC_DENSE_TEST_EQ(test, storage, before);
    ASC_DENSE_TEST_EQ(test, report.input_bytes, consumed);
  }
  for (const bool truncated : {true, false}) {
    storage.fill(T{37});
    const auto before = storage;
    Source source(
        sink.bytes().first(sink.bytes().size() - (truncated ? 1 : 0)));
    asc::ArrayIoReport report;
    auto reader = binary ? asc::DenseArrayReader::PrepareBinary(
                               source, metadata, scratch, {}, report, true)
                         : asc::DenseArrayReader::PrepareText(
                               source, metadata, scratch, {}, report, true);
    const auto status = reader.ok() ? asc::ReadDenseArrayInto(
                                          *reader, target, std::span(staging))
                                    : reader.status();
    ASC_DENSE_TEST_EQ(test, status.ok(), !truncated);
    ASC_DENSE_TEST_EQ(test, report.committed, !truncated);
    ASC_DENSE_TEST_CHECK(test, report.input_bytes > 0);
    ASC_DENSE_TEST_EQ(test, storage, truncated ? before : expected);
  }
}

template <typename T, std::size_t Rank, typename Layout>
void Shape(TestContext& test, const char* code, Layout layout,
           const char* layout_name, bool empty) {
  std::array<asc::extent_t, Rank> shape{};
  shape.fill(2);
  std::size_t count = std::size_t{1} << Rank;
  if constexpr (Rank > 0) {
    if (empty) {
      shape[Rank - 1] = 0;
      count = 0;
    }
  }
  const auto mapping = Take(asc::DenseLayout<Rank>::Create(shape, layout));
  std::array<T, 256> storage{};
  storage.fill(T{37});
  const auto target = Take(asc::DenseView<T, Rank>::Create(
      storage.data() + 2, mapping, asc::MemorySpace::kHost));
  // Decode a dimension-zero-fastest ordinal independently of DenseLayout.
  for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
    std::size_t remainder = ordinal;
    std::size_t offset = 2;
    std::size_t physical_stride =
        std::is_same_v<Layout, asc::LayoutStride<Rank>> ? 2 : 1;
    if constexpr (std::is_same_v<Layout, asc::LayoutRight>) {
      physical_stride = Rank == 0 ? 1 : std::size_t{1} << (Rank - 1);
    }
    for (std::size_t axis = 0; axis < Rank; ++axis) {
      offset += (remainder % 2) * physical_stride;
      remainder /= 2;
      if constexpr (std::is_same_v<Layout, asc::LayoutRight>) {
        physical_stride /= 2;
      } else {
        physical_stride *= std::is_same_v<Layout, asc::LayoutLeft> ? 2 : 3;
      }
    }
    storage[offset] = Value<T>(ordinal);
  }
  const auto expected = storage;
  const auto text = ExpectedText<T>(code, shape, count);
  std::array<std::byte, 512> scratch{};
  for (const bool binary : {false, true}) {
    Sink sink;
    asc::ArrayIoReport report;
    const auto status =
        binary ? asc::WriteDenseArrayBinary(target, sink, {}, scratch, report)
               : asc::WriteDenseArrayText(target, sink, {}, scratch, report);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, storage, expected);
    if (!binary) {
      ASC_DENSE_TEST_EQ(test, sink.text(), text);
    } else {
      ASC_DENSE_TEST_EQ(test, sink.bytes().size(),
                        56 + 8 * Rank + sizeof(T) * count + 4);
    }
    ReadCases(test, target, storage, sink, binary, expected);
  }
  std::printf("dense archive shape: %s rank=%zu layout=%s empty=%d\n", code,
              Rank, layout_name, static_cast<int>(empty));
}

template <typename T, std::size_t Rank>
void Layouts(TestContext& test, const char* code) {
  std::array<asc::stride_t, Rank> strides{};
  asc::stride_t stride = 2;
  for (auto& value : strides) {
    value = stride;
    stride *= 3;
  }
  for (const bool empty : {false, true}) {
    if constexpr (Rank == 0) {
      if (empty) {
        continue;  // Rank-zero Dense has exactly one value by contract.
      }
    }
    Shape<T, Rank>(test, code, asc::LayoutLeft{}, "left", empty);
    Shape<T, Rank>(test, code, asc::LayoutRight{}, "right", empty);
    Shape<T, Rank>(test, code, asc::LayoutStride<Rank>{strides}, "strided",
                   empty);
  }
}

template <typename T>
void Scalars(TestContext& test, const char* code) {
  Layouts<T, 0>(test, code);
  Layouts<T, 1>(test, code);
  Layouts<T, 2>(test, code);
  Layouts<T, 3>(test, code);
  Layouts<T, 4>(test, code);
}
}  // namespace

int main() {
  TestContext test;
  Scalars<std::int8_t>(test, "i8");
  Scalars<std::uint8_t>(test, "u8");
  Scalars<std::int16_t>(test, "i16");
  Scalars<std::uint16_t>(test, "u16");
  Scalars<std::int32_t>(test, "i32");
  Scalars<std::uint32_t>(test, "u32");
  Scalars<std::int64_t>(test, "i64");
  Scalars<std::uint64_t>(test, "u64");
  Scalars<float>(test, "f32");
  Scalars<double>(test, "f64");
  Scalars<std::complex<float>>(test, "c64");
  Scalars<std::complex<double>>(test, "c128");
  return test.Finish();
}
