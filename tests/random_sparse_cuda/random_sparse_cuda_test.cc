#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "../random_cuda/philox_oracle.h"
#include "../random_cuda/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/status.h"
#include "asc/random/providers/sparse_cuda.h"
#include "asc/random/sparse.h"
#include "asc/sparse/coordinate.h"

namespace {

constexpr asc::RandomStream kStructureStream = UINT64_C(0x1029384756abcdef);
constexpr asc::RandomSubsequence kStructureSubsequence =
    UINT64_C(0xfedcba6547382910);
constexpr asc::RandomOffset kStructureOffset = 13;
constexpr asc::RandomStream kValueStream = UINT64_C(0x5555aaaaffff0000);
constexpr asc::RandomSubsequence kValueSubsequence =
    UINT64_C(0x0000ffffaaaa5555);
constexpr asc::RandomOffset kValueOffset = 29;

using asc_random_cuda_test::TestContext;

struct CudaFixture {
  asc::ExecutionContext execution;
  std::unique_ptr<asc::CudaMemoryResource> resource;
};

asc::Result<CudaFixture> MakeFixture() {
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
  return CudaFixture{*execution, std::move(*resource)};
}

template <typename Element>
inline constexpr asc::RandomOffset kWordsPerValue =
    std::same_as<Element, float> ? 1U : 2U;

struct Candidate {
  std::uint64_t priority;
  std::uint64_t ordinal;
};

bool CandidateLess(const Candidate& left, const Candidate& right) {
  return left.priority < right.priority ||
         (left.priority == right.priority && left.ordinal < right.ordinal);
}

Candidate MakeCandidate(std::uint64_t ordinal) {
  const asc::RandomOffset address = kStructureOffset + 2U * ordinal;
  const std::uint64_t high = asc_random_cuda_test::PhiloxWordOracle(
      kStructureStream, kStructureSubsequence, address);
  const std::uint64_t low = asc_random_cuda_test::PhiloxWordOracle(
      kStructureStream, kStructureSubsequence, address + 1U);
  return Candidate{(high << 32U) | low, ordinal};
}

template <typename Element>
Element ExpectedValue(std::uint64_t position) {
  const asc::RandomOffset address =
      kValueOffset + position * kWordsPerValue<Element>;
  if constexpr (std::same_as<Element, float>) {
    return asc_random_cuda_test::Uniform01FloatOracle(
        asc_random_cuda_test::PhiloxWordOracle(kValueStream, kValueSubsequence,
                                               address));
  } else {
    return asc_random_cuda_test::Uniform01DoubleOracle(
        asc_random_cuda_test::PhiloxWordOracle(kValueStream, kValueSubsequence,
                                               address),
        asc_random_cuda_test::PhiloxWordOracle(kValueStream, kValueSubsequence,
                                               address + 1U));
  }
}

template <typename ExtentsType>
std::vector<asc::index_t> ExpectedCoordinates(const ExtentsType& extents,
                                              asc::nnz_t exact_count) {
  const auto logical_size = static_cast<std::uint64_t>(extents.logical_size());
  std::vector<Candidate> candidates;
  candidates.reserve(static_cast<std::size_t>(logical_size));
  for (std::uint64_t ordinal = 0; ordinal < logical_size; ++ordinal) {
    candidates.push_back(MakeCandidate(ordinal));
  }
  std::sort(candidates.begin(), candidates.end(), CandidateLess);
  candidates.resize(static_cast<std::size_t>(exact_count));
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& left, const Candidate& right) {
              return left.ordinal < right.ordinal;
            });

  std::vector<asc::index_t> coordinates(static_cast<std::size_t>(exact_count) *
                                        ExtentsType::kRank);
  for (std::size_t position = 0; position < candidates.size(); ++position) {
    std::uint64_t ordinal = candidates[position].ordinal;
    for (std::size_t reverse = ExtentsType::kRank; reverse > 0; --reverse) {
      const std::size_t dimension = reverse - 1;
      const auto extent =
          static_cast<std::uint64_t>(extents.values()[dimension]);
      coordinates[position * ExtentsType::kRank + dimension] =
          static_cast<asc::index_t>(ordinal % extent);
      ordinal /= extent;
    }
  }
  return coordinates;
}

