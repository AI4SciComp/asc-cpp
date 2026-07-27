#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "asc/random/sparse.h"
#include "test_support.h"

namespace {

static_assert(asc::internal_random_sparse::PriorityLess(
    asc::internal_random_sparse::Priority{7, 3},
    asc::internal_random_sparse::Priority{7, 4}));
static_assert(!asc::internal_random_sparse::PriorityLess(
    asc::internal_random_sparse::Priority{7, 4},
    asc::internal_random_sparse::Priority{7, 3}));

constexpr asc::RandomStream kStructureStream = 0x1020304050607080ULL;
constexpr asc::RandomSubsequence kStructureSubsequence = 0x90a0b0c0d0e0f000ULL;
constexpr asc::RandomOffset kStructureOffset = 41;
constexpr asc::RandomStream kValueStream = 0x8877665544332211ULL;
constexpr asc::RandomSubsequence kValueSubsequence = 0x0f1e2d3c4b5a6978ULL;
constexpr asc::RandomOffset kValueOffset = 73;

struct Candidate {
  std::uint64_t priority;
  std::uint64_t ordinal;
};

bool CandidateLess(Candidate left, Candidate right) {
  return left.priority < right.priority ||
         (left.priority == right.priority && left.ordinal < right.ordinal);
}

template <std::size_t Rank>
std::array<asc::index_t, Rank> OracleCoordinate(
    std::uint64_t ordinal, const std::array<asc::extent_t, Rank>& shape) {
  std::array<asc::index_t, Rank> coordinate{};
  for (std::size_t reverse = Rank; reverse > 0; --reverse) {
    const std::size_t dimension = reverse - 1;
    const auto extent = static_cast<std::uint64_t>(shape[dimension]);
    coordinate[dimension] = static_cast<asc::index_t>(ordinal % extent);
    ordinal /= extent;
  }
  return coordinate;
}

template <std::size_t Rank>
std::vector<std::array<asc::index_t, Rank>> OracleCoordinates(
    const std::array<asc::extent_t, Rank>& shape, asc::nnz_t exact_count,
    asc::RandomStream stream, asc::RandomSubsequence subsequence,
    asc::RandomOffset offset) {
  std::uint64_t logical_size = 1;
  for (asc::extent_t extent : shape) {
    logical_size *= static_cast<std::uint64_t>(extent);
  }
  std::vector<Candidate> candidates;
  candidates.reserve(static_cast<std::size_t>(logical_size));
  for (std::uint64_t ordinal = 0; ordinal < logical_size; ++ordinal) {
    const auto high = static_cast<std::uint64_t>(
        asc::Philox4x32Word(stream, subsequence, offset + 2U * ordinal));
    const auto low = static_cast<std::uint64_t>(
        asc::Philox4x32Word(stream, subsequence, offset + 2U * ordinal + 1U));
    candidates.push_back(Candidate{(high << 32U) | low, ordinal});
  }
  std::sort(candidates.begin(), candidates.end(), CandidateLess);
  std::vector<std::array<asc::index_t, Rank>> coordinates;
  coordinates.reserve(static_cast<std::size_t>(exact_count));
  for (asc::nnz_t position = 0; position < exact_count; ++position) {
    coordinates.push_back(OracleCoordinate<Rank>(
        candidates[static_cast<std::size_t>(position)].ordinal, shape));
  }
  std::sort(coordinates.begin(), coordinates.end());
  return coordinates;
}

template <typename Element>
Element OracleValue(std::uint64_t position,
                    asc::RandomStream stream = kValueStream,
                    asc::RandomSubsequence subsequence = kValueSubsequence,
                    asc::RandomOffset offset = kValueOffset) {
  if constexpr (std::same_as<Element, float>) {
    return asc::Uniform01<float>(
        asc::Philox4x32Word(stream, subsequence, offset + position));
  } else {
    return asc::Uniform01<double>(
        asc::Philox4x32Word(stream, subsequence, offset + 2U * position),
        asc::Philox4x32Word(stream, subsequence, offset + 2U * position + 1U));
  }
}

template <typename Element, typename ExtentsType>
void CheckGeneration(
    asc_random_sparse_test::TestContext& context,
    const asc::SparseUniform01Generation<Element, ExtentsType>& generation,
    asc::nnz_t exact_count, asc::RandomStream structure_stream,
    asc::RandomSubsequence structure_subsequence,
    asc::RandomOffset structure_offset, asc::RandomStream value_stream,
    asc::RandomSubsequence value_subsequence, asc::RandomOffset value_offset) {
  auto view = generation.array.view();
  ASC_RANDOM_SPARSE_TEST_CHECK(context, view.ok());
  if (!view.ok()) {
    return;
  }
  ASC_RANDOM_SPARSE_TEST_EQ(context, view->nnz(), exact_count);
  const auto expected_coordinates = OracleCoordinates<ExtentsType::kRank>(
      view->shape(), exact_count, structure_stream, structure_subsequence,
      structure_offset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context,
                               std::adjacent_find(expected_coordinates.begin(),
                                                  expected_coordinates.end()) ==
                                   expected_coordinates.end());
  for (asc::nnz_t position = 0; position < exact_count; ++position) {
    auto coordinate = view->CoordinateAt(position);
    auto value = view->ValueAt(position);
    ASC_RANDOM_SPARSE_TEST_CHECK(context, coordinate.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(context, value.ok());
    if (coordinate.ok()) {
      const auto& expected =
          expected_coordinates[static_cast<std::size_t>(position)];
      ASC_RANDOM_SPARSE_TEST_CHECK(
          context, std::equal(coordinate->begin(), coordinate->end(),
                              expected.begin(), expected.end()));
    }
    if (value.ok()) {
      ASC_RANDOM_SPARSE_TEST_EQ(
          context, **value,
          OracleValue<Element>(static_cast<std::uint64_t>(position),
                               value_stream, value_subsequence, value_offset));
    }
  }
}

void CheckRankZeroAndZeroExtent(asc_random_sparse_test::TestContext& context) {
  auto scalar_extents = asc::Extents<>::Create();
  ASC_RANDOM_SPARSE_TEST_CHECK(context, scalar_extents.ok());
  if (!scalar_extents.ok()) {
    return;
  }

  asc_random_sparse_test::CountingMemoryResource empty_resource;
  auto empty = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *scalar_extents, 0, empty_resource,
      kStructureStream, kStructureSubsequence,
      std::numeric_limits<asc::RandomOffset>::max(), kValueStream,
      kValueSubsequence, std::numeric_limits<asc::RandomOffset>::max());
  ASC_RANDOM_SPARSE_TEST_CHECK(context, empty.ok());
  if (empty.ok()) {
    ASC_RANDOM_SPARSE_TEST_EQ(context, empty->array.nnz(), 0);
    ASC_RANDOM_SPARSE_TEST_EQ(context, empty->next_structure_offset,
                              std::numeric_limits<asc::RandomOffset>::max());
    ASC_RANDOM_SPARSE_TEST_EQ(context, empty->next_value_offset,
                              std::numeric_limits<asc::RandomOffset>::max());
  }

  asc_random_sparse_test::CountingMemoryResource excessive_scalar_resource;
  const auto excessive_scalar = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *scalar_extents, 2,
      excessive_scalar_resource, kStructureStream, kStructureSubsequence,
      kStructureOffset, kValueStream, kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, !excessive_scalar.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(
      context, excessive_scalar_resource.allocation_attempts(), std::size_t{0});

  asc_random_sparse_test::CountingMemoryResource scalar_resource;
  {
    auto scalar = asc::GenerateSparseUniform01<double>(
        asc::ExecutionContext::Serial(), *scalar_extents, 1, scalar_resource,
        kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
        kValueSubsequence, kValueOffset);
    ASC_RANDOM_SPARSE_TEST_CHECK(context, scalar.ok());
    if (scalar.ok()) {
      CheckGeneration(context, *scalar, 1, kStructureStream,
                      kStructureSubsequence, kStructureOffset, kValueStream,
                      kValueSubsequence, kValueOffset);
      ASC_RANDOM_SPARSE_TEST_EQ(context, scalar->next_structure_offset,
                                kStructureOffset + 2U);
      ASC_RANDOM_SPARSE_TEST_EQ(context, scalar->next_value_offset,
                                kValueOffset + 2U);
    }
  }
  ASC_RANDOM_SPARSE_TEST_EQ(context, scalar_resource.live_allocations(),
                            std::size_t{0});

  auto zero_extents = asc::Extents<asc::kDynamicExtent, 3>::Create(0);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, zero_extents.ok());
  if (!zero_extents.ok()) {
    return;
  }
  asc_random_sparse_test::CountingMemoryResource zero_resource;
  auto zero = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *zero_extents, 0, zero_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, zero.ok());
  if (zero.ok()) {
    ASC_RANDOM_SPARSE_TEST_EQ(context, zero->array.nnz(), 0);
    ASC_RANDOM_SPARSE_TEST_EQ(context, zero->next_structure_offset,
                              kStructureOffset);
    ASC_RANDOM_SPARSE_TEST_EQ(context, zero->next_value_offset, kValueOffset);
  }

  asc_random_sparse_test::CountingMemoryResource invalid_zero_resource;
  const auto invalid_zero = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *zero_extents, 1, invalid_zero_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, !invalid_zero.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(
      context, invalid_zero_resource.allocation_attempts(), std::size_t{0});
}

