#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <tuple>
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
#include "asc/random/distribution.h"
#include "asc/random/generator.h"
#include "asc/random/providers/sparse_cuda.h"
#include "asc/random/sparse.h"
#include "asc/sparse/compressed.h"
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

struct Fixture {
  asc::ExecutionContext execution;
  std::unique_ptr<asc::CudaMemoryResource> resource;
};

asc::Result<Fixture> MakeFixture() {
  auto execution =
      asc::CreateCudaExecutionContext(asc_random_cuda_test::kCudaDevice);
  if (!execution.ok()) {
    return execution.status();
  }
  auto resource = asc::CudaMemoryResource::Create(
      asc_random_cuda_test::kCudaDevice, asc::MemorySpace::kDevice);
  if (!resource.ok()) {
    return resource.status();
  }
  return Fixture{*execution, std::move(*resource)};
}

struct Candidate {
  std::uint64_t priority;
  std::uint64_t ordinal;
};

bool CandidateLess(const Candidate& left, const Candidate& right) {
  return std::tie(left.priority, left.ordinal) <
         std::tie(right.priority, right.ordinal);
}

Candidate MakeCandidate(
    std::uint64_t ordinal, asc::RandomStream stream = kStructureStream,
    asc::RandomSubsequence subsequence = kStructureSubsequence,
    asc::RandomOffset offset = kStructureOffset) {
  const asc::RandomOffset address = offset + 2U * ordinal;
  const std::uint64_t high =
      asc_random_cuda_test::PhiloxWordOracle(stream, subsequence, address);
  const std::uint64_t low =
      asc_random_cuda_test::PhiloxWordOracle(stream, subsequence, address + 1U);
  return Candidate{(high << 32U) | low, ordinal};
}

template <typename ExtentsType>
std::vector<asc::index_t> ExpectedCoordinates(
    const ExtentsType& extents, asc::nnz_t count,
    asc::RandomStream stream = kStructureStream,
    asc::RandomSubsequence subsequence = kStructureSubsequence,
    asc::RandomOffset offset = kStructureOffset) {
  std::vector<Candidate> candidates;
  candidates.reserve(static_cast<std::size_t>(extents.logical_size()));
  for (std::uint64_t ordinal = 0;
       ordinal < static_cast<std::uint64_t>(extents.logical_size());
       ++ordinal) {
    candidates.push_back(MakeCandidate(ordinal, stream, subsequence, offset));
  }
  std::sort(candidates.begin(), candidates.end(), CandidateLess);
  candidates.resize(static_cast<std::size_t>(count));
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& left, const Candidate& right) {
              return left.ordinal < right.ordinal;
            });

  std::vector<asc::index_t> coordinates(static_cast<std::size_t>(count) *
                                        ExtentsType::kRank);
  for (std::size_t position = 0; position < candidates.size(); ++position) {
    std::uint64_t ordinal = candidates[position].ordinal;
    for (std::size_t reverse = ExtentsType::kRank; reverse > 0; --reverse) {
      const std::size_t dimension = reverse - 1U;
      const std::uint64_t extent =
          static_cast<std::uint64_t>(extents.values()[dimension]);
      coordinates[position * ExtentsType::kRank + dimension] =
          static_cast<asc::index_t>(ordinal % extent);
      ordinal /= extent;
    }
  }
  return coordinates;
}

template <typename Element>
Element ExpectedValue(std::uint64_t position,
                      asc::RandomStream stream = kValueStream,
                      asc::RandomSubsequence subsequence = kValueSubsequence,
                      asc::RandomOffset offset = kValueOffset) {
  if constexpr (std::same_as<Element, float>) {
    return asc_random_cuda_test::Uniform01FloatOracle(
        asc_random_cuda_test::PhiloxWordOracle(stream, subsequence,
                                               offset + position));
  } else {
    return asc_random_cuda_test::Uniform01DoubleOracle(
        asc_random_cuda_test::PhiloxWordOracle(stream, subsequence,
                                               offset + 2U * position),
        asc_random_cuda_test::PhiloxWordOracle(stream, subsequence,
                                               offset + 2U * position + 1U));
  }
}

class TrackingDeviceResource final : public asc::MemoryResource {
 public:
  explicit TrackingDeviceResource(asc::MemoryResource& backing)
      : backing_(backing) {}