template <typename Element, asc::SparseExtents ExtentsType>
void CheckCase(const ExtentsType& extents, asc::nnz_t exact_count,
               CudaFixture& fixture, TestContext& test) {
  using Generation = asc::CudaSparseUniform01Generation<Element, ExtentsType>;
  static_assert(!std::is_copy_constructible_v<Generation>);
  static_assert(std::is_nothrow_move_constructible_v<Generation>);

  auto generated = asc::CudaGenerateSparseUniform01<Element>(
      fixture.execution, extents, exact_count, *fixture.resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_M7_CUDA_CHECK(test, generated.ok());
  if (!generated.ok()) {
    return;
  }
  const auto logical_size =
      static_cast<asc::RandomOffset>(extents.logical_size());
  const asc::RandomOffset expected_structure =
      exact_count == 0 ? kStructureOffset
                       : kStructureOffset + 2U * logical_size;
  const asc::RandomOffset expected_value =
      kValueOffset +
      static_cast<asc::RandomOffset>(exact_count) * kWordsPerValue<Element>;
  ASC_M7_CUDA_EQ(test, generated->next_structure_offset, expected_structure);
  ASC_M7_CUDA_EQ(test, generated->next_value_offset, expected_value);

  Generation moved(std::move(*generated));
  ASC_M7_CUDA_CHECK(test, moved.completion.Wait().ok());
  auto view = moved.array.view();
  ASC_M7_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  ASC_M7_CUDA_EQ(test, view->nnz(), exact_count);
  for (std::size_t dimension = 0; dimension < ExtentsType::kRank; ++dimension) {
    ASC_M7_CUDA_EQ(test, view->shape()[dimension], extents.values()[dimension]);
  }
  auto coordinates = asc_random_cuda_test::Download<asc::index_t>(
      view->coordinate_data(),
      static_cast<std::size_t>(exact_count) * ExtentsType::kRank,
      asc::MemorySpace::kDevice, fixture.execution);
  auto values = asc_random_cuda_test::Download<Element>(
      view->value_data(), static_cast<std::size_t>(exact_count),
      asc::MemorySpace::kDevice, fixture.execution);
  ASC_M7_CUDA_CHECK(test, coordinates.ok());
  ASC_M7_CUDA_CHECK(test, values.ok());
  if (!coordinates.ok() || !values.ok()) {
    return;
  }

  const auto expected_coordinates = ExpectedCoordinates(extents, exact_count);
  ASC_M7_CUDA_CHECK(test, *coordinates == expected_coordinates);
  for (std::size_t position = 0; position < values->size(); ++position) {
    ASC_M7_CUDA_BIT_EQ(test, (*values)[position],
                       ExpectedValue<Element>(position));
  }

  asc::HostMemoryResource host;
  auto cpu = asc::GenerateSparseUniform01<Element>(
      asc::ExecutionContext::Serial(), extents, exact_count, host,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_M7_CUDA_CHECK(test, cpu.ok());
  if (!cpu.ok()) {
    return;
  }
  auto cpu_view = cpu->array.view();
  ASC_M7_CUDA_CHECK(test, cpu_view.ok());
  if (!cpu_view.ok()) {
    return;
  }
  ASC_M7_CUDA_EQ(test, cpu->next_structure_offset, moved.next_structure_offset);
  ASC_M7_CUDA_EQ(test, cpu->next_value_offset, moved.next_value_offset);
  for (asc::nnz_t position = 0; position < exact_count; ++position) {
    auto cpu_coordinate = cpu_view->CoordinateAt(position);
    auto cpu_value = cpu_view->ValueAt(position);
    ASC_M7_CUDA_CHECK(test, cpu_coordinate.ok());
    ASC_M7_CUDA_CHECK(test, cpu_value.ok());
    if (cpu_coordinate.ok() && cpu_value.ok()) {
      for (std::size_t dimension = 0; dimension < ExtentsType::kRank;
           ++dimension) {
        ASC_M7_CUDA_EQ(test, (*cpu_coordinate)[dimension],
                       (*coordinates)[static_cast<std::size_t>(position) *
                                          ExtentsType::kRank +
                                      dimension]);
      }
      ASC_M7_CUDA_BIT_EQ(test, **cpu_value,
                         (*values)[static_cast<std::size_t>(position)]);
    }
  }
}

class FailSecondAllocationResource final : public asc::MemoryResource {
 public:
  explicit FailSecondAllocationResource(asc::MemoryResource& backing)
      : backing_(backing) {}

  asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kDevice;
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_calls;
    if (allocation_calls == 2) {
      return asc::Status(asc::ErrorCode::kAllocation,
                         "Injected second allocation failure");
    }
    auto pointer = backing_.Allocate(bytes, alignment);
    if (pointer.ok()) {
      ++live_allocations;
    }
    return pointer;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    ++deallocation_calls;
    --live_allocations;
    backing_.Deallocate(pointer, bytes, alignment);
  }

  int allocation_calls = 0;
  int deallocation_calls = 0;
  int live_allocations = 0;

 private:
  asc::MemoryResource& backing_;
};

void CheckFailures(CudaFixture& fixture, TestContext& test) {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  auto shape = Shape::Create(2, 3);
  ASC_M7_CUDA_CHECK(test, shape.ok());
  if (!shape.ok()) {
    return;
  }

  const auto negative = asc::CudaGenerateSparseUniform01<float>(
      fixture.execution, *shape, -1, *fixture.resource, kStructureStream,
      kStructureSubsequence, kStructureOffset, kValueStream, kValueSubsequence,
      kValueOffset);
  const auto too_many = asc::CudaGenerateSparseUniform01<float>(
      fixture.execution, *shape, 7, *fixture.resource, kStructureStream,
      kStructureSubsequence, kStructureOffset, kValueStream, kValueSubsequence,
      kValueOffset);
  const auto same_domain = asc::CudaGenerateSparseUniform01<float>(
      fixture.execution, *shape, 1, *fixture.resource, kStructureStream,
      kStructureSubsequence, kStructureOffset, kStructureStream,
      kStructureSubsequence, kValueOffset);
  ASC_M7_CUDA_CHECK(test, !negative.ok());
  ASC_M7_CUDA_CHECK(test, !too_many.ok());
  ASC_M7_CUDA_CHECK(test, !same_domain.ok());

  const auto structure_overflow = asc::CudaGenerateSparseUniform01<float>(
      fixture.execution, *shape, 1, *fixture.resource, kStructureStream,
      kStructureSubsequence,
      std::numeric_limits<asc::RandomOffset>::max() - 10U, kValueStream,
      kValueSubsequence, kValueOffset);
  const auto value_overflow = asc::CudaGenerateSparseUniform01<double>(
      fixture.execution, *shape, 2, *fixture.resource, kStructureStream,
      kStructureSubsequence, kStructureOffset, kValueStream, kValueSubsequence,
      std::numeric_limits<asc::RandomOffset>::max() - 2U);
  ASC_M7_CUDA_CHECK(test, !structure_overflow.ok());
  ASC_M7_CUDA_CHECK(test, !value_overflow.ok());
  if (!structure_overflow.ok()) {
    ASC_M7_CUDA_EQ(test, structure_overflow.status().code(),
                   asc::ErrorCode::kOverflow);
  }
  if (!value_overflow.ok()) {
    ASC_M7_CUDA_EQ(test, value_overflow.status().code(),
                   asc::ErrorCode::kOverflow);
  }

  asc::HostMemoryResource host;
  const auto host_resource = asc::CudaGenerateSparseUniform01<float>(
      fixture.execution, *shape, 1, host, kStructureStream,
      kStructureSubsequence, kStructureOffset, kValueStream, kValueSubsequence,
      kValueOffset);
  const auto serial = asc::CudaGenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *shape, 1, *fixture.resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_M7_CUDA_CHECK(test, !host_resource.ok());
  ASC_M7_CUDA_CHECK(test, !serial.ok());

  FailSecondAllocationResource failing(*fixture.resource);
  const auto allocation_failure = asc::CudaGenerateSparseUniform01<float>(
      fixture.execution, *shape, 2, failing, kStructureStream,
      kStructureSubsequence, kStructureOffset, kValueStream, kValueSubsequence,
      kValueOffset);
  ASC_M7_CUDA_CHECK(test, !allocation_failure.ok());
  ASC_M7_CUDA_EQ(test, failing.allocation_calls, 2);
  ASC_M7_CUDA_EQ(test, failing.deallocation_calls, 1);
  ASC_M7_CUDA_EQ(test, failing.live_allocations, 0);

  using RankNine = asc::Extents<1, 1, 1, 1, 1, 1, 1, 1, 1>;
  auto rank_nine = RankNine::Create();
  ASC_M7_CUDA_CHECK(test, rank_nine.ok());
  if (rank_nine.ok()) {
    const auto unsupported = asc::CudaGenerateSparseUniform01<float>(
        fixture.execution, *rank_nine, 1, *fixture.resource, kStructureStream,
        kStructureSubsequence, kStructureOffset, kValueStream,
        kValueSubsequence, kValueOffset);
    ASC_M7_CUDA_CHECK(test, !unsupported.ok());
    if (!unsupported.ok()) {
      ASC_M7_CUDA_EQ(test, unsupported.status().code(),
                     asc::ErrorCode::kUnsupported);
    }
  }
}

}  // namespace