void CheckPriorityOracleAndAllocation(
    asc_random_sparse_test::TestContext& context) {
  auto extents = asc::Extents<2, 3>::Create();
  ASC_RANDOM_SPARSE_TEST_CHECK(context, extents.ok());
  if (!extents.ok()) {
    return;
  }
  asc_random_sparse_test::CountingMemoryResource resource;
  {
    auto generated = asc::GenerateSparseUniform01<float>(
        asc::ExecutionContext::Serial(), *extents, 3, resource,
        kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
        kValueSubsequence, kValueOffset);
    ASC_RANDOM_SPARSE_TEST_CHECK(context, generated.ok());
    if (!generated.ok()) {
      return;
    }
    CheckGeneration(context, *generated, 3, kStructureStream,
                    kStructureSubsequence, kStructureOffset, kValueStream,
                    kValueSubsequence, kValueOffset);
    ASC_RANDOM_SPARSE_TEST_EQ(context, generated->next_structure_offset,
                              kStructureOffset + 12U);
    ASC_RANDOM_SPARSE_TEST_EQ(context, generated->next_value_offset,
                              kValueOffset + 3U);
    ASC_RANDOM_SPARSE_TEST_EQ(context, resource.allocation_attempts(),
                              std::size_t{2});
    ASC_RANDOM_SPARSE_TEST_EQ(context, resource.successful_allocations(),
                              std::size_t{2});
    ASC_RANDOM_SPARSE_TEST_EQ(context, resource.live_allocations(),
                              std::size_t{2});
  }
  ASC_RANDOM_SPARSE_TEST_EQ(context, resource.deallocations(), std::size_t{2});
  ASC_RANDOM_SPARSE_TEST_EQ(context, resource.live_allocations(),
                            std::size_t{0});
  ASC_RANDOM_SPARSE_TEST_EQ(context, resource.allocated_bytes(),
                            resource.deallocated_bytes());

  asc_random_sparse_test::CountingMemoryResource rerun_resource;
  auto rerun = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 3, rerun_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, rerun.ok());
  if (rerun.ok()) {
    CheckGeneration(context, *rerun, 3, kStructureStream, kStructureSubsequence,
                    kStructureOffset, kValueStream, kValueSubsequence,
                    kValueOffset);
  }
}

