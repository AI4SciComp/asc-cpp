#include "asc/dense/matrix_market.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include "../allocation_observation.h"
#include "../array_io/decimal_cases.h"
#include "../locale_test_support.h"
#include "allocation_probe.h"
#include "asc/core/array_io.h"
#include "asc/core/contracts.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/matrix_market.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "counting_memory_resource.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;
using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using Symmetry = asc::MatrixMarketSymmetry;
constexpr std::string_view kRealGeneral =
    "%%MatrixMarket matrix array real general\n2 3\n1\n4\n2\n5\n3\n6\n";
constexpr std::string_view kIntegerGeneral =
    "%%MatrixMarket matrix array integer general\n2 3\n1\n4\n2\n5\n3\n6\n";
constexpr std::string_view kComplexGeneral =
    "%%MatrixMarket matrix array complex general\n2 3\n1 0\n4 0\n2 0\n5 0\n3 "
    "0\n6 0\n";

class Source final : public asc::ByteSource {
 public:
  explicit Source(std::string_view bytes) : bytes_(bytes) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    ++calls;
    if (position == fail_after) {
      if (!error.ok()) {
        return std::move(error);
      }
      return asc::Status(asc::ErrorCode::kIo);
    }
    if (invalid_count) {
      return output.size() + 1;
    }
    const auto count = std::min(
        {output.size(), bytes_.size() - position, fail_after - position});
    for (std::size_t i = 0; i < count; ++i) {
      output[i] = static_cast<std::byte>(bytes_[position + i]);
    }
    position += count;
    return count;
  }
  std::size_t position = 0;
  std::size_t calls = 0;
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();
  bool invalid_count = false;
  asc::Status error;

 private:
  std::string_view bytes_;
};

class Sink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> input) override {
    if (zero_progress) {
      return std::size_t{0};
    }
    if (size_ == fail_after) {
      if (!error.ok()) {
        return std::move(error);
      }
      return asc::Status(asc::ErrorCode::kIo);
    }
    const auto count = std::min(
        {input.size(), data_.size() - size_, max_chunk, fail_after - size_});
    std::copy_n(input.data(), count, data_.data() + size_);
    size_ += count;
    return count;
  }
  [[nodiscard]] std::string_view text() const {
    return {reinterpret_cast<const char*>(data_.data()), size_};
  }
  std::size_t max_chunk = std::numeric_limits<std::size_t>::max();
  std::size_t fail_after = std::numeric_limits<std::size_t>::max();
  bool zero_progress = false;
  asc::Status error;

 private:
  std::array<std::byte, 8192> data_{};
  std::size_t size_ = 0;
};

template <typename T, std::size_t Rank, typename Layout = asc::LayoutLeft>
auto View(T* values, const std::array<asc::extent_t, Rank>& shape,
          Layout layout = {},
          asc::MemorySpace space = asc::MemorySpace::kHost) {
  auto mapping = asc::DenseLayout<Rank>::Create(shape, layout);
  ASC_CHECK(mapping.ok());
  return asc::DenseView<T, Rank>::Create(values, *mapping, space);
}

template <typename T, std::size_t Count>
void CheckOwningFixture(TestContext& test, std::string_view text,
                        const std::array<asc::extent_t, 2>& shape,
                        const std::array<T, Count>& columns,
                        Symmetry symmetry) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoReport report;
  asc_dense_test::CountingMemoryResource resource;
  Source left_source(text);
  auto left = asc::ReadDenseMatrixMarket<T, Shape>(
      left_source, resource, asc::LayoutLeft{}, scratch, {}, report);
  ASC_DENSE_TEST_CHECK(test, left.ok());
  if (left.ok()) {
    auto data = left->view();
    ASC_DENSE_TEST_CHECK(
        test, std::equal(columns.begin(), columns.end(), data->data()));
  }
  Source right_source(text);
  auto right = asc::ReadDenseMatrixMarket<T, Shape>(
      right_source, resource, asc::LayoutRight{}, scratch, {}, report);
  ASC_DENSE_TEST_CHECK(test, right.ok());
  if (right.ok()) {
    auto data = right->view();
    const auto rows = static_cast<std::size_t>(shape[0]);
    const auto cols = static_cast<std::size_t>(shape[1]);
    for (std::size_t row = 0; row < rows; ++row) {
      for (std::size_t column = 0; column < cols; ++column) {
        ASC_DENSE_TEST_EQ(test, data->data()[row * cols + column],
                          columns[row + rows * column]);
      }
    }
    Sink owner_sink;
    ASC_DENSE_TEST_CHECK(
        test, asc::WriteDenseMatrixMarket(*right, owner_sink, symmetry, {},
                                          scratch, report)
                  .ok());
    ASC_DENSE_TEST_EQ(test, owner_sink.text(), text);
  }
  ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{2});
}

