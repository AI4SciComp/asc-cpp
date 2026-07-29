#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "asc/random/sparse.h"
#include "asc/sparse/coordinate.h"
#include "test_support.h"

namespace {

using asc_random_sparse_test::TestContext;
using asc_random_sparse_test::TrackingMemoryResource;
using DynamicShape2 = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using MixedShape3 = asc::Extents<2, asc::kDynamicExtent, 3>;
using StaticShape2 = asc::Extents<3, 4>;
using ScalarShape = asc::Extents<>;

struct PriorityOrdinal {
  std::uint64_t priority;
  std::uint64_t ordinal;
};

void CheckForcedPriorityCollision(TestContext& test) {
  constexpr std::uint64_t kCollidingPriority = UINT64_C(0x5a5a5a5a5a5a5a5a);
  std::array<PriorityOrdinal, 4> candidates{{
      {kCollidingPriority, 7},
      {kCollidingPriority, 1},
      {kCollidingPriority, 5},
      {kCollidingPriority, 2},
  }};
  std::sort(candidates.begin(), candidates.end(),
            [](const PriorityOrdinal& left, const PriorityOrdinal& right) {
              return asc::internal_random_sparse::PriorityOrdinalLess(
                  left.priority, left.ordinal, right.priority, right.ordinal);
            });
  constexpr std::array<std::uint64_t, 4> kExpectedOrdinals{1, 2, 5, 7};
  for (std::size_t position = 0; position < candidates.size(); ++position) {
    ASC_RANDOM_SPARSE_TEST_EQ(test, candidates[position].priority,
                              kCollidingPriority);
    ASC_RANDOM_SPARSE_TEST_EQ(test, candidates[position].ordinal,
                              kExpectedOrdinals[position]);
  }
  ASC_RANDOM_SPARSE_TEST_CHECK(
      test, !asc::internal_random_sparse::PriorityOrdinalLess(
                kCollidingPriority, 3, kCollidingPriority, 3));
}

template <std::size_t Rank>
std::array<asc::index_t, Rank> DecodeCanonicalOrdinal(
    std::uint64_t ordinal, std::span<const asc::extent_t, Rank> extents) {
  std::array<asc::index_t, Rank> coordinate{};
  for (std::size_t reverse = Rank; reverse > 0; --reverse) {
    const std::size_t dimension = reverse - 1;
    const auto extent = static_cast<std::uint64_t>(extents[dimension]);
    coordinate[dimension] = static_cast<asc::index_t>(ordinal % extent);
    ordinal /= extent;
  }
  return coordinate;
}

template <std::size_t Rank>
std::vector<std::array<asc::index_t, Rank>> ExpectedCoordinates(
    std::span<const asc::extent_t, Rank> extents, asc::nnz_t count,
    asc::RandomStream stream, asc::RandomSubsequence subsequence,
    asc::RandomOffset offset) {
  std::uint64_t logical_size = 1;
  for (asc::extent_t extent : extents) {
    logical_size *= static_cast<std::uint64_t>(extent);
  }
  std::vector<PriorityOrdinal> candidates;
  candidates.reserve(static_cast<std::size_t>(logical_size));
  for (std::uint64_t ordinal = 0; ordinal < logical_size; ++ordinal) {
    const std::uint32_t high =
        asc::GeneratePhilox4x32Word(stream, subsequence, offset + 2 * ordinal);
    const std::uint32_t low = asc::GeneratePhilox4x32Word(
        stream, subsequence, offset + 2 * ordinal + 1);
    candidates.push_back({(static_cast<std::uint64_t>(high) << 32U) |
                              static_cast<std::uint64_t>(low),
                          ordinal});
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const PriorityOrdinal& left, const PriorityOrdinal& right) {
              return std::tie(left.priority, left.ordinal) <
                     std::tie(right.priority, right.ordinal);
            });
  std::vector<std::array<asc::index_t, Rank>> selected;
  selected.reserve(static_cast<std::size_t>(count));
  for (asc::nnz_t position = 0; position < count; ++position) {
    selected.push_back(DecodeCanonicalOrdinal(
        candidates[static_cast<std::size_t>(position)].ordinal, extents));
  }
  std::sort(selected.begin(), selected.end());
  return selected;
}

