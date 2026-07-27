#include "asc/sparse/coordinate.h"

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "allocation_counter.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "test_support.h"

namespace {

using ThreeDimensionalExtents = asc::Extents<2, 3, 2>;
using ThreeDimensionalBuilder =
    asc::CoordinateBuilder<std::int64_t, ThreeDimensionalExtents>;
using ThreeDimensionalArray =
    asc::CoordinateArray<std::int64_t, ThreeDimensionalExtents>;

static_assert(!std::copy_constructible<ThreeDimensionalBuilder>);
static_assert(!std::is_copy_assignable_v<ThreeDimensionalBuilder>);
static_assert(std::move_constructible<ThreeDimensionalBuilder>);
static_assert(!std::copy_constructible<ThreeDimensionalArray>);
static_assert(std::move_constructible<ThreeDimensionalArray>);
static_assert(std::is_trivially_copyable_v<asc::CoordinateView<double, 3>>);
static_assert(
    std::is_trivially_copyable_v<asc::CoordinateView<const double, 3>>);
static_assert(std::constructible_from<asc::CoordinateView<const double, 3>,
                                      asc::CoordinateView<double, 3>>);
static_assert(!std::constructible_from<asc::CoordinateView<double, 3>,
                                       asc::CoordinateView<const double, 3>>);
static_assert(asc::ReadableExpression<asc::CoordinateView<const double, 3>>);
static_assert(
    asc::PlacedReadableExpression<asc::CoordinateView<const double, 3>>);
static_assert(asc::WritableExpression<asc::CoordinateView<double, 3>>);
static_assert(!asc::WritableExpression<asc::CoordinateView<const double, 3>>);

template <typename Element, std::size_t Rank>
std::span<const asc::index_t> Coordinates(
    const asc::CoordinateView<Element, Rank>& view) {
  return std::span<const asc::index_t>(
      view.coordinate_data(),
      static_cast<std::size_t>(view.nnz()) * static_cast<std::size_t>(Rank));
}

template <typename Element, std::size_t Rank>
std::span<Element> Values(const asc::CoordinateView<Element, Rank>& view) {
  return std::span<Element>(view.value_data(),
                            static_cast<std::size_t>(view.nnz()));
}

void AddOracleEntries(ThreeDimensionalBuilder& builder,
                      asc_sparse_test::TestContext& context) {
  ASC_SPARSE_TEST_CHECK(
      context, builder.Add(std::array<asc::index_t, 3>{1, 0, 1}, 5).ok());
  ASC_SPARSE_TEST_CHECK(
      context, builder.Add(std::array<asc::index_t, 3>{0, 2, 1}, -2).ok());
  ASC_SPARSE_TEST_CHECK(
      context, builder.Add(std::array<asc::index_t, 3>{1, 0, 1}, 7).ok());
  ASC_SPARSE_TEST_CHECK(
      context, builder.Add(std::array<asc::index_t, 3>{0, 0, 0}, 0).ok());
  ASC_SPARSE_TEST_CHECK(
      context, builder.Add(std::array<asc::index_t, 3>{0, 2, 0}, 4).ok());
}

void CheckCanonicalOracle(asc_sparse_test::TestContext& context) {
  auto extents = ThreeDimensionalExtents::Create();
  ASC_SPARSE_TEST_CHECK(context, extents.ok());
  asc_sparse_test::CountingMemoryResource resource;
  auto builder_result = ThreeDimensionalBuilder::Create(*extents, 5, resource);
  ASC_SPARSE_TEST_CHECK(context, builder_result.ok());
  ThreeDimensionalBuilder builder = std::move(*builder_result);
  AddOracleEntries(builder, context);

  const std::size_t attempts_before = resource.allocation_attempts();
  asc::Result<ThreeDimensionalArray> finalized =
      asc::Status(asc::ErrorCode::kInternal, "not evaluated");
  std::size_t heap_allocations = 1;
  {
    asc_sparse_test::AllocationCountScope allocation_scope;
    finalized = builder.Finalize(asc::ExecutionContext::Serial(),
                                 asc::DuplicatePolicy::kSum,
                                 asc::ExplicitZeroPolicy::kKeep);
    heap_allocations = allocation_scope.count();
  }
  ASC_SPARSE_TEST_CHECK(context, finalized.ok());
  ASC_SPARSE_TEST_EQ(context, heap_allocations, 0U);
  ASC_SPARSE_TEST_EQ(context, resource.allocation_attempts(), attempts_before);
  ASC_SPARSE_TEST_EQ(context, builder.size(), 0);
  ASC_SPARSE_TEST_CHECK(context, builder.resource() == nullptr);

  auto view = finalized->view();
  ASC_SPARSE_TEST_CHECK(context, view.ok());
  ASC_SPARSE_TEST_EQ(context, view->shape(),
                     (std::array<asc::extent_t, 3>{2, 3, 2}));
  ASC_SPARSE_TEST_EQ(context, view->nnz(), 4);
  ASC_SPARSE_TEST_RANGE_EQ(
      context, Coordinates(*view),
      (std::span<const asc::index_t>(
          std::array<asc::index_t, 12>{0, 0, 0, 0, 2, 0, 0, 2, 1, 1, 0, 1})));
  ASC_SPARSE_TEST_RANGE_EQ(context, Values(*view),
                           (std::span<const std::int64_t>(
                               std::array<std::int64_t, 4>{0, 4, -2, 12})));

  auto stored_coordinate = view->CoordinateAt(2);
  auto stored_value = view->ValueAt(2);
  ASC_SPARSE_TEST_CHECK(context, stored_coordinate.ok());
  ASC_SPARSE_TEST_CHECK(context, stored_value.ok());
  ASC_SPARSE_TEST_EQ(context, (*stored_coordinate)[0], 0);
  ASC_SPARSE_TEST_EQ(context, (*stored_coordinate)[1], 2);
  ASC_SPARSE_TEST_EQ(context, (*stored_coordinate)[2], 1);
  ASC_SPARSE_TEST_EQ(context, **stored_value, -2);

  auto found = view->Find(
      std::array<asc::index_t, 3>{static_cast<asc::index_t>(1), 0, 1});
  auto missing = view->Find(
      std::array<asc::index_t, 3>{static_cast<asc::index_t>(1), 2, 1});
  ASC_SPARSE_TEST_CHECK(context, found.ok());
  ASC_SPARSE_TEST_CHECK(context, missing.ok());
  ASC_SPARSE_TEST_EQ(context, **found, 12);
  ASC_SPARSE_TEST_CHECK(context, *missing == nullptr);
  ASC_SPARSE_TEST_EQ(
      context,
      asc::ReadExpression(*view, std::span<const asc::index_t, 3>(
                                     std::array<asc::index_t, 3>{1, 2, 1})),
      0);
  ASC_SPARSE_TEST_EQ(context, asc::kExpressionSparsityEffect<decltype(*view)>,
                     asc::SparsityEffect::kStructurePreserving);

  const ThreeDimensionalArray& const_owner = *finalized;
  auto const_view = const_owner.view();
  ASC_SPARSE_TEST_CHECK(context, const_view.ok());
  ASC_SPARSE_TEST_CHECK(
      context, asc::MayAlias(*const_view, asc::AliasToken::FromIdentity(
                                              view->value_data())));
}

void CheckPoliciesAndRollback(asc_sparse_test::TestContext& context) {
  auto extents = ThreeDimensionalExtents::Create();
  asc_sparse_test::CountingMemoryResource resource;
  auto builder_result = ThreeDimensionalBuilder::Create(*extents, 5, resource);
  ThreeDimensionalBuilder builder = std::move(*builder_result);
  AddOracleEntries(builder, context);

  auto rejected = builder.Finalize(asc::ExecutionContext::Serial(),
                                   asc::DuplicatePolicy::kReject,
                                   asc::ExplicitZeroPolicy::kKeep);
  ASC_SPARSE_TEST_CHECK(context, !rejected.ok());
  ASC_SPARSE_TEST_EQ(context, rejected.status().code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, builder.size(), 5);
  ASC_SPARSE_TEST_CHECK(context, builder.resource() == &resource);

  auto finalized = builder.Finalize(asc::ExecutionContext::Serial(),
                                    asc::DuplicatePolicy::kSum,
                                    asc::ExplicitZeroPolicy::kDrop);
  ASC_SPARSE_TEST_CHECK(context, finalized.ok());
  auto view = finalized->view();
  ASC_SPARSE_TEST_EQ(context, view->nnz(), 3);
  ASC_SPARSE_TEST_RANGE_EQ(
      context, Coordinates(*view),
      (std::span<const asc::index_t>(
          std::array<asc::index_t, 9>{0, 2, 0, 0, 2, 1, 1, 0, 1})));
  ASC_SPARSE_TEST_RANGE_EQ(
      context, Values(*view),
      (std::span<const std::int64_t>(std::array<std::int64_t, 3>{4, -2, 12})));

  using ScalarExtents = asc::Extents<>;
  auto scalar_extents = ScalarExtents::Create();
  auto scalar_builder = asc::CoordinateBuilder<int, ScalarExtents>::Create(
      *scalar_extents, 2, resource);
  ASC_SPARSE_TEST_CHECK(context, scalar_builder.ok());
  ASC_SPARSE_TEST_CHECK(
      context, scalar_builder->Add(std::array<asc::index_t, 0>{}, 4).ok());
  ASC_SPARSE_TEST_CHECK(
      context, scalar_builder->Add(std::array<asc::index_t, 0>{}, -1).ok());
  auto scalar = scalar_builder->Finalize(asc::ExecutionContext::Serial(),
                                         asc::DuplicatePolicy::kSum,
                                         asc::ExplicitZeroPolicy::kKeep);
  ASC_SPARSE_TEST_CHECK(context, scalar.ok());
  auto scalar_view = scalar->view();
  ASC_SPARSE_TEST_EQ(context, scalar_view->nnz(), 1);
  ASC_SPARSE_TEST_EQ(context, Values(*scalar_view)[0], 3);

  using OneExtents = asc::Extents<1>;
  auto one_extents = OneExtents::Create();
  auto overflow_builder =
      asc::CoordinateBuilder<std::int64_t, OneExtents>::Create(*one_extents, 2,
                                                               resource);
  ASC_SPARSE_TEST_CHECK(context,
                        overflow_builder
                            ->Add(std::array<asc::index_t, 1>{0},
                                  std::numeric_limits<std::int64_t>::max())
                            .ok());
  ASC_SPARSE_TEST_CHECK(
      context, overflow_builder->Add(std::array<asc::index_t, 1>{0}, 1).ok());
  auto overflow = overflow_builder->Finalize(asc::ExecutionContext::Serial(),
                                             asc::DuplicatePolicy::kSum,
                                             asc::ExplicitZeroPolicy::kKeep);
  ASC_SPARSE_TEST_CHECK(context, !overflow.ok());
  ASC_SPARSE_TEST_EQ(context, overflow.status().code(),
                     asc::ErrorCode::kOverflow);
  ASC_SPARSE_TEST_EQ(context, overflow_builder->size(), 2);
}

void CheckStableFloatingAndNaN(asc_sparse_test::TestContext& context) {
  using OneExtents = asc::Extents<1>;
  auto extents = OneExtents::Create();
  asc::HostMemoryResource resource;

  auto stable =
      asc::CoordinateBuilder<double, OneExtents>::Create(*extents, 3, resource);
  ASC_SPARSE_TEST_CHECK(
      context, stable->Add(std::array<asc::index_t, 1>{0}, 1.0e20).ok());
  ASC_SPARSE_TEST_CHECK(
      context, stable->Add(std::array<asc::index_t, 1>{0}, -1.0e20).ok());
  ASC_SPARSE_TEST_CHECK(context,
                        stable->Add(std::array<asc::index_t, 1>{0}, 3.0).ok());
  auto stable_result = stable->Finalize(asc::ExecutionContext::Serial(),
                                        asc::DuplicatePolicy::kSum,
                                        asc::ExplicitZeroPolicy::kKeep);
  ASC_SPARSE_TEST_CHECK(context, stable_result.ok());
  auto stable_view = stable_result->view();
  ASC_SPARSE_TEST_EQ(context, Values(*stable_view)[0], 3.0);

  constexpr std::uint64_t kNanBits = 0x7ff8000000001234ULL;
  const double nan = std::bit_cast<double>(kNanBits);
  auto nan_builder =
      asc::CoordinateBuilder<double, OneExtents>::Create(*extents, 1, resource);
  ASC_SPARSE_TEST_CHECK(
      context, nan_builder->Add(std::array<asc::index_t, 1>{0}, nan).ok());
  auto nan_result = nan_builder->Finalize(asc::ExecutionContext::Serial(),
                                          asc::DuplicatePolicy::kSum,
                                          asc::ExplicitZeroPolicy::kDrop);
  ASC_SPARSE_TEST_CHECK(context, nan_result.ok());
  auto nan_view = nan_result->view();
  ASC_SPARSE_TEST_EQ(context, nan_view->nnz(), 1);
  ASC_SPARSE_TEST_EQ(
      context, std::bit_cast<std::uint64_t>(Values(*nan_view)[0]), kNanBits);
}

void CheckBoundariesAndMoves(asc_sparse_test::TestContext& context) {
  using ZeroExtents = asc::Extents<2, 0, 3>;
  auto zero_extents = ZeroExtents::Create();
  asc_sparse_test::CountingMemoryResource resource;
  auto zero_builder = asc::CoordinateBuilder<int, ZeroExtents>::Create(
      *zero_extents, 1, resource);
  ASC_SPARSE_TEST_CHECK(context, zero_builder.ok());
  const asc::Status empty_add =
      zero_builder->Add(std::array<asc::index_t, 3>{0, 0, 0}, 1);
  ASC_SPARSE_TEST_CHECK(context, !empty_add.ok());
  ASC_SPARSE_TEST_EQ(context, empty_add.code(), asc::ErrorCode::kIndex);

  auto zero_capacity = asc::CoordinateBuilder<int, ZeroExtents>::Create(
      *zero_extents, 0, resource);
  ASC_SPARSE_TEST_CHECK(context, zero_capacity.ok());
  ASC_SPARSE_TEST_CHECK(
      context,
      !zero_capacity->Add(std::array<asc::index_t, 3>{0, 0, 0}, 1).ok());

  auto negative_capacity = asc::CoordinateBuilder<int, ZeroExtents>::Create(
      *zero_extents, -1, resource);
  ASC_SPARSE_TEST_CHECK(context, !negative_capacity.ok());
  ASC_SPARSE_TEST_EQ(context, negative_capacity.status().code(),
                     asc::ErrorCode::kInvalidArgument);

  using RankThree = asc::Extents<1, 1, 1>;
  auto rank_three = RankThree::Create();
  auto overflow = asc::CoordinateBuilder<int, RankThree>::Create(
      *rank_three, std::numeric_limits<asc::nnz_t>::max(), resource);
  ASC_SPARSE_TEST_CHECK(context, !overflow.ok());
  ASC_SPARSE_TEST_EQ(context, overflow.status().code(),
                     asc::ErrorCode::kOverflow);

  auto extents = ThreeDimensionalExtents::Create();
  auto source = ThreeDimensionalBuilder::Create(*extents, 1, resource);
  ThreeDimensionalBuilder moved(std::move(*source));
  ASC_SPARSE_TEST_CHECK(
      context, !source->Add(std::array<asc::index_t, 3>{0, 0, 0}, 1).ok());
  ASC_SPARSE_TEST_CHECK(
      context, moved.Add(std::array<asc::index_t, 3>{0, 0, 0}, 1).ok());
  auto owner = moved.Finalize(asc::ExecutionContext::Serial(),
                              asc::DuplicatePolicy::kReject,
                              asc::ExplicitZeroPolicy::kKeep);
  ThreeDimensionalArray moved_owner(std::move(*owner));
  ASC_SPARSE_TEST_CHECK(context, !owner->view().ok());
  ASC_SPARSE_TEST_CHECK(context, moved_owner.view().ok());
}

void CheckExternalValidationAndFailureResource(
    asc_sparse_test::TestContext& context) {
  constexpr std::array<asc::extent_t, 2> kShape = {2, 2};
  std::array<double, 2> values = {1.0, 2.0};
  std::array<asc::index_t, 4> sorted_coordinates = {0, 0, 1, 1};
  auto valid = asc::CoordinateView<double, 2>::Create(sorted_coordinates.data(),
                                                      values.data(), kShape, 2,
                                                      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, valid.ok());

  std::array<asc::index_t, 4> unsorted = {1, 1, 0, 0};
  auto bad_order = asc::CoordinateView<double, 2>::Create(
      unsorted.data(), values.data(), kShape, 2, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !bad_order.ok());
  ASC_SPARSE_TEST_EQ(context, bad_order.status().code(),
                     asc::ErrorCode::kInvalidArgument);

  std::array<asc::index_t, 4> duplicate = {0, 0, 0, 0};
  auto bad_duplicate = asc::CoordinateView<double, 2>::Create(
      duplicate.data(), values.data(), kShape, 2, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !bad_duplicate.ok());
  std::array<asc::index_t, 4> out_of_range = {0, 0, 2, 0};
  auto bad_index = asc::CoordinateView<double, 2>::Create(
      out_of_range.data(), values.data(), kShape, 2, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !bad_index.ok());
  ASC_SPARSE_TEST_EQ(context, bad_index.status().code(),
                     asc::ErrorCode::kIndex);

  auto null_coordinates = asc::CoordinateView<double, 2>::Create(
      nullptr, values.data(), kShape, 2, asc::MemorySpace::kHost);
  auto null_values = asc::CoordinateView<double, 2>::Create(
      sorted_coordinates.data(), nullptr, kShape, 2, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !null_coordinates.ok());
  ASC_SPARSE_TEST_CHECK(context, !null_values.ok());

  auto bad_space = asc::CoordinateView<double, 2>::Create(
      sorted_coordinates.data(), values.data(), kShape, 2,
      static_cast<asc::MemorySpace>(255));
  ASC_SPARSE_TEST_CHECK(context, !bad_space.ok());

  alignas(std::max_align_t) std::array<std::byte, 64> overlapping_storage{};
  const auto overlapping_snapshot = overlapping_storage;
  auto overlapping_spans = asc::CoordinateView<double, 2>::Create(
      reinterpret_cast<const asc::index_t*>(overlapping_storage.data()),
      reinterpret_cast<double*>(overlapping_storage.data()), kShape, 1,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, !overlapping_spans.ok());
  ASC_SPARSE_TEST_EQ(context, overlapping_spans.status().code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, overlapping_storage, overlapping_snapshot);

  auto device = asc::CoordinateView<double, 2>::Create(
      sorted_coordinates.data(), values.data(), kShape, 2,
      asc::MemorySpace::kDevice);
  ASC_SPARSE_TEST_CHECK(context, device.ok());
  ASC_SPARSE_TEST_CHECK(context, !device->ValueAt(0).ok());
  ASC_SPARSE_TEST_CHECK(context,
                        !device->Find(std::array<asc::index_t, 2>{0, 0}).ok());

  auto extents = ThreeDimensionalExtents::Create();
  asc_sparse_test::CountingMemoryResource failing;
  failing.FailAllocationAttempt(2);
  auto partial = ThreeDimensionalBuilder::Create(*extents, 3, failing);
  ASC_SPARSE_TEST_CHECK(context, !partial.ok());
  ASC_SPARSE_TEST_EQ(context, failing.successful_allocations(), 1U);
  ASC_SPARSE_TEST_EQ(context, failing.deallocations(), 1U);
  ASC_SPARSE_TEST_EQ(context, failing.live_allocations(), 0U);

  asc_sparse_test::CountingMemoryResource device_resource;
  device_resource.set_space(asc::MemorySpace::kDevice);
  auto unsupported =
      ThreeDimensionalBuilder::Create(*extents, 1, device_resource);
  ASC_SPARSE_TEST_CHECK(context, !unsupported.ok());
  ASC_SPARSE_TEST_EQ(context, unsupported.status().code(),
                     asc::ErrorCode::kUnsupported);
}

}  // namespace

int main() {
  asc_sparse_test::TestContext context;
  CheckCanonicalOracle(context);
  CheckPoliciesAndRollback(context);
  CheckStableFloatingAndNaN(context);
  CheckBoundariesAndMoves(context);
  CheckExternalValidationAndFailureResource(context);
  return context.Finish();
}
