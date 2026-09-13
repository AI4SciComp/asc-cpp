#include <algorithm>
#include <array>
#include <bit>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <istream>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>

#include "asc/core/array_io.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#ifdef TEST_DENSE
#include "asc/dense/io.h"
#include "asc/dense/layout.h"
#else
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/io.h"
#endif

namespace {
class Resource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kHost;
  }
  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    auto result = host_.Allocate(bytes, alignment);
    if (result.ok() && *result != nullptr) {
      ++live;
    }
    return result;
  }
  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    if (pointer != nullptr) {
      --live;
    }
    host_.Deallocate(pointer, bytes, alignment);
  }
  std::size_t live = 0;

 private:
  asc::HostMemoryResource host_;
};

class Source final : public asc::ByteSource {
 public:
  explicit Source(const std::filesystem::path& path)
      : stream_(path, std::ios::binary) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    stream_.read(reinterpret_cast<char*>(output.data()),
                 static_cast<std::streamsize>(output.size()));
    if (stream_.bad()) {
      return asc::Status(asc::ErrorCode::kIo);
    }
    return static_cast<std::size_t>(stream_.gcount());
  }

 private:
  std::ifstream stream_;
};

// First-party test representation: component bits only, never an ASC codec.
template <typename T>
T ReadScalar(std::istream& input) {
  if constexpr (requires { typename T::value_type; }) {
    const auto real = ReadScalar<typename T::value_type>(input);
    const auto imag = ReadScalar<typename T::value_type>(input);
    return T(real, imag);
  } else {
    std::array<char, sizeof(T)> bytes{};
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if constexpr (std::endian::native == std::endian::big) {
      std::ranges::reverse(bytes);
    }
    return std::bit_cast<T>(bytes);
  }
}

template <typename T>
void ObserveScalar(std::ostream& output, T value) {
  if constexpr (requires { typename T::value_type; }) {
    ObserveScalar(output, value.real());
    ObserveScalar(output, value.imag());
  } else {
    auto bytes = std::bit_cast<std::array<unsigned char, sizeof(T)>>(value);
    if constexpr (std::endian::native == std::endian::big) {
      std::ranges::reverse(bytes);
    }
    for (auto byte : bytes) {
      output << std::hex << std::setfill('0') << std::setw(2)
             << static_cast<unsigned int>(byte);
    }
  }
}

template <typename View>
auto* Value(const View& view, std::size_t ordinal) {
#ifdef TEST_DENSE
  const std::array<asc::index_t, 2> coordinate{
      static_cast<asc::index_t>(ordinal % 2),
      static_cast<asc::index_t>(ordinal / 2)};
  auto result = view.At(coordinate);
  return result.ok() ? *result : nullptr;
#else
  return view.values() + ordinal;
#endif
}

template <typename View>
std::size_t Count(const View& view) {
#ifdef TEST_DENSE
  return static_cast<std::size_t>(view.extents()[0] * view.extents()[1]);
#else
  return static_cast<std::size_t>(view.nnz());
#endif
}

template <typename View>
std::string Observe(const View& view) {
  std::ostringstream output;
  output << "{\"shape\":[" << view.extents()[0] << ',' << view.extents()[1]
         << "],\"structure\":[";
#ifndef TEST_DENSE
  if constexpr (requires { view.coordinates(); }) {
    for (std::size_t i = 0; i < Count(view) * 2; ++i) {
      output << (i == 0 ? "" : ",") << view.coordinates()[i];
    }
  } else {
    const auto outer = static_cast<std::size_t>(
        view.extents()[View::kFormat == asc::SparseCompressedFormat::kCsr ? 0
                                                                          : 1]);
    for (std::size_t i = 0; i <= outer; ++i) {
      output << (i == 0 ? "" : ",") << view.outer_offsets()[i];
    }
    for (std::size_t i = 0; i < Count(view); ++i) {
      output << ',' << view.inner_indices()[i];
    }
  }
#endif
  output << "],\"values\":[";
  for (std::size_t i = 0; i < Count(view); ++i) {
    output << (i == 0 ? "\"" : ",\"");
    ObserveScalar(output, *Value(view, i));
    output << '"';
  }
  output << "]}\n";
  return output.str();
}

template <typename View>
bool RejectMalformed(const std::filesystem::path& input, const View& view) {
  const auto before = Observe(view);
  std::array<typename View::value_type, 4> staging{};
  for (const auto* name : {"corrupt.ascb", "truncated.ascb", "trailing.ascb"}) {
    Source source(input / name);
    std::array<asc::extent_t, 2> metadata{};
    std::array<std::byte, 512> scratch{};
    asc::ArrayIoReport report;
#ifdef TEST_DENSE
    auto reader = asc::DenseArrayReader::PrepareBinary(
        source, metadata, scratch, {}, report, true);
    auto status =
        reader.ok() ? asc::ReadDenseArrayInto(*reader, view, std::span(staging))
                    : reader.status();
#else
    auto status = asc::ReadSparseArrayBinaryInto(
        source, view, std::span(staging), metadata, scratch, {}, report, true);
#endif
    if (status.ok() || report.committed || report.input_bytes == 0 ||
        Observe(view) != before) {
      return false;
    }
  }
  return true;
}