template <typename Element>
Element ExpectedValue(asc::RandomStream stream,
                      asc::RandomSubsequence subsequence,
                      asc::RandomOffset offset, std::uint64_t position) {
  if constexpr (std::same_as<Element, float>) {
    return asc::Uniform01<float>(
        asc::GeneratePhilox4x32Word(stream, subsequence, offset + position));
  } else {
    return asc::Uniform01<double>(
        asc::GeneratePhilox4x32Word(stream, subsequence, offset + 2 * position),
        asc::GeneratePhilox4x32Word(stream, subsequence,
                                    offset + 2 * position + 1));
  }
}

template <typename Element, typename ExtentsType>
void CheckGenerationAgainstOracle(TestContext& test, ExtentsType extents,
                                  asc::nnz_t count,
                                  asc::RandomStream structure_stream,
                                  asc::RandomSubsequence structure_subsequence,
                                  asc::RandomOffset structure_offset,
                                  asc::RandomStream value_stream,
                                  asc::RandomSubsequence value_subsequence,
                                  asc::RandomOffset value_offset) {
  constexpr std::size_t kRank = ExtentsType::kRank;
  TrackingMemoryResource resource;
  auto generated = asc::GenerateSparseUniform01<Element>(
      asc::ExecutionContext::Serial(), extents, count, resource,
      structure_stream, structure_subsequence, structure_offset, value_stream,
      value_subsequence, value_offset);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, generated.ok());
  if (!generated.ok()) {
    return;
  }
  ASC_RANDOM_SPARSE_TEST_EQ(test, generated->array.nnz(), count);
  const auto expected_coordinates =
      ExpectedCoordinates(extents.values(), count, structure_stream,
                          structure_subsequence, structure_offset);
  auto view = generated->array.view();
  ASC_RANDOM_SPARSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  for (asc::nnz_t position = 0; position < count; ++position) {
    auto coordinate = view->Coordinate(position);
    auto value = view->AtStored(position);
    ASC_RANDOM_SPARSE_TEST_CHECK(test, coordinate.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(test, value.ok());
    if (coordinate.ok()) {
      for (std::size_t dimension = 0; dimension < kRank; ++dimension) {
        ASC_RANDOM_SPARSE_TEST_EQ(test, (*coordinate)[dimension],
                                  expected_coordinates[static_cast<std::size_t>(
                                      position)][dimension]);
      }
    }
    if (value.ok()) {
      ASC_RANDOM_SPARSE_TEST_EQ(
          test, **value,
          ExpectedValue<Element>(value_stream, value_subsequence, value_offset,
                                 static_cast<std::uint64_t>(position)));
    }
  }
  const auto structure_words =
      count == 0 ? std::uint64_t{0}
                 : std::uint64_t{2} *
                       static_cast<std::uint64_t>(extents.logical_size());
  const auto value_words =
      static_cast<std::uint64_t>(count) *
      (std::same_as<Element, float> ? std::uint64_t{1} : std::uint64_t{2});
  ASC_RANDOM_SPARSE_TEST_EQ(test, generated->next_structure_offset,
                            structure_offset + structure_words);
  ASC_RANDOM_SPARSE_TEST_EQ(test, generated->next_value_offset,
                            value_offset + value_words);
  ASC_RANDOM_SPARSE_TEST_EQ(test, resource.allocation_attempts(),
                            std::size_t{2});
  ASC_RANDOM_SPARSE_TEST_EQ(
      test, resource.record(0).bytes,
      static_cast<std::size_t>(count) * kRank * sizeof(asc::index_t));
  ASC_RANDOM_SPARSE_TEST_EQ(test, resource.record(1).bytes,
                            static_cast<std::size_t>(count) * sizeof(Element));
  ASC_RANDOM_SPARSE_TEST_EQ(test, resource.record(0).alignment,
                            alignof(asc::index_t));
  ASC_RANDOM_SPARSE_TEST_EQ(test, resource.record(1).alignment,
                            alignof(Element));
}

