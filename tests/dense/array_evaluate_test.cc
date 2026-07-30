#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/evaluate.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"
#include "counting_memory_resource.h"
#include "test_support.h"

namespace m3_test_types {

struct InstrumentedExpression {
  const double* values;
  std::array<asc::extent_t, 2> shape;
  std::array<std::array<asc::index_t, 2>, 6>* coordinates;
  std::size_t* reads;
};

}  // namespace m3_test_types

namespace asc {

template <>
struct ExpressionAdapter<m3_test_types::InstrumentedExpression> {
  using value_type = double;
  static constexpr rank_t rank = 2;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 2> Shape(
      const m3_test_types::InstrumentedExpression& expression) noexcept {
    return expression.shape;
  }

  static double Read(const m3_test_types::InstrumentedExpression& expression,
                     std::span<const index_t, 2> coordinates) noexcept {
    const std::size_t read = (*expression.reads)++;
    (*expression.coordinates)[read] = {coordinates[0], coordinates[1]};
    return expression.values[static_cast<std::size_t>(
        coordinates[0] + expression.shape[0] * coordinates[1])];
  }

  static bool MayAlias(const m3_test_types::InstrumentedExpression&,
                       AliasToken) noexcept {
    return false;
  }
};

}  // namespace asc

namespace {

using asc_dense_test::TestContext;
using DynamicMatrixExtents =
    asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using DynamicVectorExtents = asc::Extents<asc::kDynamicExtent>;

static_assert(std::is_move_constructible_v<
              asc::DenseArray<double, DynamicMatrixExtents>>);
static_assert(
    std::is_move_assignable_v<asc::DenseArray<double, DynamicMatrixExtents>>);
static_assert(!std::is_copy_constructible_v<
              asc::DenseArray<double, DynamicMatrixExtents>>);
static_assert(
    !std::is_copy_assignable_v<asc::DenseArray<double, DynamicMatrixExtents>>);

template <typename Result>
void CheckError(TestContext& test, const Result& result,
                asc::ErrorCode expected) {
  ASC_DENSE_TEST_CHECK(test, !result.ok());
  if (!result.ok()) {
    ASC_DENSE_TEST_EQ(test, result.status().code(), expected);
  }
}

void CheckStatusError(TestContext& test, const asc::Status& status,
                      asc::ErrorCode expected) {
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  if (!status.ok()) {
    ASC_DENSE_TEST_EQ(test, status.code(), expected);
  }
}

template <typename Element, std::size_t Rank>
Element& ElementAt(asc::DenseView<Element, Rank> view,
                   const std::array<asc::index_t, Rank>& coordinates) {
  auto element = view.At(std::span<const asc::index_t, Rank>(coordinates));
  if (!element.ok()) {
    std::abort();
  }
  return **element;
}

template <typename Element, std::size_t Rank>
asc::DenseView<Element, Rank> MakeView(
    Element* data, const std::array<asc::extent_t, Rank>& extents,
    const std::array<asc::stride_t, Rank>& strides,
    asc::MemorySpace space = asc::MemorySpace::kHost) {
  auto mapping = asc::DenseLayout<Rank>::Create(
      std::span<const asc::extent_t, Rank>(extents),
      asc::LayoutStride<Rank>{.strides = strides});
  if (!mapping.ok()) {
    std::abort();
  }
  auto view = asc::DenseView<Element, Rank>::Create(data, *mapping, space);
  if (!view.ok()) {
    std::abort();
  }
  return *view;
}

void TestOwnerLifecycle(TestContext& test) {
  asc_dense_test::CountingMemoryResource resource;
  auto extents = DynamicMatrixExtents::Create(2, 3);
  ASC_DENSE_TEST_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }

  {
    auto array = asc::DenseArray<double, DynamicMatrixExtents>::Create(
        resource, *extents);
    ASC_DENSE_TEST_CHECK(test, array.ok());
    if (!array.ok()) {
      return;
    }
    ASC_DENSE_TEST_EQ(test, resource.successful_allocations(), std::size_t{1});
    ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{1});
    ASC_DENSE_TEST_EQ(test, array->logical_size(), asc::extent_t{6});
    ASC_DENSE_TEST_EQ(test, array->mapping().kind(),
                      asc::DenseLayoutKind::kLeft);
    auto view = array->view();
    ASC_DENSE_TEST_CHECK(test, view.ok());
    if (!view.ok()) {
      return;
    }
    for (asc::index_t column = 0; column < 3; ++column) {
      for (asc::index_t row = 0; row < 2; ++row) {
        ASC_DENSE_TEST_EQ(
            test, ElementAt(*view, std::array<asc::index_t, 2>{row, column}),
            0.0);
      }
    }
    ElementAt(*view, std::array<asc::index_t, 2>{1, 2}) = 7.5;

    auto clone = array->Clone(resource, asc::ExecutionContext::Serial());
    ASC_DENSE_TEST_CHECK(test, clone.ok());
    if (!clone.ok()) {
      return;
    }
    auto clone_view = clone->view();
    ASC_DENSE_TEST_CHECK(test, clone_view.ok());
    if (!clone_view.ok()) {
      return;
    }
    ASC_DENSE_TEST_CHECK(test, clone_view->data() != view->data());
    ASC_DENSE_TEST_EQ(
        test, ElementAt(*clone_view, std::array<asc::index_t, 2>{1, 2}), 7.5);
    ElementAt(*clone_view, std::array<asc::index_t, 2>{1, 2}) = -4.0;
    ASC_DENSE_TEST_EQ(test, ElementAt(*view, std::array<asc::index_t, 2>{1, 2}),
                      7.5);

    auto moved = std::move(*clone);
    ASC_DENSE_TEST_CHECK(test, moved.valid());
    ASC_DENSE_TEST_CHECK(test, !clone->valid());
    CheckError(test, clone->view(), asc::ErrorCode::kInvalidState);
    CheckError(test, clone->Clone(resource, asc::ExecutionContext::Serial()),
               asc::ErrorCode::kInvalidState);
    CheckStatusError(test, clone->DiscardResize(*extents),
                     asc::ErrorCode::kInvalidState);

    auto replacement_extents = DynamicMatrixExtents::Create(3, 1);
    ASC_DENSE_TEST_CHECK(test, replacement_extents.ok());
    if (!replacement_extents.ok()) {
      return;
    }
    asc::Status resize_status = moved.DiscardResize(*replacement_extents);
    ASC_DENSE_TEST_CHECK(test, resize_status.ok());
    auto resized_view = moved.view();
    ASC_DENSE_TEST_CHECK(test, resized_view.ok());
    if (resized_view.ok()) {
      ASC_DENSE_TEST_EQ(test, resized_view->extents()[0], asc::extent_t{3});
      ASC_DENSE_TEST_EQ(test, resized_view->extents()[1], asc::extent_t{1});
      for (asc::index_t row = 0; row < 3; ++row) {
        ASC_DENSE_TEST_EQ(
            test, ElementAt(*resized_view, std::array<asc::index_t, 2>{row, 0}),
            0.0);
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});
  ASC_DENSE_TEST_EQ(test, resource.successful_allocations(),
                    resource.deallocations());
  ASC_DENSE_TEST_EQ(test, resource.allocated_bytes(),
                    resource.deallocated_bytes());
}

