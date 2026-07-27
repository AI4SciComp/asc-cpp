#include "asc/sparse/evaluate.h"

#include <array>
#include <cstddef>
#include <span>
#include <utility>

#include "allocation_counter.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "test_support.h"

namespace external_sparse_expression {

template <asc::SparsityEffect Effect>
struct Matrix {
  std::array<asc::extent_t, 2> shape = {2, 3};
  std::array<double, 6> values = {};
  const void* alias_identity = nullptr;
};

}  // namespace external_sparse_expression

namespace asc {

template <SparsityEffect Effect>
struct ExpressionAdapter<external_sparse_expression::Matrix<Effect>> {
  using value_type = double;
  static constexpr rank_t kRank = 2;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect = Effect;

  [[nodiscard]] static std::array<extent_t, 2> Shape(
      const external_sparse_expression::Matrix<Effect>& matrix) noexcept {
    return matrix.shape;
  }

  [[nodiscard]] static double Read(
      const external_sparse_expression::Matrix<Effect>& matrix,
      std::span<const index_t, 2> coordinate) noexcept {
    const auto row = static_cast<std::size_t>(coordinate[0]);
    const auto column = static_cast<std::size_t>(coordinate[1]);
    return matrix.values[row * 3 + column];
  }

  [[nodiscard]] static bool MayAlias(
      const external_sparse_expression::Matrix<Effect>& matrix,
      AliasToken alias) noexcept {
    return AliasToken::FromIdentity(matrix.alias_identity) == alias;
  }
};

}  // namespace asc