void CheckDynamicRankAndFullCount(
    asc_random_sparse_test::TestContext& context) {
  using DynamicExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent,
                                      asc::kDynamicExtent>;
  auto extents = DynamicExtents::Create(2, 2, 2);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, extents.ok());
  if (!extents.ok()) {
    return;
  }
  asc_random_sparse_test::CountingMemoryResource partial_resource;
  auto partial = asc::GenerateSparseUniform01<double>(
      asc::ExecutionContext::Serial(), *extents, 5, partial_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, partial.ok());
  if (partial.ok()) {
    CheckGeneration(context, *partial, 5, kStructureStream,
                    kStructureSubsequence, kStructureOffset, kValueStream,
                    kValueSubsequence, kValueOffset);
    ASC_RANDOM_SPARSE_TEST_EQ(context, partial->next_structure_offset,
                              kStructureOffset + 16U);
    ASC_RANDOM_SPARSE_TEST_EQ(context, partial->next_value_offset,
                              kValueOffset + 10U);
  }

  asc_random_sparse_test::CountingMemoryResource full_resource;
  auto full = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 8, full_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, full.ok());
  if (full.ok()) {
    CheckGeneration(context, *full, 8, kStructureStream, kStructureSubsequence,
                    kStructureOffset, kValueStream, kValueSubsequence,
                    kValueOffset);
  }
}

