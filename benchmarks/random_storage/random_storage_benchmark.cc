#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <utility>
#include <vector>

#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/sparse.h"
#include "philox_oracle.h"

namespace {

class CountingResource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kHost;
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_calls_;
    allocated_bytes_ += bytes;
    auto result = host_.Allocate(bytes, alignment);
    if (result.ok() && *result != nullptr) {
      ++live_allocations_;
    }
    return result;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    if (pointer != nullptr) {
      ++deallocation_calls_;
      --live_allocations_;
    }
    host_.Deallocate(pointer, bytes, alignment);
  }

  [[nodiscard]] std::size_t allocation_calls() const noexcept {
    return allocation_calls_;
  }
  [[nodiscard]] std::size_t deallocation_calls() const noexcept {
    return deallocation_calls_;
  }
  [[nodiscard]] std::size_t allocated_bytes() const noexcept {
    return allocated_bytes_;
  }
  [[nodiscard]] std::size_t live_allocations() const noexcept {
    return live_allocations_;
  }

 private:
  asc::HostMemoryResource host_;
  std::size_t allocation_calls_ = 0;
  std::size_t deallocation_calls_ = 0;
  std::size_t allocated_bytes_ = 0;
  std::size_t live_allocations_ = 0;
};

std::uint64_t Mix(std::uint64_t checksum, std::uint64_t value) {
  return (checksum ^ value) * 1099511628211ULL;
}

std::uint64_t PriorityOracle(asc::RandomStream stream,
                             asc::RandomSubsequence subsequence,
                             asc::RandomOffset offset) {
  const std::uint64_t high =
      asc_random_cuda_test::PhiloxWordOracle(stream, subsequence, offset);
  const std::uint64_t low =
      asc_random_cuda_test::PhiloxWordOracle(stream, subsequence, offset + 1);
  return (high << 32U) | low;
}

template <typename Layout>
bool BenchmarkDense(Layout layout, const char* layout_name,
                    std::uint64_t& aggregate_checksum) {
  constexpr std::array<asc::extent_t, 2> kExtents{128, 128};
  constexpr std::size_t kRepetitions = 100;
  auto mapping = asc::DenseLayout<2>::Create(
      std::span<const asc::extent_t, 2>(kExtents), layout);
  if (!mapping.ok()) {
    return false;
  }
  std::array<float, 16384> storage{};
  auto view = asc::DenseView<float, 2>::Create(storage.data(), *mapping,
                                               asc::MemorySpace::kHost);
  if (!view.ok()) {
    return false;
  }

  std::uint64_t checksum = 1469598103934665603ULL;
  std::size_t allocation_calls = 0;
  const auto begin = std::chrono::steady_clock::now();
  asc::RandomOffset next = 0;
  for (std::size_t repetition = 0; repetition < kRepetitions; ++repetition) {
    asc::Result<asc::RandomOffset> generated =
        asc::Status(asc::ErrorCode::kInternal, "not generated");
    {
      asc_random_storage_benchmark::AllocationProbe probe;
      generated = asc::FillDenseUniform01(
          asc::ExecutionContext::Serial(), *view, 0x0123456789ABCDEFULL,
          static_cast<asc::RandomSubsequence>(repetition), 17);
      allocation_calls += probe.count();
    }
    if (!generated.ok()) {
      return false;
    }
    next = *generated;
    for (float value : storage) {
      checksum = Mix(checksum, std::bit_cast<std::uint32_t>(value));
    }
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - begin);
  std::uint64_t expected_checksum = 1469598103934665603ULL;
  std::array<float, 16384> expected_storage{};
  for (std::size_t repetition = 0; repetition < kRepetitions; ++repetition) {
    for (std::size_t ordinal = 0; ordinal < expected_storage.size();
         ++ordinal) {
      const std::size_t row = ordinal % 128U;
      const std::size_t column = ordinal / 128U;
      const std::size_t physical =
          row * static_cast<std::size_t>(view->strides()[0]) +
          column * static_cast<std::size_t>(view->strides()[1]);
      const std::uint32_t word = asc_random_cuda_test::PhiloxWordOracle(
          0x0123456789ABCDEFULL,
          static_cast<asc::RandomSubsequence>(repetition), 17 + ordinal);
      expected_storage[physical] =
          asc_random_cuda_test::Uniform01FloatOracle(word);
    }
    for (float value : expected_storage) {
      expected_checksum =
          Mix(expected_checksum, std::bit_cast<std::uint32_t>(value));
    }
  }
  if (checksum != expected_checksum || next != 16401) {
    return false;
  }
  aggregate_checksum = Mix(aggregate_checksum, checksum);
  std::cout << "dense layout=" << layout_name << " scalar=float shape=128x128"
            << " repetitions=" << kRepetitions
            << " operation_allocation_calls=" << allocation_calls
            << " elapsed_ns=" << elapsed.count() << " next_offset=" << next
            << " checksum=" << checksum << " oracle=independent\n";
  return asc_test::ProcessAllocationCountMatches(allocation_calls, 0);
}