void CheckShapesCountsAndOracles(TestContext& test) {
  auto dynamic = DynamicShape2::Create(3, 4);
  auto mixed = MixedShape3::Create(2);
  auto fixed = StaticShape2::Create();
  auto scalar = ScalarShape::Create();
  ASC_RANDOM_SPARSE_TEST_CHECK(test, dynamic.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, mixed.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, fixed.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, scalar.ok());
  if (!dynamic.ok() || !mixed.ok() || !fixed.ok() || !scalar.ok()) {
    return;
  }
  CheckGenerationAgainstOracle<float>(test, *dynamic, 5, 11, 13, 17, 19, 23,
                                      29);
  CheckGenerationAgainstOracle<double>(test, *mixed, 7, 31, 37, 41, 43, 47, 53);
  CheckGenerationAgainstOracle<float>(test, *fixed, fixed->logical_size(), 59,
                                      61, 67, 71, 73, 79);
  CheckGenerationAgainstOracle<double>(test, *scalar, 0, 83, 89, 97, 101, 103,
                                       107);
  CheckGenerationAgainstOracle<float>(test, *scalar, 1, 109, 113, 127, 131, 137,
                                      139);

  auto empty = DynamicShape2::Create(3, 0);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, empty.ok());
  if (empty.ok()) {
    CheckGenerationAgainstOracle<float>(test, *empty, 0, 149, 151, 157, 163,
                                        167, 173);
  }
}

template <typename Element, typename ExtentsType>
auto Generate(ExtentsType extents, asc::nnz_t count,
              TrackingMemoryResource& resource,
              asc::RandomStream structure_stream = 1,
              asc::RandomSubsequence structure_subsequence = 2,
              asc::RandomOffset structure_offset = 3,
              asc::RandomStream value_stream = 4,
              asc::RandomSubsequence value_subsequence = 5,
              asc::RandomOffset value_offset = 6) {
  return asc::GenerateSparseUniform01<Element>(
      asc::ExecutionContext::Serial(), extents, count, resource,
      structure_stream, structure_subsequence, structure_offset, value_stream,
      value_subsequence, value_offset);
}

void CheckFailureBeforeAllocation(TestContext& test) {
  auto extents = DynamicShape2::Create(3, 4);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }

  TrackingMemoryResource equal_domain_resource;
  auto equal_domain =
      Generate<float>(*extents, 3, equal_domain_resource, 1, 2, 3, 1, 2, 4);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !equal_domain.ok());
  if (!equal_domain.ok()) {
    ASC_RANDOM_SPARSE_TEST_EQ(test, equal_domain.status().code(),
                              asc::ErrorCode::kInvalidArgument);
  }
  ASC_RANDOM_SPARSE_TEST_EQ(test, equal_domain_resource.allocation_attempts(),
                            std::size_t{0});

  TrackingMemoryResource negative_resource;
  auto negative = Generate<float>(*extents, -1, negative_resource);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !negative.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, negative_resource.allocation_attempts(),
                            std::size_t{0});

  TrackingMemoryResource excess_resource;
  auto excess = Generate<float>(*extents, 13, excess_resource);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !excess.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, excess_resource.allocation_attempts(),
                            std::size_t{0});

  TrackingMemoryResource structure_overflow_resource;
  auto structure_overflow = Generate<float>(
      *extents, 3, structure_overflow_resource, 1, 2,
      std::numeric_limits<asc::RandomOffset>::max() - 22, 4, 5, 6);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !structure_overflow.ok());
  if (!structure_overflow.ok()) {
    ASC_RANDOM_SPARSE_TEST_EQ(test, structure_overflow.status().code(),
                              asc::ErrorCode::kOverflow);
  }
  ASC_RANDOM_SPARSE_TEST_EQ(
      test, structure_overflow_resource.allocation_attempts(), std::size_t{0});

  TrackingMemoryResource value_overflow_resource;
  auto value_overflow =
      Generate<double>(*extents, 3, value_overflow_resource, 1, 2, 3, 4, 5,
                       std::numeric_limits<asc::RandomOffset>::max() - 4);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !value_overflow.ok());
  if (!value_overflow.ok()) {
    ASC_RANDOM_SPARSE_TEST_EQ(test, value_overflow.status().code(),
                              asc::ErrorCode::kOverflow);
  }
  ASC_RANDOM_SPARSE_TEST_EQ(test, value_overflow_resource.allocation_attempts(),
                            std::size_t{0});

  TrackingMemoryResource device_resource(asc::MemorySpace::kDevice);
  auto device = Generate<float>(*extents, 3, device_resource);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !device.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, device_resource.allocation_attempts(),
                            std::size_t{0});
}