void CheckStreamSeparationAndIndependence(
    asc_random_sparse_test::TestContext& context) {
  auto extents = asc::Extents<2, 4>::Create();
  ASC_RANDOM_SPARSE_TEST_CHECK(context, extents.ok());
  if (!extents.ok()) {
    return;
  }

  asc_random_sparse_test::CountingMemoryResource equal_domain_resource;
  const auto equal_domain = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 3, equal_domain_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset,
      kStructureStream, kStructureSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, !equal_domain.ok());
  if (!equal_domain.ok()) {
    ASC_RANDOM_SPARSE_TEST_EQ(context, equal_domain.status().code(),
                              asc::ErrorCode::kInvalidArgument);
  }
  ASC_RANDOM_SPARSE_TEST_EQ(
      context, equal_domain_resource.allocation_attempts(), std::size_t{0});

  asc_random_sparse_test::CountingMemoryResource first_resource;
  asc_random_sparse_test::CountingMemoryResource value_changed_resource;
  asc_random_sparse_test::CountingMemoryResource structure_changed_resource;
  auto first = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 3, first_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  auto value_changed = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 3, value_changed_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset,
      kValueStream + 1U, kValueSubsequence, kValueOffset);
  auto structure_changed = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 3, structure_changed_resource,
      kStructureStream + 1U, kStructureSubsequence, kStructureOffset,
      kValueStream, kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, first.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(context, value_changed.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(context, structure_changed.ok());
  if (!first.ok() || !value_changed.ok() || !structure_changed.ok()) {
    return;
  }
  auto first_view = first->array.view();
  auto value_changed_view = value_changed->array.view();
  auto structure_changed_view = structure_changed->array.view();
  ASC_RANDOM_SPARSE_TEST_CHECK(context, first_view.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(context, value_changed_view.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(context, structure_changed_view.ok());
  if (!first_view.ok() || !value_changed_view.ok() ||
      !structure_changed_view.ok()) {
    return;
  }
  for (asc::nnz_t position = 0; position < 3; ++position) {
    auto first_coordinate = first_view->CoordinateAt(position);
    auto changed_coordinate = value_changed_view->CoordinateAt(position);
    auto first_value = first_view->ValueAt(position);
    auto structure_value = structure_changed_view->ValueAt(position);
    ASC_RANDOM_SPARSE_TEST_CHECK(context, first_coordinate.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(context, changed_coordinate.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(context, first_value.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(context, structure_value.ok());
    if (first_coordinate.ok() && changed_coordinate.ok()) {
      ASC_RANDOM_SPARSE_TEST_CHECK(
          context,
          std::equal(first_coordinate->begin(), first_coordinate->end(),
                     changed_coordinate->begin()));
    }
    if (first_value.ok() && structure_value.ok()) {
      ASC_RANDOM_SPARSE_TEST_EQ(context, **first_value, **structure_value);
    }
  }
}

void CheckValidationAndRollback(asc_random_sparse_test::TestContext& context) {
  auto extents = asc::Extents<2, 3>::Create();
  ASC_RANDOM_SPARSE_TEST_CHECK(context, extents.ok());
  if (!extents.ok()) {
    return;
  }

  asc_random_sparse_test::CountingMemoryResource negative_resource;
  const auto negative = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, -1, negative_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, !negative.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(context, negative_resource.allocation_attempts(),
                            std::size_t{0});

  asc_random_sparse_test::CountingMemoryResource excessive_resource;
  const auto excessive = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 7, excessive_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, !excessive.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(context, excessive_resource.allocation_attempts(),
                            std::size_t{0});

  const auto maximum = std::numeric_limits<asc::RandomOffset>::max();
  asc_random_sparse_test::CountingMemoryResource structure_overflow_resource;
  const auto structure_overflow = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 1, structure_overflow_resource,
      kStructureStream, kStructureSubsequence, maximum - 11U, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, !structure_overflow.ok());
  if (!structure_overflow.ok()) {
    ASC_RANDOM_SPARSE_TEST_EQ(context, structure_overflow.status().code(),
                              asc::ErrorCode::kOverflow);
  }
  ASC_RANDOM_SPARSE_TEST_EQ(context,
                            structure_overflow_resource.allocation_attempts(),
                            std::size_t{0});

  asc_random_sparse_test::CountingMemoryResource value_overflow_resource;
  const auto value_overflow = asc::GenerateSparseUniform01<double>(
      asc::ExecutionContext::Serial(), *extents, 3, value_overflow_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, maximum - 5U);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, !value_overflow.ok());
  if (!value_overflow.ok()) {
    ASC_RANDOM_SPARSE_TEST_EQ(context, value_overflow.status().code(),
                              asc::ErrorCode::kOverflow);
  }
  ASC_RANDOM_SPARSE_TEST_EQ(
      context, value_overflow_resource.allocation_attempts(), std::size_t{0});

  asc_random_sparse_test::CountingMemoryResource device_resource(
      asc::MemorySpace::kDevice);
  const auto device = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 1, device_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, !device.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(context, device_resource.allocation_attempts(),
                            std::size_t{0});

  asc_random_sparse_test::CountingMemoryResource first_failure_resource;
  first_failure_resource.FailOnAttempt(1);
  const auto first_failure = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 3, first_failure_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, !first_failure.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(
      context, first_failure_resource.allocation_attempts(), std::size_t{1});
  ASC_RANDOM_SPARSE_TEST_EQ(context, first_failure_resource.live_allocations(),
                            std::size_t{0});

  asc_random_sparse_test::CountingMemoryResource second_failure_resource;
  second_failure_resource.FailOnAttempt(2);
  const auto second_failure = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 3, second_failure_resource,
      kStructureStream, kStructureSubsequence, kStructureOffset, kValueStream,
      kValueSubsequence, kValueOffset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, !second_failure.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(
      context, second_failure_resource.allocation_attempts(), std::size_t{2});
  ASC_RANDOM_SPARSE_TEST_EQ(context,
                            second_failure_resource.successful_allocations(),
                            std::size_t{1});
  ASC_RANDOM_SPARSE_TEST_EQ(context, second_failure_resource.deallocations(),
                            std::size_t{1});
  ASC_RANDOM_SPARSE_TEST_EQ(context, second_failure_resource.live_allocations(),
                            std::size_t{0});
}

void CheckExplicitZeroRetention(asc_random_sparse_test::TestContext& context) {
  constexpr asc::RandomStream kZeroStream = 0x7250ULL;
  constexpr asc::RandomSubsequence kZeroSubsequence = 0x3812ULL;
  constexpr asc::RandomOffset kSearchBound = 6156830ULL;
  asc::RandomOffset zero_offset = 0;
  bool found = false;
  for (asc::RandomOffset offset = 0; offset <= kSearchBound; ++offset) {
    const auto word =
        asc::Philox4x32Word(kZeroStream, kZeroSubsequence, offset);
    if (asc::Uniform01<float>(word) == 0.0F) {
      zero_offset = offset;
      found = true;
      break;
    }
  }
  ASC_RANDOM_SPARSE_TEST_CHECK(context, found);
  if (!found) {
    return;
  }
  ASC_RANDOM_SPARSE_TEST_EQ(context, zero_offset, kSearchBound);

  auto extents = asc::Extents<>::Create();
  ASC_RANDOM_SPARSE_TEST_CHECK(context, extents.ok());
  if (!extents.ok()) {
    return;
  }
  asc_random_sparse_test::CountingMemoryResource resource;
  auto generated = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 1, resource, kStructureStream,
      kStructureSubsequence, kStructureOffset, kZeroStream, kZeroSubsequence,
      zero_offset);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, generated.ok());
  if (!generated.ok()) {
    return;
  }
  auto view = generated->array.view();
  ASC_RANDOM_SPARSE_TEST_CHECK(context, view.ok());
  if (view.ok()) {
    ASC_RANDOM_SPARSE_TEST_EQ(context, view->nnz(), 1);
    auto value = view->ValueAt(0);
    ASC_RANDOM_SPARSE_TEST_CHECK(context, value.ok());
    if (value.ok()) {
      ASC_RANDOM_SPARSE_TEST_EQ(context, **value, 0.0F);
    }
  }
}

struct ConcurrentResult {
  bool ok = false;
  asc::RandomOffset next_structure = 0;
  asc::RandomOffset next_value = 0;
  std::array<asc::index_t, 6> coordinates{};
  std::array<float, 3> values{};
};

void GenerateConcurrently(asc::RandomStream structure_stream,
                          asc::RandomStream value_stream,
                          ConcurrentResult& output) {
  auto extents = asc::Extents<2, 3>::Create();
  if (!extents.ok()) {
    return;
  }
  asc_random_sparse_test::CountingMemoryResource resource;
  auto generated = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 3, resource, structure_stream,
      kStructureSubsequence, kStructureOffset, value_stream, kValueSubsequence,
      kValueOffset);
  if (!generated.ok()) {
    return;
  }
  auto view = generated->array.view();
  if (!view.ok()) {
    return;
  }
  for (asc::nnz_t position = 0; position < 3; ++position) {
    auto coordinate = view->CoordinateAt(position);
    auto value = view->ValueAt(position);
    if (!coordinate.ok() || !value.ok()) {
      return;
    }
    output.coordinates[static_cast<std::size_t>(2 * position)] =
        (*coordinate)[0];
    output.coordinates[static_cast<std::size_t>(2 * position + 1)] =
        (*coordinate)[1];
    output.values[static_cast<std::size_t>(position)] = **value;
  }
  output.next_structure = generated->next_structure_offset;
  output.next_value = generated->next_value_offset;
  output.ok = true;
}

