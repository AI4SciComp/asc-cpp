#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_io.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/io.h"
#include "test_support.h"

namespace {

using asc_sparse_test::TestContext;

class Source final : public asc::ByteSource {
 public:
  Source(std::span<const std::byte> bytes, std::size_t chunk)
      : bytes_(bytes), chunk_(chunk) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> destination) override {
    const auto size = std::min({bytes_.size(), destination.size(), chunk_});
    if (size != 0) {
      std::copy_n(bytes_.begin(), size, destination.begin());
    }
    bytes_ = bytes_.subspan(size);
    position += size;
    return size;
  }
  std::size_t position = 0;

 private:
  std::span<const std::byte> bytes_;
  std::size_t chunk_;
};

class Sink final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> input) override {
    if (input.size() > bytes.size() - size) {
      return asc::Status(asc::ErrorCode::kAllocation);
    }
    std::copy(input.begin(), input.end(), bytes.begin() + size);
    size += input.size();
    return input.size();
  }
  std::array<std::byte, 512> bytes{};
  std::size_t size = 0;
};

std::uint64_t Next(std::uint64_t& state) {
  // Local deterministic corpus generation does not consume ASC Random state.
  state = state * std::uint64_t{6364136223846793005} +
          std::uint64_t{1442695040888963407};
  return state;
}

template <typename View>
void Check(TestContext& test, const View& view,
           std::span<const std::byte> bytes, bool binary,
           std::uint64_t selector, bool valid_seed) {
  const std::array<double, 3> original{-73.5, -73.5, -73.5};
  std::copy(original.begin(), original.end(), view.values());
  std::array<double, 3> staging{};
  std::array<asc::extent_t, 2> metadata{};
  std::array<std::byte, 128> scratch{};
  Source source(bytes, 1 + selector % 13);
  asc::ArrayIoLimits limits;
  limits.max_input_bytes = selector % 5 == 0 ? selector % 190 : 1024;
  limits.max_header_bytes = 128;
  limits.max_token_bytes = 32;
  limits.max_rank = 2;
  limits.max_extent = 3;
  limits.max_logical_elements = 6;
  limits.max_stored_elements = 3;
  limits.max_structure_bytes = 56;
  limits.max_decoded_bytes = 80;
  limits.max_staging_bytes = sizeof(staging);
  limits.max_scratch_bytes = sizeof(metadata) + sizeof(scratch);
  limits.max_allocations = 0;
  limits.max_allocation_bytes = 0;
  asc::ArrayIoReport report;
  asc::Status status;
  std::size_t allocations = 0;
  {
    asc_sparse_test::AllocationProbe probe;
    status = binary
                 ? asc::ReadSparseArrayBinaryInto(source, view,
                                                  std::span(staging), metadata,
                                                  scratch, limits, report, true)
                 : asc::ReadSparseArrayTextInto(source, view,
                                                std::span(staging), metadata,
                                                scratch, limits, report, true);
    allocations = probe.count();
  }
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_SPARSE_TEST_EQ(test, report.input_bytes, source.position);
  ASC_SPARSE_TEST_CHECK(test, report.input_bytes <= limits.max_input_bytes);
  ASC_SPARSE_TEST_EQ(test, report.committed, status.ok());
  if (valid_seed) {
    ASC_SPARSE_TEST_CHECK(test, status.ok());
  }
  if (!status.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, std::equal(original.begin(), original.end(), view.values()));
  }
}

template <typename View>
void Corpus(TestContext& test, const View& view, bool binary) {
  std::array<std::byte, 128> scratch{};
  asc::ArrayIoLimits limits;
  asc::ArrayIoReport report;
  Sink seed;
  const auto status =
      binary ? asc::WriteSparseArrayBinary(view, seed, limits, scratch, report)
             : asc::WriteSparseArrayText(view, seed, limits, scratch, report);
  ASC_SPARSE_TEST_CHECK(test, status.ok());
  if (!status.ok()) {
    return;
  }
  Check(test, view, std::span(seed.bytes).first(seed.size), binary, 1, true);
  std::uint64_t state = std::uint64_t{0x8d12e97a340fb563};
  for (std::size_t iteration = 0; iteration < 4096; ++iteration) {
    auto candidate = seed.bytes;
    auto size = seed.size;
    const auto selector = Next(state);
    switch (iteration % 4) {
      case 0:
        candidate[Next(state) % size] ^=
            static_cast<std::byte>(Next(state) >> 24);
        break;
      case 1:
        size = Next(state) % (size + 1);
        break;
      case 2:
        for (std::size_t change = 0; change < 1 + selector % 7; ++change) {
          candidate[Next(state) % size] =
              static_cast<std::byte>(Next(state) >> 32);
        }
        break;
      default:
        candidate[size++] = static_cast<std::byte>(Next(state) >> 48);
        break;
    }
    if (binary && size >= 4 && iteration % 7 == 0) {
      // Repaired checksums exercise deeper header/structure/value validation.
      const auto crc =
          asc::internal_array_io::UpdateCrc(
              std::uint32_t{0xffffffff}, std::span(candidate).first(size - 4)) ^
          std::uint32_t{0xffffffff};
      ASC_SPARSE_TEST_CHECK(test,
                            asc::EncodeLittleEndian(
                                crc, std::span(candidate).subspan(size - 4, 4))
                                .ok());
    }
    Check(test, view, std::span(candidate).first(size), binary, selector,
          false);
  }
  if (test.Finish() == 0) {
    std::cout << "sparse_fuzz kind="
              << static_cast<int>(
                     asc::internal_sparse_io::ViewTraits<View>::kKind)
              << " binary=" << binary << " checked_cases=4097\n";
  }
}

}  // namespace

int main() {
  TestContext test;
  std::array<double, 3> values{0, 3, -2};
  const std::array<asc::extent_t, 2> shape{2, 3};
  const std::array<asc::index_t, 6> coordinates{0, 0, 1, 1, 1, 2};
  auto coo = asc::CoordinateView<double, 2>::Create(
      coordinates.data(), values.data(), shape, 3, asc::MemorySpace::kHost);
  const std::array<asc::nnz_t, 3> rows{0, 1, 3};
  const std::array<asc::index_t, 3> columns{0, 1, 2};
  auto csr =
      asc::CsrView<double>::Create(rows.data(), columns.data(), values.data(),
                                   shape, 3, asc::MemorySpace::kHost);
  const std::array<asc::nnz_t, 4> offsets{0, 1, 2, 3};
  const std::array<asc::index_t, 3> indices{0, 1, 1};
  auto csc = asc::CscView<double>::Create(offsets.data(), indices.data(),
                                          values.data(), shape, 3,
                                          asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, coo.ok() && csr.ok() && csc.ok());
  if (!coo.ok() || !csr.ok() || !csc.ok()) {
    return test.Finish();
  }
  for (bool binary : {false, true}) {
    Corpus(test, *coo, binary);
    Corpus(test, *csr, binary);
    Corpus(test, *csc, binary);
  }
  return test.Finish();
}
