#include <cuda_runtime_api.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "../../tests/random_cuda/philox_oracle.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/providers/cuda.h"
#include "asc/random/providers/dense_cuda.h"
#include "asc/random/providers/sparse_cuda.h"

namespace {

constexpr std::size_t kWarmup = 3;
constexpr std::size_t kRepetitions = 12;
constexpr asc::RandomStream kStream = 0x0123456789ABCDEFULL;
constexpr asc::RandomSubsequence kSubsequence = 0xFEDCBA9876543210ULL;
constexpr asc::RandomOffset kOffset = 37;

class CountingResource final : public asc::MemoryResource {
 public:
  explicit CountingResource(asc::MemoryResource& upstream) noexcept
      : upstream_(upstream) {}

  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return upstream_.space();
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_calls_;
    allocated_bytes_ += bytes;
    auto result = upstream_.Allocate(bytes, alignment);
    if (result.ok() && *result != nullptr) {
      ++live_;
    }
    return result;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    if (pointer != nullptr) {
      ++deallocation_calls_;
      --live_;
    }
    upstream_.Deallocate(pointer, bytes, alignment);
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
  [[nodiscard]] std::size_t live() const noexcept { return live_; }

 private:
  asc::MemoryResource& upstream_;
  std::size_t allocation_calls_ = 0;
  std::size_t deallocation_calls_ = 0;
  std::size_t allocated_bytes_ = 0;
  std::size_t live_ = 0;
};

std::uint64_t Mix(std::uint64_t checksum, std::uint64_t value) {
  return (checksum ^ value) * 1099511628211ULL;
}

template <typename Operation>
std::int64_t Measure(Operation&& operation) {
  for (std::size_t iteration = 0; iteration < kWarmup; ++iteration) {
    if (!operation()) {
      return -1;
    }
  }
  const auto begin = std::chrono::steady_clock::now();
  for (std::size_t iteration = 0; iteration < kRepetitions; ++iteration) {
    if (!operation()) {
      return -1;
    }
  }
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now() - begin)
      .count();
}

void Report(std::string_view operation, std::int64_t elapsed,
            std::size_t logical_items, std::uint64_t checksum,
            std::size_t allocation_calls, std::size_t allocated_bytes,
            std::string_view allocation_scope) {
  const long double seconds = static_cast<long double>(elapsed) / 1.0e9L;
  const long double throughput =
      seconds > 0
          ? static_cast<long double>(logical_items) * kRepetitions / seconds
          : 0;
  std::cout << "operation=" << operation << " warmups=" << kWarmup
            << " repetitions=" << kRepetitions << " elapsed_ns=" << elapsed
            << " logical_items_per_repetition=" << logical_items
            << " logical_items_per_second=" << static_cast<double>(throughput)
            << " checksum=" << checksum
            << " asc_resource_allocation_calls=" << allocation_calls
            << " asc_resource_allocated_bytes=" << allocated_bytes
            << " allocation_scope=" << allocation_scope << '\n';
}

bool BenchmarkRaw(const asc::ExecutionContext& execution,
                  asc::MemoryResource& resource) {
  constexpr std::size_t kWords = 1U << 20;
  constexpr std::size_t kBytes = kWords * sizeof(std::uint32_t);
  auto device = asc::Buffer::Allocate(resource, kBytes, alignof(std::uint32_t));
  if (!device.ok()) {
    return false;
  }
  auto destination = device->mutable_view();
  if (!destination.ok()) {
    return false;
  }
  const auto operation = [&]() {
    auto generation = asc::CudaFillPhilox4x32(execution, *destination, kWords,
                                              kStream, kSubsequence, kOffset);
    return generation.ok() && generation->next_offset == kOffset + kWords &&
           generation->completion.Wait().ok();
  };
  const std::int64_t elapsed = Measure(operation);
  std::vector<std::uint32_t> host(kWords);
  if (elapsed < 0 || cudaMemcpy(host.data(), device->data(), kBytes,
                                cudaMemcpyDeviceToHost) != cudaSuccess) {
    return false;
  }
  std::uint64_t checksum = 1469598103934665603ULL;
  for (std::size_t index = 0; index < kWords; ++index) {
    const std::uint32_t expected = asc_random_cuda_test::PhiloxWordOracle(
        kStream, kSubsequence, kOffset + index);
    if (host[index] != expected) {
      return false;
    }
    checksum = Mix(checksum, host[index]);
  }
  Report("raw_philox4x32", elapsed, kWords, checksum, 0, 0,
         "caller-storage setup excluded; operation allocates zero");
  return true;
}