template <typename T, std::size_t Count>
void CheckFixture(TestContext& test, std::string_view text,
                  const std::array<asc::extent_t, 2>& shape,
                  const std::array<T, Count>& columns, Symmetry symmetry,
                  std::size_t stored) {
  // Explicit coordinates and expected values do not use the codec's traversal.
  std::array<T, 32> backing{};
  backing.fill(T{57});
  auto view = View(backing.data(), shape, asc::LayoutStride<2>{{2, 9}});
  ASC_CHECK(view.ok());
  std::array<T, Count> staging{};
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoReport report;
  Source source(text);
  std::size_t allocations = 0;
  asc::Status status;
  {
    asc_dense_test::AllocationProbe probe;
    auto reader =
        asc::DenseMatrixMarketReader::Prepare(source, scratch, {}, report);
    ASC_CHECK(reader.ok());
    auto moved = std::move(*reader);
    ASC_CHECK(!reader->ready() && moved.ready());
    ASC_CHECK(moved.stored_count() == stored && moved.count() == Count);
    status = asc::ReadDenseMatrixMarketInto(moved, *view, std::span(staging));
    ASC_CHECK(!moved.ready());
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_DENSE_TEST_CHECK(test, report.committed);
  ASC_DENSE_TEST_EQ(test, report.values_processed, stored);
  ASC_DENSE_TEST_EQ(test, report.input_bytes, text.size());
  const auto rows = static_cast<std::size_t>(shape[0]);
  const auto cols = static_cast<std::size_t>(shape[1]);
  for (std::size_t i = 0; i < backing.size(); ++i) {
    bool logical = false;
    for (std::size_t column = 0; column < cols; ++column) {
      for (std::size_t row = 0; row < rows; ++row) {
        if (i == row * 2 + column * 9) {
          ASC_DENSE_TEST_EQ(test, backing[i], columns[row + rows * column]);
          logical = true;
        }
      }
    }
    if (!logical) {
      ASC_DENSE_TEST_EQ(test, backing[i], T{57});
    }
  }
  Sink sink;
  sink.max_chunk = 1;
  {
    asc_dense_test::AllocationProbe probe;
    status =
        asc::WriteDenseMatrixMarket(*view, sink, symmetry, {}, scratch, report);
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_DENSE_TEST_EQ(test, sink.text(), text);
  ASC_DENSE_TEST_EQ(test, report.output_bytes, text.size());
  ASC_DENSE_TEST_EQ(test, report.values_processed, stored);
  ASC_DENSE_TEST_CHECK(test, !report.committed);

  CheckOwningFixture(test, text, shape, columns, symmetry);
}

template <typename T>
void CheckGeneral(TestContext& test, std::string_view text) {
  CheckFixture<T>(test, text, {2, 3},
                  std::array<T, 6>{T{1}, T{4}, T{2}, T{5}, T{3}, T{6}},
                  Symmetry::kGeneral, 6);
}

void TestAllNativeScalars(TestContext& test) {
  CheckGeneral<std::int8_t>(test, kIntegerGeneral);
  CheckGeneral<std::uint8_t>(test, kIntegerGeneral);
  CheckGeneral<std::int16_t>(test, kIntegerGeneral);
  CheckGeneral<std::uint16_t>(test, kIntegerGeneral);
  CheckGeneral<std::int32_t>(test, kIntegerGeneral);
  CheckGeneral<std::uint32_t>(test, kIntegerGeneral);
  CheckGeneral<std::int64_t>(test, kIntegerGeneral);
  CheckGeneral<std::uint64_t>(test, kIntegerGeneral);
  CheckGeneral<float>(test, kRealGeneral);
  CheckGeneral<double>(test, kRealGeneral);
  CheckGeneral<std::complex<float>>(test, kComplexGeneral);
  CheckGeneral<std::complex<double>>(test, kComplexGeneral);
}

void TestAllStructuredClasses(TestContext& test) {
  CheckFixture<int>(
      test, "%%MatrixMarket matrix array integer symmetric\n2 2\n1\n2\n3\n",
      {2, 2}, std::array<int, 4>{1, 2, 2, 3}, Symmetry::kSymmetric, 3);
  CheckFixture<int>(
      test, "%%MatrixMarket matrix array integer skew-symmetric\n2 2\n2\n",
      {2, 2}, std::array<int, 4>{0, 2, -2, 0}, Symmetry::kSkewSymmetric, 1);
  CheckFixture<float>(
      test, "%%MatrixMarket matrix array real symmetric\n2 2\n1\n2\n3\n",
      {2, 2}, std::array<float, 4>{1, 2, 2, 3}, Symmetry::kSymmetric, 3);
  CheckFixture<double>(
      test, "%%MatrixMarket matrix array real skew-symmetric\n2 2\n2\n", {2, 2},
      std::array<double, 4>{0, 2, -2, 0}, Symmetry::kSkewSymmetric, 1);
  using C = std::complex<double>;
  CheckFixture<C>(
      test,
      "%%MatrixMarket matrix array complex symmetric\n2 2\n1 1\n2 3\n4 0\n",
      {2, 2}, std::array<C, 4>{C(1, 1), C(2, 3), C(2, 3), C(4, 0)},
      Symmetry::kSymmetric, 3);
  CheckFixture<C>(
      test, "%%MatrixMarket matrix array complex skew-symmetric\n2 2\n2 3\n",
      {2, 2}, std::array<C, 4>{C(0, 0), C(2, 3), C(-2, -3), C(0, 0)},
      Symmetry::kSkewSymmetric, 1);
  CheckFixture<C>(
      test,
      "%%MatrixMarket matrix array complex hermitian\n2 2\n4 0\n2 2\n11 0\n",
      {2, 2}, std::array<C, 4>{C(4, 0), C(2, 2), C(2, -2), C(11, 0)},
      Symmetry::kHermitian, 3);
}

template <typename T>
asc::Status ReadSingle(std::string_view text, T& destination,
                       asc::ArrayIoReport& report) {
  Source source(text);
  std::array<std::byte, 1024> scratch{};
  std::array<T, 1> staging{};
  auto view = View(&destination, std::array<asc::extent_t, 2>{1, 1});
  auto reader =
      asc::DenseMatrixMarketReader::Prepare(source, scratch, {}, report);
  if (!reader.ok()) {
    return reader.status();
  }
  return asc::ReadDenseMatrixMarketInto(*reader, *view, std::span(staging));
}

void TestExactConversions(TestContext& test) {
  asc::ArrayIoReport report;
  float value = 19;
  auto status =
      ReadSingle("%%MatrixMarket matrix array integer general\n1 1\n16777217\n",
                 value, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, value, 19.0F);
  status =
      ReadSingle("%%MatrixMarket matrix array integer general\n1 1\n16777218\n",
                 value, report);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, value, 16777218.0F);
  status = ReadSingle(
      "%%MatrixMarket matrix array real general\n1 "
      "1\n1.000000059604644775390625\n",
      value, report);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, value, 1.0F);  // Exact halfway case, ties-to-even.
  status = ReadSingle("%%MatrixMarket matrix array real general\n1 1\n-0\n",
                      value, report);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, std::bit_cast<std::uint32_t>(value),
                    std::uint32_t{0x80000000});
  std::uint64_t integer = 0;
  status = ReadSingle(
      "%%MatrixMarket matrix array integer general\n1 "
      "1\n18446744073709551615\n",
      integer, report);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, integer, std::numeric_limits<std::uint64_t>::max());
  status = ReadSingle(
      "%%MatrixMarket matrix array integer general\n1 "
      "1\n18446744073709551616\n",
      integer, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, integer, std::numeric_limits<std::uint64_t>::max());
  status = ReadSingle("%%MatrixMarket matrix array real general\n1 1\n1\n",
                      integer, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kUnsupported);
  std::complex<double> complex(19, 23);
  status = ReadSingle("%%MatrixMarket matrix array real general\n1 1\n-0\n",
                      complex, report);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, std::bit_cast<std::uint64_t>(complex.imag()),
                    std::uint64_t{0});
  ASC_DENSE_TEST_EQ(test, std::bit_cast<std::uint64_t>(complex.real()),
                    std::uint64_t{1} << 63);
  status = ReadSingle("%%MatrixMarket matrix array complex general\n1 1\n2 0\n",
                      value, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kUnsupported);
  for (std::string_view token :
       {"nan", "inf", "-inf", "1e400", "1e-4000", "0x1p0"}) {
    const std::string text = "%%MatrixMarket matrix array real general\n1 1\n" +
                             std::string(token) + "\n";
    value = 19;
    status = ReadSingle(text, value, report);
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_EQ(test, value, 19.0F);
    ASC_DENSE_TEST_CHECK(test, !report.committed);
  }
}