bool BenchmarkSparse(std::uint64_t& aggregate_checksum) {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  constexpr std::size_t kRepetitions = 10;
  auto shape = Shape::Create(64, 64);
  if (!shape.ok()) {
    return false;
  }
  CountingResource resource;
  std::uint64_t checksum = 1469598103934665603ULL;
  std::size_t process_allocation_calls = 0;
  asc::RandomOffset next_structure = 0;
  asc::RandomOffset next_value = 0;
  const auto begin = std::chrono::steady_clock::now();
  for (std::size_t repetition = 0; repetition < kRepetitions; ++repetition) {
    using Generation = asc::SparseUniform01Generation<float, Shape>;
    asc::Result<Generation> generated =
        asc::Status(asc::ErrorCode::kInternal, "not generated");
    {
      asc_random_storage_benchmark::AllocationProbe probe;
      generated = asc::GenerateSparseUniform01<float>(
          asc::ExecutionContext::Serial(), *shape, 256, resource, 101,
          static_cast<asc::RandomSubsequence>(repetition), 103, 107, 109, 113);
      process_allocation_calls += probe.count();
    }
    if (!generated.ok()) {
      return false;
    }
    next_structure = generated->next_structure_offset;
    next_value = generated->next_value_offset;
    auto view = generated->array.view();
    if (!view.ok()) {
      return false;
    }
    for (asc::nnz_t position = 0; position < view->nnz(); ++position) {
      auto coordinate = view->Coordinate(position);
      auto value = view->AtStored(position);
      if (!coordinate.ok() || !value.ok()) {
        return false;
      }
      checksum = Mix(checksum, static_cast<std::uint64_t>((*coordinate)[0]));
      checksum = Mix(checksum, static_cast<std::uint64_t>((*coordinate)[1]));
      checksum = Mix(checksum, std::bit_cast<std::uint32_t>(**value));
    }
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - begin);
  std::uint64_t expected_checksum = 1469598103934665603ULL;
  for (std::size_t repetition = 0; repetition < kRepetitions; ++repetition) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> candidates;
    candidates.reserve(64U * 64U);
    for (std::uint64_t ordinal = 0; ordinal < 64U * 64U; ++ordinal) {
      candidates.emplace_back(
          PriorityOracle(101, static_cast<asc::RandomSubsequence>(repetition),
                         103 + 2U * ordinal),
          ordinal);
    }
    std::sort(candidates.begin(), candidates.end());
    std::vector<std::uint64_t> selected;
    selected.reserve(256);
    for (std::size_t position = 0; position < 256; ++position) {
      selected.push_back(candidates[position].second);
    }
    std::sort(selected.begin(), selected.end());
    for (std::size_t position = 0; position < selected.size(); ++position) {
      const std::uint64_t ordinal = selected[position];
      const std::uint32_t word =
          asc_random_cuda_test::PhiloxWordOracle(107, 109, 113 + position);
      expected_checksum = Mix(expected_checksum, ordinal / 64U);
      expected_checksum = Mix(expected_checksum, ordinal % 64U);
      expected_checksum =
          Mix(expected_checksum,
              std::bit_cast<std::uint32_t>(
                  asc_random_cuda_test::Uniform01FloatOracle(word)));
    }
  }
  if (checksum != expected_checksum || next_structure != 8295 ||
      next_value != 369) {
    return false;
  }
  aggregate_checksum = Mix(aggregate_checksum, checksum);
  std::cout << "sparse format=coordinate scalar=float shape=64x64 count=256"
            << " repetitions=" << kRepetitions
            << " allocation_calls=" << resource.allocation_calls()
            << " process_allocation_calls=" << process_allocation_calls
            << " deallocation_calls=" << resource.deallocation_calls()
            << " allocated_bytes=" << resource.allocated_bytes()
            << " live_allocations=" << resource.live_allocations()
            << " elapsed_ns=" << elapsed.count()
            << " next_structure_offset=" << next_structure
            << " next_value_offset=" << next_value << " checksum=" << checksum
            << " oracle=independent\n";
  constexpr std::size_t kExpectedResourceAllocations = 2U * kRepetitions;
  return resource.allocation_calls() == kExpectedResourceAllocations &&
         resource.deallocation_calls() == kExpectedResourceAllocations &&
         resource.live_allocations() == 0 &&
         asc_test::ProcessAllocationCountMatches(
             process_allocation_calls,
             asc_test::ProcessVisibleResourceAllocationCount(
                 kExpectedResourceAllocations));
}

