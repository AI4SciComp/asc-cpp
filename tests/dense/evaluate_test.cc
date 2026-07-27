#include "asc/dense/evaluate.h"

#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>

#include "allocation_counter.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"
#include "test_support.h"

namespace external_expression {

struct Matrix {
  std::array<asc::extent_t, 2> shape = {2, 3};
  std::array<double, 6> values = {};
  std::array<int, 6>* read_order = nullptr;
  std::size_t* read_count = nullptr;
};

}  // namespace external_expression

namespace asc {

template <>
struct ExpressionAdapter<external_expression::Matrix> {
  using value_type = double;
  static constexpr rank_t kRank = 2;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  [[nodiscard]] static std::array<extent_t, 2> Shape(
      const external_expression::Matrix& matrix) {
    return matrix.shape;
  }

  [[nodiscard]] static double Read(const external_expression::Matrix& matrix,
                                   std::span<const index_t, 2> indices) {
    const std::size_t row = static_cast<std::size_t>(indices[0]);
    const std::size_t column = static_cast<std::size_t>(indices[1]);
    if (matrix.read_order != nullptr && matrix.read_count != nullptr) {
      (*matrix.read_order)[*matrix.read_count] =
          static_cast<int>(10 * row + column);
      ++*matrix.read_count;
    }
    return matrix.values[row + 2U * column];
  }

  [[nodiscard]] static bool MayAlias(const external_expression::Matrix&,
                                     AliasToken) {
    return false;
  }
};

}  // namespace asc