  asc::MemorySpace space() const noexcept override { return backing_.space(); }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    const std::size_t call = attempts_++;
    if (call == failure_call_) {
      return asc::Status(
          asc::ErrorCode::kAllocation,
          "Injected Random Sparse CUDA device allocation failure");
    }
    auto result = backing_.Allocate(bytes, alignment);
    if (result.ok() && *result != nullptr) {
      ++live_;
    }
    return result;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    if (pointer != nullptr) {
      --live_;
      ++deallocations_;
    }
    backing_.Deallocate(pointer, bytes, alignment);
  }

  void FailOnCall(std::size_t call) noexcept { failure_call_ = call; }
  std::size_t attempts() const noexcept { return attempts_; }
  std::size_t live() const noexcept { return live_; }
  std::size_t deallocations() const noexcept { return deallocations_; }

 private:
  asc::MemoryResource& backing_;
  std::size_t attempts_ = 0;
  std::size_t live_ = 0;
  std::size_t deallocations_ = 0;
  std::size_t failure_call_ = std::numeric_limits<std::size_t>::max();
};

template <typename Element, asc::SparseExtents ExtentsType>
void CheckCase(Fixture& fixture, const ExtentsType& extents, asc::nnz_t count,
               TestContext& test) {
  using Generation = asc::CudaSparseUniform01Generation<Element, ExtentsType>;
  static_assert(!std::is_copy_constructible_v<Generation>);
  static_assert(std::is_nothrow_move_constructible_v<Generation>);
  const std::size_t expected_physical_allocations =
      (count > 0 && ExtentsType::kRank > 0 ? 1U : 0U) + (count > 0 ? 1U : 0U);

  TrackingDeviceResource tracking(*fixture.resource);
  {
    auto generated = asc::CudaGenerateSparseUniform01<Element>(
        fixture.execution, extents, count, tracking, kStructureStream,
        kStructureSubsequence, kStructureOffset, kValueStream,
        kValueSubsequence, kValueOffset);
    ASC_M7_CUDA_CHECK(test, generated.ok());
    if (!generated.ok()) {
      return;
    }
    const asc::RandomOffset expected_structure =
        count == 0
            ? kStructureOffset
            : kStructureOffset +
                  2U * static_cast<asc::RandomOffset>(extents.logical_size());
    const asc::RandomOffset expected_value =
        kValueOffset + static_cast<asc::RandomOffset>(count) *
                           (std::same_as<Element, float> ? 1U : 2U);
    ASC_M7_CUDA_EQ(test, generated->next_structure_offset, expected_structure);
    ASC_M7_CUDA_EQ(test, generated->next_value_offset, expected_value);
    Generation moved(std::move(*generated));
    ASC_M7_CUDA_CHECK(test, moved.completion.Wait().ok());
    auto view = moved.array.view();
    ASC_M7_CUDA_CHECK(test, view.ok());
    if (!view.ok()) {
      return;
    }
    ASC_M7_CUDA_EQ(test, view->nnz(), count);
    auto coordinates = asc_random_cuda_test::Download<asc::index_t>(
        view->coordinates(),
        static_cast<std::size_t>(count) * ExtentsType::kRank,
        asc::MemorySpace::kDevice, fixture.execution);
    auto values = asc_random_cuda_test::Download<Element>(
        view->values(), static_cast<std::size_t>(count),
        asc::MemorySpace::kDevice, fixture.execution);
    ASC_M7_CUDA_CHECK(test, coordinates.ok());
    ASC_M7_CUDA_CHECK(test, values.ok());
    if (!coordinates.ok() || !values.ok()) {
      return;
    }
    ASC_M7_CUDA_EQ(test, *coordinates, ExpectedCoordinates(extents, count));
    for (std::size_t position = 0; position < values->size(); ++position) {
      ASC_M7_CUDA_BIT_EQ(test, (*values)[position],
                         ExpectedValue<Element>(position));
    }

    asc::HostMemoryResource host;
    auto cpu = asc::GenerateSparseUniform01<Element>(
        asc::ExecutionContext::Serial(), extents, count, host, kStructureStream,
        kStructureSubsequence, kStructureOffset, kValueStream,
        kValueSubsequence, kValueOffset);
    ASC_M7_CUDA_CHECK(test, cpu.ok());
    if (cpu.ok()) {
      auto cpu_view = cpu->array.view();
      ASC_M7_CUDA_CHECK(test, cpu_view.ok());
      if (cpu_view.ok()) {
        for (asc::nnz_t position = 0; position < count; ++position) {
          auto cpu_coordinate = cpu_view->Coordinate(position);
          auto cpu_value = cpu_view->AtStored(position);
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
    }
    ASC_M7_CUDA_EQ(test, tracking.attempts(), std::size_t{2});
    ASC_M7_CUDA_EQ(test, tracking.live(), expected_physical_allocations);
  }
  ASC_M7_CUDA_EQ(test, tracking.live(), std::size_t{0});
  ASC_M7_CUDA_EQ(test, tracking.deallocations(), expected_physical_allocations);
}

void CheckCases(Fixture& fixture, TestContext& test) {
  using Dynamic2 = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  using Dynamic1 = asc::Extents<asc::kDynamicExtent>;
  using Static3 = asc::Extents<2, 3, 4>;
  using Scalar = asc::Extents<>;
  using Rank9 = asc::Extents<
      asc::kDynamicExtent, asc::kDynamicExtent, asc::kDynamicExtent,
      asc::kDynamicExtent, asc::kDynamicExtent, asc::kDynamicExtent,
      asc::kDynamicExtent, asc::kDynamicExtent, asc::kDynamicExtent>;
  auto dynamic = Dynamic2::Create(3, 4);
  auto rank1 = Dynamic1::Create(7);
  auto full = Static3::Create();
  auto scalar = Scalar::Create();
  auto empty = Dynamic2::Create(3, 0);
  auto rank9 = Rank9::Create(1, 1, 1, 1, 1, 1, 1, 1, 1);
  ASC_M7_CUDA_CHECK(test, dynamic.ok());
  ASC_M7_CUDA_CHECK(test, rank1.ok());
  ASC_M7_CUDA_CHECK(test, full.ok());
  ASC_M7_CUDA_CHECK(test, scalar.ok());
  ASC_M7_CUDA_CHECK(test, empty.ok());
  ASC_M7_CUDA_CHECK(test, rank9.ok());
  if (dynamic.ok()) {
    CheckCase<float>(fixture, *dynamic, 5, test);
    CheckCase<double>(fixture, *dynamic, 12, test);
  }
  if (rank1.ok()) {
    CheckCase<float>(fixture, *rank1, 3, test);
  }
  if (full.ok()) {
    CheckCase<double>(fixture, *full, 7, test);
  }
  if (scalar.ok()) {
    CheckCase<float>(fixture, *scalar, 0, test);
    CheckCase<double>(fixture, *scalar, 1, test);
  }
  if (empty.ok()) {
    CheckCase<float>(fixture, *empty, 0, test);
  }
  if (rank9.ok()) {
    CheckCase<float>(fixture, *rank9, 1, test);
  }
}

void CheckIndependentContexts(Fixture& fixture, TestContext& test) {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  auto shape = Shape::Create(3, 4);
  auto second = MakeFixture();
  ASC_M7_CUDA_CHECK(test, shape.ok());
  ASC_M7_CUDA_CHECK(test, second.ok());
  if (!shape.ok() || !second.ok()) {
    return;
  }
  auto first_generation = asc::CudaGenerateSparseUniform01<float>(
      fixture.execution, *shape, 5, *fixture.resource, kStructureStream,
      kStructureSubsequence, kStructureOffset, kValueStream, kValueSubsequence,
      kValueOffset);
  auto second_generation = asc::CudaGenerateSparseUniform01<float>(
      second->execution, *shape, 5, *second->resource, kStructureStream,
      kStructureSubsequence, kStructureOffset, kValueStream, kValueSubsequence,
      kValueOffset);
  ASC_M7_CUDA_CHECK(test, first_generation.ok());
  ASC_M7_CUDA_CHECK(test, second_generation.ok());
  if (!first_generation.ok() || !second_generation.ok()) {
    return;
  }
  ASC_M7_CUDA_CHECK(test, second_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, first_generation->completion.Wait().ok());
  auto first_view = first_generation->array.view();
  auto second_view = second_generation->array.view();
  ASC_M7_CUDA_CHECK(test, first_view.ok());
  ASC_M7_CUDA_CHECK(test, second_view.ok());
  if (!first_view.ok() || !second_view.ok()) {
    return;
  }
  auto first_coordinates = asc_random_cuda_test::Download<asc::index_t>(
      first_view->coordinates(), 5U * Shape::kRank, asc::MemorySpace::kDevice,
      fixture.execution);
  auto second_coordinates = asc_random_cuda_test::Download<asc::index_t>(
      second_view->coordinates(), 5U * Shape::kRank, asc::MemorySpace::kDevice,
      second->execution);
  auto first_values = asc_random_cuda_test::Download<float>(
      first_view->values(), 5, asc::MemorySpace::kDevice, fixture.execution);
  auto second_values = asc_random_cuda_test::Download<float>(
      second_view->values(), 5, asc::MemorySpace::kDevice, second->execution);
  ASC_M7_CUDA_CHECK(test, first_coordinates.ok());
  ASC_M7_CUDA_CHECK(test, second_coordinates.ok());
  ASC_M7_CUDA_CHECK(test, first_values.ok());
  ASC_M7_CUDA_CHECK(test, second_values.ok());
  if (first_coordinates.ok() && second_coordinates.ok()) {
    ASC_M7_CUDA_EQ(test, *first_coordinates, *second_coordinates);
    ASC_M7_CUDA_EQ(test, *first_coordinates, ExpectedCoordinates(*shape, 5));
  }
  if (first_values.ok() && second_values.ok()) {
    ASC_M7_CUDA_EQ(test, *first_values, *second_values);
    for (std::size_t position = 0; position < first_values->size();
         ++position) {
      ASC_M7_CUDA_BIT_EQ(test, (*first_values)[position],
                         ExpectedValue<float>(position));
    }
  }
}

void CheckFailures(Fixture& fixture, TestContext& test) {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  auto extents = Shape::Create(3, 4);
  ASC_M7_CUDA_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }
  const auto generate = [&](asc::nnz_t count, asc::MemoryResource& resource,
                            asc::RandomStream structure_stream,
                            asc::RandomSubsequence structure_subsequence,
                            asc::RandomOffset structure_offset,
                            asc::RandomStream value_stream,
                            asc::RandomSubsequence value_subsequence,
                            asc::RandomOffset value_offset) {
    return asc::CudaGenerateSparseUniform01<float>(
        fixture.execution, *extents, count, resource, structure_stream,
        structure_subsequence, structure_offset, value_stream,
        value_subsequence, value_offset);
  };

  const auto negative = generate(-1, *fixture.resource, 1, 2, 3, 4, 5, 6);
  const auto excess = generate(13, *fixture.resource, 1, 2, 3, 4, 5, 6);
  const auto equal_domain = generate(3, *fixture.resource, 1, 2, 3, 1, 2, 4);
  const auto structure_overflow =
      generate(3, *fixture.resource, 1, 2,
               std::numeric_limits<asc::RandomOffset>::max() - 22U, 4, 5, 6);
  const auto value_overflow =
      generate(3, *fixture.resource, 1, 2, 3, 4, 5,
               std::numeric_limits<asc::RandomOffset>::max() - 1U);
  ASC_M7_CUDA_CHECK(test, !negative.ok());
  ASC_M7_CUDA_CHECK(test, !excess.ok());
  ASC_M7_CUDA_CHECK(test, !equal_domain.ok());
  ASC_M7_CUDA_CHECK(test, !structure_overflow.ok());
  ASC_M7_CUDA_CHECK(test, !value_overflow.ok());

  asc::HostMemoryResource host;
  const auto host_resource = generate(3, host, 1, 2, 3, 4, 5, 6);
  ASC_M7_CUDA_CHECK(test, !host_resource.ok());

  TrackingDeviceResource failing(*fixture.resource);
  failing.FailOnCall(1);
  const auto allocation_failure = generate(3, failing, 1, 2, 3, 4, 5, 6);
  ASC_M7_CUDA_CHECK(test, !allocation_failure.ok());
  ASC_M7_CUDA_EQ(test, failing.attempts(), std::size_t{2});
  ASC_M7_CUDA_EQ(test, failing.live(), std::size_t{0});
  ASC_M7_CUDA_EQ(test, failing.deallocations(), std::size_t{1});

  std::array<asc::SparseRandomStructureCandidate, 12> workspace{};
  std::array<std::uint64_t, 3> ordinals{17, 17, 17};
  const auto cpu_only = asc::GenerateSparseStructure(
      fixture.execution, *extents, 3, 7, 11, 13, workspace, ordinals);
  ASC_M7_CUDA_CHECK(test, !cpu_only.ok());
  ASC_M7_CUDA_EQ(test, cpu_only.status().code(), asc::ErrorCode::kUnsupported);
  ASC_M7_CUDA_EQ(test, ordinals, (std::array<std::uint64_t, 3>{17, 17, 17}));

  constexpr std::array<asc::extent_t, 2> kShape{3, 4};
  constexpr std::array<asc::index_t, 6> kCoordinates{0, 1, 1, 2, 2, 3};
  constexpr std::array<asc::nnz_t, 4> kOffsets{0, 1, 2, 3};
  constexpr std::array<asc::index_t, 3> kIndices{1, 2, 3};
  std::array<float, 3> coordinate_values{23.0F, 23.0F, 23.0F};
  std::array<float, 3> compressed_values{29.0F, 29.0F, 29.0F};
  auto coordinate_view = asc::CoordinateView<float, 2>::Create(
      kCoordinates.data(), coordinate_values.data(), kShape, 3,
      asc::MemorySpace::kHost);
  auto compressed_view =
      asc::CompressedSparseView<float, asc::SparseCompressedFormat::kCsr>::
          Create(kOffsets, kIndices, std::span<float>(compressed_values),
                 kShape, asc::MemorySpace::kHost);
  ASC_M7_CUDA_CHECK(test, coordinate_view.ok());
  ASC_M7_CUDA_CHECK(test, compressed_view.ok());
  if (!coordinate_view.ok() || !compressed_view.ok()) {
    return;
  }
  const auto coordinate_uniform =
      asc::FillSparseUniform01(fixture.execution, *coordinate_view, 31, 37, 41);
  const auto compressed_uniform =
      asc::FillSparseUniform01(fixture.execution, *compressed_view, 31, 37, 41);
  ASC_M7_CUDA_CHECK(test, !coordinate_uniform.ok());
  ASC_M7_CUDA_CHECK(test, !compressed_uniform.ok());
  ASC_M7_CUDA_EQ(test, coordinate_uniform.status().code(),
                 asc::ErrorCode::kUnsupported);
  ASC_M7_CUDA_EQ(test, compressed_uniform.status().code(),
                 asc::ErrorCode::kUnsupported);

  auto uniform = asc::UniformRealDistribution<float>::Create(0.0F, 1.0F);
  asc::UniformGenerator<asc::Pcg32, float> coordinate_generator(
      asc::Pcg32(43, 47), *uniform);
  asc::UniformGenerator<asc::Pcg32, float> compressed_generator(
      asc::Pcg32(53, 59), *uniform);
  const auto coordinate_state = coordinate_generator.engine().ExportState();
  const auto compressed_state = compressed_generator.engine().ExportState();
  const auto coordinate_pseudo = asc::FillSparsePseudo(
      fixture.execution, *coordinate_view, coordinate_generator);
  const auto compressed_pseudo = asc::FillSparsePseudo(
      fixture.execution, *compressed_view, compressed_generator);
  ASC_M7_CUDA_CHECK(test, !coordinate_pseudo.ok());
  ASC_M7_CUDA_CHECK(test, !compressed_pseudo.ok());
  ASC_M7_CUDA_EQ(test, coordinate_pseudo.code(), asc::ErrorCode::kUnsupported);
  ASC_M7_CUDA_EQ(test, compressed_pseudo.code(), asc::ErrorCode::kUnsupported);
  ASC_M7_CUDA_EQ(test, coordinate_generator.engine().ExportState(),
                 coordinate_state);
  ASC_M7_CUDA_EQ(test, compressed_generator.engine().ExportState(),
                 compressed_state);
  ASC_M7_CUDA_EQ(test, coordinate_values,
                 (std::array<float, 3>{23.0F, 23.0F, 23.0F}));
  ASC_M7_CUDA_EQ(test, compressed_values,
                 (std::array<float, 3>{29.0F, 29.0F, 29.0F}));
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
  CheckCases(*fixture, test);
  CheckIndependentContexts(*fixture, test);
  CheckFailures(*fixture, test);
  return test.Finish();
}
