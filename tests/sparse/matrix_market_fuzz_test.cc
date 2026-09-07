#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/array_io.h"
#include "asc/core/io.h"
#include "asc/core/matrix_market.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/matrix_market.h"
#include "test_support.h"

namespace {

using T = std::complex<double>;
using asc_sparse_test::TestContext;

template <typename Value>
Value Take(asc::Result<Value> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}

class Source final : public asc::ByteSource {
 public:
  explicit Source(std::span<const char> input) : input_(input) {}
  asc::Result<std::size_t> ReadSome(std::span<std::byte> output) override {
    const auto count = std::min(output.size(), input_.size() - position);
    for (std::size_t i = 0; i < count; ++i) {
      output[i] = static_cast<std::byte>(input_[position + i]);
    }
    position += count;
    return count;
  }
  std::size_t position = 0;

 private:
  std::span<const char> input_;
};

std::uint32_t Next(std::uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

asc::ArrayIoLimits Limits() {
  asc::ArrayIoLimits limits;
  limits.max_extent = 2;
  limits.max_logical_elements = 4;
  limits.max_stored_elements = 8;
  limits.max_input_bytes = 256;
  limits.max_token_bytes = 32;
  limits.max_header_bytes = 128;
  limits.max_allocations = 0;
  limits.max_allocation_bytes = 0;
  limits.max_scratch_bytes = 128;
  limits.max_staging_bytes = 512;
  return limits;
}

template <typename View>
void Observe(TestContext& test, const View& view, std::span<T> storage,
             std::span<const char> text, bool valid_seed) {
  const std::array<T, 6> sentinels{T{91, 1}, T{92, 2}, T{93, 3},
                                   T{94, 4}, T{95, 5}, T{96, 6}};
  std::copy(sentinels.begin(), sentinels.end(), storage.begin());
  std::array<asc::index_t, 16> coordinates{};
  std::array<T, 8> values{};
  std::array<std::byte, 128> scratch{};
  asc::SparseMatrixMarketWorkspace<T> workspace{coordinates, values};
  asc::SparseMatrixMarketReadOptions options;
  options.duplicate_policy = asc::DuplicatePolicy::kSum;
  options.pattern_policy = asc::MatrixMarketPatternPolicy::kUnit;
  options.max_sort_comparisons = 128;
  asc::SparseMatrixMarketReport report;
  Source source(text);
  asc_sparse_test::AllocationProbe probe;
  const auto status = asc::ReadSparseMatrixMarketInto(
      source, view, scratch, workspace, Limits(), options, report);
  ASC_SPARSE_TEST_EQ(test, report.io.committed, status.ok());
  ASC_SPARSE_TEST_EQ(test, report.io.input_bytes, source.position);
  ASC_SPARSE_TEST_CHECK(test, source.position <= text.size());
  ASC_SPARSE_TEST_CHECK(test, source.position <= Limits().max_input_bytes);
  ASC_SPARSE_TEST_EQ(test, storage.front(), sentinels.front());
  ASC_SPARSE_TEST_EQ(test, storage.back(), sentinels.back());
  if (!status.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, std::equal(storage.begin(), storage.end(), sentinels.begin()));
  }
  if (valid_seed) {
    ASC_SPARSE_TEST_CHECK(test, status.ok());
    const auto upper = asc::internal_sparse_matrix_market::Find(view, {0, 1});
    const auto lower = asc::internal_sparse_matrix_market::Find(view, {1, 0});
    ASC_SPARSE_TEST_EQ(test, view.values()[upper], (T{2, -2}));
    ASC_SPARSE_TEST_EQ(test, view.values()[lower], (T{2, 2}));
  }
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
}

template <typename View>
std::size_t Exercise(TestContext& test, const View& view,
                     std::span<T> storage) {
  constexpr std::array<std::string_view, 3> kSeeds{
      "%%MatrixMarket matrix coordinate complex hermitian\n2 2 3\n1 1 4 0\n2 1 "
      "2 2\n2 2 11 0\n",
      "%%MatrixMarket matrix coordinate complex general\n2 2 4\n2 2 11 0\n1 2 "
      "2 -2\n2 1 2 2\n1 1 4 0\n",
      "%%MatrixMarket matrix coordinate complex symmetric\n2 2 4\n2 1 1 1\n1 1 "
      "4 0\n2 2 11 0\n2 1 1 1\n"};
  for (std::size_t i = 0; i < 2; ++i) {
    Observe(test, view, storage, std::span(kSeeds[i].data(), kSeeds[i].size()),
            true);
  }
  std::uint32_t state = 0x56a7bd13U;
  for (std::size_t iteration = 0; iteration < 4096; ++iteration) {
    const auto seed = kSeeds[iteration % kSeeds.size()];
    std::array<char, 256> bytes{};
    std::copy(seed.begin(), seed.end(), bytes.begin());
    std::size_t size = seed.size();
    const auto position = static_cast<std::size_t>(Next(state)) % size;
    switch (iteration % 4) {
      case 0:
        bytes[position] = static_cast<char>(Next(state) & 0x7fU);
        break;
      case 1:
        size = position;
        break;
      case 2:
        std::move_backward(bytes.begin() + position, bytes.begin() + size,
                           bytes.begin() + size + 1);
        bytes[position] = static_cast<char>(Next(state) & 0xffU);
        ++size;
        break;
      case 3:
        std::move(bytes.begin() + position + 1, bytes.begin() + size,
                  bytes.begin() + position);
        --size;
        break;
      default:
        std::abort();
    }
    Observe(test, view, storage, std::span(bytes).first(size), false);
  }
  return 4098;
}

}  // namespace

int main() {
  TestContext test;
  constexpr std::array<asc::extent_t, 2> kShape{2, 2};
  constexpr std::array<asc::index_t, 8> kCoordinates{0, 0, 0, 1, 1, 0, 1, 1};
  constexpr std::array<asc::nnz_t, 3> kOffsets{0, 2, 4};
  constexpr std::array<asc::index_t, 4> kIndices{0, 1, 0, 1};
  std::array<T, 6> storage{};
  auto values = std::span(storage).subspan(1, 4);
  auto coo = Take(asc::CoordinateView<T, 2>::Create(
      kCoordinates.data(), values.data(), kShape, 4, asc::MemorySpace::kHost));
  auto csr = Take(
      asc::CompressedSparseView<T, asc::SparseCompressedFormat::kCsr>::Create(
          kOffsets, kIndices, values, kShape, asc::MemorySpace::kHost));
  auto csc = Take(
      asc::CompressedSparseView<T, asc::SparseCompressedFormat::kCsc>::Create(
          kOffsets, kIndices, values, kShape, asc::MemorySpace::kHost));
  std::size_t executed = Exercise(test, coo, storage);
  executed += Exercise(test, csr, storage);
  executed += Exercise(test, csc, storage);
  std::cout << "sparse_matrix_market_fuzz executed=" << executed << '\n';
  return test.Finish();
}