void CheckAllocationRollback(TestContext& test) {
  auto extents = DynamicShape2::Create(3, 4);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }

  TrackingMemoryResource fail_first;
  fail_first.FailOnCall(0);
  auto first = Generate<float>(*extents, 3, fail_first);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !first.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, fail_first.allocation_attempts(),
                            std::size_t{1});
  ASC_RANDOM_SPARSE_TEST_EQ(test, fail_first.live_allocations(),
                            std::size_t{0});
  ASC_RANDOM_SPARSE_TEST_EQ(
      test, fail_first.duplicate_or_unknown_deallocations(), std::size_t{0});

  TrackingMemoryResource fail_second;
  fail_second.FailOnCall(1);
  auto second = Generate<float>(*extents, 3, fail_second);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !second.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, fail_second.allocation_attempts(),
                            std::size_t{2});
  ASC_RANDOM_SPARSE_TEST_EQ(test, fail_second.live_allocations(),
                            std::size_t{0});
  ASC_RANDOM_SPARSE_TEST_EQ(test, fail_second.deallocations(), std::size_t{1});
  ASC_RANDOM_SPARSE_TEST_EQ(
      test, fail_second.duplicate_or_unknown_deallocations(), std::size_t{0});

  TrackingMemoryResource success;
  {
    auto generated = Generate<float>(*extents, 3, success);
    ASC_RANDOM_SPARSE_TEST_CHECK(test, generated.ok());
    ASC_RANDOM_SPARSE_TEST_EQ(test, success.live_allocations(), std::size_t{2});
  }
  ASC_RANDOM_SPARSE_TEST_EQ(test, success.live_allocations(), std::size_t{0});
  ASC_RANDOM_SPARSE_TEST_EQ(test, success.deallocations(), std::size_t{2});
  ASC_RANDOM_SPARSE_TEST_EQ(test, success.duplicate_or_unknown_deallocations(),
                            std::size_t{0});
}

void CheckExplicitZeroRetention(TestContext& test) {
  auto scalar = ScalarShape::Create();
  ASC_RANDOM_SPARSE_TEST_CHECK(test, scalar.ok());
  if (!scalar.ok()) {
    return;
  }
  constexpr asc::RandomStream kValueStream = 191;
  constexpr asc::RandomSubsequence kValueSubsequence = 193;
  constexpr asc::RandomOffset kZeroOffset = 35873479;
  ASC_RANDOM_SPARSE_TEST_EQ(
      test,
      asc::GeneratePhilox4x32Word(kValueStream, kValueSubsequence, kZeroOffset),
      std::uint32_t{206});
  ASC_RANDOM_SPARSE_TEST_EQ(test,
                            asc::Uniform01<float>(asc::GeneratePhilox4x32Word(
                                kValueStream, kValueSubsequence, kZeroOffset)),
                            0.0F);

  TrackingMemoryResource resource;
  auto generated =
      Generate<float>(*scalar, 1, resource, 197, 199, 211, kValueStream,
                      kValueSubsequence, kZeroOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, generated.ok());
  if (!generated.ok()) {
    return;
  }
  ASC_RANDOM_SPARSE_TEST_EQ(test, generated->array.nnz(), asc::nnz_t{1});
  auto view = generated->array.view();
  ASC_RANDOM_SPARSE_TEST_CHECK(test, view.ok());
  if (view.ok()) {
    auto value = view->AtStored(0);
    ASC_RANDOM_SPARSE_TEST_CHECK(test, value.ok());
    if (value.ok()) {
      ASC_RANDOM_SPARSE_TEST_EQ(test, **value, 0.0F);
    }
  }
}