void CheckConcurrency(asc_random_sparse_test::TestContext& context) {
  ConcurrentResult first;
  ConcurrentResult second;
  std::thread first_thread(GenerateConcurrently, asc::RandomStream{101},
                           asc::RandomStream{201}, std::ref(first));
  std::thread second_thread(GenerateConcurrently, asc::RandomStream{102},
                            asc::RandomStream{202}, std::ref(second));
  first_thread.join();
  second_thread.join();
  ASC_RANDOM_SPARSE_TEST_CHECK(context, first.ok);
  ASC_RANDOM_SPARSE_TEST_CHECK(context, second.ok);
  ASC_RANDOM_SPARSE_TEST_EQ(context, first.next_structure,
                            kStructureOffset + 12U);
  ASC_RANDOM_SPARSE_TEST_EQ(context, first.next_value, kValueOffset + 3U);
  ASC_RANDOM_SPARSE_TEST_EQ(context, second.next_structure,
                            kStructureOffset + 12U);
  ASC_RANDOM_SPARSE_TEST_EQ(context, second.next_value, kValueOffset + 3U);

  ConcurrentResult serial_first;
  ConcurrentResult serial_second;
  GenerateConcurrently(101, 201, serial_first);
  GenerateConcurrently(102, 202, serial_second);
  ASC_RANDOM_SPARSE_TEST_EQ(context, first.coordinates,
                            serial_first.coordinates);
  ASC_RANDOM_SPARSE_TEST_EQ(context, first.values, serial_first.values);
  ASC_RANDOM_SPARSE_TEST_EQ(context, second.coordinates,
                            serial_second.coordinates);
  ASC_RANDOM_SPARSE_TEST_EQ(context, second.values, serial_second.values);
}

}  // namespace

int main() {
  asc_random_sparse_test::TestContext context;
  CheckRankZeroAndZeroExtent(context);
  CheckPriorityOracleAndAllocation(context);
  CheckDynamicRankAndFullCount(context);
  CheckStreamSeparationAndIndependence(context);
  CheckValidationAndRollback(context);
  CheckExplicitZeroRetention(context);
  CheckConcurrency(context);
  return context.Finish();
}