void TestLexicalFailuresAndRollback(TestContext& test) {
  const std::array<std::string_view, 15> invalid{
      "%%MatrixMarket matrix array pattern general\n2 2\n",
      "%%MatrixMarket matrix array real hermitian\n2 2\n1\n2\n3\n",
      "%%MatrixMarket matrix array integer hermitian\n2 2\n1\n2\n3\n",
      "%%MatrixMarket vector array real general\n2 2\n1\n2\n3\n4\n",
      "%%matrixmarket matrix array real general\n2 2\n1\n2\n3\n4\n",
      "%%MatrixMarket matrix array real symmetric\n2 3\n1\n2\n3\n4\n",
      "%%MatrixMarket matrix array real general\n+2 2\n1\n2\n3\n4\n",
      "%%MatrixMarket matrix array real general\n2 -2\n1\n2\n3\n4\n",
      "%%MatrixMarket matrix array real general\n2 2 4\n1\n2\n3\n4\n",
      "%%MatrixMarket matrix array real general\n2 2\n1 2\n3\n4\n",
      "%%MatrixMarket matrix array real general\n2 2\n1\n2\n3\n4\n5\n",
      "%%MatrixMarket matrix array real general\n2 "
      "2\n1\n2\n3\n4\n%%MatrixMarket matrix array real general\n",
      "%%MatrixMarket matrix array real general\n2 2\n1\n2\n3\n4%comment\n",
      "%%MatrixMarket matrix array real general\r2 2\n1\n2\n3\n4\n",
      "% first banner cannot be a comment\n%%MatrixMarket matrix array real "
      "general\n2 2\n"};
  for (const auto text : invalid) {
    std::array<double, 10> destination{};
    destination.fill(29);
    const auto original = destination;
    auto view = View(destination.data(), std::array<asc::extent_t, 2>{2, 2},
                     asc::LayoutStride<2>{{2, 7}});
    std::array<double, 4> staging{};
    std::array<std::byte, 1024> scratch{};
    asc::ArrayIoReport report;
    Source source(text);
    auto reader =
        asc::DenseMatrixMarketReader::Prepare(source, scratch, {}, report);
    auto status = reader.ok() ? asc::ReadDenseMatrixMarketInto(
                                    *reader, *view, std::span(staging))
                              : reader.status();
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_EQ(test, destination, original);
    ASC_DENSE_TEST_CHECK(test, !report.committed);
  }
  constexpr std::string_view kCoordinate =
      "%%MatrixMarket matrix coordinate complex hermitian\n2 2 3\n1 1 4 0\n2 1 "
      "2 2\n2 2 11 0\n";
  Source source(kCoordinate);
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoReport report;
  auto reader =
      asc::DenseMatrixMarketReader::Prepare(source, scratch, {}, report);
  ASC_DENSE_TEST_EQ(test, reader.status().code(), asc::ErrorCode::kUnsupported);
  ASC_DENSE_TEST_CHECK(test, source.position < kCoordinate.size());
  double scalar = 19;
  auto status = ReadSingle(
      "%%MatrixMarket MATRIX ARRAY REAL GENERAL\r\n\t%before\r\n\t1  1\t\r\n"
      "\r\n%between\n\t2.5\t\r\n  %after\r\n\t",
      scalar, report);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, scalar, 2.5);
  status = ReadSingle("%%MatrixMarket matrix array real general\n1 1\n3.5",
                      scalar, report);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, scalar, 3.5);
}