bool BenchmarkDense(const asc::ExecutionContext& execution,
                    asc::MemoryResource& resource) {
  constexpr std::array<asc::extent_t, 2> kExtents = {1024, 1024};
  constexpr std::size_t kElements = 1024U * 1024U;
  constexpr std::size_t kBytes = kElements * sizeof(float);
  auto mapping = asc::DenseLayout<2>::Create(
      std::span<const asc::extent_t, 2>(kExtents), asc::LayoutLeft{});
  auto device = asc::Buffer::Allocate(resource, kBytes, alignof(float));
  if (!mapping.ok() || !device.ok()) {
    return false;
  }
  auto view = asc::DenseView<float, 2>::Create(
      static_cast<float*>(device->data()), *mapping, asc::MemorySpace::kDevice);
  if (!view.ok()) {
    return false;
  }
  const auto operation = [&]() {
    auto generation = asc::CudaFillDenseUniform01(execution, *view, kStream,
                                                  kSubsequence, kOffset);
    return generation.ok() && generation->next_offset == kOffset + kElements &&
           generation->completion.Wait().ok();
  };
  const std::int64_t elapsed = Measure(operation);
  std::vector<float> host(kElements);
  if (elapsed < 0 || cudaMemcpy(host.data(), device->data(), kBytes,
                                cudaMemcpyDeviceToHost) != cudaSuccess) {
    return false;
  }
  std::uint64_t checksum = 1469598103934665603ULL;
  for (std::size_t index = 0; index < kElements; ++index) {
    const float expected = asc_random_cuda_test::Uniform01FloatOracle(
        asc_random_cuda_test::PhiloxWordOracle(kStream, kSubsequence,
                                               kOffset + index));
    if (std::bit_cast<std::uint32_t>(host[index]) !=
        std::bit_cast<std::uint32_t>(expected)) {
      return false;
    }
    checksum = Mix(checksum, std::bit_cast<std::uint32_t>(host[index]));
  }
  Report("dense_uniform01_float", elapsed, kElements, checksum, 0, 0,
         "caller-storage setup excluded; operation allocates zero");
  return true;
}

struct Candidate {
  std::uint64_t priority;
  std::uint64_t ordinal;
};