using Shape = asc::Extents<2, 2>;
#ifdef TEST_DENSE
template <typename T, typename Layout>
bool CaseBody(const std::filesystem::path& input,
              const std::filesystem::path& output, Resource& resource,
              Layout layout) {
#else
template <typename T, typename Owner>
bool CaseBody(const std::filesystem::path& input,
              const std::filesystem::path& output, Resource& resource) {
#endif
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 512> scratch{};
  asc::ArrayIoReport report;
#ifdef TEST_DENSE
  auto writer =
      asc::LoadDenseArrayText<T, Shape>(input / "independent.asc", resource,
                                        layout, metadata, scratch, {}, report);
#else
  auto writer = asc::LoadSparseArrayText<Owner>(
      input / "independent.asc", resource, metadata, scratch, {}, report);
#endif
  if (!writer.ok()) {
    return false;
  }
  auto view = writer->view();
  if (!view.ok()) {
    return false;
  }
  std::ifstream values(input / "values.bits", std::ios::binary);
  for (std::size_t i = 0; i < Count(*view); ++i) {
    *Value(*view, i) = ReadScalar<T>(values);
  }
  if (!values || values.peek() != std::char_traits<char>::eof()) {
    return false;
  }
#ifdef TEST_DENSE
  auto status = asc::SaveDenseArrayBinary(
#else
  auto status = asc::SaveSparseArrayBinary(
#endif
      output.string() + ".ascb", *view, asc::ArrayFileOverwrite::kTruncate, {},
      scratch, report);
  if (!status.ok() || report.section != asc::ArrayIoSection::kComplete) {
    return false;
  }
#ifdef TEST_DENSE
  auto reader = asc::LoadDenseArrayBinary<T, Shape>(input / "independent.ascb",
                                                    resource, layout, metadata,
                                                    scratch, {}, report);
#else
  auto reader = asc::LoadSparseArrayBinary<Owner>(
      input / "independent.ascb", resource, metadata, scratch, {}, report);
#endif
  if (!reader.ok() || !report.committed ||
      report.cleanup_error != asc::ErrorCode::kOk) {
    return false;
  }
  auto read_view = reader->view();
  if (!read_view.ok()) {
    return false;
  }
  std::ofstream observation(output.string() + ".json", std::ios::binary);
  observation << Observe(*read_view);
  observation.close();
  return observation.good() && RejectMalformed(input, *read_view);
}

template <typename T, typename... Configuration>
bool Case(const std::filesystem::path& input,
          const std::filesystem::path& output, const std::string& name,
          Configuration... configuration) {
  Resource resource;
  const bool passed =
      CaseBody<T>(input / name, output / name, resource, configuration...);
  std::printf("binary interop: %s passed=%d live=%zu\n", name.c_str(),
              static_cast<int>(passed), resource.live);
  return passed && resource.live == 0;
}

#ifndef TEST_DENSE
template <typename T, typename Owner>
bool SparseCase(const std::filesystem::path& input,
                const std::filesystem::path& output, const std::string& name) {
  Resource resource;
  const bool passed = CaseBody<T, Owner>(input / name, output / name, resource);
  std::printf("binary interop: %s passed=%d live=%zu\n", name.c_str(),
              static_cast<int>(passed), resource.live);
  return passed && resource.live == 0;
}
#endif

template <typename T>
bool Scalar(const std::filesystem::path& input,
            const std::filesystem::path& output, const char* code) {
  bool passed = true;
  for (const auto* suffix : {"", "-bits"}) {
    if (*suffix != '\0' && std::is_integral_v<T>) {
      continue;
    }
#ifdef TEST_DENSE
    passed = Case<T>(input, output, std::string(code) + "-dense-left" + suffix,
                     asc::LayoutLeft{}) &&
             passed;
    passed = Case<T>(input, output, std::string(code) + "-dense-right" + suffix,
                     asc::LayoutRight{}) &&
             passed;
#else
    passed = SparseCase<T, asc::CoordinateArray<T, Shape>>(
                 input, output, std::string(code) + "-coo" + suffix) &&
             passed;
    passed = SparseCase<T, asc::CsrArray<T>>(
                 input, output, std::string(code) + "-csr" + suffix) &&
             passed;
    passed = SparseCase<T, asc::CscArray<T>>(
                 input, output, std::string(code) + "-csc" + suffix) &&
             passed;
#endif
  }
  return passed;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    return 2;
  }
  const std::filesystem::path input = argv[1];
  const std::filesystem::path output = argv[2];
  bool passed = true;
  passed = Scalar<std::int8_t>(input, output, "i8") && passed;
  passed = Scalar<std::uint8_t>(input, output, "u8") && passed;
  passed = Scalar<std::int16_t>(input, output, "i16") && passed;
  passed = Scalar<std::uint16_t>(input, output, "u16") && passed;
  passed = Scalar<std::int32_t>(input, output, "i32") && passed;
  passed = Scalar<std::uint32_t>(input, output, "u32") && passed;
  passed = Scalar<std::int64_t>(input, output, "i64") && passed;
  passed = Scalar<std::uint64_t>(input, output, "u64") && passed;
  passed = Scalar<float>(input, output, "f32") && passed;
  passed = Scalar<double>(input, output, "f64") && passed;
  passed = Scalar<std::complex<float>>(input, output, "c64") && passed;
  passed = Scalar<std::complex<double>>(input, output, "c128") && passed;
  return passed ? 0 : 1;
}