void TestOwnerRankZeroAndZeroExtent(TestContext& test) {
  asc_dense_test::CountingMemoryResource resource;
  auto scalar_extents = asc::Extents<>::Create();
  ASC_DENSE_TEST_CHECK(test, scalar_extents.ok());
  if (!scalar_extents.ok()) {
    return;
  }
  {
    auto scalar = asc::DenseArray<double, asc::Extents<>>::Create(
        resource, *scalar_extents);
    ASC_DENSE_TEST_CHECK(test, scalar.ok());
    if (!scalar.ok()) {
      return;
    }
    ASC_DENSE_TEST_EQ(test, scalar->logical_size(), asc::extent_t{1});
    auto scalar_view = scalar->view();
    ASC_DENSE_TEST_CHECK(test, scalar_view.ok());
    if (scalar_view.ok()) {
      const std::array<asc::index_t, 0> coordinate{};
      ASC_DENSE_TEST_EQ(test, ElementAt(*scalar_view, coordinate), 0.0);
    }
  }
  ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});

  using MixedExtents = asc::Extents<asc::kDynamicExtent, 3>;
  auto empty_extents = MixedExtents::Create(0);
  ASC_DENSE_TEST_CHECK(test, empty_extents.ok());
  if (!empty_extents.ok()) {
    return;
  }
  const std::size_t successful_before = resource.successful_allocations();
  {
    auto empty = asc::DenseArray<double, MixedExtents>::Create(
        resource, *empty_extents, asc::LayoutRight{});
    ASC_DENSE_TEST_CHECK(test, empty.ok());
    if (!empty.ok()) {
      return;
    }
    ASC_DENSE_TEST_CHECK(test, empty->valid());
    ASC_DENSE_TEST_EQ(test, empty->logical_size(), asc::extent_t{0});
    ASC_DENSE_TEST_EQ(test, empty->mapping().required_span_size(),
                      std::size_t{0});
    ASC_DENSE_TEST_EQ(test, empty->mapping().kind(),
                      asc::DenseLayoutKind::kRight);
    auto empty_view = empty->view();
    ASC_DENSE_TEST_CHECK(test, empty_view.ok());
    if (empty_view.ok()) {
      ASC_DENSE_TEST_EQ(test, empty_view->data(), nullptr);
    }
  }
  ASC_DENSE_TEST_EQ(test, resource.successful_allocations(), successful_before);
  ASC_DENSE_TEST_EQ(test, resource.live_allocations(), std::size_t{0});

  CheckError(test, MixedExtents::Create(-1), asc::ErrorCode::kShape);
}

void TestRightOwnerAndFailureTransaction(TestContext& test) {
  asc_dense_test::CountingMemoryResource resource;
  auto extents = DynamicMatrixExtents::Create(2, 3);
  ASC_DENSE_TEST_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }
  auto array = asc::DenseArray<int, DynamicMatrixExtents>::Create(
      resource, *extents, asc::LayoutRight{});
  ASC_DENSE_TEST_CHECK(test, array.ok());
  if (!array.ok()) {
    return;
  }
  ASC_DENSE_TEST_EQ(test, array->mapping().kind(),
                    asc::DenseLayoutKind::kRight);
  auto view = array->view();
  ASC_DENSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  ElementAt(*view, std::array<asc::index_t, 2>{0, 0}) = 11;
  ElementAt(*view, std::array<asc::index_t, 2>{1, 2}) = 29;
  int* original_pointer = view->data();
  const std::size_t live_before = resource.live_allocations();

  resource.FailRequest(resource.allocation_requests());
  auto replacement_extents = DynamicMatrixExtents::Create(4, 2);
  ASC_DENSE_TEST_CHECK(test, replacement_extents.ok());
  if (!replacement_extents.ok()) {
    return;
  }
  CheckStatusError(test, array->DiscardResize(*replacement_extents),
                   asc::ErrorCode::kAllocation);
  resource.DisableFailure();

  auto preserved = array->view();
  ASC_DENSE_TEST_CHECK(test, preserved.ok());
  if (preserved.ok()) {
    ASC_DENSE_TEST_EQ(test, preserved->data(), original_pointer);
    ASC_DENSE_TEST_EQ(test, preserved->extents()[0], asc::extent_t{2});
    ASC_DENSE_TEST_EQ(test, preserved->extents()[1], asc::extent_t{3});
    ASC_DENSE_TEST_EQ(
        test, ElementAt(*preserved, std::array<asc::index_t, 2>{0, 0}), 11);
    ASC_DENSE_TEST_EQ(
        test, ElementAt(*preserved, std::array<asc::index_t, 2>{1, 2}), 29);
  }
  ASC_DENSE_TEST_EQ(test, resource.live_allocations(), live_before);

  resource.FailRequest(resource.allocation_requests());
  CheckError(test, array->Clone(resource, asc::ExecutionContext::Serial()),
             asc::ErrorCode::kAllocation);
  resource.DisableFailure();
  ASC_DENSE_TEST_EQ(test, resource.live_allocations(), live_before);
}