bool BenchmarkAdvancedAdapters(std::uint64_t& aggregate_checksum) {
  constexpr std::size_t kSamples = 2048;
  constexpr std::size_t kDimensions = 8;
  constexpr std::size_t kDenseRepetitions = 10;
  constexpr std::array<asc::extent_t, 2> kDenseExtents{kSamples, kDimensions};
  constexpr std::array<asc::extent_t, 1> kPointExtents{kDimensions};
  auto dense_mapping =
      asc::DenseLayout<2>::Create(kDenseExtents, asc::LayoutRight{});
  auto point_mapping =
      asc::DenseLayout<1>::Create(kPointExtents, asc::LayoutLeft{});
  if (!dense_mapping.ok() || !point_mapping.ok()) {
    return false;
  }
  std::vector<double> dense_storage(kSamples * kDimensions);
  std::array<double, kDimensions> point_storage{};
  auto dense_view = asc::DenseView<double, 2>::Create(
      dense_storage.data(), *dense_mapping, asc::MemorySpace::kHost);
  auto point_view = asc::DenseView<double, 1>::Create(
      point_storage.data(), *point_mapping, asc::MemorySpace::kHost);
  if (!dense_view.ok() || !point_view.ok()) {
    return false;
  }

  std::uint64_t dense_checksum = 1469598103934665603ULL;
  std::size_t dense_allocations = 0;
  const auto dense_begin = std::chrono::steady_clock::now();
  for (std::size_t repetition = 0; repetition < kDenseRepetitions;
       ++repetition) {
    asc::Status status;
    {
      asc_random_storage_benchmark::AllocationProbe probe;
      status = asc::FillDenseSobol(
          asc::ExecutionContext::Serial(), *dense_view,
          static_cast<std::uint64_t>(repetition * kSamples), *point_view);
      dense_allocations += probe.count();
    }
    if (!status.ok()) {
      return false;
    }
    for (double value : dense_storage) {
      dense_checksum = Mix(dense_checksum, std::bit_cast<std::uint64_t>(value));
    }
  }
  const auto dense_elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - dense_begin);
  std::uint64_t expected_dense_checksum = 1469598103934665603ULL;
  for (std::size_t repetition = 0; repetition < kDenseRepetitions;
       ++repetition) {
    for (std::size_t sample = 0; sample < kSamples; ++sample) {
      for (std::size_t dimension = 0; dimension < kDimensions; ++dimension) {
        auto expected = asc::SobolCoordinate<double>(
            static_cast<std::uint64_t>(repetition * kSamples + sample),
            dimension);
        if (!expected.ok()) {
          return false;
        }
        expected_dense_checksum = Mix(expected_dense_checksum,
                                      std::bit_cast<std::uint64_t>(*expected));
      }
    }
  }
  if (dense_checksum != expected_dense_checksum ||
      !asc_test::ProcessAllocationCountMatches(dense_allocations, 0)) {
    return false;
  }
  aggregate_checksum = Mix(aggregate_checksum, dense_checksum);
  std::cout << "dense adapter=sobol scalar=double shape=2048x8 repetitions="
            << kDenseRepetitions
            << " operation_allocation_calls=" << dense_allocations
            << " elapsed_ns=" << dense_elapsed.count()
            << " checksum=" << dense_checksum
            << " oracle=scalar-sobol-coordinate\n";

  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  auto sparse_shape = Shape::Create(64, 64);
  if (!sparse_shape.ok()) {
    return false;
  }
  constexpr std::size_t kSparseCount = 256;
  constexpr std::size_t kSparseRepetitions = 10;
  std::vector<asc::SparseRandomStructureCandidate> candidate_workspace(4096);
  std::array<std::uint64_t, kSparseCount> ordinals{};
  std::uint64_t sparse_checksum = 1469598103934665603ULL;
  std::size_t sparse_allocations = 0;
  asc::RandomOffset next_offset = 0;
  const auto sparse_begin = std::chrono::steady_clock::now();
  for (std::size_t repetition = 0; repetition < kSparseRepetitions;
       ++repetition) {
    asc::Result<asc::RandomOffset> generated =
        asc::Status(asc::ErrorCode::kInternal, "not generated");
    {
      asc_random_storage_benchmark::AllocationProbe probe;
      generated = asc::GenerateSparseStructure(
          asc::ExecutionContext::Serial(), *sparse_shape, kSparseCount, 131,
          static_cast<asc::RandomSubsequence>(repetition), 137,
          candidate_workspace, ordinals);
      sparse_allocations += probe.count();
    }
    if (!generated.ok()) {
      return false;
    }
    next_offset = *generated;
    for (std::uint64_t ordinal : ordinals) {
      sparse_checksum = Mix(sparse_checksum, ordinal);
    }
  }
  const auto sparse_elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - sparse_begin);
  std::uint64_t expected_sparse_checksum = 1469598103934665603ULL;
  for (std::size_t repetition = 0; repetition < kSparseRepetitions;
       ++repetition) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> candidates;
    candidates.reserve(4096);
    for (std::uint64_t ordinal = 0; ordinal < 4096; ++ordinal) {
      candidates.emplace_back(
          PriorityOracle(131, static_cast<asc::RandomSubsequence>(repetition),
                         137 + 2 * ordinal),
          ordinal);
    }
    std::sort(candidates.begin(), candidates.end());
    std::vector<std::uint64_t> expected;
    expected.reserve(kSparseCount);
    for (std::size_t position = 0; position < kSparseCount; ++position) {
      expected.push_back(candidates[position].second);
    }
    std::sort(expected.begin(), expected.end());
    for (std::uint64_t ordinal : expected) {
      expected_sparse_checksum = Mix(expected_sparse_checksum, ordinal);
    }
  }
  if (sparse_checksum != expected_sparse_checksum || next_offset != 8329 ||
      !asc_test::ProcessAllocationCountMatches(sparse_allocations, 0)) {
    return false;
  }
  aggregate_checksum = Mix(aggregate_checksum, sparse_checksum);
  std::cout << "sparse adapter=structure-only shape=64x64 count=256"
            << " repetitions=" << kSparseRepetitions
            << " operation_allocation_calls=" << sparse_allocations
            << " elapsed_ns=" << sparse_elapsed.count()
            << " next_offset=" << next_offset << " checksum=" << sparse_checksum
            << " oracle=independent-priority-sort\n";
  return true;
}

}  // namespace

int main() {
#ifdef NDEBUG
  constexpr const char* kConfiguration = "release";
#else
  constexpr const char* kConfiguration = "debug";
#endif
#if defined(_MSC_FULL_VER)
  constexpr std::uint64_t kCompilerVersion = _MSC_FULL_VER;
  std::cout << "compiler=MSVC-" << kCompilerVersion;
#elif defined(__clang_version__)
  std::cout << "compiler=Clang-" << __clang_version__;
#elif defined(__VERSION__)
  std::cout << "compiler=" << __VERSION__;
#else
  std::cout << "compiler=unknown";
#endif
  std::cout << " configuration=" << kConfiguration << '\n';
  std::uint64_t checksum = 1469598103934665603ULL;
  if (!BenchmarkDense(asc::LayoutLeft{}, "left", checksum) ||
      !BenchmarkDense(asc::LayoutRight{}, "right", checksum) ||
      !BenchmarkSparse(checksum) || !BenchmarkAdvancedAdapters(checksum)) {
    return 1;
  }
  std::cout << "aggregate_checksum=" << checksum << '\n';
  return 0;
}