namespace {

template <std::size_t Rank>
auto MakeLeftView(double* data, const std::array<asc::extent_t, Rank>& shape) {
  auto mapping =
      asc::DenseLayoutMapping<Rank>::Create(asc::LayoutLeft{}, shape);
  if (!mapping.ok()) {
    return asc::Result<asc::DenseView<double, Rank>>(mapping.status());
  }
  return asc::DenseView<double, Rank>::Create(data, *mapping,
                                              asc::MemorySpace::kHost);
}

void CheckExpressionMetadata(asc_dense_test::TestContext& context) {
  static_assert(asc::ReadableExpression<asc::DenseView<double, 2>>);
  static_assert(asc::ReadableExpression<asc::DenseView<const double, 2>>);
  static_assert(asc::kExpressionRank<asc::DenseView<double, 2>> == 2);
  static_assert(
      std::same_as<asc::ExpressionValue<asc::DenseView<const double, 2>>,
                   double>);
  static_assert(asc::kExpressionOperationCategory<asc::DenseView<double, 2>> ==
                asc::ExpressionOperationCategory::kTerminal);
  static_assert(asc::kExpressionSparsityEffect<asc::DenseView<double, 2>> ==
                asc::SparsityEffect::kStructurePreserving);

  const std::array<asc::extent_t, 2> shape = {2, 3};
  std::array<double, 6> values{};
  const auto view = MakeLeftView(values.data(), shape);
  ASC_DENSE_TEST_CHECK(context, view.ok());
  ASC_DENSE_TEST_EQ(context, asc::ExpressionShape(*view), shape);
  ASC_DENSE_TEST_CHECK(
      context,
      asc::MayAlias(*view, asc::AliasToken::FromIdentity(values.data())));
  int unrelated = 0;
  ASC_DENSE_TEST_CHECK(
      context,
      !asc::MayAlias(*view, asc::AliasToken::FromIdentity(&unrelated)));
}

void CheckNestedAndScalarEvaluation(asc_dense_test::TestContext& context) {
  const std::array<asc::extent_t, 2> shape = {2, 3};
  std::array<double, 6> source_values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  std::array<double, 10> destination_storage{};

  const auto source = MakeLeftView(source_values.data(), shape);
  const std::array<asc::stride_t, 2> padded_strides = {1, 4};
  const auto destination_mapping = asc::DenseLayoutMapping<2>::Create(
      asc::LayoutStride{}, shape, padded_strides);
  const auto destination = asc::DenseView<double, 2>::Create(
      destination_storage.data(), *destination_mapping,
      asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(context, source.ok());
  ASC_DENSE_TEST_CHECK(context, destination_mapping.ok());
  ASC_DENSE_TEST_CHECK(context, destination.ok());

  auto add = asc::MakeAdd(*source, 2.0);
  ASC_DENSE_TEST_CHECK(context, add.ok());
  auto negate = asc::MakeNegate(std::move(*add));
  ASC_DENSE_TEST_CHECK(context, negate.ok());
  auto expression = asc::MakeMultiply(std::move(*negate), 0.5);
  ASC_DENSE_TEST_CHECK(context, expression.ok());

  asc::Status status = asc::Status::Ok();
  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationCountScope allocation_scope;
    status = asc::Evaluate(asc::ExecutionContext::Serial(), *destination,
                           *expression);
    allocations = allocation_scope.count();
  }
  ASC_DENSE_TEST_CHECK(context, status.ok());
  ASC_DENSE_TEST_EQ(context, allocations, 0U);
  for (asc::index_t column = 0; column < 3; ++column) {
    for (asc::index_t row = 0; row < 2; ++row) {
      const std::array<asc::index_t, 2> index = {row, column};
      const auto source_element = source->At(index);
      const auto destination_element = destination->At(index);
      ASC_DENSE_TEST_CHECK(context, source_element.ok());
      ASC_DENSE_TEST_CHECK(context, destination_element.ok());
      ASC_DENSE_TEST_EQ(context, **destination_element,
                        -0.5 * (**source_element + 2.0));
    }
  }

  const asc::ScalarExpression<double> scalar(7.25);
  const asc::Status scalar_status =
      asc::Evaluate(asc::ExecutionContext::Serial(), *destination, scalar);
  ASC_DENSE_TEST_CHECK(context, scalar_status.ok());
  for (asc::index_t column = 0; column < 3; ++column) {
    for (asc::index_t row = 0; row < 2; ++row) {
      const std::array<asc::index_t, 2> index = {row, column};
      const auto element = destination->At(index);
      ASC_DENSE_TEST_CHECK(context, element.ok());
      ASC_DENSE_TEST_EQ(context, **element, 7.25);
    }
  }

  double scalar_storage = 0.0;
  const std::array<asc::extent_t, 0> scalar_shape = {};
  const auto scalar_destination = MakeLeftView(&scalar_storage, scalar_shape);
  ASC_DENSE_TEST_CHECK(context, scalar_destination.ok());
  ASC_DENSE_TEST_CHECK(
      context,
      asc::Evaluate(asc::ExecutionContext::Serial(), *scalar_destination,
                    asc::ScalarExpression<double>(-3.0))
          .ok());
  ASC_DENSE_TEST_EQ(context, scalar_storage, -3.0);
}

void CheckExternalTraversal(asc_dense_test::TestContext& context) {
  external_expression::Matrix external;
  external.values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  std::array<int, 6> read_order{};
  std::size_t read_count = 0;
  external.read_order = &read_order;
  external.read_count = &read_count;

  const std::array<asc::extent_t, 2> shape = {2, 3};
  std::array<double, 6> destination_values{};
  const auto destination = MakeLeftView(destination_values.data(), shape);
  ASC_DENSE_TEST_CHECK(context, destination.ok());
  const asc::Status status =
      asc::Evaluate(asc::ExecutionContext::Serial(), *destination, external);
  ASC_DENSE_TEST_CHECK(context, status.ok());
  ASC_DENSE_TEST_EQ(context, read_count, 6U);
  ASC_DENSE_TEST_EQ(context, read_order,
                    (std::array<int, 6>{0, 10, 1, 11, 2, 12}));
  ASC_DENSE_TEST_EQ(context, destination_values, external.values);
}

void CheckTransactionsAndAliases(asc_dense_test::TestContext& context) {
  const std::array<asc::extent_t, 2> destination_shape = {2, 3};
  const std::array<asc::extent_t, 2> wrong_shape = {3, 2};
  std::array<double, 6> destination_values{};
  destination_values.fill(91.0);
  std::array<double, 6> source_values{};
  source_values.fill(4.0);
  const auto destination =
      MakeLeftView(destination_values.data(), destination_shape);
  const auto wrong_source = MakeLeftView(source_values.data(), wrong_shape);
  ASC_DENSE_TEST_CHECK(context, destination.ok());
  ASC_DENSE_TEST_CHECK(context, wrong_source.ok());
  const asc::Status wrong_shape_status = asc::Evaluate(
      asc::ExecutionContext::Serial(), *destination, *wrong_source);
  ASC_DENSE_TEST_CHECK(context, !wrong_shape_status.ok());
  ASC_DENSE_TEST_EQ(context, wrong_shape_status.code(), asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(
      context, destination_values,
      (std::array<double, 6>{91.0, 91.0, 91.0, 91.0, 91.0, 91.0}));

  const asc::Status self_status = asc::Evaluate(asc::ExecutionContext::Serial(),
                                                *destination, *destination);
  ASC_DENSE_TEST_CHECK(context, self_status.ok());

  auto in_place_expression = asc::MakeAdd(*destination, 1.0);
  ASC_DENSE_TEST_CHECK(context, in_place_expression.ok());
  const asc::Status nested_alias_status = asc::Evaluate(
      asc::ExecutionContext::Serial(), *destination, *in_place_expression);
  ASC_DENSE_TEST_CHECK(context, !nested_alias_status.ok());
  ASC_DENSE_TEST_EQ(context, nested_alias_status.code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(
      context, destination_values,
      (std::array<double, 6>{91.0, 91.0, 91.0, 91.0, 91.0, 91.0}));

  std::array<double, 4> overlap_values = {1.0, 2.0, 3.0, 4.0};
  const std::array<asc::extent_t, 1> parent_shape = {4};
  const auto parent = MakeLeftView(overlap_values.data(), parent_shape);
  ASC_DENSE_TEST_CHECK(context, parent.ok());
  const std::array<asc::index_t, 1> source_offset = {0};
  const std::array<asc::index_t, 1> destination_offset = {1};
  const std::array<asc::extent_t, 1> subshape = {3};
  const auto overlap_source = parent->Subview(source_offset, subshape);
  const auto overlap_destination =
      parent->Subview(destination_offset, subshape);
  ASC_DENSE_TEST_CHECK(context, overlap_source.ok());
  ASC_DENSE_TEST_CHECK(context, overlap_destination.ok());
  const asc::Status overlap_status = asc::Evaluate(
      asc::ExecutionContext::Serial(), *overlap_destination, *overlap_source);
  ASC_DENSE_TEST_CHECK(context, !overlap_status.ok());
  ASC_DENSE_TEST_EQ(context, overlap_status.code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(context, overlap_values,
                    (std::array<double, 4>{1.0, 2.0, 3.0, 4.0}));

  const auto device_source = asc::DenseView<const double, 2>::Create(
      source_values.data(), destination->mapping(), asc::MemorySpace::kDevice);
  ASC_DENSE_TEST_CHECK(context, device_source.ok());
  const asc::Status device_source_status = asc::Evaluate(
      asc::ExecutionContext::Serial(), *destination, *device_source);
  ASC_DENSE_TEST_CHECK(context, !device_source_status.ok());
  ASC_DENSE_TEST_EQ(context, device_source_status.code(),
                    asc::ErrorCode::kMemoryAccess);

  const auto device_destination = asc::DenseView<double, 2>::Create(
      destination_values.data(), destination->mapping(),
      asc::MemorySpace::kDevice);
  ASC_DENSE_TEST_CHECK(context, device_destination.ok());
  const asc::Status device_destination_status =
      asc::Evaluate(asc::ExecutionContext::Serial(), *device_destination,
                    asc::ScalarExpression<double>(1.0));
  ASC_DENSE_TEST_CHECK(context, !device_destination_status.ok());
  ASC_DENSE_TEST_EQ(context, device_destination_status.code(),
                    asc::ErrorCode::kMemoryAccess);
}

void CheckReductions(asc_dense_test::TestContext& context) {
  const std::array<asc::extent_t, 2> shape = {2, 3};
  const std::array<asc::stride_t, 2> padded_strides = {1, 4};
  const auto mapping = asc::DenseLayoutMapping<2>::Create(
      asc::LayoutStride{}, shape, padded_strides);
  std::array<double, 10> storage{};
  const auto view = asc::DenseView<double, 2>::Create(storage.data(), *mapping,
                                                      asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(context, mapping.ok());
  ASC_DENSE_TEST_CHECK(context, view.ok());
  const std::array<double, 6> logical_values = {3.0, -2.0, 7.0, 4.0, -5.0, 1.0};
  std::size_t logical = 0;
  for (asc::index_t column = 0; column < 3; ++column) {
    for (asc::index_t row = 0; row < 2; ++row) {
      const std::array<asc::index_t, 2> index = {row, column};
      const auto element = view->At(index);
      ASC_DENSE_TEST_CHECK(context, element.ok());
      **element = logical_values[logical++];
    }
  }

  std::size_t allocations = 0;
  asc::Result<double> sum = 0.0;
  {
    asc_dense_test::AllocationCountScope allocation_scope;
    sum = asc::ReduceSum(asc::ExecutionContext::Serial(), *view);
    allocations = allocation_scope.count();
  }
  const auto minimum = asc::ReduceMin(asc::ExecutionContext::Serial(), *view);
  const auto maximum = asc::ReduceMax(asc::ExecutionContext::Serial(), *view);
  ASC_DENSE_TEST_CHECK(context, sum.ok());
  ASC_DENSE_TEST_CHECK(context, minimum.ok());
  ASC_DENSE_TEST_CHECK(context, maximum.ok());
  ASC_DENSE_TEST_EQ(context, allocations, 0U);
  ASC_DENSE_TEST_EQ(context, *sum, 8.0);
  ASC_DENSE_TEST_EQ(context, *minimum, -5.0);
  ASC_DENSE_TEST_EQ(context, *maximum, 7.0);

  const std::array<asc::extent_t, 1> empty_shape = {0};
  const auto empty_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, empty_shape);
  const auto empty = asc::DenseView<double, 1>::Create(nullptr, *empty_mapping,
                                                       asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(context, empty_mapping.ok());
  ASC_DENSE_TEST_CHECK(context, empty.ok());
  const auto empty_sum =
      asc::ReduceSum(asc::ExecutionContext::Serial(), *empty);
  const auto empty_min =
      asc::ReduceMin(asc::ExecutionContext::Serial(), *empty);
  const auto empty_max =
      asc::ReduceMax(asc::ExecutionContext::Serial(), *empty);
  ASC_DENSE_TEST_CHECK(context, empty_sum.ok());
  ASC_DENSE_TEST_EQ(context, *empty_sum, 0.0);
  ASC_DENSE_TEST_CHECK(context, !empty_min.ok());
  ASC_DENSE_TEST_CHECK(context, !empty_max.ok());
  ASC_DENSE_TEST_EQ(context, empty_min.status().code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(context, empty_max.status().code(),
                    asc::ErrorCode::kInvalidArgument);

  const std::array<asc::extent_t, 1> integer_shape = {2};
  const auto integer_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, integer_shape);
  std::array<asc::index_t, 2> integer_values = {
      std::numeric_limits<asc::index_t>::max(), 1};
  const auto integer_view = asc::DenseView<asc::index_t, 1>::Create(
      integer_values.data(), *integer_mapping, asc::MemorySpace::kHost);
  ASC_DENSE_TEST_CHECK(context, integer_mapping.ok());
  ASC_DENSE_TEST_CHECK(context, integer_view.ok());
  const auto overflowing_sum =
      asc::ReduceSum(asc::ExecutionContext::Serial(), *integer_view);
  ASC_DENSE_TEST_CHECK(context, !overflowing_sum.ok());
  ASC_DENSE_TEST_EQ(context, overflowing_sum.status().code(),
                    asc::ErrorCode::kOverflow);
}

}  // namespace

int main() {
  asc_dense_test::TestContext context;
  CheckExpressionMetadata(context);
  CheckNestedAndScalarEvaluation(context);
  CheckExternalTraversal(context);
  CheckTransactionsAndAliases(context);
  CheckReductions(context);
  return context.Finish();
}