void TestOwnerRejections(TestContext& test) {
  auto extents = DynamicVectorExtents::Create(4);
  ASC_DENSE_TEST_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }
  asc_dense_test::CountingMemoryResource device_resource(
      asc::MemorySpace::kDevice);
  CheckError(test,
             asc::DenseArray<double, DynamicVectorExtents>::Create(
                 device_resource, *extents),
             asc::ErrorCode::kUnsupported);
  ASC_DENSE_TEST_EQ(test, device_resource.allocation_requests(),
                    std::size_t{0});

  asc_dense_test::CountingMemoryResource host_resource;
  auto huge =
      DynamicVectorExtents::Create(std::numeric_limits<asc::extent_t>::max());
  ASC_DENSE_TEST_CHECK(test, huge.ok());
  if (huge.ok()) {
    CheckError(test,
               asc::DenseArray<double, DynamicVectorExtents>::Create(
                   host_resource, *huge),
               asc::ErrorCode::kOverflow);
  }
  ASC_DENSE_TEST_EQ(test, host_resource.allocation_requests(), std::size_t{0});
}

void TestEvaluationAndTraversal(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape{2, 3};
  constexpr std::array<double, 6> kInput{1, 4, -2, 0, 3, -1};
  constexpr std::array<double, 6> kExpected{1, 7, -5, -1, 5, -3};
  constexpr std::array<std::array<asc::stride_t, 2>, 3> kStrides{
      std::array<asc::stride_t, 2>{1, 2},
      std::array<asc::stride_t, 2>{3, 1},
      std::array<asc::stride_t, 2>{2, 5},
  };

  std::array<double, 6> input_storage = kInput;
  auto input = MakeView<const double, 2>(input_storage.data(), kShape, {1, 2});
  auto multiplied = asc::MakeMultiply(2.0, input);
  ASC_DENSE_TEST_CHECK(test, multiplied.ok());
  if (!multiplied.ok()) {
    return;
  }
  auto expression = asc::MakeSubtract(*multiplied, 1.0);
  ASC_DENSE_TEST_CHECK(test, expression.ok());
  if (!expression.ok()) {
    return;
  }

  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  for (const auto strides : kStrides) {
    std::array<double, 13> output_storage{};
    auto output = MakeView<double, 2>(output_storage.data(), kShape, strides);
    asc::Status status = asc::Evaluate(context, *expression, output);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    std::size_t expected_index = 0;
    for (asc::index_t column = 0; column < kShape[1]; ++column) {
      for (asc::index_t row = 0; row < kShape[0]; ++row) {
        ASC_DENSE_TEST_EQ(
            test, ElementAt(output, std::array<asc::index_t, 2>{row, column}),
            kExpected[expected_index++]);
      }
    }
  }

  std::array<double, 13> traversal_storage{};
  auto traversal_output =
      MakeView<double, 2>(traversal_storage.data(), kShape, {2, 5});
  std::array<std::array<asc::index_t, 2>, 6> coordinates{};
  std::size_t reads = 0;
  const m3_test_types::InstrumentedExpression instrumented{
      .values = kInput.data(),
      .shape = kShape,
      .coordinates = &coordinates,
      .reads = &reads,
  };
  ASC_DENSE_TEST_CHECK(
      test, asc::Evaluate(context, instrumented, traversal_output).ok());
  ASC_DENSE_TEST_EQ(test, reads, std::size_t{6});
  const std::array<std::array<asc::index_t, 2>, 6> expected_coordinates{
      std::array<asc::index_t, 2>{0, 0}, std::array<asc::index_t, 2>{1, 0},
      std::array<asc::index_t, 2>{0, 1}, std::array<asc::index_t, 2>{1, 1},
      std::array<asc::index_t, 2>{0, 2}, std::array<asc::index_t, 2>{1, 2},
  };
  ASC_DENSE_TEST_EQ(test, coordinates, expected_coordinates);

  std::size_t allocations = 0;
  reads = 0;
  {
    asc_dense_test::AllocationProbe probe;
    const asc::Status status =
        asc::Evaluate(context, instrumented, traversal_output);
    allocations = probe.count();
    ASC_DENSE_TEST_CHECK(test, status.ok());
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

void TestEvaluationFailuresAndIdentity(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const std::array<asc::extent_t, 1> shape{4};
  std::array<double, 6> storage{1, 2, 3, 4, 5, 6};
  auto source = MakeView<const double, 1>(storage.data(), shape, {1});
  auto exact = MakeView<double, 1>(storage.data(), shape, {1});
  ASC_DENSE_TEST_CHECK(test, asc::Evaluate(context, source, exact).ok());
  ASC_DENSE_TEST_EQ(test, storage, (std::array<double, 6>{1, 2, 3, 4, 5, 6}));

  auto partial_source = MakeView<const double, 1>(storage.data(), shape, {1});
  auto partial_destination =
      MakeView<double, 1>(storage.data() + 1, shape, {1});
  const auto before_partial = storage;
  CheckStatusError(test,
                   asc::Evaluate(context, partial_source, partial_destination),
                   asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, storage, before_partial);

  auto alias_expression = asc::MakeAdd(exact, 1.0);
  ASC_DENSE_TEST_CHECK(test, alias_expression.ok());
  if (alias_expression.ok()) {
    const auto before_alias = storage;
    CheckStatusError(test, asc::Evaluate(context, *alias_expression, exact),
                     asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, storage, before_alias);
  }

  const std::array<asc::extent_t, 1> short_shape{3};
  std::array<double, 3> short_storage{};
  auto short_output =
      MakeView<double, 1>(short_storage.data(), short_shape, {1});
  CheckStatusError(test, asc::Evaluate(context, source, short_output),
                   asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(test, short_storage, (std::array<double, 3>{0, 0, 0}));

  std::array<double, 4> device_storage{};
  auto device_output = MakeView<double, 1>(device_storage.data(), shape, {1},
                                           asc::MemorySpace::kDevice);
  CheckStatusError(test, asc::Evaluate(context, 3.0, device_output),
                   asc::ErrorCode::kMemoryAccess);
  ASC_DENSE_TEST_EQ(test, device_storage, (std::array<double, 4>{0, 0, 0, 0}));

  std::array<double, 4> device_source_storage{9, 8, 7, 6};
  auto device_source = MakeView<const double, 1>(
      device_source_storage.data(), shape, {1}, asc::MemorySpace::kDevice);
  std::array<double, 4> host_destination_storage{31, 32, 33, 34};
  auto host_destination =
      MakeView<double, 1>(host_destination_storage.data(), shape, {1});
  const auto before_device_source = host_destination_storage;
  CheckStatusError(test,
                   asc::Evaluate(context, device_source, host_destination),
                   asc::ErrorCode::kMemoryAccess);
  ASC_DENSE_TEST_EQ(test, host_destination_storage, before_device_source);

  auto nested_device_source = asc::MakeAdd(device_source, 1.0);
  ASC_DENSE_TEST_CHECK(test, nested_device_source.ok());
  if (nested_device_source.ok()) {
    CheckStatusError(
        test, asc::Evaluate(context, *nested_device_source, host_destination),
        asc::ErrorCode::kMemoryAccess);
    ASC_DENSE_TEST_EQ(test, host_destination_storage, before_device_source);
  }
}

void TestReductions(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const std::array<asc::extent_t, 1> shape{4};
  std::array<double, 7> storage{3, 0, -1, 0, 4, 0, 2};
  auto view = MakeView<const double, 1>(storage.data(), shape, {2});
  auto sum = asc::ReduceSum(context, view);
  auto minimum = asc::ReduceMin(context, view);
  auto maximum = asc::ReduceMax(context, view);
  ASC_DENSE_TEST_CHECK(test, sum.ok());
  ASC_DENSE_TEST_CHECK(test, minimum.ok());
  ASC_DENSE_TEST_CHECK(test, maximum.ok());
  if (sum.ok()) {
    ASC_DENSE_TEST_EQ(test, *sum, 8.0);
  }
  if (minimum.ok()) {
    ASC_DENSE_TEST_EQ(test, *minimum, -1.0);
  }
  if (maximum.ok()) {
    ASC_DENSE_TEST_EQ(test, *maximum, 4.0);
  }

  const std::array<asc::extent_t, 1> empty_shape{0};
  auto empty = MakeView<const double, 1>(nullptr, empty_shape, {1});
  auto empty_sum = asc::ReduceSum(context, empty);
  ASC_DENSE_TEST_CHECK(test, empty_sum.ok());
  if (empty_sum.ok()) {
    ASC_DENSE_TEST_EQ(test, *empty_sum, 0.0);
    ASC_DENSE_TEST_CHECK(test, !std::signbit(*empty_sum));
  }
  CheckError(test, asc::ReduceMin(context, empty),
             asc::ErrorCode::kInvalidArgument);
  CheckError(test, asc::ReduceMax(context, empty),
             asc::ErrorCode::kInvalidArgument);

  const std::array<asc::extent_t, 1> cancellation_shape{3};
  std::array<float, 3> cancellation{1.0e20F, 1.0F, -1.0e20F};
  auto cancellation_view =
      MakeView<const float, 1>(cancellation.data(), cancellation_shape, {1});
  auto cancellation_sum = asc::ReduceSum(context, cancellation_view);
  ASC_DENSE_TEST_CHECK(test, cancellation_sum.ok());
  if (cancellation_sum.ok()) {
    ASC_DENSE_TEST_EQ(test, *cancellation_sum, 0.0F);
  }

  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    const auto allocation_sum = asc::ReduceSum(context, view);
    allocations = probe.count();
    ASC_DENSE_TEST_CHECK(test, allocation_sum.ok());
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

void TestIntegralReductionOverflow(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  constexpr std::array<asc::extent_t, 1> kShape{2};

  std::array<std::int64_t, 2> positive_overflow{
      std::numeric_limits<std::int64_t>::max(), 1};
  auto positive_view =
      MakeView<const std::int64_t, 1>(positive_overflow.data(), kShape, {1});
  CheckError(test, asc::ReduceSum(context, positive_view),
             asc::ErrorCode::kOverflow);

  std::array<std::int64_t, 2> negative_overflow{
      std::numeric_limits<std::int64_t>::min(), -1};
  auto negative_view =
      MakeView<const std::int64_t, 1>(negative_overflow.data(), kShape, {1});
  CheckError(test, asc::ReduceSum(context, negative_view),
             asc::ErrorCode::kOverflow);

  std::array<std::uint64_t, 2> unsigned_overflow{
      std::numeric_limits<std::uint64_t>::max(), 1};
  auto unsigned_view =
      MakeView<const std::uint64_t, 1>(unsigned_overflow.data(), kShape, {1});
  CheckError(test, asc::ReduceSum(context, unsigned_view),
             asc::ErrorCode::kOverflow);

  std::array<std::int64_t, 3> safe{-7, 11, 19};
  constexpr std::array<asc::extent_t, 1> kSafeShape{3};
  auto safe_view =
      MakeView<const std::int64_t, 1>(safe.data(), kSafeShape, {1});
  auto safe_sum = asc::ReduceSum(context, safe_view);
  ASC_DENSE_TEST_CHECK(test, safe_sum.ok());
  if (safe_sum.ok()) {
    ASC_DENSE_TEST_EQ(test, *safe_sum, std::int64_t{23});
  }
}

}  // namespace

int main() {
  TestContext test;
  TestOwnerLifecycle(test);
  TestOwnerRankZeroAndZeroExtent(test);
  TestRightOwnerAndFailureTransaction(test);
  TestOwnerRejections(test);
  TestEvaluationAndTraversal(test);
  TestEvaluationFailuresAndIdentity(test);
  TestReductions(test);
  TestIntegralReductionOverflow(test);
  return test.Finish();
}