void TestEveryRequiredPrefixAndSourceFailure(TestContext& test) {
  constexpr std::string_view kText =
      "%%MatrixMarket matrix array real symmetric\n2 2\n1\n2\n3.25\n";
  for (std::size_t cut = 0; cut < kText.size(); ++cut) {
    std::array<double, 4> values{23, 23, 23, 23};
    const auto original = values;
    std::array<double, 4> staging{};
    std::array<std::byte, 1024> scratch{};
    asc::ArrayIoReport report;
    Source source(kText);
    source.fail_after = cut;  // Failure, not valid EOF on a numeric prefix.
    auto reader =
        asc::DenseMatrixMarketReader::Prepare(source, scratch, {}, report);
    auto view = View(values.data(), std::array<asc::extent_t, 2>{2, 2});
    auto status = reader.ok() ? asc::ReadDenseMatrixMarketInto(
                                    *reader, *view, std::span(staging))
                              : reader.status();
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIo);
    ASC_DENSE_TEST_EQ(test, values, original);
    ASC_DENSE_TEST_EQ(test, report.input_bytes, cut);
    ASC_DENSE_TEST_CHECK(test, !report.committed);
  }
  // A final numeric prefix may itself be a valid different value: only prefixes
  // before the final required scalar are unconditionally malformed truncations.
  for (std::size_t cut = 0; cut < kText.find("3.25"); ++cut) {
    Source source(kText.substr(0, cut));
    asc_dense_test::CountingMemoryResource resource;
    std::array<std::byte, 1024> scratch{};
    asc::ArrayIoReport report;
    auto owner = asc::ReadDenseMatrixMarket<double, Shape>(
        source, resource, asc::LayoutLeft{}, scratch, {}, report);
    ASC_DENSE_TEST_CHECK(test, !owner.ok());
    ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, !report.committed);
  }
}

void TestSymmetryFailureBeforeCommitAndWrite(TestContext& test) {
  asc::ArrayIoReport report;
  std::complex<double> value(19, 23);
  auto status =
      ReadSingle("%%MatrixMarket matrix array complex hermitian\n1 1\n4 1\n",
                 value, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kEncoding);
  ASC_DENSE_TEST_EQ(test, value, std::complex<double>(19, 23));
  status =
      ReadSingle("%%MatrixMarket matrix array complex hermitian\n1 1\n4 -0\n",
                 value, report);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, std::bit_cast<std::uint64_t>(value.imag()),
                    std::uint64_t{1} << 63);
  for (std::string_view text :
       {"%%MatrixMarket matrix array integer skew-symmetric\n2 2\n-128\n",
        "%%MatrixMarket matrix array integer skew-symmetric\n2 2\n128\n",
        "%%MatrixMarket matrix array integer skew-symmetric\n2 2\n0\n1\n"}) {
    Source source(text);
    std::array<std::byte, 1024> scratch{};
    asc_dense_test::CountingMemoryResource resource;
    auto owner = asc::ReadDenseMatrixMarket<std::int8_t, Shape>(
        source, resource, asc::LayoutLeft{}, scratch, {}, report);
    ASC_DENSE_TEST_CHECK(test, !owner.ok());
    ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
  }
  std::array<std::complex<double>, 4> data{{{4, 0}, {2, 2}, {2, 2}, {11, 0}}};
  auto view = View(data.data(), std::array<asc::extent_t, 2>{2, 2});
  std::array<std::byte, 1024> scratch{};
  Sink sink;
  status = asc::WriteDenseMatrixMarket(*view, sink, Symmetry::kHermitian, {},
                                       scratch, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kEncoding);
  ASC_DENSE_TEST_CHECK(test, sink.text().empty());
  data[2] = {2, -2};
  data[0] = {4, 1};
  status = asc::WriteDenseMatrixMarket(*view, sink, Symmetry::kHermitian, {},
                                       scratch, report);
  ASC_DENSE_TEST_CHECK(test, !status.ok() && sink.text().empty());
  data[0] = {0, 0};
  status = asc::WriteDenseMatrixMarket(*view, sink, Symmetry::kSkewSymmetric,
                                       {}, scratch, report);
  ASC_DENSE_TEST_CHECK(test, !status.ok() && sink.text().empty());
  data[0] = {std::numeric_limits<double>::infinity(), 0};
  status = asc::WriteDenseMatrixMarket(*view, sink, Symmetry::kGeneral, {},
                                       scratch, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(test, sink.text().empty());
}

void TestEmptyShapes(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoReport report;
  for (std::string_view text :
       {"%%MatrixMarket matrix array real general\n0 3\n",
        "%%MatrixMarket matrix array real general\n3 0\n",
        "%%MatrixMarket matrix array real symmetric\n0 0\n",
        "%%MatrixMarket matrix array real skew-symmetric\n0 0\n",
        "%%MatrixMarket matrix array real skew-symmetric\n1 1\n"}) {
    Source source(text);
    asc_dense_test::CountingMemoryResource resource;
    auto owner = asc::ReadDenseMatrixMarket<double, Shape>(
        source, resource, asc::LayoutLeft{}, scratch, {}, report);
    ASC_DENSE_TEST_CHECK(test, owner.ok() && report.committed);
    ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{1});
    ASC_DENSE_TEST_EQ(test, report.values_processed, std::size_t{0});
    if (owner.ok() && owner->logical_size() != 0) {
      auto view = owner->view();
      ASC_DENSE_TEST_EQ(test, view->data()[0], 0.0);
    }
  }
}

