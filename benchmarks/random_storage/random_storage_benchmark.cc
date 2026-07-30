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
      !BenchmarkSparse(checksum)) {
    return 1;
  }
  std::cout << "aggregate_checksum=" << checksum << '\n';
  return 0;
}
