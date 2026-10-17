#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/matrix_market.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/matrix_market.h"
#include "asc/sparse/print.h"

namespace {

using Value = std::complex<double>;
using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using Coo = asc::CoordinateArray<Value, MatrixExtents>;

// Independent lower-half fixture: the implied upper entry is 2 - 2i.
constexpr std::string_view kInput =
    "%%MatrixMarket matrix coordinate complex hermitian\n"
    "% Hand-derived matrix: [[4, 2-2i], [2+2i, 11]].\n"
    "2 2 3\n1 1 4 0\n2 1 2 2\n2 2 11 0\n";
constexpr std::array<Value, 4> kRowValues{Value{4, 0}, Value{2, -2},
                                          Value{2, 2}, Value{11, 0}};

class TextSource final : public asc::ByteSource {
 public:
  explicit TextSource(std::string_view input) : input_(input) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    const auto count = std::min({output.size(), input_.size(), std::size_t{3}});
    const auto bytes = std::as_bytes(std::span(input_.data(), count));
    std::copy(bytes.begin(), bytes.end(), output.begin());
    input_.remove_prefix(count);
    return count;
  }

 private:
  std::string_view input_;
};

class StandardOutput final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> input) override {
    std::cout.write(reinterpret_cast<const char*>(input.data()),
                    static_cast<std::streamsize>(input.size()));
    return std::cout ? asc::Result<std::size_t>(input.size())
                     : asc::Status(asc::ErrorCode::kIo);
  }
};

template <typename T>
bool Check(const asc::CoordinateView<T, 2>& view) {
  constexpr std::array<asc::index_t, 8> kCoordinates{0, 0, 0, 1, 1, 0, 1, 1};
  return view.extents()[0] == 2 && view.extents()[1] == 2 && view.nnz() == 4 &&
         std::ranges::equal(std::span(view.coordinates(), 8), kCoordinates) &&
         std::ranges::equal(std::span(view.values(), 4), kRowValues);
}

template <typename T, asc::SparseCompressedFormat Format>
bool Check(const asc::CompressedSparseView<T, Format>& view) {
  constexpr std::array<asc::nnz_t, 3> kOffsets{0, 2, 4};
  constexpr std::array<asc::index_t, 4> kIndices{0, 1, 0, 1};
  constexpr std::array<Value, 4> kColumnValues{Value{4, 0}, Value{2, 2},
                                               Value{2, -2}, Value{11, 0}};
  const auto& expected =
      Format == asc::SparseCompressedFormat::kCsc ? kColumnValues : kRowValues;
  return view.extents()[0] == 2 && view.extents()[1] == 2 && view.nnz() == 4 &&
         std::ranges::equal(std::span(view.outer_offsets(), 3), kOffsets) &&
         std::ranges::equal(std::span(view.inner_indices(), 4), kIndices) &&
         std::ranges::equal(std::span(view.values(), 4), expected);
}

