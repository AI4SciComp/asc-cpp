#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "asc/random/sparse.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "test_support.h"

namespace {

using asc_random_sparse_test::TestContext;

class ZeroGenerator {
 public:
  asc::Result<float> operator()() {
    ++calls_;
    return 0.0F;
  }
  [[nodiscard]] std::size_t calls() const noexcept { return calls_; }

 private:
  std::size_t calls_ = 0;
};

class FailingGenerator {
 public:
  asc::Result<float> operator()() {
    if (calls_++ == 1) {
      return asc::Status(asc::ErrorCode::kNumerical,
                         "intentional generator failure");
    }
    return 5.0F;
  }

 private:
  std::size_t calls_ = 0;
};

void CheckCoordinateValueFill(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape{3, 4};
  constexpr std::array<asc::index_t, 6> kCoordinates{0, 1, 1, 3, 2, 0};
  std::array<float, 3> values{-1.0F, -1.0F, -1.0F};
  auto view = asc::CoordinateView<float, 2>::Create(
      kCoordinates.data(), values.data(), kShape, 3, asc::MemorySpace::kHost);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  const auto next =
      asc::FillSparseUniform01(asc::ExecutionContext::Serial(), *view, 3, 5, 7);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, next.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, *next, asc::RandomOffset{10});
  for (std::size_t position = 0; position < values.size(); ++position) {
    const float expected =
        asc::Uniform01<float>(asc::GeneratePhilox4x32Word(3, 5, 7 + position));
    ASC_RANDOM_SPARSE_TEST_EQ(test,
                              std::bit_cast<std::uint32_t>(values[position]),
                              std::bit_cast<std::uint32_t>(expected));
  }
  ASC_RANDOM_SPARSE_TEST_EQ(test, view->nnz(), asc::nnz_t{3});
  ASC_RANDOM_SPARSE_TEST_CHECK(
      test, std::equal(kCoordinates.begin(), kCoordinates.end(),
                       view->coordinates()));

  values.fill(-3.0F);
  FailingGenerator failing;
  const auto failed =
      asc::FillSparsePseudo(asc::ExecutionContext::Serial(), *view, failing);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !failed.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, failed.code(), asc::ErrorCode::kNumerical);
  ASC_RANDOM_SPARSE_TEST_EQ(test, values[0], 5.0F);
  ASC_RANDOM_SPARSE_TEST_EQ(test, values[1], -3.0F);
  ASC_RANDOM_SPARSE_TEST_EQ(test, values[2], -3.0F);
  ASC_RANDOM_SPARSE_TEST_CHECK(
      test, std::equal(kCoordinates.begin(), kCoordinates.end(),
                       view->coordinates()));

  ZeroGenerator zero;
  const auto zeroed =
      asc::FillSparsePseudo(asc::ExecutionContext::Serial(), *view, zero);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, zeroed.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, zero.calls(), std::size_t{3});
  ASC_RANDOM_SPARSE_TEST_EQ(test, view->nnz(), asc::nnz_t{3});
  for (float value : values) {
    ASC_RANDOM_SPARSE_TEST_EQ(test, value, 0.0F);
  }
  ASC_RANDOM_SPARSE_TEST_CHECK(
      test, std::equal(kCoordinates.begin(), kCoordinates.end(),
                       view->coordinates()));
}

void CheckCompressedValueFill(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape{3, 4};
  constexpr std::array<asc::nnz_t, 4> kOffsets{0, 1, 2, 3};
  constexpr std::array<asc::index_t, 3> kIndices{1, 3, 0};
  std::array<double, 3> values{-1.0, -1.0, -1.0};
  auto view =
      asc::CompressedSparseView<double, asc::SparseCompressedFormat::kCsr>::
          Create(kOffsets, kIndices, std::span<double>(values), kShape,
                 asc::MemorySpace::kHost);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  const auto next = asc::FillSparseUniform01(asc::ExecutionContext::Serial(),
                                             *view, 11, 13, 17);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, next.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, *next, asc::RandomOffset{23});
  for (std::size_t position = 0; position < values.size(); ++position) {
    const auto word_offset = 17 + 2 * position;
    const double expected = asc::Uniform01<double>(
        asc::GeneratePhilox4x32Word(11, 13, word_offset),
        asc::GeneratePhilox4x32Word(11, 13, word_offset + 1));
    ASC_RANDOM_SPARSE_TEST_EQ(test,
                              std::bit_cast<std::uint64_t>(values[position]),
                              std::bit_cast<std::uint64_t>(expected));
  }
  ASC_RANDOM_SPARSE_TEST_CHECK(
      test,
      std::equal(kOffsets.begin(), kOffsets.end(), view->outer_offsets()));
  ASC_RANDOM_SPARSE_TEST_CHECK(
      test,
      std::equal(kIndices.begin(), kIndices.end(), view->inner_indices()));
}