void TestShapeAndResources(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoReport report;
  for (int dimension = 0; dimension < 11; ++dimension) {
    asc::ArrayIoLimits limits;
    switch (dimension) {
      case 0:
        limits.max_rank = 1;
        break;
      case 1:
        limits.max_extent = 1;
        break;
      case 2:
        limits.max_logical_elements = 5;
        break;
      case 3:
        limits.max_stored_elements = 5;
        break;
      case 4:
        limits.max_decoded_bytes = 47;
        break;
      case 5:
        limits.max_staging_bytes = 47;
        break;
      case 6:
        limits.max_allocation_bytes = 47;
        break;
      case 7:
        limits.max_allocations = 0;
        break;
      case 8:
        limits.max_scratch_bytes = 1023;
        break;
      case 9:
        limits.max_input_bytes = kRealGeneral.size();
        break;
      case 10:
        limits.max_header_bytes = 20;
        break;
      default:
        ASC_CHECK(false);
    }
    Source source(kRealGeneral);
    asc_dense_test::CountingMemoryResource resource;
    auto owner = asc::ReadDenseMatrixMarket<double, Shape>(
        source, resource, asc::LayoutLeft{}, scratch, limits, report);
    ASC_DENSE_TEST_CHECK(test, !owner.ok());
    ASC_DENSE_TEST_EQ(test, owner.status().code(), asc::ErrorCode::kAllocation);
    ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, !report.committed);
  }
  Source source(kRealGeneral);
  asc_dense_test::CountingMemoryResource resource;
  auto wrong_rank =
      asc::ReadDenseMatrixMarket<double, asc::Extents<asc::kDynamicExtent>>(
          source, resource, asc::LayoutLeft{}, scratch, {}, report);
  ASC_DENSE_TEST_EQ(test, wrong_rank.status().code(), asc::ErrorCode::kShape);
  Source fixed_source(kRealGeneral);
  auto wrong_static = asc::ReadDenseMatrixMarket<double, asc::Extents<2, 2>>(
      fixed_source, resource, asc::LayoutRight{}, scratch, {}, report);
  ASC_DENSE_TEST_EQ(test, wrong_static.status().code(), asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(test, resource.allocation_requests(), std::size_t{0});
  Source failed_source(kRealGeneral);
  resource.FailRequest(0);
  auto failed = asc::ReadDenseMatrixMarket<double, Shape>(
      failed_source, resource, asc::LayoutLeft{}, scratch, {}, report);
  ASC_DENSE_TEST_EQ(test, failed.status().code(), asc::ErrorCode::kAllocation);
  ASC_DENSE_TEST_CHECK(test, !failed.status().message().empty());
  ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
}

void TestStagingAliasingPlacementAndSinks(TestContext& test) {
  std::array<double, 6> values{1, 4, 2, 5, 3, 6};
  const auto original = values;
  std::array<double, 6> staging{};
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoReport report;
  Source source(kRealGeneral);
  auto reader =
      asc::DenseMatrixMarketReader::Prepare(source, scratch, {}, report);
  auto view = View(values.data(), std::array<asc::extent_t, 2>{2, 3});
  auto status =
      asc::ReadDenseMatrixMarketInto(*reader, *view, std::span(values));
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, values, original);
  ASC_DENSE_TEST_CHECK(test, reader->ready());
  status = asc::ReadDenseMatrixMarketInto(*reader, *view,
                                          std::span(staging).first(5));
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kAllocation);
  ASC_DENSE_TEST_EQ(test, values, original);
  auto device = View(values.data(), std::array<asc::extent_t, 2>{2, 3},
                     asc::LayoutLeft{}, asc::MemorySpace::kDevice);
  status = asc::ReadDenseMatrixMarketInto(*reader, *device, std::span(staging));
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
  Sink device_sink;
  status = asc::WriteDenseMatrixMarket(*device, device_sink, Symmetry::kGeneral,
                                       {}, scratch, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
  ASC_DENSE_TEST_CHECK(test, device_sink.text().empty());
  for (std::size_t cut = 0; cut < kRealGeneral.size(); ++cut) {
    Sink sink;
    sink.fail_after = cut;
    status = asc::WriteDenseMatrixMarket(*view, sink, Symmetry::kGeneral, {},
                                         scratch, report);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIo);
    ASC_DENSE_TEST_EQ(test, report.output_bytes, cut);
    ASC_DENSE_TEST_EQ(test, sink.text(), kRealGeneral.substr(0, cut));
  }
  Sink stalled;
  stalled.zero_progress = true;
  status = asc::WriteDenseMatrixMarket(*view, stalled, Symmetry::kGeneral, {},
                                       scratch, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIo);
  Source invalid_source(kRealGeneral);
  invalid_source.invalid_count = true;
  auto invalid = asc::DenseMatrixMarketReader::Prepare(invalid_source, scratch,
                                                       {}, report);
  ASC_DENSE_TEST_EQ(test, invalid.status().code(), asc::ErrorCode::kIo);
}

