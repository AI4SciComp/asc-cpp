#include "asc/dense/linalg.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "test_support.h"

namespace {

template <typename Element, std::size_t Rank>
auto MakeStridedView(Element* data,
                     const std::array<asc::extent_t, Rank>& shape,
                     const std::array<asc::stride_t, Rank>& strides,
                     asc::MemorySpace space = asc::MemorySpace::kHost) {
  auto mapping = asc::DenseLayoutMapping<Rank>::Create(asc::LayoutStride{},
                                                       shape, strides);
  if (!mapping.ok()) {
    return asc::Result<asc::DenseView<Element, Rank>>(mapping.status());
  }
  return asc::DenseView<Element, Rank>::Create(data, *mapping, space);
}

template <typename Element, std::size_t Rank>
auto MakeLeftView(Element* data, const std::array<asc::extent_t, Rank>& shape) {
  auto mapping =
      asc::DenseLayoutMapping<Rank>::Create(asc::LayoutLeft{}, shape);
  if (!mapping.ok()) {
    return asc::Result<asc::DenseView<Element, Rank>>(mapping.status());
  }
  return asc::DenseView<Element, Rank>::Create(data, *mapping,
                                               asc::MemorySpace::kHost);
}

template <typename Scalar>
void SetVector(asc::DenseView<Scalar, 1> view, std::span<const Scalar> values,
               asc_dense_test::TestContext& context) {
  ASC_DENSE_TEST_EQ(context, view.shape()[0],
                    static_cast<asc::extent_t>(values.size()));
  for (std::size_t index = 0; index < values.size(); ++index) {
    const std::array<asc::index_t, 1> coordinate = {
        static_cast<asc::index_t>(index)};
    const auto element = view.At(coordinate);
    ASC_DENSE_TEST_CHECK(context, element.ok());
    **element = values[index];
  }
}

template <typename Scalar>
void CheckVector(asc::DenseView<const Scalar, 1> view,
                 std::span<const Scalar> expected, double absolute_tolerance,
                 double relative_tolerance,
                 asc_dense_test::TestContext& context) {
  ASC_DENSE_TEST_EQ(context, view.shape()[0],
                    static_cast<asc::extent_t>(expected.size()));
  for (std::size_t index = 0; index < expected.size(); ++index) {
    const std::array<asc::index_t, 1> coordinate = {
        static_cast<asc::index_t>(index)};
    const auto element = view.At(coordinate);
    ASC_DENSE_TEST_CHECK(context, element.ok());
    ASC_DENSE_TEST_NEAR(context, **element, expected[index], absolute_tolerance,
                        relative_tolerance);
  }
}

template <typename Scalar>
void SetMatrix(asc::DenseView<Scalar, 2> view,
               std::span<const Scalar> row_major_values,
               asc_dense_test::TestContext& context) {
  const std::size_t rows = static_cast<std::size_t>(view.shape()[0]);
  const std::size_t columns = static_cast<std::size_t>(view.shape()[1]);
  ASC_DENSE_TEST_EQ(context, row_major_values.size(), rows * columns);
  for (std::size_t row = 0; row < rows; ++row) {
    for (std::size_t column = 0; column < columns; ++column) {
      const std::array<asc::index_t, 2> coordinate = {
          static_cast<asc::index_t>(row), static_cast<asc::index_t>(column)};
      const auto element = view.At(coordinate);
      ASC_DENSE_TEST_CHECK(context, element.ok());
      **element = row_major_values[row * columns + column];
    }
  }
}

template <typename Scalar>
void CheckMatrix(asc::DenseView<const Scalar, 2> view,
                 std::span<const Scalar> expected, double absolute_tolerance,
                 double relative_tolerance,
                 asc_dense_test::TestContext& context) {
  const std::size_t rows = static_cast<std::size_t>(view.shape()[0]);
  const std::size_t columns = static_cast<std::size_t>(view.shape()[1]);
  ASC_DENSE_TEST_EQ(context, expected.size(), rows * columns);
  for (std::size_t row = 0; row < rows; ++row) {
    for (std::size_t column = 0; column < columns; ++column) {
      const std::array<asc::index_t, 2> coordinate = {
          static_cast<asc::index_t>(row), static_cast<asc::index_t>(column)};
      const auto element = view.At(coordinate);
      ASC_DENSE_TEST_CHECK(context, element.ok());
      ASC_DENSE_TEST_NEAR(context, **element, expected[row * columns + column],
                          absolute_tolerance, relative_tolerance);
    }
  }
}

void CheckCopyScalAxpy(asc_dense_test::TestContext& context) {
  constexpr std::array<asc::extent_t, 1> kShape = {3};
  constexpr std::array<asc::stride_t, 1> kSourceStride = {2};
  constexpr std::array<asc::stride_t, 1> kDestinationStride = {3};
  std::array<double, 5> source_storage = {1.0, -99.0, 2.0, -99.0, 4.0};
  std::array<double, 7> destination_storage{};
  const auto source = MakeStridedView<const double>(source_storage.data(),
                                                    kShape, kSourceStride);
  const auto destination =
      MakeStridedView(destination_storage.data(), kShape, kDestinationStride);
  ASC_DENSE_TEST_CHECK(context, source.ok());
  ASC_DENSE_TEST_CHECK(context, destination.ok());

  ASC_DENSE_TEST_CHECK(
      context,
      asc::Copy(asc::ExecutionContext::Serial(), *source, *destination).ok());
  CheckVector<double>(*destination, std::array<double, 3>{1.0, 2.0, 4.0}, 0.0,
                      0.0, context);

  ASC_DENSE_TEST_CHECK(
      context,
      asc::Scal(asc::ExecutionContext::Serial(), -2.0, *destination).ok());
  CheckVector<double>(*destination, std::array<double, 3>{-2.0, -4.0, -8.0},
                      0.0, 0.0, context);

  ASC_DENSE_TEST_CHECK(context, asc::Axpy(asc::ExecutionContext::Serial(), 0.5,
                                          *source, *destination)
                                    .ok());
  CheckVector<double>(*destination, std::array<double, 3>{-1.5, -3.0, -6.0},
                      0.0, 0.0, context);

  const asc::DenseView<const double, 1> const_destination(*destination);
  ASC_DENSE_TEST_CHECK(context, asc::Copy(asc::ExecutionContext::Serial(),
                                          const_destination, *destination)
                                    .ok());
  ASC_DENSE_TEST_CHECK(context, asc::Axpy(asc::ExecutionContext::Serial(), 2.0,
                                          const_destination, *destination)
                                    .ok());
  CheckVector<double>(*destination, std::array<double, 3>{-4.5, -9.0, -18.0},
                      0.0, 0.0, context);

  std::array<double, 4> overlap_storage = {1.0, 2.0, 3.0, 4.0};
  const std::array<asc::extent_t, 1> parent_shape = {4};
  const auto parent = MakeLeftView(overlap_storage.data(), parent_shape);
  const std::array<asc::index_t, 1> source_offset = {0};
  const std::array<asc::index_t, 1> destination_offset = {1};
  const auto overlap_source =
      parent->Subview(source_offset, std::array<asc::extent_t, 1>{3});
  const auto overlap_destination =
      parent->Subview(destination_offset, std::array<asc::extent_t, 1>{3});
  ASC_DENSE_TEST_CHECK(context, parent.ok());
  ASC_DENSE_TEST_CHECK(context, overlap_source.ok());
  ASC_DENSE_TEST_CHECK(context, overlap_destination.ok());
  const asc::DenseView<const double, 1> const_overlap_source(*overlap_source);
  const asc::Status overlap_copy =
      asc::Copy(asc::ExecutionContext::Serial(), const_overlap_source,
                *overlap_destination);
  const asc::Status overlap_axpy =
      asc::Axpy(asc::ExecutionContext::Serial(), 1.0, const_overlap_source,
                *overlap_destination);
  ASC_DENSE_TEST_CHECK(context, !overlap_copy.ok());
  ASC_DENSE_TEST_CHECK(context, !overlap_axpy.ok());
  ASC_DENSE_TEST_EQ(context, overlap_storage,
                    (std::array<double, 4>{1.0, 2.0, 3.0, 4.0}));

  constexpr std::array<asc::extent_t, 2> kMatrixShape = {2, 2};
  constexpr std::array<asc::stride_t, 2> kPadded = {1, 3};
  std::array<float, 5> float_source_storage{};
  std::array<float, 5> float_destination_storage{};
  const auto float_source_mutable =
      MakeStridedView(float_source_storage.data(), kMatrixShape, kPadded);
  const auto float_destination =
      MakeStridedView(float_destination_storage.data(), kMatrixShape, kPadded);
  SetMatrix<float>(*float_source_mutable,
                   std::array<float, 4>{1.0F, 2.0F, 3.0F, 4.0F}, context);
  ASC_DENSE_TEST_CHECK(
      context, asc::Copy(asc::ExecutionContext::Serial(), *float_source_mutable,
                         *float_destination)
                   .ok());
  ASC_DENSE_TEST_CHECK(context, asc::Scal(asc::ExecutionContext::Serial(), 2.0F,
                                          *float_destination)
                                    .ok());
  ASC_DENSE_TEST_CHECK(context,
                       asc::Axpy(asc::ExecutionContext::Serial(), -1.0F,
                                 *float_source_mutable, *float_destination)
                           .ok());
  CheckMatrix<float>(*float_destination,
                     std::array<float, 4>{1.0F, 2.0F, 3.0F, 4.0F}, 0.0, 0.0,
                     context);

  const std::array<asc::extent_t, 1> empty_shape = {0};
  const std::array<asc::stride_t, 1> unit_stride = {1};
  const auto empty_const =
      MakeStridedView<const double>(nullptr, empty_shape, unit_stride);
  const auto empty_mutable =
      MakeStridedView<double>(nullptr, empty_shape, unit_stride);
  ASC_DENSE_TEST_CHECK(context, empty_const.ok());
  ASC_DENSE_TEST_CHECK(context, empty_mutable.ok());
  ASC_DENSE_TEST_CHECK(context, asc::Copy(asc::ExecutionContext::Serial(),
                                          *empty_const, *empty_mutable)
                                    .ok());
  ASC_DENSE_TEST_CHECK(
      context,
      asc::Scal(asc::ExecutionContext::Serial(), 3.0, *empty_mutable).ok());
  ASC_DENSE_TEST_CHECK(context, asc::Axpy(asc::ExecutionContext::Serial(), 3.0,
                                          *empty_const, *empty_mutable)
                                    .ok());
}

void CheckDotAndNrm2(asc_dense_test::TestContext& context) {
  const std::array<asc::extent_t, 1> shape = {3};
  const std::array<asc::stride_t, 1> left_stride = {2};
  const std::array<asc::stride_t, 1> right_stride = {3};
  std::array<double, 5> left_storage = {2.0, 0.0, -1.0, 0.0, 4.0};
  std::array<double, 7> right_storage = {3.0, 0.0, 0.0, 5.0, 0.0, 0.0, -2.0};
  const auto left =
      MakeStridedView<const double>(left_storage.data(), shape, left_stride);
  const auto right =
      MakeStridedView<const double>(right_storage.data(), shape, right_stride);
  ASC_DENSE_TEST_CHECK(context, left.ok());
  ASC_DENSE_TEST_CHECK(context, right.ok());
  const auto dot = asc::Dot(asc::ExecutionContext::Serial(), *left, *right);
  ASC_DENSE_TEST_CHECK(context, dot.ok());
  ASC_DENSE_TEST_EQ(context, *dot, -7.0);

  std::array<double, 2> short_values = {1.0, 2.0};
  const auto short_view = MakeLeftView<const double>(
      short_values.data(), std::array<asc::extent_t, 1>{2});
  const auto wrong_shape_dot =
      asc::Dot(asc::ExecutionContext::Serial(), *left, *short_view);
  ASC_DENSE_TEST_CHECK(context, !wrong_shape_dot.ok());
  ASC_DENSE_TEST_EQ(context, wrong_shape_dot.status().code(),
                    asc::ErrorCode::kShape);

  std::array<double, 2> norm_values = {3.0, 4.0};
  const auto norm_view =
      MakeLeftView(norm_values.data(), std::array<asc::extent_t, 1>{2});
  const auto norm = asc::Nrm2(asc::ExecutionContext::Serial(), *norm_view);
  ASC_DENSE_TEST_CHECK(context, norm.ok());
  ASC_DENSE_TEST_EQ(context, *norm, 5.0);

  const double quarter_max = std::numeric_limits<double>::max() / 4.0;
  std::array<double, 2> extreme_values = {quarter_max, quarter_max};
  const auto extreme = MakeLeftView<const double>(
      extreme_values.data(), std::array<asc::extent_t, 1>{2});
  const auto extreme_norm =
      asc::Nrm2(asc::ExecutionContext::Serial(), *extreme);
  const double expected_extreme = quarter_max * std::sqrt(2.0);
  ASC_DENSE_TEST_CHECK(context, extreme_norm.ok());
  ASC_DENSE_TEST_CHECK(context, std::isfinite(*extreme_norm));
  ASC_DENSE_TEST_NEAR(context, *extreme_norm, expected_extreme, 0.0, 4.0e-15);

  std::array<double, 2> infinity_values = {
      std::numeric_limits<double>::infinity(), 1.0};
  const auto infinity = MakeLeftView<const double>(
      infinity_values.data(), std::array<asc::extent_t, 1>{2});
  const auto infinity_norm =
      asc::Nrm2(asc::ExecutionContext::Serial(), *infinity);
  ASC_DENSE_TEST_CHECK(context, infinity_norm.ok());
  ASC_DENSE_TEST_CHECK(context, std::isinf(*infinity_norm));

  std::array<double, 2> nan_values = {std::numeric_limits<double>::quiet_NaN(),
                                      1.0};
  const auto nan = MakeLeftView<const double>(nan_values.data(),
                                              std::array<asc::extent_t, 1>{2});
  const auto nan_norm = asc::Nrm2(asc::ExecutionContext::Serial(), *nan);
  ASC_DENSE_TEST_CHECK(context, nan_norm.ok());
  ASC_DENSE_TEST_CHECK(context, std::isnan(*nan_norm));

  const std::array<asc::extent_t, 1> empty_shape = {0};
  const auto empty = MakeLeftView<const double>(nullptr, empty_shape);
  const auto empty_dot =
      asc::Dot(asc::ExecutionContext::Serial(), *empty, *empty);
  const auto empty_norm = asc::Nrm2(asc::ExecutionContext::Serial(), *empty);
  ASC_DENSE_TEST_CHECK(context, empty_dot.ok());
  ASC_DENSE_TEST_CHECK(context, empty_norm.ok());
  ASC_DENSE_TEST_EQ(context, *empty_dot, 0.0);
  ASC_DENSE_TEST_EQ(context, *empty_norm, 0.0);

  std::array<float, 3> float_left_values = {1.0F, 2.0F, 3.0F};
  std::array<float, 3> float_right_values = {4.0F, 5.0F, 6.0F};
  const auto float_left =
      MakeLeftView(float_left_values.data(), std::array<asc::extent_t, 1>{3});
  const auto float_right =
      MakeLeftView(float_right_values.data(), std::array<asc::extent_t, 1>{3});
  const auto float_dot =
      asc::Dot(asc::ExecutionContext::Serial(), *float_left, *float_right);
  ASC_DENSE_TEST_CHECK(context, float_dot.ok());
  ASC_DENSE_TEST_EQ(context, *float_dot, 32.0F);
}

void CheckGemv(asc_dense_test::TestContext& context) {
  std::array<double, 8> matrix_storage{};
  const std::array<asc::extent_t, 2> matrix_shape = {2, 3};
  const std::array<asc::stride_t, 2> matrix_strides = {1, 3};
  const auto matrix_mutable =
      MakeStridedView(matrix_storage.data(), matrix_shape, matrix_strides);
  SetMatrix<double>(*matrix_mutable,
                    std::array<double, 6>{1.0, 2.0, 3.0, 4.0, 5.0, 6.0},
                    context);
  const asc::DenseView<const double, 2> matrix(*matrix_mutable);

  std::array<double, 3> input_values = {2.0, -1.0, 3.0};
  std::array<double, 2> output_values = {5.0, 7.0};
  const auto input =
      MakeLeftView(input_values.data(), std::array<asc::extent_t, 1>{3});
  const auto output =
      MakeLeftView(output_values.data(), std::array<asc::extent_t, 1>{2});
  const asc::Status ordinary =
      asc::Gemv(asc::ExecutionContext::Serial(), asc::MatrixOperation::kNone,
                2.0, *matrix_mutable, *input, -1.0, *output);
  ASC_DENSE_TEST_CHECK(context, ordinary.ok());
  CheckVector<double>(*output, std::array<double, 2>{13.0, 35.0}, 0.0, 0.0,
                      context);

  output_values.fill(std::numeric_limits<double>::quiet_NaN());
  const asc::Status beta_zero =
      asc::Gemv(asc::ExecutionContext::Serial(), asc::MatrixOperation::kNone,
                1.0, matrix, *input, 0.0, *output);
  ASC_DENSE_TEST_CHECK(context, beta_zero.ok());
  CheckVector<double>(*output, std::array<double, 2>{9.0, 21.0}, 0.0, 0.0,
                      context);

  std::array<double, 2> transpose_input_values = {10.0, -2.0};
  std::array<double, 3> transpose_output_values{};
  const auto transpose_input = MakeLeftView<const double>(
      transpose_input_values.data(), std::array<asc::extent_t, 1>{2});
  const auto transpose_output = MakeLeftView(transpose_output_values.data(),
                                             std::array<asc::extent_t, 1>{3});
  const asc::Status transpose = asc::Gemv(
      asc::ExecutionContext::Serial(), asc::MatrixOperation::kTranspose, 1.0,
      matrix, *transpose_input, 0.0, *transpose_output);
  ASC_DENSE_TEST_CHECK(context, transpose.ok());
  CheckVector<double>(*transpose_output, std::array<double, 3>{2.0, 10.0, 18.0},
                      0.0, 0.0, context);

  std::array<double, 1> wrong_output_values = {77.0};
  const auto wrong_output =
      MakeLeftView(wrong_output_values.data(), std::array<asc::extent_t, 1>{1});
  const asc::Status wrong_shape_status =
      asc::Gemv(asc::ExecutionContext::Serial(), asc::MatrixOperation::kNone,
                1.0, matrix, *input, 1.0, *wrong_output);
  ASC_DENSE_TEST_CHECK(context, !wrong_shape_status.ok());
  ASC_DENSE_TEST_EQ(context, wrong_output_values[0], 77.0);

  const std::array<double, 2> before_invalid_operation = output_values;
  const asc::Status invalid_operation = asc::Gemv(
      asc::ExecutionContext::Serial(), static_cast<asc::MatrixOperation>(255),
      1.0, matrix, *input, 1.0, *output);
  ASC_DENSE_TEST_CHECK(context, !invalid_operation.ok());
  ASC_DENSE_TEST_EQ(context, invalid_operation.code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(context, output_values, before_invalid_operation);

  const auto overlapping_output =
      MakeLeftView(matrix_storage.data(), std::array<asc::extent_t, 1>{2});
  const asc::Status overlap_status =
      asc::Gemv(asc::ExecutionContext::Serial(), asc::MatrixOperation::kNone,
                1.0, matrix, *input, 0.0, *overlapping_output);
  ASC_DENSE_TEST_CHECK(context, !overlap_status.ok());
  ASC_DENSE_TEST_EQ(context, overlap_status.code(),
                    asc::ErrorCode::kInvalidArgument);

  std::array<double, 2> empty_matrix_storage{};
  const auto empty_matrix_mutable = MakeLeftView(
      empty_matrix_storage.data(), std::array<asc::extent_t, 2>{2, 0});
  const asc::DenseView<const double, 2> empty_matrix(*empty_matrix_mutable);
  const auto empty_input =
      MakeLeftView<const double>(nullptr, std::array<asc::extent_t, 1>{0});
  std::array<double, 2> empty_output_values = {
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::quiet_NaN()};
  const auto empty_output =
      MakeLeftView(empty_output_values.data(), std::array<asc::extent_t, 1>{2});
  const asc::Status degenerate =
      asc::Gemv(asc::ExecutionContext::Serial(), asc::MatrixOperation::kNone,
                1.0, empty_matrix, *empty_input, 0.0, *empty_output);
  ASC_DENSE_TEST_CHECK(context, degenerate.ok());
  CheckVector<double>(*empty_output, std::array<double, 2>{0.0, 0.0}, 0.0, 0.0,
                      context);
}

void RunGemmTransposeCase(asc::MatrixOperation left_operation,
                          asc::MatrixOperation right_operation,
                          asc_dense_test::TestContext& context) {
  constexpr std::array<double, 6> kLeftLogical = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  constexpr std::array<double, 6> kRightLogical = {7.0,  8.0,  9.0,
                                                   10.0, 11.0, 12.0};
  std::array<double, 16> left_storage{};
  std::array<double, 16> right_storage{};
  const std::array<asc::extent_t, 2> left_shape =
      left_operation == asc::MatrixOperation::kNone
          ? std::array<asc::extent_t, 2>{2, 3}
          : std::array<asc::extent_t, 2>{3, 2};
  const std::array<asc::extent_t, 2> right_shape =
      right_operation == asc::MatrixOperation::kNone
          ? std::array<asc::extent_t, 2>{3, 2}
          : std::array<asc::extent_t, 2>{2, 3};
  const auto left_mutable =
      MakeStridedView(left_storage.data(), left_shape,
                      std::array<asc::stride_t, 2>{1, left_shape[0] + 1});
  const auto right_mutable =
      MakeStridedView(right_storage.data(), right_shape,
                      std::array<asc::stride_t, 2>{right_shape[1] + 1, 1});

  std::array<double, 6> stored_left{};
  std::array<double, 6> stored_right{};
  if (left_operation == asc::MatrixOperation::kNone) {
    stored_left = kLeftLogical;
  } else {
    stored_left = {1.0, 4.0, 2.0, 5.0, 3.0, 6.0};
  }
  if (right_operation == asc::MatrixOperation::kNone) {
    stored_right = kRightLogical;
  } else {
    stored_right = {7.0, 9.0, 11.0, 8.0, 10.0, 12.0};
  }
  SetMatrix<double>(*left_mutable, stored_left, context);
  SetMatrix<double>(*right_mutable, stored_right, context);
  const asc::DenseView<const double, 2> left(*left_mutable);
  const asc::DenseView<const double, 2> right(*right_mutable);

  std::array<double, 8> output_storage{};
  const std::array<asc::extent_t, 2> output_shape = {2, 2};
  const auto output = MakeStridedView(output_storage.data(), output_shape,
                                      std::array<asc::stride_t, 2>{1, 4});
  SetMatrix<double>(*output, std::array<double, 4>{1.0, 2.0, 3.0, 4.0},
                    context);
  const asc::Status status =
      asc::Gemm(asc::ExecutionContext::Serial(), left_operation,
                right_operation, 0.5, left, right, 2.0, *output);
  ASC_DENSE_TEST_CHECK(context, status.ok());
  CheckMatrix<double>(*output, std::array<double, 4>{31.0, 36.0, 75.5, 85.0},
                      1.0e-14, 1.0e-14, context);
}

void CheckGemm(asc_dense_test::TestContext& context) {
  RunGemmTransposeCase(asc::MatrixOperation::kNone, asc::MatrixOperation::kNone,
                       context);
  RunGemmTransposeCase(asc::MatrixOperation::kNone,
                       asc::MatrixOperation::kTranspose, context);
  RunGemmTransposeCase(asc::MatrixOperation::kTranspose,
                       asc::MatrixOperation::kNone, context);
  RunGemmTransposeCase(asc::MatrixOperation::kTranspose,
                       asc::MatrixOperation::kTranspose, context);

  std::array<double, 6> left_values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  std::array<double, 6> right_values = {7.0, 8.0, 9.0, 10.0, 11.0, 12.0};
  const auto left_mutable =
      MakeLeftView(left_values.data(), std::array<asc::extent_t, 2>{2, 3});
  const auto right_mutable =
      MakeLeftView(right_values.data(), std::array<asc::extent_t, 2>{3, 2});
  SetMatrix<double>(*left_mutable,
                    std::array<double, 6>{1.0, 2.0, 3.0, 4.0, 5.0, 6.0},
                    context);
  SetMatrix<double>(*right_mutable,
                    std::array<double, 6>{7.0, 8.0, 9.0, 10.0, 11.0, 12.0},
                    context);
  const asc::DenseView<const double, 2> left(*left_mutable);
  const asc::DenseView<const double, 2> right(*right_mutable);
  std::array<double, 4> output_values;
  output_values.fill(std::numeric_limits<double>::quiet_NaN());
  const auto output =
      MakeLeftView(output_values.data(), std::array<asc::extent_t, 2>{2, 2});
  const asc::Status beta_zero =
      asc::Gemm(asc::ExecutionContext::Serial(), asc::MatrixOperation::kNone,
                asc::MatrixOperation::kNone, 1.0, *left_mutable, *right_mutable,
                0.0, *output);
  ASC_DENSE_TEST_CHECK(context, beta_zero.ok());
  CheckMatrix<double>(*output, std::array<double, 4>{58.0, 64.0, 139.0, 154.0},
                      0.0, 0.0, context);

  const auto overlapping_output =
      MakeLeftView(left_values.data(), std::array<asc::extent_t, 2>{2, 2});
  const std::array<double, 6> before_overlap = left_values;
  const asc::Status overlap = asc::Gemm(
      asc::ExecutionContext::Serial(), asc::MatrixOperation::kNone,
      asc::MatrixOperation::kNone, 1.0, left, right, 0.0, *overlapping_output);
  ASC_DENSE_TEST_CHECK(context, !overlap.ok());
  ASC_DENSE_TEST_EQ(context, left_values, before_overlap);

  std::array<double, 2> wrong_output_values = {81.0, 82.0};
  const auto wrong_output = MakeLeftView(wrong_output_values.data(),
                                         std::array<asc::extent_t, 2>{1, 2});
  const asc::Status wrong_shape = asc::Gemm(
      asc::ExecutionContext::Serial(), asc::MatrixOperation::kNone,
      asc::MatrixOperation::kNone, 1.0, left, right, 1.0, *wrong_output);
  ASC_DENSE_TEST_CHECK(context, !wrong_shape.ok());
  ASC_DENSE_TEST_EQ(context, wrong_shape.code(), asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(context, wrong_output_values,
                    (std::array<double, 2>{81.0, 82.0}));

  const auto empty_left_mutable = MakeLeftView(
      static_cast<double*>(nullptr), std::array<asc::extent_t, 2>{2, 0});
  const auto empty_right_mutable = MakeLeftView(
      static_cast<double*>(nullptr), std::array<asc::extent_t, 2>{0, 2});
  const asc::DenseView<const double, 2> empty_left(*empty_left_mutable);
  const asc::DenseView<const double, 2> empty_right(*empty_right_mutable);
  output_values.fill(std::numeric_limits<double>::quiet_NaN());
  const asc::Status degenerate = asc::Gemm(
      asc::ExecutionContext::Serial(), asc::MatrixOperation::kNone,
      asc::MatrixOperation::kNone, 1.0, empty_left, empty_right, 0.0, *output);
  ASC_DENSE_TEST_CHECK(context, degenerate.ok());
  CheckMatrix<double>(*output, std::array<double, 4>{0.0, 0.0, 0.0, 0.0}, 0.0,
                      0.0, context);

  std::array<float, 1> float_left_value = {2.0F};
  std::array<float, 1> float_right_value = {3.0F};
  std::array<float, 1> float_output_value = {5.0F};
  const auto float_left_mutable =
      MakeLeftView(float_left_value.data(), std::array<asc::extent_t, 2>{1, 1});
  const auto float_right_mutable = MakeLeftView(
      float_right_value.data(), std::array<asc::extent_t, 2>{1, 1});
  const auto float_output = MakeLeftView(float_output_value.data(),
                                         std::array<asc::extent_t, 2>{1, 1});
  const asc::DenseView<const float, 2> float_left(*float_left_mutable);
  const asc::DenseView<const float, 2> float_right(*float_right_mutable);
  const asc::Status float_status =
      asc::Gemm(asc::ExecutionContext::Serial(), asc::MatrixOperation::kNone,
                asc::MatrixOperation::kNone, 2.0F, float_left, float_right,
                -1.0F, *float_output);
  ASC_DENSE_TEST_CHECK(context, float_status.ok());
  ASC_DENSE_TEST_EQ(context, float_output_value[0], 7.0F);
}

}  // namespace

int main() {
  asc_dense_test::TestContext context;
  CheckCopyScalAxpy(context);
  CheckDotAndNrm2(context);
  CheckGemv(context);
  CheckGemm(context);
  return context.Finish();
}
