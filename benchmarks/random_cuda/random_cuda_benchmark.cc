#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "../../tests/random_cuda/philox_oracle.h"
#include "../../tests/random_cuda/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/array.h"
#include "asc/random/providers/cuda.h"
#include "asc/random/providers/dense_cuda.h"
#include "asc/random/providers/sparse_cuda.h"

namespace {

constexpr asc::RandomStream kStream = UINT64_C(0x123456789abcdef0);
constexpr asc::RandomSubsequence kSubsequence = UINT64_C(0x0fedcba987654321);
constexpr asc::RandomOffset kOffset = 17;

struct SparseCandidate {
  std::uint64_t priority;
  std::uint64_t ordinal;
};

std::vector<asc::index_t> ExpectedSparseCoordinates(std::uint64_t rows,
                                                    std::uint64_t columns,
                                                    std::size_t count) {
  std::vector<SparseCandidate> candidates;
  candidates.reserve(static_cast<std::size_t>(rows * columns));
  for (std::uint64_t ordinal = 0; ordinal < rows * columns; ++ordinal) {
    const asc::RandomOffset address = kOffset + 2U * ordinal;
    const std::uint64_t high =
        asc_random_cuda_test::PhiloxWordOracle(kStream, kSubsequence, address);
    const std::uint64_t low = asc_random_cuda_test::PhiloxWordOracle(
        kStream, kSubsequence, address + 1U);
    candidates.push_back({(high << 32U) | low, ordinal});
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const SparseCandidate& left, const SparseCandidate& right) {
              return left.priority < right.priority ||
                     (left.priority == right.priority &&
                      left.ordinal < right.ordinal);
            });
  candidates.resize(count);
  std::sort(candidates.begin(), candidates.end(),
            [](const SparseCandidate& left, const SparseCandidate& right) {
              return left.ordinal < right.ordinal;
            });
  std::vector<asc::index_t> coordinates(count * 2U);
  for (std::size_t position = 0; position < count; ++position) {
    coordinates[position * 2U] =
        static_cast<asc::index_t>(candidates[position].ordinal / columns);
    coordinates[position * 2U + 1U] =
        static_cast<asc::index_t>(candidates[position].ordinal % columns);
  }
  return coordinates;
}

struct Fixture {
  asc::ExecutionContext execution;
  std::unique_ptr<asc::CudaMemoryResource> resource;
};

asc::Result<Fixture> MakeFixture() {
  auto execution =
      asc::CreateCudaExecutionContext(asc_random_cuda_test::CudaDevice());
  if (!execution.ok()) {
    return execution.status();
  }
  auto resource = asc::CudaMemoryResource::Create(
      asc_random_cuda_test::CudaDevice(), asc::MemorySpace::kDevice);
  if (!resource.ok()) {
    return resource.status();
  }
  return Fixture{*execution, std::move(*resource)};
}

int BenchmarkRaw(Fixture& fixture) {
  constexpr std::uint64_t kCount = UINT64_C(1) << 20U;
  constexpr int kWarmups = 3;
  constexpr int kRepetitions = 20;
  auto storage = asc::Buffer::Allocate(
      *fixture.resource,
      static_cast<std::size_t>(kCount) * sizeof(std::uint32_t),
      alignof(std::uint32_t));
  if (!storage.ok()) {
    return 1;
  }
  const auto run_once = [&]() {
    auto generated = asc::CudaFillPhilox4x32(
        fixture.execution, static_cast<std::uint32_t*>(storage->data()), kCount,
        kStream, kSubsequence, kOffset);
    return generated.ok() ? generated->completion.Wait() : generated.status();
  };
  for (int repetition = 0; repetition < kWarmups; ++repetition) {
    if (!run_once().ok()) {
      return 2;
    }
  }
  const auto begin = std::chrono::steady_clock::now();
  for (int repetition = 0; repetition < kRepetitions; ++repetition) {
    if (!run_once().ok()) {
      return 3;
    }
  }
  const auto end = std::chrono::steady_clock::now();
  auto words = asc_random_cuda_test::Download<std::uint32_t>(*storage,
                                                             fixture.execution);
  if (!words.ok()) {
    return 4;
  }
  std::uint64_t checksum = 0;
  std::uint64_t expected = 0;
  for (std::size_t index = 0; index < words->size(); ++index) {
    checksum += (*words)[index];
    expected += asc_random_cuda_test::PhiloxWordOracle(kStream, kSubsequence,
                                                       kOffset + index);
  }
  if (checksum != expected) {
    return 5;
  }
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
          .count();
  std::cout << "operation=raw_philox words=" << kCount
            << " warmups=" << kWarmups << " repetitions=" << kRepetitions
            << " elapsed_us=" << elapsed << " checksum=" << checksum << '\n';
  return 0;
}