void TestPreparedCursorRetriesAndPlacement(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  std::array<double, 6> values{};
  std::array<double, 6> staging{};
  asc::ArrayIoReport report;
  Source source(kRealGeneral);
  auto prepared =
      asc::DenseMatrixMarketReader::Prepare(source, scratch, {}, report);
  ASC_CHECK(prepared.ok());
  auto reader = std::move(*prepared);
  auto host = View(values.data(), std::array<asc::extent_t, 2>{2, 3});
  ASC_DENSE_TEST_EQ(
      test,
      asc::ReadDenseMatrixMarketInto(*prepared, *host, std::span(staging))
          .code(),
      asc::ErrorCode::kInvalidState);
  const auto header_calls = source.calls;
  for (const auto space :
       {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
    auto unavailable = View(values.data(), std::array<asc::extent_t, 2>{2, 3},
                            asc::LayoutLeft{}, space);
    ASC_DENSE_TEST_EQ(
        test,
        asc::ReadDenseMatrixMarketInto(reader, *unavailable, std::span(staging))
            .code(),
        asc::ErrorCode::kMemoryAccess);
    ASC_DENSE_TEST_CHECK(test, reader.ready());
    ASC_DENSE_TEST_EQ(test, source.calls, header_calls);
  }
  auto pinned = View(values.data(), std::array<asc::extent_t, 2>{2, 3},
                     asc::LayoutLeft{}, asc::MemorySpace::kPinnedHost);
  ASC_DENSE_TEST_CHECK(
      test,
      asc::ReadDenseMatrixMarketInto(reader, *pinned, std::span(staging)).ok());
  ASC_DENSE_TEST_CHECK(test, report.committed && !reader.ready());
  const auto complete_calls = source.calls;
  const auto complete = values;
  ASC_DENSE_TEST_EQ(
      test,
      asc::ReadDenseMatrixMarketInto(reader, *host, std::span(staging)).code(),
      asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_EQ(test, source.calls, complete_calls);
  ASC_DENSE_TEST_EQ(test, values, complete);
}

void TestPreparedMetadataAndReportAliases(TestContext& test) {
  constexpr std::string_view kSingle =
      "%%MatrixMarket matrix array integer general\n1 1\n7\n";
  Source source(kSingle);
  std::array<std::byte, 256> scratch{};
  asc::ArrayIoReport report;
  auto reader =
      asc::DenseMatrixMarketReader::Prepare(source, scratch, {}, report);
  ASC_CHECK(reader.ok());
  std::array<asc::extent_t, 1> value{91};
  auto view = View(value.data(), std::array<asc::extent_t, 2>{1, 1});
  auto* metadata = const_cast<asc::extent_t*>(reader->shape().data());
  const auto calls = source.calls;
  auto status =
      asc::ReadDenseMatrixMarketInto(*reader, *view, std::span(metadata, 1));
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, reader->shape()[0], asc::extent_t{1});
  ASC_DENSE_TEST_EQ(test, value[0], asc::extent_t{91});
  ASC_DENSE_TEST_EQ(test, source.calls, calls);
  ASC_DENSE_TEST_CHECK(test, reader->ready());

  std::array<std::size_t, 1> unsigned_value{91};
  auto unsigned_view =
      View(unsigned_value.data(), std::array<asc::extent_t, 2>{1, 1});
  const auto input_bytes = report.input_bytes;
  status = asc::ReadDenseMatrixMarketInto(*reader, *unsigned_view,
                                          std::span(&report.input_bytes, 1));
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, unsigned_value[0], std::size_t{91});
  ASC_DENSE_TEST_EQ(test, report.input_bytes, input_bytes);
  ASC_DENSE_TEST_EQ(test, source.calls, calls);
  ASC_DENSE_TEST_CHECK(test, reader->ready());
}

class FailingResource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kHost;
  }
  asc::Result<void*> Allocate(std::size_t /*bytes*/,
                              std::size_t /*alignment*/) override {
    return std::move(error);
  }
  void Deallocate(void* /*pointer*/, std::size_t /*bytes*/,
                  std::size_t /*alignment*/) noexcept override {}
  asc::Status error{asc::ErrorCode::kAllocation, std::string(4096, 'r'),
                    "explicit-resource", 404};
};