template <typename Owner>
asc::Status RoundTrip(std::string_view name,
                      const std::filesystem::path& directory,
                      asc::MemoryResource& resource) {
  TextSource source(kInput);
  std::array<std::byte, 512> scratch{};
  const asc::ArrayIoLimits limits;
  const asc::SparseMatrixMarketReadOptions read_options;
  asc::SparseMatrixMarketReport report;
  auto original = asc::ReadSparseMatrixMarket<Owner>(
      source, resource, scratch, limits, read_options, report);
  if (!original.ok()) {
    return original.status();
  }
  auto view = original->view();
  if (!view.ok()) {
    return view.status();
  }
  if (!report.io.committed || !Check(*view)) {
    return asc::Status(asc::ErrorCode::kInternal,
                       "Incorrect Hermitian assembly");
  }
  asc::SparseMatrixMarketWriteOptions write_options;
  write_options.symmetry = asc::MatrixMarketSymmetry::kHermitian;
  const auto path = directory / (std::string(name) + ".mtx");
  auto status = asc::SaveSparseMatrixMarket(
      path, *view, asc::ArrayFileOverwrite::kTruncate, limits, write_options,
      scratch, report);
  if (!status.ok()) {
    return status;
  }
  auto reloaded = asc::LoadSparseMatrixMarket<Owner>(
      path, resource, scratch, limits, read_options, report);
  if (!reloaded.ok()) {
    return reloaded.status();
  }
  auto reloaded_view = reloaded->view();
  if (!reloaded_view.ok()) {
    return reloaded_view.status();
  }
  if (!report.io.committed || !Check(*reloaded_view)) {
    return asc::Status(asc::ErrorCode::kInternal, "Incorrect Hermitian reload");
  }
  StandardOutput output;
  asc::ArrayPrintOptions options;
  options.max_elements = 4;
  asc::ArrayPrintReport print_report;
  status =
      asc::PrintArray(*reloaded_view, output, options, scratch, print_report);
  if (status.ok()) {
    std::cout << '\n' << name << ": Hermitian structure and values verified\n";
  }
  return status;
}

asc::Status CheckRollback(asc::MemoryResource& resource) {
  TextSource source(kInput);
  std::array<std::byte, 512> scratch{};
  const asc::ArrayIoLimits limits;
  const asc::SparseMatrixMarketReadOptions options;
  asc::SparseMatrixMarketReport report;
  auto original = asc::ReadSparseMatrixMarket<asc::CsrArray<Value>>(
      source, resource, scratch, limits, options, report);
  if (!original.ok()) {
    return original.status();
  }
  auto view = original->view();
  if (!view.ok()) {
    return view.status();
  }
  std::array<std::byte, sizeof(Value) * 4> before{};
  const auto value_bytes = std::as_bytes(std::span(view->values(), 4));
  std::copy(value_bytes.begin(), value_bytes.end(), before.begin());
  std::array<asc::index_t, 8> coordinates{};
  std::array<Value, 4> values{};
  const asc::SparseMatrixMarketWorkspace<Value> staging{coordinates, values};
  // The third record violates the real-diagonal Hermitian rule after the
  // earlier records have already been staged. The existing CSR is untouched.
  TextSource malformed(
      "%%MatrixMarket matrix coordinate complex hermitian\n"
      "2 2 3\n1 1 9 0\n2 1 7 3\n2 2 11 1\n");
  const auto status = asc::ReadSparseMatrixMarketInto(
      malformed, *view, scratch, staging, limits, options, report);
  if (status.ok() || report.io.committed || report.source_records != 2 ||
      !std::ranges::equal(value_bytes, before) || !Check(*view)) {
    return asc::Status(asc::ErrorCode::kInternal,
                       "Malformed Hermitian input did not roll back");
  }
  std::cout << "Malformed diagonal rejected after staging; CSR structure and "
               "every destination value bit preserved\n";
  return asc::Status::Ok();
}

asc::Status Run(const std::filesystem::path& directory) {
  asc::HostMemoryResource resource;
  auto status = RoundTrip<Coo>("coo", directory, resource);
  if (status.ok()) {
    status = RoundTrip<asc::CsrArray<Value>>("csr", directory, resource);
  }
  if (status.ok()) {
    status = RoundTrip<asc::CscArray<Value>>("csc", directory, resource);
  }
  return status.ok() ? CheckRollback(resource) : status;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3 || std::string_view(argv[1]) != "--truncate") {
    std::cerr << "Usage: sparse_matrix_market --truncate "
                 "EXISTING_OUTPUT_DIRECTORY\n"
                 "Overwrites coo.mtx, csr.mtx and csc.mtx in that directory.\n";
    return 2;
  }
  const auto status = Run(std::filesystem::path(argv[2]));
  if (!status.ok()) {
    std::cerr << status.ToString() << '\n';
    return 1;
  }
  return 0;
}
