#include "asc/dense/blas.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <span>
#include <type_traits>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;

void CheckStatusError(TestContext& test, const asc::Status& status,
                      asc::ErrorCode expected) {
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  if (!status.ok()) {
    ASC_DENSE_TEST_EQ(test, status.code(), expected);
  }
}

template <typename Result>
void CheckError(TestContext& test, const Result& result,
                asc::ErrorCode expected) {
  ASC_DENSE_TEST_CHECK(test, !result.ok());
  if (!result.ok()) {
    ASC_DENSE_TEST_EQ(test, result.status().code(), expected);
  }
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

template <typename Element, std::size_t Rank>
Element& ElementAt(asc::DenseView<Element, Rank> view,
                   const std::array<asc::index_t, Rank>& coordinates) {
  auto element = view.At(std::span<const asc::index_t, Rank>(coordinates));
  if (!element.ok()) {
    std::abort();
  }
  return **element;
}

template <typename Element>
void SetVector(asc::DenseView<Element, 1> view,
               std::span<const std::remove_const_t<Element>> values) {
  for (asc::index_t index = 0; index < view.extents()[0]; ++index) {
    ElementAt(view, std::array<asc::index_t, 1>{index}) =
        values[static_cast<std::size_t>(index)];
  }
}

template <typename Element>
void CheckVector(TestContext& test, asc::DenseView<Element, 1> view,
                 std::span<const std::remove_const_t<Element>> expected) {
  ASC_DENSE_TEST_EQ(test, view.extents()[0],
                    static_cast<asc::extent_t>(expected.size()));
  for (asc::index_t index = 0; index < view.extents()[0]; ++index) {
    ASC_DENSE_TEST_EQ(test, ElementAt(view, std::array<asc::index_t, 1>{index}),
                      expected[static_cast<std::size_t>(index)]);
  }
}

template <typename Element>
void SetMatrix(asc::DenseView<Element, 2> view,
               std::span<const std::remove_const_t<Element>> row_major) {
  const asc::extent_t columns = view.extents()[1];
  for (asc::index_t row = 0; row < view.extents()[0]; ++row) {
    for (asc::index_t column = 0; column < columns; ++column) {
      ElementAt(view, std::array<asc::index_t, 2>{row, column}) =
          row_major[static_cast<std::size_t>(row * columns + column)];
    }
  }
}

template <typename Element>
void CheckMatrix(TestContext& test, asc::DenseView<Element, 2> view,
                 std::span<const std::remove_const_t<Element>> expected) {
  const asc::extent_t columns = view.extents()[1];
  ASC_DENSE_TEST_EQ(test, view.extents()[0] * columns,
                    static_cast<asc::extent_t>(expected.size()));
  for (asc::index_t row = 0; row < view.extents()[0]; ++row) {
    for (asc::index_t column = 0; column < columns; ++column) {
      ASC_DENSE_TEST_EQ(
          test, ElementAt(view, std::array<asc::index_t, 2>{row, column}),
          expected[static_cast<std::size_t>(row * columns + column)]);
    }
  }
}

template <typename Real>
void TestVectorOperations(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  constexpr std::array<asc::extent_t, 1> kShape{3};
  constexpr std::array<Real, 3> kX{Real{1}, Real{-2}, Real{3}};
  constexpr std::array<Real, 3> kY{Real{4}, Real{5}, Real{6}};
  std::array<Real, 5> x_storage{};
  std::array<Real, 3> y_storage{};
  std::array<Real, 7> destination_storage{};
  auto x_mutable = MakeView<Real, 1>(x_storage.data(), kShape, {2});
  auto y_mutable = MakeView<Real, 1>(y_storage.data(), kShape, {1});
  auto destination = MakeView<Real, 1>(destination_storage.data(), kShape, {3});
  SetVector(x_mutable, kX);
  SetVector(y_mutable, kY);
  asc::DenseView<const Real, 1> x(x_mutable);
  asc::DenseView<const Real, 1> y(y_mutable);

  const auto dot = asc::Dot(context, x, y);
  ASC_DENSE_TEST_CHECK(test, dot.ok());
  if (dot.ok()) {
    ASC_DENSE_TEST_EQ(test, *dot, Real{12});
  }
  ASC_DENSE_TEST_CHECK(test, asc::Copy(context, x, destination).ok());
  CheckVector(test, destination, std::span<const Real>(kX));
  ASC_DENSE_TEST_CHECK(test, asc::Scal(context, Real{-2}, destination).ok());
  constexpr std::array<Real, 3> kScaled{Real{-2}, Real{4}, Real{-6}};
  CheckVector(test, destination, std::span<const Real>(kScaled));

  ASC_DENSE_TEST_CHECK(test, asc::Axpy(context, Real{3}, x, y_mutable).ok());
  constexpr std::array<Real, 3> kAxpy{Real{7}, Real{-1}, Real{15}};
  CheckVector(test, y_mutable, std::span<const Real>(kAxpy));

  ASC_DENSE_TEST_CHECK(test, asc::Copy(context, x, x_mutable).ok());
  CheckVector(test, x_mutable, std::span<const Real>(kX));
  ASC_DENSE_TEST_CHECK(test, asc::Axpy(context, Real{2}, x, x_mutable).ok());
  constexpr std::array<Real, 3> kSelfAxpy{Real{3}, Real{-6}, Real{9}};
  CheckVector(test, x_mutable, std::span<const Real>(kSelfAxpy));

  constexpr std::array<asc::extent_t, 1> kNormShape{2};
  std::array<Real, 2> norm_storage{Real{3}, Real{4}};
  auto norm = MakeView<const Real, 1>(norm_storage.data(), kNormShape, {1});
  const auto norm_result = asc::Nrm2(context, norm);
  ASC_DENSE_TEST_CHECK(test, norm_result.ok());
  if (norm_result.ok()) {
    ASC_DENSE_TEST_EQ(test, *norm_result, Real{5});
  }

  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    const auto allocation_dot = asc::Dot(context, x, y);
    allocations = probe.count();
    ASC_DENSE_TEST_CHECK(test, allocation_dot.ok());
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

template <typename Real>
void TestVectorFailuresAndEmpty(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const std::array<asc::extent_t, 1> shape{3};
  std::array<Real, 6> storage{Real{1}, Real{2}, Real{3},
                              Real{4}, Real{5}, Real{6}};
  auto source = MakeView<const Real, 1>(storage.data(), shape, {1});
  auto partial = MakeView<Real, 1>(storage.data() + 1, shape, {1});
  const auto before = storage;
  CheckStatusError(test, asc::Copy(context, source, partial),
                   asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, storage, before);
  CheckStatusError(test, asc::Axpy(context, Real{2}, source, partial),
                   asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, storage, before);

  const std::array<asc::extent_t, 1> short_shape{2};
  std::array<Real, 2> short_storage{};
  auto short_destination =
      MakeView<Real, 1>(short_storage.data(), short_shape, {1});
  CheckStatusError(test, asc::Copy(context, source, short_destination),
                   asc::ErrorCode::kShape);
  CheckError(test,
             asc::Dot(context, source,
                      asc::DenseView<const Real, 1>(short_destination)),
             asc::ErrorCode::kShape);

  auto device =
      MakeView<Real, 1>(storage.data(), shape, {1}, asc::MemorySpace::kDevice);
  CheckStatusError(test, asc::Scal(context, Real{2}, device),
                   asc::ErrorCode::kMemoryAccess);
  ASC_DENSE_TEST_EQ(test, storage, before);

  const std::array<asc::extent_t, 1> empty_shape{0};
  auto empty_const = MakeView<const Real, 1>(nullptr, empty_shape, {1});
  auto empty_mutable = MakeView<Real, 1>(nullptr, empty_shape, {1});
  ASC_DENSE_TEST_CHECK(test,
                       asc::Copy(context, empty_const, empty_mutable).ok());
  ASC_DENSE_TEST_CHECK(test, asc::Scal(context, Real{2}, empty_mutable).ok());
  ASC_DENSE_TEST_CHECK(
      test, asc::Axpy(context, Real{2}, empty_const, empty_mutable).ok());
  const auto empty_dot = asc::Dot(context, empty_const, empty_const);
  const auto empty_norm = asc::Nrm2(context, empty_const);
  ASC_DENSE_TEST_CHECK(test, empty_dot.ok());
  ASC_DENSE_TEST_CHECK(test, empty_norm.ok());
  if (empty_dot.ok()) {
    ASC_DENSE_TEST_EQ(test, *empty_dot, Real{0});
    ASC_DENSE_TEST_CHECK(test, !std::signbit(*empty_dot));
  }
  if (empty_norm.ok()) {
    ASC_DENSE_TEST_EQ(test, *empty_norm, Real{0});
    ASC_DENSE_TEST_CHECK(test, !std::signbit(*empty_norm));
  }
}

void TestDeterministicDotOrder(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const std::array<asc::extent_t, 1> shape{3};
  std::array<float, 3> values{1.0e20F, 1.0F, -1.0e20F};
  std::array<float, 3> ones{1.0F, 1.0F, 1.0F};
  auto left = MakeView<const float, 1>(values.data(), shape, {1});
  auto right = MakeView<const float, 1>(ones.data(), shape, {1});
  auto result = asc::Dot(context, left, right);
  ASC_DENSE_TEST_CHECK(test, result.ok());
  if (result.ok()) {
    ASC_DENSE_TEST_EQ(test, *result, 0.0F);
  }
}

template <typename Real>
void TestStableNormSpecialValues(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const std::array<asc::extent_t, 1> shape{2};

  const Real large = std::numeric_limits<Real>::max() / Real{4};
  std::array<Real, 2> large_values{large, large};
  auto large_view = MakeView<const Real, 1>(large_values.data(), shape, {1});
  auto large_norm = asc::Nrm2(context, large_view);
  ASC_DENSE_TEST_CHECK(test, large_norm.ok());
  if (large_norm.ok()) {
    const Real oracle = std::hypot(large, large);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(*large_norm));
    ASC_DENSE_TEST_NEAR(test, *large_norm, oracle, Real{0},
                        Real{16} * std::numeric_limits<Real>::epsilon());
  }

  const Real small = std::numeric_limits<Real>::min();
  std::array<Real, 2> small_values{small, small};
  auto small_view = MakeView<const Real, 1>(small_values.data(), shape, {1});
  auto small_norm = asc::Nrm2(context, small_view);
  ASC_DENSE_TEST_CHECK(test, small_norm.ok());
  if (small_norm.ok()) {
    const Real oracle = std::hypot(small, small);
    ASC_DENSE_TEST_CHECK(test, *small_norm > Real{0});
    ASC_DENSE_TEST_NEAR(test, *small_norm, oracle,
                        std::numeric_limits<Real>::denorm_min(),
                        Real{16} * std::numeric_limits<Real>::epsilon());
  }

  const Real infinity = std::numeric_limits<Real>::infinity();
  std::array<Real, 2> one_infinity{infinity, Real{2}};
  std::array<Real, 2> repeated_infinity{infinity, -infinity};
  auto one_infinity_view =
      MakeView<const Real, 1>(one_infinity.data(), shape, {1});
  auto repeated_infinity_view =
      MakeView<const Real, 1>(repeated_infinity.data(), shape, {1});
  const auto one_infinity_norm = asc::Nrm2(context, one_infinity_view);
  const auto repeated_infinity_norm =
      asc::Nrm2(context, repeated_infinity_view);
  ASC_DENSE_TEST_CHECK(test, one_infinity_norm.ok());
  ASC_DENSE_TEST_CHECK(test, repeated_infinity_norm.ok());
  if (one_infinity_norm.ok()) {
    ASC_DENSE_TEST_CHECK(test, std::isinf(*one_infinity_norm));
  }
  if (repeated_infinity_norm.ok()) {
    ASC_DENSE_TEST_CHECK(test, std::isinf(*repeated_infinity_norm));
  }

  const Real nan = std::numeric_limits<Real>::quiet_NaN();
  std::array<Real, 2> nan_values{nan, infinity};
  auto nan_view = MakeView<const Real, 1>(nan_values.data(), shape, {1});
  auto nan_norm = asc::Nrm2(context, nan_view);
  ASC_DENSE_TEST_CHECK(test, nan_norm.ok());
  if (nan_norm.ok()) {
    ASC_DENSE_TEST_CHECK(test, std::isnan(*nan_norm));
  }

  std::array<Real, 2> signed_zeros{-Real{0}, Real{0}};
  auto zeros_view = MakeView<const Real, 1>(signed_zeros.data(), shape, {1});
  auto zero_norm = asc::Nrm2(context, zeros_view);
  ASC_DENSE_TEST_CHECK(test, zero_norm.ok());
  if (zero_norm.ok()) {
    ASC_DENSE_TEST_EQ(test, *zero_norm, Real{0});
    ASC_DENSE_TEST_CHECK(test, !std::signbit(*zero_norm));
  }
}

template <typename Real>
void TestGemv(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  constexpr std::array<asc::extent_t, 2> kMatrixShape{2, 3};
  constexpr std::array<Real, 6> kMatrixValues{Real{1}, Real{-2}, Real{3},
                                              Real{4}, Real{0},  Real{-1}};
  std::array<Real, 13> matrix_storage{};
  auto matrix_mutable =
      MakeView<Real, 2>(matrix_storage.data(), kMatrixShape, {2, 5});
  SetMatrix(matrix_mutable, kMatrixValues);
  asc::DenseView<const Real, 2> matrix(matrix_mutable);

  constexpr std::array<asc::extent_t, 1> kInputShape{3};
  constexpr std::array<Real, 3> kInputValues{Real{2}, Real{-1}, Real{0.5}};
  std::array<Real, 5> input_storage{};
  auto input_mutable =
      MakeView<Real, 1>(input_storage.data(), kInputShape, {2});
  SetVector(input_mutable, kInputValues);
  asc::DenseView<const Real, 1> input(input_mutable);

  constexpr std::array<asc::extent_t, 1> kOutputShape{2};
  std::array<Real, 4> output_storage{};
  auto output = MakeView<Real, 1>(output_storage.data(), kOutputShape, {3});
  ASC_DENSE_TEST_CHECK(test, asc::Gemv(context, asc::DenseBlasTranspose::kNone,
                                       Real{1}, matrix, input, Real{0}, output)
                                 .ok());
  constexpr std::array<Real, 2> kExpected{Real{5.5}, Real{7.5}};
  CheckVector(test, output, std::span<const Real>(kExpected));

  constexpr std::array<Real, 2> kPrior{Real{10}, Real{20}};
  SetVector(output, kPrior);
  ASC_DENSE_TEST_CHECK(test, asc::Gemv(context, asc::DenseBlasTranspose::kNone,
                                       Real{2}, matrix, input, Real{-1}, output)
                                 .ok());
  constexpr std::array<Real, 2> kAlphaBeta{Real{1}, Real{-5}};
  CheckVector(test, output, std::span<const Real>(kAlphaBeta));

  std::array<Real, 2> nan_output_storage{
      std::numeric_limits<Real>::quiet_NaN(),
      std::numeric_limits<Real>::quiet_NaN()};
  auto nan_output =
      MakeView<Real, 1>(nan_output_storage.data(), kOutputShape, {1});
  ASC_DENSE_TEST_CHECK(
      test, asc::Gemv(context, asc::DenseBlasTranspose::kNone, Real{1}, matrix,
                      input, Real{0}, nan_output)
                .ok());
  CheckVector(test, nan_output, std::span<const Real>(kExpected));

  constexpr std::array<asc::extent_t, 1> kTransposeInputShape{2};
  constexpr std::array<Real, 2> kTransposeInputValues{Real{2}, Real{-1}};
  std::array<Real, 2> transpose_input_storage{};
  auto transpose_input_mutable = MakeView<Real, 1>(
      transpose_input_storage.data(), kTransposeInputShape, {1});
  SetVector(transpose_input_mutable, kTransposeInputValues);
  asc::DenseView<const Real, 1> transpose_input(transpose_input_mutable);
  std::array<Real, 5> transpose_output_storage{};
  auto transpose_output =
      MakeView<Real, 1>(transpose_output_storage.data(), kInputShape, {2});
  ASC_DENSE_TEST_CHECK(
      test, asc::Gemv(context, asc::DenseBlasTranspose::kTranspose, Real{1},
                      matrix, transpose_input, Real{0}, transpose_output)
                .ok());
  constexpr std::array<Real, 3> kTransposeExpected{Real{-2}, Real{-4}, Real{7}};
  CheckVector(test, transpose_output,
              std::span<const Real>(kTransposeExpected));
}

template <typename Real>
void TestGemvFailures(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const std::array<asc::extent_t, 2> matrix_shape{2, 3};
  const std::array<asc::extent_t, 1> input_shape{3};
  const std::array<asc::extent_t, 1> output_shape{2};
  std::array<Real, 10> matrix_storage{};
  std::array<Real, 3> input_storage{Real{1}, Real{2}, Real{3}};
  std::array<Real, 2> output_storage{Real{7}, Real{8}};
  auto matrix_mutable =
      MakeView<Real, 2>(matrix_storage.data(), matrix_shape, {1, 3});
  auto matrix = asc::DenseView<const Real, 2>(matrix_mutable);
  auto input = MakeView<const Real, 1>(input_storage.data(), input_shape, {1});
  auto output = MakeView<Real, 1>(output_storage.data(), output_shape, {1});
  const auto before = output_storage;

  CheckStatusError(test,
                   asc::Gemv(context, static_cast<asc::DenseBlasTranspose>(255),
                             Real{1}, matrix, input, Real{0}, output),
                   asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, output_storage, before);

  const std::array<asc::extent_t, 1> short_input_shape{2};
  auto short_input =
      MakeView<const Real, 1>(input_storage.data(), short_input_shape, {1});
  CheckStatusError(test,
                   asc::Gemv(context, asc::DenseBlasTranspose::kNone, Real{1},
                             matrix, short_input, Real{0}, output),
                   asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(test, output_storage, before);

  auto overlapping_output =
      MakeView<Real, 1>(matrix_storage.data(), output_shape, {1});
  CheckStatusError(test,
                   asc::Gemv(context, asc::DenseBlasTranspose::kNone, Real{1},
                             matrix, input, Real{0}, overlapping_output),
                   asc::ErrorCode::kInvalidArgument);

  auto device_output = MakeView<Real, 1>(output_storage.data(), output_shape,
                                         {1}, asc::MemorySpace::kDevice);
  CheckStatusError(test,
                   asc::Gemv(context, asc::DenseBlasTranspose::kNone, Real{1},
                             matrix, input, Real{0}, device_output),
                   asc::ErrorCode::kMemoryAccess);
  ASC_DENSE_TEST_EQ(test, output_storage, before);
}

template <typename Real>
void TestGemm(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  constexpr std::array<asc::extent_t, 2> kShape{2, 2};
  constexpr std::array<Real, 4> kA{Real{1}, Real{2}, Real{3}, Real{4}};
  constexpr std::array<Real, 4> kB{Real{5}, Real{6}, Real{7}, Real{8}};
  std::array<Real, 5> left_storage{};
  std::array<Real, 4> right_storage{};
  std::array<Real, 7> output_storage{};
  auto left_mutable = MakeView<Real, 2>(left_storage.data(), kShape, {1, 3});
  auto right_mutable = MakeView<Real, 2>(right_storage.data(), kShape, {2, 1});
  auto output = MakeView<Real, 2>(output_storage.data(), kShape, {2, 4});
  SetMatrix(left_mutable, kA);
  SetMatrix(right_mutable, kB);
  asc::DenseView<const Real, 2> left(left_mutable);
  asc::DenseView<const Real, 2> right(right_mutable);

  constexpr std::array<std::array<asc::DenseBlasTranspose, 2>, 4> kModes{
      std::array<asc::DenseBlasTranspose, 2>{asc::DenseBlasTranspose::kNone,
                                             asc::DenseBlasTranspose::kNone},
      std::array<asc::DenseBlasTranspose, 2>{
          asc::DenseBlasTranspose::kTranspose, asc::DenseBlasTranspose::kNone},
      std::array<asc::DenseBlasTranspose, 2>{
          asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kTranspose},
      std::array<asc::DenseBlasTranspose, 2>{
          asc::DenseBlasTranspose::kTranspose,
          asc::DenseBlasTranspose::kTranspose},
  };
  constexpr std::array<std::array<Real, 4>, 4> kExpected{
      std::array<Real, 4>{Real{19}, Real{22}, Real{43}, Real{50}},
      std::array<Real, 4>{Real{26}, Real{30}, Real{38}, Real{44}},
      std::array<Real, 4>{Real{17}, Real{23}, Real{39}, Real{53}},
      std::array<Real, 4>{Real{23}, Real{31}, Real{34}, Real{46}},
  };
  for (std::size_t mode = 0; mode < kModes.size(); ++mode) {
    SetMatrix(output,
              std::array<Real, 4>{Real{-1}, Real{-1}, Real{-1}, Real{-1}});
    ASC_DENSE_TEST_CHECK(
        test, asc::Gemm(context, kModes[mode][0], kModes[mode][1], Real{1},
                        left, right, Real{0}, output)
                  .ok());
    CheckMatrix(test, output, std::span<const Real>(kExpected[mode]));
  }

  constexpr std::array<Real, 4> kPrior{Real{1}, Real{2}, Real{3}, Real{4}};
  SetMatrix(output, kPrior);
  ASC_DENSE_TEST_CHECK(test, asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                                       asc::DenseBlasTranspose::kNone, Real{2},
                                       left, right, Real{-1}, output)
                                 .ok());
  constexpr std::array<Real, 4> kAlphaBeta{Real{37}, Real{42}, Real{83},
                                           Real{96}};
  CheckMatrix(test, output, std::span<const Real>(kAlphaBeta));

  SetMatrix(output, std::array<Real, 4>{
                        std::numeric_limits<Real>::quiet_NaN(),
                        std::numeric_limits<Real>::quiet_NaN(),
                        std::numeric_limits<Real>::quiet_NaN(),
                        std::numeric_limits<Real>::quiet_NaN(),
                    });
  ASC_DENSE_TEST_CHECK(test, asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                                       asc::DenseBlasTranspose::kNone, Real{1},
                                       left, right, Real{0}, output)
                                 .ok());
  CheckMatrix(test, output, std::span<const Real>(kExpected[0]));

  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    const asc::Status status = asc::Gemm(
        context, asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kNone,
        Real{1}, left, right, Real{0}, output);
    allocations = probe.count();
    ASC_DENSE_TEST_CHECK(test, status.ok());
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

template <typename Real>
void TestRectangularPaddedGemm(TestContext& test) {
  constexpr asc::extent_t kRows = 2;
  constexpr asc::extent_t kInner = 3;
  constexpr asc::extent_t kColumns = 4;
  constexpr std::array<Real, 6> kLogicalLeft{Real{1}, Real{2}, Real{3},
                                             Real{4}, Real{5}, Real{6}};
  constexpr std::array<Real, 12> kLogicalRight{
      Real{7},  Real{8},  Real{9},  Real{10}, Real{11}, Real{12},
      Real{13}, Real{14}, Real{15}, Real{16}, Real{17}, Real{18}};
  constexpr std::array<Real, 8> kExpected{Real{74},  Real{80},  Real{86},
                                          Real{92},  Real{173}, Real{188},
                                          Real{203}, Real{218}};
  constexpr std::array<std::array<asc::DenseBlasTranspose, 2>, 4> kModes{
      std::array{asc::DenseBlasTranspose::kNone,
                 asc::DenseBlasTranspose::kNone},
      std::array{asc::DenseBlasTranspose::kTranspose,
                 asc::DenseBlasTranspose::kNone},
      std::array{asc::DenseBlasTranspose::kNone,
                 asc::DenseBlasTranspose::kTranspose},
      std::array{asc::DenseBlasTranspose::kTranspose,
                 asc::DenseBlasTranspose::kTranspose},
  };
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();

  for (const auto& mode : kModes) {
    const bool transpose_left = mode[0] == asc::DenseBlasTranspose::kTranspose;
    const bool transpose_right = mode[1] == asc::DenseBlasTranspose::kTranspose;
    const std::array<asc::extent_t, 2> left_shape{
        transpose_left ? kInner : kRows, transpose_left ? kRows : kInner};
    const std::array<asc::extent_t, 2> right_shape{
        transpose_right ? kColumns : kInner,
        transpose_right ? kInner : kColumns};
    constexpr std::array<asc::extent_t, 2> kOutputShape{kRows, kColumns};

    std::array<Real, 32> left_storage{};
    std::array<Real, 40> right_storage{};
    std::array<Real, 24> output_storage{};
    left_storage.fill(Real{-701});
    right_storage.fill(Real{-702});
    output_storage.fill(Real{-703});
    auto left_mutable =
        MakeView<Real, 2>(left_storage.data(), left_shape, {1, 7});
    auto right_mutable =
        MakeView<Real, 2>(right_storage.data(), right_shape, {8, 1});
    auto output =
        MakeView<Real, 2>(output_storage.data(), kOutputShape, {1, 5});

    for (asc::index_t row = 0; row < kRows; ++row) {
      for (asc::index_t inner = 0; inner < kInner; ++inner) {
        const std::array<asc::index_t, 2> coordinate =
            transpose_left ? std::array<asc::index_t, 2>{inner, row}
                           : std::array<asc::index_t, 2>{row, inner};
        ElementAt(left_mutable, coordinate) =
            kLogicalLeft[static_cast<std::size_t>(row * kInner + inner)];
      }
    }
    for (asc::index_t inner = 0; inner < kInner; ++inner) {
      for (asc::index_t column = 0; column < kColumns; ++column) {
        const std::array<asc::index_t, 2> coordinate =
            transpose_right ? std::array<asc::index_t, 2>{column, inner}
                            : std::array<asc::index_t, 2>{inner, column};
        ElementAt(right_mutable, coordinate) =
            kLogicalRight[static_cast<std::size_t>(inner * kColumns + column)];
      }
    }

    const asc::Status status = asc::Gemm(
        context, mode[0], mode[1], Real{1},
        asc::DenseView<const Real, 2>(left_mutable),
        asc::DenseView<const Real, 2>(right_mutable), Real{0}, output);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    if (status.ok()) {
      CheckMatrix(test, output, std::span<const Real>(kExpected));
      constexpr std::array<std::size_t, 8> kOutputHoles{2, 3, 4,  7,
                                                        8, 9, 12, 13};
      for (std::size_t hole : kOutputHoles) {
        ASC_DENSE_TEST_EQ(test, output_storage[hole], Real{-703});
      }
    }
  }
}

template <typename Real>
void TestGemmZeroInnerAndFailures(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const std::array<asc::extent_t, 2> left_shape{2, 0};
  const std::array<asc::extent_t, 2> right_shape{0, 3};
  const std::array<asc::extent_t, 2> output_shape{2, 3};
  auto left = MakeView<const Real, 2>(nullptr, left_shape, {1, 2});
  auto right = MakeView<const Real, 2>(nullptr, right_shape, {1, 1});
  std::array<Real, 6> output_storage{Real{1}, Real{2}, Real{3},
                                     Real{4}, Real{5}, Real{6}};
  auto output = MakeView<Real, 2>(output_storage.data(), output_shape, {1, 2});
  constexpr std::array<Real, 6> kPrior{Real{1}, Real{2}, Real{3},
                                       Real{4}, Real{5}, Real{6}};
  SetMatrix(output, kPrior);
  ASC_DENSE_TEST_CHECK(test, asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                                       asc::DenseBlasTranspose::kNone, Real{3},
                                       left, right, Real{2}, output)
                                 .ok());
  constexpr std::array<Real, 6> kScaled{Real{2}, Real{4},  Real{6},
                                        Real{8}, Real{10}, Real{12}};
  CheckMatrix(test, output, std::span<const Real>(kScaled));

  SetMatrix(output, std::array<Real, 6>{
                        std::numeric_limits<Real>::quiet_NaN(),
                        std::numeric_limits<Real>::quiet_NaN(),
                        std::numeric_limits<Real>::quiet_NaN(),
                        std::numeric_limits<Real>::quiet_NaN(),
                        std::numeric_limits<Real>::quiet_NaN(),
                        std::numeric_limits<Real>::quiet_NaN(),
                    });
  ASC_DENSE_TEST_CHECK(test, asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                                       asc::DenseBlasTranspose::kNone, Real{3},
                                       left, right, Real{0}, output)
                                 .ok());
  constexpr std::array<Real, 6> kZeros{};
  CheckMatrix(test, output, std::span<const Real>(kZeros));

  const std::array<asc::extent_t, 2> square{2, 2};
  std::array<Real, 12> storage{};
  auto left_mutable = MakeView<Real, 2>(storage.data(), square, {1, 2});
  auto square_left = asc::DenseView<const Real, 2>(left_mutable);
  auto square_right =
      MakeView<const Real, 2>(storage.data() + 4, square, {1, 2});
  auto overlapping_output =
      MakeView<Real, 2>(storage.data() + 1, square, {1, 2});
  CheckStatusError(
      test,
      asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                asc::DenseBlasTranspose::kNone, Real{1}, square_left,
                square_right, Real{0}, overlapping_output),
      asc::ErrorCode::kInvalidArgument);

  std::array<Real, 4> independent_output_storage{Real{1}, Real{2}, Real{3},
                                                 Real{4}};
  auto independent_output =
      MakeView<Real, 2>(independent_output_storage.data(), square, {1, 2});
  const auto before = independent_output_storage;
  CheckStatusError(
      test,
      asc::Gemm(context, static_cast<asc::DenseBlasTranspose>(255),
                asc::DenseBlasTranspose::kNone, Real{1}, square_left,
                square_right, Real{0}, independent_output),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, independent_output_storage, before);

  const std::array<asc::extent_t, 2> mismatched{3, 2};
  auto mismatched_right =
      MakeView<const Real, 2>(storage.data() + 4, mismatched, {1, 3});
  CheckStatusError(
      test,
      asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                asc::DenseBlasTranspose::kNone, Real{1}, square_left,
                mismatched_right, Real{0}, independent_output),
      asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(test, independent_output_storage, before);
}

}  // namespace

int main() {
  TestContext test;
  TestVectorOperations<float>(test);
  TestVectorOperations<double>(test);
  TestVectorFailuresAndEmpty<float>(test);
  TestVectorFailuresAndEmpty<double>(test);
  TestDeterministicDotOrder(test);
  TestStableNormSpecialValues<float>(test);
  TestStableNormSpecialValues<double>(test);
  TestGemv<float>(test);
  TestGemv<double>(test);
  TestGemvFailures<float>(test);
  TestGemvFailures<double>(test);
  TestGemm<float>(test);
  TestGemm<double>(test);
  TestRectangularPaddedGemm<float>(test);
  TestRectangularPaddedGemm<double>(test);
  TestGemmZeroInnerAndFailures<float>(test);
  TestGemmZeroInnerAndFailures<double>(test);
  return test.Finish();
}