void TestLongErrorsAndAllocationBoundaries(TestContext& test) {
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoReport report;
  std::size_t allocations = 0;
  Source source(kRealGeneral);
  source.fail_after = kRealGeneral.size() - 2;
  source.error = asc::Status(asc::ErrorCode::kIo, std::string(4096, 's'),
                             std::string(4096, 'p'), 919);
  std::array<double, 6> values{19, 19, 19, 19, 19, 19};
  const auto original = values;
  std::array<double, 6> staging{};
  auto view = View(values.data(), std::array<asc::extent_t, 2>{2, 3});
  asc::Status status;
  {
    asc_dense_test::AllocationProbe probe;
    auto reader =
        asc::DenseMatrixMarketReader::Prepare(source, scratch, {}, report);
    ASC_CHECK(reader.ok());
    status = asc::ReadDenseMatrixMarketInto(*reader, *view, std::span(staging));
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_DENSE_TEST_EQ(test, values, original);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIo);
  ASC_DENSE_TEST_EQ(test, status.native_code(), std::int64_t{919});
  ASC_DENSE_TEST_CHECK(test,
                       status.message().empty() && status.provider().empty());
  Sink sink;
  sink.fail_after = 7;
  sink.error = asc::Status(asc::ErrorCode::kIo, std::string(4096, 'w'),
                           std::string(4096, 'p'), 929);
  {
    asc_dense_test::AllocationProbe probe;
    status = asc::WriteDenseMatrixMarket(*view, sink, Symmetry::kGeneral, {},
                                         scratch, report);
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_DENSE_TEST_EQ(test, status.native_code(), std::int64_t{929});
  ASC_DENSE_TEST_CHECK(test,
                       status.message().empty() && status.provider().empty());
  FailingResource resource;
  Source resource_source(kRealGeneral);
  asc::Result<asc::DenseArray<double, Shape>> owner{
      asc::Status(asc::ErrorCode::kInvalidState)};
  {
    asc_dense_test::AllocationProbe probe;
    owner = asc::ReadDenseMatrixMarket<double, Shape>(
        resource_source, resource, asc::LayoutLeft{}, scratch, {}, report);
    ASC_CHECK(!owner.ok());
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_DENSE_TEST_EQ(test, owner.status().code(), asc::ErrorCode::kAllocation);
  ASC_DENSE_TEST_EQ(test, owner.status().native_code(), std::int64_t{404});
  ASC_DENSE_TEST_EQ(test, owner.status().message().size(), std::size_t{4096});
  ASC_DENSE_TEST_EQ(test, owner.status().provider(),
                    std::string_view("explicit-resource"));
}

void TestWriterLimitsAliasesAndHugeEmpty(TestContext& test) {
  std::array<double, 6> values{1, 4, 2, 5, 3, 6};
  const auto original = values;
  auto view = View(values.data(), std::array<asc::extent_t, 2>{2, 3});
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoReport report;
  Sink alias_sink;
  auto status = asc::WriteDenseMatrixMarket(
      *view, alias_sink, Symmetry::kGeneral, {},
      std::as_writable_bytes(std::span(values)), report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, values, original);
  report.input_bytes = 1729;
  status = asc::WriteDenseMatrixMarket(
      *view, alias_sink, Symmetry::kGeneral, {},
      std::as_writable_bytes(std::span(&report, 1)), report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, report.input_bytes, std::size_t{1729});
  for (std::size_t cap = 0; cap < kRealGeneral.size(); ++cap) {
    asc::ArrayIoLimits limits;
    limits.max_output_bytes = cap;
    Sink sink;
    status = asc::WriteDenseMatrixMarket(*view, sink, Symmetry::kGeneral,
                                         limits, scratch, report);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kAllocation);
    ASC_DENSE_TEST_CHECK(test, report.output_bytes <= cap);
  }
  for (const auto space :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kManaged}) {
    auto placed = View(values.data(), std::array<asc::extent_t, 2>{2, 3},
                       asc::LayoutLeft{}, space);
    Sink sink;
    status = asc::WriteDenseMatrixMarket(*placed, sink, Symmetry::kGeneral, {},
                                         scratch, report);
    ASC_DENSE_TEST_EQ(test, status.ok(),
                      space == asc::MemorySpace::kPinnedHost);
  }
  constexpr std::string_view kHugeEmpty =
      "%%MatrixMarket matrix array real general\n0 2147483647\n";
  Source source(kHugeEmpty);
  asc_dense_test::CountingMemoryResource resource;
  auto owner = asc::ReadDenseMatrixMarket<double, Shape>(
      source, resource, asc::LayoutRight{}, scratch, {}, report);
  ASC_DENSE_TEST_CHECK(test, owner.ok() && report.committed);
  if (owner.ok()) {
    Sink sink;
    ASC_DENSE_TEST_CHECK(
        test, asc::WriteDenseMatrixMarket(*owner, sink, Symmetry::kGeneral, {},
                                          scratch, report)
                  .ok());
    ASC_DENSE_TEST_EQ(test, sink.text(), kHugeEmpty);
  }
  for (const auto* const text :
       {"%%MatrixMarket matrix array real general\n18446744073709551616 0\n",
        "%%MatrixMarket matrix array real general\n9223372036854775808 0\n",
        "%%MatrixMarket matrix array real general\n9223372036854775807 2\n"}) {
    asc::ArrayIoLimits limits;
    limits.max_extent =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    limits.max_logical_elements = limits.max_extent;
    Source overflow(text);
    auto reader = asc::DenseMatrixMarketReader::Prepare(overflow, scratch,
                                                        limits, report);
    ASC_DENSE_TEST_CHECK(test, !reader.ok());
    ASC_DENSE_TEST_EQ(test, reader.status().code(), asc::ErrorCode::kOverflow);
  }
  asc::ArrayIoLimits invalid_limits;
  invalid_limits.max_extent = std::numeric_limits<std::uint64_t>::max();
  Source invalid_limit_source(kRealGeneral);
  auto invalid_limits_reader = asc::DenseMatrixMarketReader::Prepare(
      invalid_limit_source, scratch, invalid_limits, report);
  ASC_DENSE_TEST_EQ(test, invalid_limits_reader.status().code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, invalid_limit_source.calls, std::size_t{0});
}

void TestBoundedMutationRollback(TestContext& test) {
  constexpr std::string_view kBase =
      "%%MatrixMarket matrix array complex hermitian\n2 2\n4 0\n2 2\n11 0\n";
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoLimits limits;
  limits.max_input_bytes = 256;
  limits.max_header_bytes = 128;
  limits.max_extent = 4;
  limits.max_logical_elements = 16;
  limits.max_stored_elements = 16;
  std::uint32_t state = 0x52b713ef;
  std::size_t failures = 0;
  for (std::size_t iteration = 0; iteration < 1024; ++iteration) {
    state = state * 1664525U + 1013904223U;
    std::string text(kBase);
    const auto position = static_cast<std::size_t>(state) % text.size();
    state = state * 1664525U + 1013904223U;
    text[position] = static_cast<char>(state >> 24);
    std::array<std::complex<double>, 10> target{};
    target.fill({19, 23});
    const auto original = target;
    std::array<std::complex<double>, 4> staging{};
    auto view = View(target.data(), std::array<asc::extent_t, 2>{2, 2},
                     asc::LayoutStride<2>{{2, 7}});
    Source source(text);
    asc::ArrayIoReport report;
    auto reader =
        asc::DenseMatrixMarketReader::Prepare(source, scratch, limits, report);
    auto status = reader.ok() ? asc::ReadDenseMatrixMarketInto(
                                    *reader, *view, std::span(staging))
                              : reader.status();
    if (!status.ok()) {
      ++failures;
      ASC_DENSE_TEST_EQ(test, target, original);
      ASC_DENSE_TEST_CHECK(test, !report.committed);
    } else {
      ASC_DENSE_TEST_CHECK(test, report.committed);
      ASC_DENSE_TEST_EQ(test, target[1], original[1]);
      ASC_DENSE_TEST_EQ(test, target[8], original[8]);
    }
    ASC_DENSE_TEST_CHECK(test, report.input_bytes <= limits.max_input_bytes);
  }
  ASC_DENSE_TEST_CHECK(test, failures > 900);
}