int main() {
  if (!asc_random_cuda_test::HasCudaDevice()) {
    return asc_random_cuda_test::kSkipReturnCode;
  }
  TestContext test;
  auto fixture = MakeFixture();
  ASC_M7_CUDA_CHECK(test, fixture.ok());
  if (!fixture.ok()) {
    return test.Finish();
  }

  const auto scalar = asc::Extents<>::Create();
  const auto vector = asc::Extents<asc::kDynamicExtent>::Create(7);
  const auto matrix =
      asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>::Create(3, 4);
  const auto empty =
      asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>::Create(0, 5);
  const auto cube = asc::Extents<2, 3, 2>::Create();
  ASC_M7_CUDA_CHECK(test, scalar.ok());
  ASC_M7_CUDA_CHECK(test, vector.ok());
  ASC_M7_CUDA_CHECK(test, matrix.ok());
  ASC_M7_CUDA_CHECK(test, empty.ok());
  ASC_M7_CUDA_CHECK(test, cube.ok());
  if (scalar.ok()) {
    CheckCase<float>(*scalar, 0, *fixture, test);
    CheckCase<double>(*scalar, 1, *fixture, test);
  }
  if (vector.ok()) {
    CheckCase<float>(*vector, 3, *fixture, test);
  }
  if (matrix.ok()) {
    CheckCase<float>(*matrix, 12, *fixture, test);
    CheckCase<double>(*matrix, 5, *fixture, test);
  }
  if (empty.ok()) {
    CheckCase<double>(*empty, 0, *fixture, test);
  }
  if (cube.ok()) {
    CheckCase<double>(*cube, 4, *fixture, test);
  }
  CheckFailures(*fixture, test);
  return test.Finish();
}