// Structure-only generation and its address oracle form one scenario.
// NOLINTNEXTLINE(readability-function-size)
void CheckStructureOnly(TestContext& test) {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  auto shape = Shape::Create(4, 5);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, shape.ok());
  if (!shape.ok()) {
    return;
  }
  constexpr asc::nnz_t kCount = 6;
  constexpr asc::RandomStream kStream = 19;
  constexpr asc::RandomSubsequence kSubsequence = 23;
  constexpr asc::RandomOffset kOffset = 29;
  std::array<asc::SparseRandomStructureCandidate, 20> workspace{};
  std::array<std::uint64_t, kCount> ordinals{};
  const auto next = asc::GenerateSparseStructure(
      asc::ExecutionContext::Serial(), *shape, kCount, kStream, kSubsequence,
      kOffset, workspace, ordinals);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, next.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, *next, asc::RandomOffset{69});
  ASC_RANDOM_SPARSE_TEST_CHECK(
      test, std::is_sorted(ordinals.begin(), ordinals.end()));
  ASC_RANDOM_SPARSE_TEST_EQ(
      test, std::adjacent_find(ordinals.begin(), ordinals.end()),
      ordinals.end());

  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;
  for (std::uint64_t ordinal = 0; ordinal < 20; ++ordinal) {
    const std::uint64_t high = asc::GeneratePhilox4x32Word(
        kStream, kSubsequence, kOffset + 2 * ordinal);
    const std::uint64_t low = asc::GeneratePhilox4x32Word(
        kStream, kSubsequence, kOffset + 2 * ordinal + 1);
    expected.emplace_back((high << 32U) | low, ordinal);
  }
  std::sort(expected.begin(), expected.end());
  std::array<std::uint64_t, kCount> expected_ordinals{};
  for (std::size_t position = 0; position < expected_ordinals.size();
       ++position) {
    expected_ordinals[position] = expected[position].second;
  }
  std::sort(expected_ordinals.begin(), expected_ordinals.end());
  ASC_RANDOM_SPARSE_TEST_EQ(test, ordinals, expected_ordinals);

  ordinals.fill(71);
  const auto overflow = asc::GenerateSparseStructure(
      asc::ExecutionContext::Serial(), *shape, kCount, kStream, kSubsequence,
      std::numeric_limits<asc::RandomOffset>::max() - 20, workspace, ordinals);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !overflow.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, overflow.status().code(),
                            asc::ErrorCode::kOverflow);
  for (std::uint64_t ordinal : ordinals) {
    ASC_RANDOM_SPARSE_TEST_EQ(test, ordinal, std::uint64_t{71});
  }

  std::span<asc::SparseRandomStructureCandidate> empty_workspace;
  std::span<std::uint64_t> empty_output;
  const auto empty = asc::GenerateSparseStructure(
      asc::ExecutionContext::Serial(), *shape, 0, kStream, kSubsequence,
      kOffset, empty_workspace, empty_output);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, empty.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, *empty, kOffset);

  std::array<std::uint64_t, 2> failure_output{73, 79};
  std::array<asc::SparseRandomStructureCandidate, 19> short_workspace{};
  const auto short_failure = asc::GenerateSparseStructure(
      asc::ExecutionContext::Serial(), *shape, 2, kStream, kSubsequence,
      kOffset, short_workspace, failure_output);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !short_failure.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, short_failure.status().code(),
                            asc::ErrorCode::kShape);
  ASC_RANDOM_SPARSE_TEST_EQ(test, failure_output[0], std::uint64_t{73});
  ASC_RANDOM_SPARSE_TEST_EQ(test, failure_output[1], std::uint64_t{79});

  const auto bad_count = asc::GenerateSparseStructure(
      asc::ExecutionContext::Serial(), *shape, -1, kStream, kSubsequence,
      kOffset, empty_workspace, empty_output);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !bad_count.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, bad_count.status().code(),
                            asc::ErrorCode::kShape);

  std::array<asc::SparseRandomStructureCandidate, 20> alias_workspace{};
  auto* alias_data = reinterpret_cast<std::uint64_t*>(alias_workspace.data());
  const auto alias_failure = asc::GenerateSparseStructure(
      asc::ExecutionContext::Serial(), *shape, 2, kStream, kSubsequence,
      kOffset, alias_workspace, std::span<std::uint64_t>(alias_data, 2));
  ASC_RANDOM_SPARSE_TEST_CHECK(test, !alias_failure.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, alias_failure.status().code(),
                            asc::ErrorCode::kInvalidArgument);
}

void CheckCombinedDomainIndependence(TestContext& test) {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  auto shape = Shape::Create(5, 6);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, shape.ok());
  if (!shape.ok()) {
    return;
  }
  asc::HostMemoryResource resource;
  auto first = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *shape, 7, resource, 31, 37, 41, 43, 47,
      53);
  auto changed_structure = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *shape, 7, resource, 59, 61, 67, 43, 47,
      53);
  auto changed_values = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *shape, 7, resource, 31, 37, 41, 71, 73,
      79);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, first.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, changed_structure.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, changed_values.ok());
  if (!first.ok() || !changed_structure.ok() || !changed_values.ok()) {
    return;
  }
  auto first_view = first->array.view();
  auto structure_view = changed_structure->array.view();
  auto value_view = changed_values->array.view();
  ASC_RANDOM_SPARSE_TEST_CHECK(test, first_view.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, structure_view.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, value_view.ok());
  for (asc::nnz_t position = 0; position < first_view->nnz(); ++position) {
    ASC_RANDOM_SPARSE_TEST_EQ(
        test,
        std::bit_cast<std::uint32_t>(
            first_view->values()[static_cast<std::size_t>(position)]),
        std::bit_cast<std::uint32_t>(
            structure_view->values()[static_cast<std::size_t>(position)]));
    auto first_coordinate = first_view->Coordinate(position);
    auto value_coordinate = value_view->Coordinate(position);
    ASC_RANDOM_SPARSE_TEST_CHECK(test, first_coordinate.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(test, value_coordinate.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(
        test, std::equal(first_coordinate->begin(), first_coordinate->end(),
                         value_coordinate->begin()));
  }
}

}  // namespace

int main() {
  TestContext test;
  CheckCoordinateValueFill(test);
  CheckCompressedValueFill(test);
  CheckStructureOnly(test);
  CheckCombinedDomainIndependence(test);
  return test.Finish();
}