template <typename Generation>
std::vector<std::vector<asc::index_t>> CoordinatesOf(Generation& generation) {
  std::vector<std::vector<asc::index_t>> result;
  auto view = generation.array.view();
  if (!view.ok()) {
    return result;
  }
  for (asc::nnz_t position = 0; position < view->nnz(); ++position) {
    auto coordinate = view->Coordinate(position);
    if (coordinate.ok()) {
      result.emplace_back(coordinate->begin(), coordinate->end());
    }
  }
  return result;
}

void CheckStreamIndependenceAndReproducibility(TestContext& test) {
  auto extents = DynamicShape2::Create(5, 7);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }
  TrackingMemoryResource first_resource;
  TrackingMemoryResource rerun_resource;
  TrackingMemoryResource value_changed_resource;
  TrackingMemoryResource structure_changed_resource;
  auto first =
      Generate<float>(*extents, 9, first_resource, 11, 13, 17, 19, 23, 29);
  auto rerun =
      Generate<float>(*extents, 9, rerun_resource, 11, 13, 17, 19, 23, 29);
  auto value_changed = Generate<float>(*extents, 9, value_changed_resource, 11,
                                       13, 17, 31, 37, 41);
  auto structure_changed = Generate<float>(
      *extents, 9, structure_changed_resource, 43, 47, 53, 19, 23, 29);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, first.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, rerun.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, value_changed.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, structure_changed.ok());
  if (!first.ok() || !rerun.ok() || !value_changed.ok() ||
      !structure_changed.ok()) {
    return;
  }
  const auto first_coordinates = CoordinatesOf(*first);
  ASC_RANDOM_SPARSE_TEST_EQ(test, CoordinatesOf(*rerun), first_coordinates);
  ASC_RANDOM_SPARSE_TEST_EQ(test, CoordinatesOf(*value_changed),
                            first_coordinates);
  ASC_RANDOM_SPARSE_TEST_CHECK(
      test, CoordinatesOf(*structure_changed) != first_coordinates);

  auto first_view = first->array.view();
  auto rerun_view = rerun->array.view();
  auto value_changed_view = value_changed->array.view();
  auto structure_changed_view = structure_changed->array.view();
  ASC_RANDOM_SPARSE_TEST_CHECK(test, first_view.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, rerun_view.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, value_changed_view.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, structure_changed_view.ok());
  if (!first_view.ok() || !rerun_view.ok() || !value_changed_view.ok() ||
      !structure_changed_view.ok()) {
    return;
  }
  for (asc::nnz_t position = 0; position < 9; ++position) {
    auto first_value = first_view->AtStored(position);
    auto rerun_value = rerun_view->AtStored(position);
    auto changed_value = value_changed_view->AtStored(position);
    auto same_value = structure_changed_view->AtStored(position);
    ASC_RANDOM_SPARSE_TEST_CHECK(test, first_value.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(test, rerun_value.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(test, changed_value.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(test, same_value.ok());
    if (first_value.ok() && rerun_value.ok() && changed_value.ok() &&
        same_value.ok()) {
      ASC_RANDOM_SPARSE_TEST_EQ(test, **first_value, **rerun_value);
      ASC_RANDOM_SPARSE_TEST_EQ(test, **first_value, **same_value);
      ASC_RANDOM_SPARSE_TEST_CHECK(test, **first_value != **changed_value);
    }
  }
}

}  // namespace

int main() {
  TestContext test;
  CheckForcedPriorityCollision(test);
  CheckShapesCountsAndOracles(test);
  CheckFailureBeforeAllocation(test);
  CheckAllocationRollback(test);
  CheckExplicitZeroRetention(test);
  CheckStreamIndependenceAndReproducibility(test);
  return test.Finish();
}