int BenchmarkDense(Fixture& fixture) {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  constexpr int kWarmups = 3;
  constexpr int kRepetitions = 20;
  auto shape = Shape::Create(1024, 1024);
  if (!shape.ok()) {
    return 1;
  }
  auto array = asc::DenseArray<float, Shape>::CreateUninitialized(
      *shape, *fixture.resource, asc::LayoutRight{});
  if (!array.ok()) {
    return 2;
  }
  auto view = array->view();
  if (!view.ok()) {
    return 3;
  }
  const auto run_once = [&]() {
    auto generated = asc::CudaFillDenseUniform01(
        fixture.execution, *view, kStream, kSubsequence, kOffset);
    return generated.ok() ? generated->completion.Wait() : generated.status();
  };
  for (int repetition = 0; repetition < kWarmups; ++repetition) {
    if (!run_once().ok()) {
      return 4;
    }
  }
  const auto begin = std::chrono::steady_clock::now();
  for (int repetition = 0; repetition < kRepetitions; ++repetition) {
    if (!run_once().ok()) {
      return 5;
    }
  }
  const auto end = std::chrono::steady_clock::now();
  asc::HostMemoryResource host;
  auto result = array->Clone(host, fixture.execution);
  if (!result.ok()) {
    return 6;
  }
  auto result_view = result->view();
  if (!result_view.ok()) {
    return 7;
  }
  long double checksum = 0;
  long double expected = 0;
  const std::uint64_t logical_size =
      static_cast<std::uint64_t>(shape->logical_size());
  for (std::uint64_t ordinal = 0; ordinal < logical_size; ++ordinal) {
    const asc::index_t row = static_cast<asc::index_t>(ordinal % 1024U);
    const asc::index_t column = static_cast<asc::index_t>(ordinal / 1024U);
    const std::array<asc::index_t, 2> coordinate = {row, column};
    auto value = result_view->At(coordinate);
    if (!value.ok()) {
      return 8;
    }
    checksum += **value;
    expected += asc_random_cuda_test::Uniform01FloatOracle(
        asc_random_cuda_test::PhiloxWordOracle(kStream, kSubsequence,
                                               kOffset + ordinal));
  }
  if (checksum != expected) {
    return 9;
  }
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
          .count();
  std::cout << "operation=dense_uniform01 scalar=float shape=1024x1024"
            << " layout=right warmups=" << kWarmups
            << " repetitions=" << kRepetitions << " elapsed_us=" << elapsed
            << " checksum=" << static_cast<double>(checksum) << '\n';
  return 0;
}

int BenchmarkSparse(Fixture& fixture) {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  constexpr asc::nnz_t kCount = 128;
  constexpr int kWarmups = 1;
  constexpr int kRepetitions = 5;
  auto shape = Shape::Create(128, 128);
  if (!shape.ok()) {
    return 1;
  }
  const auto run_once = [&]() -> asc::Status {
    auto generated = asc::CudaGenerateSparseUniform01<float>(
        fixture.execution, *shape, kCount, *fixture.resource, kStream,
        kSubsequence, kOffset, kStream + 1U, kSubsequence, kOffset);
    if (!generated.ok()) {
      return generated.status();
    }
    return generated->completion.Wait();
  };
  for (int repetition = 0; repetition < kWarmups; ++repetition) {
    if (!run_once().ok()) {
      return 2;
    }
  }
  const auto begin = std::chrono::steady_clock::now();
  for (int repetition = 0; repetition < kRepetitions; ++repetition) {
    if (!run_once().ok()) {
      return 3;
    }
  }
  const auto end = std::chrono::steady_clock::now();

  auto generated = asc::CudaGenerateSparseUniform01<float>(
      fixture.execution, *shape, kCount, *fixture.resource, kStream,
      kSubsequence, kOffset, kStream + 1U, kSubsequence, kOffset);
  if (!generated.ok() || !generated->completion.Wait().ok()) {
    return 4;
  }
  auto view = generated->array.view();
  if (!view.ok()) {
    return 5;
  }
  auto coordinates = asc_random_cuda_test::Download<asc::index_t>(
      view->coordinate_data(), static_cast<std::size_t>(kCount) * 2U,
      asc::MemorySpace::kDevice, fixture.execution);
  auto values = asc_random_cuda_test::Download<float>(
      view->value_data(), static_cast<std::size_t>(kCount),
      asc::MemorySpace::kDevice, fixture.execution);
  if (!coordinates.ok() || !values.ok()) {
    return 6;
  }
  const auto expected_coordinates =
      ExpectedSparseCoordinates(128, 128, static_cast<std::size_t>(kCount));
  if (*coordinates != expected_coordinates) {
    return 7;
  }
  for (std::size_t position = 0; position < values->size(); ++position) {
    const float expected = asc_random_cuda_test::Uniform01FloatOracle(
        asc_random_cuda_test::PhiloxWordOracle(kStream + 1U, kSubsequence,
                                               kOffset + position));
    if (std::bit_cast<std::uint32_t>((*values)[position]) !=
        std::bit_cast<std::uint32_t>(expected)) {
      return 8;
    }
  }
  std::uint64_t coordinate_checksum = 0;
  long double value_checksum = 0;
  for (const asc::index_t coordinate : *coordinates) {
    coordinate_checksum += static_cast<std::uint64_t>(coordinate);
  }
  for (const float value : *values) {
    value_checksum += value;
  }
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
          .count();
  std::cout << "operation=sparse_uniform01_including_output_allocation"
            << " scalar=float shape=128x128 count=" << kCount
            << " warmups=" << kWarmups << " repetitions=" << kRepetitions
            << " elapsed_us=" << elapsed
            << " coordinate_checksum=" << coordinate_checksum
            << " value_checksum=" << static_cast<double>(value_checksum)
            << '\n';
  return 0;
}

}  // namespace

int main() {
  if (!asc_random_cuda_test::HasCudaDevice()) {
    return asc_random_cuda_test::kSkipReturnCode;
  }
  auto fixture = MakeFixture();
  if (!fixture.ok()) {
    return 1;
  }
  const int raw = BenchmarkRaw(*fixture);
  const int dense = BenchmarkDense(*fixture);
  const int sparse = BenchmarkSparse(*fixture);
  return raw == 0 && dense == 0 && sparse == 0 ? 0 : 2;
}