namespace {

constexpr std::array<asc::extent_t, 2> kShape = {2, 3};
constexpr std::array<asc::nnz_t, 3> kOffsets = {0, 2, 3};
constexpr std::array<asc::index_t, 3> kIndices = {0, 2, 1};
constexpr std::array<asc::index_t, 6> kCoordinates = {0, 0, 0, 2, 1, 1};

template <typename Element>
std::span<Element> CoordinateValues(asc::CoordinateView<Element, 2> view) {
  return std::span<Element>(view.value_data(),
                            static_cast<std::size_t>(view.nnz()));
}

template <typename Element>
std::span<Element> CompressedValues(asc::CsrView<Element> view) {
  return std::span<Element>(view.value_data(),
                            static_cast<std::size_t>(view.nnz()));
}

external_sparse_expression::Matrix<asc::SparsityEffect::kStructurePreserving>
MakePreservingSource() {
  external_sparse_expression::Matrix<asc::SparsityEffect::kStructurePreserving>
      source;
  source.values = {2.0, 50.0, -3.0, 60.0, 4.0, 70.0};
  return source;
}

void CheckCoordinateAndCompressedEvaluation(
    asc_sparse_test::TestContext& context) {
  auto source = MakePreservingSource();
  std::array<double, 3> coordinate_values = {90.0, 90.0, 90.0};
  auto coordinate = asc::CoordinateView<double, 2>::Create(
      kCoordinates.data(), coordinate_values.data(), kShape, 3,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, coordinate.ok());

  asc::Status coordinate_status = asc::Status::Ok();
  std::size_t coordinate_allocations = 1;
  {
    asc_sparse_test::AllocationCountScope allocation_scope;
    coordinate_status =
        asc::Evaluate(asc::ExecutionContext::Serial(), *coordinate, source);
    coordinate_allocations = allocation_scope.count();
  }
  ASC_SPARSE_TEST_CHECK(context, coordinate_status.ok());
  ASC_SPARSE_TEST_EQ(context, coordinate_allocations, 0U);
  ASC_SPARSE_TEST_RANGE_EQ(context, CoordinateValues(*coordinate),
                           (std::array<double, 3>{2.0, -3.0, 4.0}));
  ASC_SPARSE_TEST_RANGE_EQ(context, kCoordinates,
                           (std::array<asc::index_t, 6>{0, 0, 0, 2, 1, 1}));

  std::array<double, 3> compressed_values = {80.0, 80.0, 80.0};
  auto compressed = asc::CsrView<double>::Create(
      kShape, kOffsets, kIndices, compressed_values, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, compressed.ok());
  auto negate = asc::MakeNegate(source);
  ASC_SPARSE_TEST_CHECK(context, negate.ok());
  asc::Status compressed_status = asc::Status::Ok();
  std::size_t compressed_allocations = 1;
  {
    asc_sparse_test::AllocationCountScope allocation_scope;
    compressed_status =
        asc::Evaluate(asc::ExecutionContext::Serial(), *compressed, *negate);
    compressed_allocations = allocation_scope.count();
  }
  ASC_SPARSE_TEST_CHECK(context, compressed_status.ok());
  ASC_SPARSE_TEST_EQ(context, compressed_allocations, 0U);
  ASC_SPARSE_TEST_RANGE_EQ(context, CompressedValues(*compressed),
                           (std::array<double, 3>{-2.0, 3.0, -4.0}));
  ASC_SPARSE_TEST_RANGE_EQ(context, kOffsets,
                           (std::array<asc::nnz_t, 3>{0, 2, 3}));
  ASC_SPARSE_TEST_RANGE_EQ(context, kIndices,
                           (std::array<asc::index_t, 3>{0, 2, 1}));

  const auto self_before = compressed_values;
  const asc::Status self_status =
      asc::Evaluate(asc::ExecutionContext::Serial(), *compressed, *compressed);
  ASC_SPARSE_TEST_CHECK(context, self_status.ok());
  ASC_SPARSE_TEST_EQ(context, compressed_values, self_before);
}

template <asc::SparsityEffect Effect>
void CheckRejectedEffect(asc_sparse_test::TestContext& context,
                         asc::CsrView<double> destination) {
  external_sparse_expression::Matrix<Effect> source;
  source.values.fill(7.0);
  const std::array<double, 3> before = {destination.value_data()[0],
                                        destination.value_data()[1],
                                        destination.value_data()[2]};
  const asc::Status status =
      asc::Evaluate(asc::ExecutionContext::Serial(), destination, source);
  ASC_SPARSE_TEST_CHECK(context, !status.ok());
  ASC_SPARSE_TEST_EQ(context, status.code(), asc::ErrorCode::kUnsupported);
  ASC_SPARSE_TEST_RANGE_EQ(
      context,
      std::span<const double>(destination.value_data(),
                              static_cast<std::size_t>(destination.nnz())),
      before);
}

void CheckSparsityAndShapeTransactions(asc_sparse_test::TestContext& context) {
  std::array<double, 3> destination_values = {11.0, 12.0, 13.0};
  auto destination = asc::CsrView<double>::Create(
      kShape, kOffsets, kIndices, destination_values, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, destination.ok());
  CheckRejectedEffect<asc::SparsityEffect::kStructureFiltering>(context,
                                                                *destination);
  CheckRejectedEffect<asc::SparsityEffect::kStructureUnion>(context,
                                                            *destination);
  CheckRejectedEffect<asc::SparsityEffect::kStructureIntersection>(
      context, *destination);
  CheckRejectedEffect<asc::SparsityEffect::kValueDependent>(context,
                                                            *destination);
  CheckRejectedEffect<asc::SparsityEffect::kDensifying>(context, *destination);
  CheckRejectedEffect<asc::SparsityEffect::kDestinationRequired>(context,
                                                                 *destination);

  auto wrong_shape = MakePreservingSource();
  wrong_shape.shape = {3, 2};
  const auto before = destination_values;
  const asc::Status wrong_shape_status =
      asc::Evaluate(asc::ExecutionContext::Serial(), *destination, wrong_shape);
  ASC_SPARSE_TEST_CHECK(context, !wrong_shape_status.ok());
  ASC_SPARSE_TEST_EQ(context, wrong_shape_status.code(),
                     asc::ErrorCode::kShape);
  ASC_SPARSE_TEST_EQ(context, destination_values, before);

  const asc::ScalarExpression<double> scalar(3.0);
  const asc::Status scalar_status =
      asc::Evaluate(asc::ExecutionContext::Serial(), *destination, scalar);
  ASC_SPARSE_TEST_CHECK(context, !scalar_status.ok());
  ASC_SPARSE_TEST_EQ(context, scalar_status.code(),
                     asc::ErrorCode::kUnsupported);
  ASC_SPARSE_TEST_EQ(context, destination_values, before);
}

void CheckAliasesAndPlacement(asc_sparse_test::TestContext& context) {
  std::array<double, 4> overlapping_values = {1.0, 2.0, 3.0, 4.0};
  std::array<asc::index_t, 6> source_coordinates = kCoordinates;
  std::array<asc::index_t, 6> destination_coordinates = kCoordinates;
  auto source = asc::CoordinateView<const double, 2>::Create(
      source_coordinates.data(), overlapping_values.data(), kShape, 3,
      asc::MemorySpace::kHost);
  auto destination = asc::CoordinateView<double, 2>::Create(
      destination_coordinates.data(), overlapping_values.data() + 1, kShape, 3,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, source.ok());
  ASC_SPARSE_TEST_CHECK(context, destination.ok());
  const auto before = overlapping_values;
  const asc::Status overlap_status =
      asc::Evaluate(asc::ExecutionContext::Serial(), *destination, *source);
  ASC_SPARSE_TEST_CHECK(context, !overlap_status.ok());
  ASC_SPARSE_TEST_EQ(context, overlap_status.code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, overlapping_values, before);

  auto alias_source = MakePreservingSource();
  std::array<double, 3> alias_destination_values = {4.0, 5.0, 6.0};
  auto alias_destination = asc::CsrView<double>::Create(
      kShape, kOffsets, kIndices, alias_destination_values,
      asc::MemorySpace::kHost);
  alias_source.alias_identity = alias_destination_values.data();
  const auto alias_before = alias_destination_values;
  const asc::Status alias_status = asc::Evaluate(
      asc::ExecutionContext::Serial(), *alias_destination, alias_source);
  ASC_SPARSE_TEST_CHECK(context, !alias_status.ok());
  ASC_SPARSE_TEST_EQ(context, alias_status.code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, alias_destination_values, alias_before);

  auto device_destination = asc::CsrView<double>::Create(
      kShape, kOffsets, kIndices, alias_destination_values,
      asc::MemorySpace::kDevice);
  ASC_SPARSE_TEST_CHECK(context, device_destination.ok());
  const asc::Status device_destination_status =
      asc::Evaluate(asc::ExecutionContext::Serial(), *device_destination,
                    MakePreservingSource());
  ASC_SPARSE_TEST_CHECK(context, !device_destination_status.ok());
  ASC_SPARSE_TEST_EQ(context, device_destination_status.code(),
                     asc::ErrorCode::kMemoryAccess);

  std::array<double, 3> source_values = {1.0, 2.0, 3.0};
  auto device_source = asc::CsrView<const double>::Create(
      kShape, kOffsets, kIndices, source_values, asc::MemorySpace::kDevice);
  ASC_SPARSE_TEST_CHECK(context, device_source.ok());
  const asc::Status device_source_status = asc::Evaluate(
      asc::ExecutionContext::Serial(), *alias_destination, *device_source);
  ASC_SPARSE_TEST_CHECK(context, !device_source_status.ok());
  ASC_SPARSE_TEST_EQ(context, device_source_status.code(),
                     asc::ErrorCode::kMemoryAccess);
}

void CheckRankZeroPreserving(asc_sparse_test::TestContext& context) {
  std::array<double, 1> source_values = {7.0};
  std::array<double, 1> destination_values = {-1.0};
  const std::array<asc::index_t, 0> coordinate{};
  const std::array<asc::extent_t, 0> shape{};
  auto source = asc::CoordinateView<const double, 0>::Create(
      nullptr, source_values.data(), shape, 1, asc::MemorySpace::kHost);
  auto destination = asc::CoordinateView<double, 0>::Create(
      nullptr, destination_values.data(), shape, 1, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, source.ok());
  ASC_SPARSE_TEST_CHECK(context, destination.ok());
  auto negate = asc::MakeNegate(*source);
  ASC_SPARSE_TEST_CHECK(context, negate.ok());
  const asc::Status status =
      asc::Evaluate(asc::ExecutionContext::Serial(), *destination, *negate);
  ASC_SPARSE_TEST_CHECK(context, status.ok());
  ASC_SPARSE_TEST_EQ(context, destination_values[0], -7.0);
  ASC_SPARSE_TEST_EQ(
      context,
      asc::ReadExpression(*destination,
                          std::span<const asc::index_t, 0>(coordinate)),
      -7.0);
}

}  // namespace

int main() {
  asc_sparse_test::TestContext context;
  CheckCoordinateAndCompressedEvaluation(context);
  CheckSparsityAndShapeTransactions(context);
  CheckAliasesAndPlacement(context);
  CheckRankZeroPreserving(context);
  return context.Finish();
}