void TestPathConveniences(TestContext& test) {
  const auto suffix =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory = std::filesystem::temp_directory_path() /
                         ("asc-dense-mm-" + std::to_string(suffix));
  ASC_CHECK(std::filesystem::create_directory(directory));
  const auto path = directory / "matrix with spaces.mtx";
  std::array<double, 6> values{1, 4, 2, 5, 3, 6};
  auto view = View(values.data(), std::array<asc::extent_t, 2>{2, 3});
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoReport report;
  auto status = asc::SaveDenseMatrixMarket(path, *view, Symmetry::kGeneral,
                                           asc::ArrayFileOverwrite::kTruncate,
                                           {}, scratch, report);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  asc_dense_test::CountingMemoryResource resource;
  auto owner = asc::LoadDenseMatrixMarket<double, Shape>(
      path, resource, asc::LayoutLeft{}, scratch, {}, report);
  ASC_DENSE_TEST_CHECK(test, owner.ok() && report.committed);
  if (owner.ok()) {
    auto loaded = owner->view();
    ASC_DENSE_TEST_CHECK(
        test, std::equal(values.begin(), values.end(), loaded->data()));
  }
  status = asc::SaveDenseMatrixMarket(path, *view, Symmetry::kSymmetric,
                                      asc::ArrayFileOverwrite::kTruncate, {},
                                      scratch, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kShape);
  auto preserved = asc::LoadDenseMatrixMarket<double, Shape>(
      path, resource, asc::LayoutLeft{}, scratch, {}, report);
  ASC_DENSE_TEST_CHECK(test,
                       preserved.ok());  // Invalid symmetry did not truncate.
  auto missing = asc::LoadDenseMatrixMarket<double, Shape>(
      directory / "absent.mtx", resource, asc::LayoutLeft{}, scratch, {},
      report);
  ASC_DENSE_TEST_CHECK(test, !missing.ok() && !report.committed);
  report.input_bytes = 1729;
  auto alias = asc::LoadDenseMatrixMarket<double, Shape>(
      path, resource, asc::LayoutLeft{},
      std::as_writable_bytes(std::span(&report, 1)), {}, report);
  ASC_DENSE_TEST_EQ(test, alias.status().code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, report.input_bytes, std::size_t{1729});
  std::error_code error;
  ASC_DENSE_TEST_CHECK(test, std::filesystem::remove(path, error) && !error);
  ASC_DENSE_TEST_CHECK(test,
                       std::filesystem::remove(directory, error) && !error);
}

template <typename T>
void TestCanonicalDecimals(TestContext& test) {
  auto values = asc_decimal_test::Values<T>();
  auto view = View(values.data(), std::array<asc::extent_t, 2>{2, 4});
  ASC_DENSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoReport report;
  Sink sink;
  constexpr bool kReal = std::is_floating_point_v<T>;
  const std::string expected = std::string("%%MatrixMarket matrix array ") +
                               (kReal ? "real" : "complex") +
                               " general\n2 4\n" +
                               asc_decimal_test::Payload<T>(true);
  ASC_DENSE_TEST_CHECK(
      test, asc::WriteDenseMatrixMarket(*view, sink, Symmetry::kGeneral, {},
                                        scratch, report)
                .ok());
  ASC_DENSE_TEST_EQ(test, sink.text(), expected);
  Source independent(expected);
  asc_dense_test::CountingMemoryResource resource;
  auto owner = asc::ReadDenseMatrixMarket<T, Shape>(
      independent, resource, asc::LayoutLeft{}, scratch, {}, report);
  ASC_DENSE_TEST_CHECK(test, owner.ok() && report.committed);
  if (owner.ok()) {
    auto loaded = owner->view();
    ASC_DENSE_TEST_CHECK(test, loaded.ok());
    if (loaded.ok()) {
      for (std::size_t i = 0; i < values.size(); ++i) {
        ASC_DENSE_TEST_CHECK(
            test, asc_decimal_test::SameBits(loaded->data()[i], values[i]));
      }
    }
  }
}

void TestCanonicalDecimals(TestContext& test) {
  TestCanonicalDecimals<float>(test);
  TestCanonicalDecimals<double>(test);
  TestCanonicalDecimals<std::complex<float>>(test);
  TestCanonicalDecimals<std::complex<double>>(test);
}

}  // namespace

int main() {
  TestContext test;
  TestCanonicalDecimals(test);
  TestAllNativeScalars(test);
  TestAllStructuredClasses(test);
  TestExactConversions(test);
  TestLexicalFailuresAndRollback(test);
  TestEveryRequiredPrefixAndSourceFailure(test);
  TestSymmetryFailureBeforeCommitAndWrite(test);
  TestEmptyShapes(test);
  TestShapeAndResources(test);
  TestStagingAliasingPlacementAndSinks(test);
  TestPreparedCursorRetriesAndPlacement(test);
  TestPreparedMetadataAndReportAliases(test);
  TestLongErrorsAndAllocationBoundaries(test);
  TestWriterLimitsAliasesAndHugeEmpty(test);
  TestBoundedMutationRollback(test);
  TestPathConveniences(test);
  {
    asc_locale_test::LocaleGuard locale;
    TestCanonicalDecimals(test);
    ASC_DENSE_TEST_CHECK(test, asc_locale_test::LocaleControl());
    TestAllNativeScalars(test);
    TestAllStructuredClasses(test);
    TestExactConversions(test);
    TestLexicalFailuresAndRollback(test);
  }
  return test.Finish();
}
