#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/io.h"
#include "asc/sparse/print.h"

namespace {

using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using Coo = asc::CoordinateArray<double, MatrixExtents>;

// A hand-written independent frame, including a stored negative zero. Sparse
// archive readers preserve this entry; they do not canonicalize it away.
constexpr std::string_view kInput =
    "ASCARRAY 1\nkind coo\nscalar f64\nrank 2\nshape 3 4\norder coo\n"
    "count 4\ncoordinates\n(0,0)\n(0,3)\n(1,1)\n(2,0)\n"
    "values\n2\n-0\n5\n-7\nend\n";

class Bytes final : public asc::ByteSource, public asc::ByteSink {
 public:
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    const auto count = std::min(output.size(), size_ - position_);
    std::copy_n(storage_.data() + position_, count, output.data());
    position_ += count;
    return count;
  }
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> input) override {
    const auto count = std::min(input.size(), storage_.size() - size_);
    if (count == 0 && !input.empty()) {
      return asc::Status(asc::ErrorCode::kAllocation);
    }
    std::copy_n(input.data(), count, storage_.data() + size_);
    size_ += count;
    return count;
  }
  [[nodiscard]] std::span<const std::byte> bytes() const {
    return std::span(storage_).first(size_);
  }
  void CorruptChecksum() { storage_[size_ - 1] ^= std::byte{1}; }

 private:
  std::array<std::byte, 4096> storage_{};
  std::size_t size_ = 0;
  std::size_t position_ = 0;
};

class StandardOutput final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> input) override {
    std::cout.write(reinterpret_cast<const char*>(input.data()),
                    static_cast<std::streamsize>(input.size()));
    if (!std::cout) {
      return asc::Status(asc::ErrorCode::kIo);
    }
    return input.size();
  }
};

template <typename Owner>
asc::Status Archive(const Owner& original, std::string_view name,
                    const std::filesystem::path& directory,
                    asc::MemoryResource& resource) {
  std::array<std::byte, 512> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  const asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  auto view = original.view();
  if (!view.ok()) {
    return view.status();
  }
  const auto text_path = directory / (std::string(name) + ".asc.txt");
  auto status = asc::SaveSparseArrayText(text_path, *view,
                                         asc::ArrayFileOverwrite::kTruncate,
                                         limits, scratch, report);
  if (!status.ok()) {
    return status;
  }
  auto text = asc::LoadSparseArrayText<Owner>(text_path, resource, metadata,
                                              scratch, limits, report);
  if (!text.ok()) {
    return text.status();
  }
  const auto binary_path = directory / (std::string(name) + ".asc.bin");
  status = asc::SaveSparseArrayBinary(binary_path, *view,
                                      asc::ArrayFileOverwrite::kTruncate,
                                      limits, scratch, report);
  if (!status.ok()) {
    return status;
  }
  auto binary = asc::LoadSparseArrayBinary<Owner>(
      binary_path, resource, metadata, scratch, limits, report);
  if (!binary.ok()) {
    return binary.status();
  }
  Bytes expected;
  Bytes from_text;
  Bytes from_binary;
  const std::array<std::pair<const Owner*, Bytes*>, 3> comparisons{
      std::pair{&original, &expected}, std::pair{&*text, &from_text},
      std::pair{&*binary, &from_binary}};
  for (auto entry : comparisons) {
    status = asc::WriteSparseArrayBinary(*entry.first, *entry.second, limits,
                                         scratch, report);
    if (!status.ok()) {
      return status;
    }
  }
  if (!std::ranges::equal(expected.bytes(), from_text.bytes()) ||
      !std::ranges::equal(expected.bytes(), from_binary.bytes())) {
    return asc::Status(asc::ErrorCode::kInternal,
                       "Sparse archive changed structure or value bits");
  }
  StandardOutput output;
  asc::ArrayPrintOptions options;
  options.max_elements = 4;
  asc::ArrayPrintReport print_report;
  status = asc::PrintArray(original, output, options, scratch, print_report);
  if (!status.ok()) {
    return status;
  }
  std::cout << '\n' << name << ": exact text/binary archive round-trip\n";
  return asc::Status::Ok();
}

asc::Status CheckRollback(asc::CsrArray<double>& owner) {
  auto view = owner.view();
  if (!view.ok()) {
    return view.status();
  }
  const std::array<double, 4> before{2.0, -0.0, 5.0, -7.0};
  std::array<double, 4> staging{};
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 512> scratch{};
  const asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  Bytes archive;
  auto status =
      asc::WriteSparseArrayBinary(owner, archive, limits, scratch, report);
  if (!status.ok()) {
    return status;
  }
  archive.CorruptChecksum();
  status =
      asc::ReadSparseArrayBinaryInto(archive, *view, std::span(staging),
                                     metadata, scratch, limits, report, true);
  if (status.ok() || report.committed ||
      !std::ranges::equal(
          std::as_bytes(std::span(view->values(), before.size())),
          std::as_bytes(std::span(before)))) {
    return asc::Status(asc::ErrorCode::kInternal,
                       "Corrupt Sparse archive did not roll back");
  }
  std::cout << "Corrupt CRC rejected; every destination value bit preserved\n";
  return asc::Status::Ok();
}

asc::Status Run(const std::filesystem::path& directory) {
  Bytes input;
  auto status = asc::WriteAll(
      input, std::as_bytes(std::span(kInput.data(), kInput.size())));
  if (!status.ok()) {
    return status;
  }
  asc::HostMemoryResource resource;
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 512> scratch{};
  asc::ArrayIoReport report;
  auto coo = asc::ReadSparseArrayText<Coo>(input, resource, metadata, scratch,
                                           asc::ArrayIoLimits{}, report);
  if (!coo.ok()) {
    return coo.status();
  }
  auto view = coo->view();
  if (!view.ok()) {
    return view.status();
  }
  auto csr = asc::ConvertToCsr<double>(asc::ExecutionContext::Serial(), *view,
                                       resource);
  if (!csr.ok()) {
    return csr.status();
  }
  auto csc = asc::ConvertToCsc<double>(asc::ExecutionContext::Serial(), *view,
                                       resource);
  if (!csc.ok()) {
    return csc.status();
  }
  status = Archive(*coo, "coo", directory, resource);
  if (status.ok()) {
    status = Archive(*csr, "csr", directory, resource);
  }
  if (status.ok()) {
    status = Archive(*csc, "csc", directory, resource);
  }
  return status.ok() ? CheckRollback(*csr) : status;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3 || std::string_view(argv[1]) != "--truncate") {
    std::cerr
        << "Usage: sparse_array_io --truncate EXISTING_OUTPUT_DIRECTORY\n"
           "Overwrites coo/csr/csc.asc.txt and .asc.bin in that directory.\n";
    return 2;
  }
  const auto status = Run(std::filesystem::path(argv[2]));
  if (!status.ok()) {
    std::cerr << status.ToString() << '\n';
    return 1;
  }
  std::cout.flush();
  return std::cout ? 0 : 1;
}