bool BenchmarkSparse(const asc::ExecutionContext& execution,
                     CountingResource& resource) {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  constexpr asc::extent_t kSide = 128;
  constexpr asc::nnz_t kCount = 128;
  auto shape = Shape::Create(kSide, kSide);
  if (!shape.ok()) {
    return false;
  }
  const std::size_t allocation_calls_before = resource.allocation_calls();
  const std::size_t allocated_bytes_before = resource.allocated_bytes();
  const auto operation = [&]() {
    auto generation = asc::CudaGenerateSparseUniform01<float>(
        execution, *shape, kCount, resource, kStream, kSubsequence, kOffset,
        kStream + 1U, kSubsequence + 1U, kOffset + 1U);
    return generation.ok() && generation->completion.Wait().ok();
  };
  const std::int64_t elapsed = Measure(operation);
  if (elapsed < 0 || resource.live() != 0) {
    return false;
  }

  auto verification = asc::CudaGenerateSparseUniform01<float>(
      execution, *shape, kCount, resource, kStream, kSubsequence, kOffset,
      kStream + 1U, kSubsequence + 1U, kOffset + 1U);
  if (!verification.ok() || !verification->completion.Wait().ok()) {
    return false;
  }
  auto view = verification->array.view();
  if (!view.ok()) {
    return false;
  }
  std::vector<asc::index_t> coordinates(static_cast<std::size_t>(kCount) * 2U);
  std::vector<float> values(static_cast<std::size_t>(kCount));
  if (cudaMemcpy(coordinates.data(), view->coordinates(),
                 coordinates.size() * sizeof(asc::index_t),
                 cudaMemcpyDeviceToHost) != cudaSuccess ||
      cudaMemcpy(values.data(), view->values(), values.size() * sizeof(float),
                 cudaMemcpyDeviceToHost) != cudaSuccess) {
    return false;
  }

  std::vector<Candidate> candidates;
  candidates.reserve(static_cast<std::size_t>(kSide * kSide));
  for (std::uint64_t ordinal = 0;
       ordinal < static_cast<std::uint64_t>(kSide * kSide); ++ordinal) {
    const std::uint64_t high = asc_random_cuda_test::PhiloxWordOracle(
        kStream, kSubsequence, kOffset + 2U * ordinal);
    const std::uint64_t low = asc_random_cuda_test::PhiloxWordOracle(
        kStream, kSubsequence, kOffset + 2U * ordinal + 1U);
    candidates.push_back(Candidate{(high << 32U) | low, ordinal});
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& left, const Candidate& right) {
              return std::pair{left.priority, left.ordinal} <
                     std::pair{right.priority, right.ordinal};
            });
  candidates.resize(static_cast<std::size_t>(kCount));
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& left, const Candidate& right) {
              return left.ordinal < right.ordinal;
            });

  std::uint64_t checksum = 1469598103934665603ULL;
  for (std::size_t position = 0; position < candidates.size(); ++position) {
    const asc::index_t expected_row =
        static_cast<asc::index_t>(candidates[position].ordinal / kSide);
    const asc::index_t expected_column =
        static_cast<asc::index_t>(candidates[position].ordinal % kSide);
    const float expected_value = asc_random_cuda_test::Uniform01FloatOracle(
        asc_random_cuda_test::PhiloxWordOracle(kStream + 1U, kSubsequence + 1U,
                                               kOffset + 1U + position));
    if (coordinates[2U * position] != expected_row ||
        coordinates[2U * position + 1U] != expected_column ||
        std::bit_cast<std::uint32_t>(values[position]) !=
            std::bit_cast<std::uint32_t>(expected_value)) {
      return false;
    }
    checksum =
        Mix(checksum, static_cast<std::uint64_t>(coordinates[2U * position]));
    checksum = Mix(checksum,
                   static_cast<std::uint64_t>(coordinates[2U * position + 1U]));
    checksum = Mix(checksum, std::bit_cast<std::uint32_t>(values[position]));
  }
  const std::size_t timed_allocation_calls =
      resource.allocation_calls() - allocation_calls_before - 2U;
  const std::size_t timed_allocated_bytes =
      resource.allocated_bytes() - allocated_bytes_before -
      coordinates.size() * sizeof(asc::index_t) - values.size() * sizeof(float);
  Report("sparse_uniform01_float", elapsed,
         static_cast<std::size_t>(kSide * kSide), checksum,
         timed_allocation_calls, timed_allocated_bytes,
         "two canonical output allocations included per generation");
  return timed_allocation_calls == 2U * (kWarmup + kRepetitions);
}

}  // namespace

int main() {
  int runtime_version = 0;
  int driver_version = 0;
  cudaDeviceProp properties{};
  if (cudaRuntimeGetVersion(&runtime_version) != cudaSuccess ||
      cudaDriverGetVersion(&driver_version) != cudaSuccess ||
      cudaGetDeviceProperties(&properties, 0) != cudaSuccess) {
    return 77;
  }
  std::cout << "gpu=\"" << properties.name
            << "\" compute_capability=" << properties.major << '.'
            << properties.minor << " runtime=" << runtime_version
            << " driver=" << driver_version
#ifdef NDEBUG
            << " configuration=release"
#else
            << " configuration=debug"
#endif
#if defined(__clang_version__)
            << " compiler=\"Clang " << __clang_version__ << "\""
#elif defined(__VERSION__)
            << " compiler=\"" << __VERSION__ << "\""
#endif
            << '\n';

  auto execution =
      asc::CreateCudaExecutionContext(0, asc::Determinism::kDeterministic);
  auto device = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  if (!execution.ok() || !device.ok()) {
    return 1;
  }
  CountingResource resource(**device);
  if (!BenchmarkRaw(*execution, resource) ||
      !BenchmarkDense(*execution, resource) ||
      !BenchmarkSparse(*execution, resource)) {
    return 2;
  }
  return resource.live() == 0 ? 0 : 3;
}
