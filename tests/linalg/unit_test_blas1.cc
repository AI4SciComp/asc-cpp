#include <asc/linalg/blas1.h>
#include <asc/linalg/capabilities.h>
#include <asc/array/tensor.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "test_operands.h"

namespace asc {
namespace {

using FloatVectorView = TensorView<float, Extents<3>>;
using ConstFloatVectorView = TensorView<const float, Extents<3>>;
using DoubleVectorView = TensorView<double, Extents<3>>;
using IntegerVectorView = TensorView<int, Extents<3>>;
using LongDoubleVectorView = TensorView<long double, Extents<3>>;
using FloatMatrixView = TensorView<float, Extents<2, 3>>;

template <typename Scalar>
concept CanScalFloatVector = requires(const ExecutionContext& context,
                                      Scalar alpha,
                                      const FloatVectorView& vector) {
  Scal(context, alpha, vector);
};

template <typename Scalar>
concept CanAxpyFloatVector = requires(const ExecutionContext& context,
                                      Scalar alpha,
                                      const ConstFloatVectorView& input,
                                      const FloatVectorView& output) {
  Axpy(context, alpha, input, output);
};

template <typename Input, typename Output>
concept CanCopyVectors = requires(const ExecutionContext& context,
                                  const Input& input,
                                  const Output& output) {
  Copy(context, input, output);
};

static_assert(ReadableLinalgVector<FloatVectorView>);
static_assert(WritableLinalgVector<FloatVectorView>);
static_assert(ReadableLinalgVector<ConstFloatVectorView>);
static_assert(!WritableLinalgVector<ConstFloatVectorView>);
static_assert(ReadableLinalgVector<DoubleVectorView>);
static_assert(!ReadableLinalgVector<IntegerVectorView>);
static_assert(!ReadableLinalgVector<LongDoubleVectorView>);
static_assert(!ReadableLinalgVector<FloatMatrixView>);
static_assert(ReadableLinalgMatrix<FloatMatrixView>);
static_assert(WritableLinalgMatrix<FloatMatrixView>);
static_assert(!ReadableLinalgVector<Tensor<float, Extents<3>>>);
static_assert(CanScalFloatVector<float>);
static_assert(!CanScalFloatVector<double>);
static_assert(CanAxpyFloatVector<float>);
static_assert(!CanAxpyFloatVector<double>);
static_assert(CanCopyVectors<ConstFloatVectorView, FloatVectorView>);
static_assert(!CanCopyVectors<DoubleVectorView, FloatVectorView>);
using ForeignFloatVector = test::LinalgTestOperand<
    float, LayoutLeftMapping<Extents<3>>>;
using ForeignConstFloatVector = test::LinalgTestOperand<
    const float, LayoutLeftMapping<Extents<3>>>;
static_assert(ReadableLinalgVector<ForeignFloatVector>);
static_assert(WritableLinalgVector<ForeignFloatVector>);
static_assert(ReadableLinalgVector<ForeignConstFloatVector>);
static_assert(!WritableLinalgVector<ForeignConstFloatVector>);

template <typename T>
class Blas1Test : public ::testing::Test {};

using FloatingTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(Blas1Test, FloatingTypes);

template <typename T>
constexpr T TestTolerance() {
  return T{64} * std::numeric_limits<T>::epsilon();
}

TEST(LinalgCapabilitiesTest, ReportsCompiledSerialReferenceProvider) {
  Result<LinalgCapabilities> result =
      GetLinalgCapabilities(ExecutionContext::Serial());

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().GetBackend(), BackendKind::kSerial);
  EXPECT_EQ(result.value().GetProviderName(), "serial-reference");
  EXPECT_TRUE(result.value().IsDeterministic());
  EXPECT_TRUE(result.value().Supports(LinalgOperation::kCopy));
  EXPECT_TRUE(result.value().Supports(LinalgOperation::kScal));
  EXPECT_TRUE(result.value().Supports(LinalgOperation::kAxpy));
  EXPECT_TRUE(result.value().Supports(LinalgOperation::kDot));
  EXPECT_TRUE(result.value().Supports(LinalgOperation::kNrm2));
  EXPECT_TRUE(result.value().Supports(LinalgOperation::kGemv));
  EXPECT_TRUE(result.value().Supports(LinalgOperation::kGemm));
  EXPECT_FALSE(result.value().Supports(
      static_cast<LinalgOperation>(255)));
}

TYPED_TEST(Blas1Test, ComputesGoldenResults) {
  using T = TypeParam;
  using ExtentsType = Extents<4>;
  auto mapping = LayoutLeftMapping<ExtentsType>::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::array<T, 4> x{T{1}, T{-2}, T{3}, T{-4}};
  std::array<T, 4> y{T{5}, T{6}, T{7}, T{8}};
  auto x_view = TensorView<const T, ExtentsType>::Create(
      x.data(), mapping.value(), x.size());
  auto y_view = TensorView<T, ExtentsType>::Create(
      y.data(), mapping.value(), y.size());
  ASSERT_TRUE(x_view.ok());
  ASSERT_TRUE(y_view.ok());
  const ExecutionContext context = ExecutionContext::Serial();

  Status copy = Copy(context, x_view.value(), y_view.value());
  ASSERT_TRUE(copy.ok());
  EXPECT_EQ(y, x);

  Status scale = Scal(context, T{-2}, y_view.value());
  ASSERT_TRUE(scale.ok());
  EXPECT_EQ(y, (std::array<T, 4>{T{-2}, T{4}, T{-6}, T{8}}));

  Status axpy = Axpy(context, T{0.5}, x_view.value(), y_view.value());
  ASSERT_TRUE(axpy.ok());
  EXPECT_EQ(y, (std::array<T, 4>{T{-1.5}, T{3}, T{-4.5}, T{6}}));

  Result<T> dot = Dot(context, x_view.value(), y_view.value());
  ASSERT_TRUE(dot.ok());
  EXPECT_EQ(dot.value(), T{-45});

  Result<T> norm = Nrm2(context, x_view.value());
  ASSERT_TRUE(norm.ok());
  EXPECT_NEAR(norm.value(), std::sqrt(T{30}), TestTolerance<T>());
}

TYPED_TEST(Blas1Test, HonorsUniqueStridesWithoutTouchingHoles) {
  using T = TypeParam;
  using ExtentsType = Extents<3>;
  using Mapping = LayoutStrideMapping<ExtentsType>;
  auto mapping = Mapping::Create(ExtentsType(),
                                 std::array<stride_t, 1>{2});
  ASSERT_TRUE(mapping.ok());
  std::array<T, 5> x{T{1}, T{101}, T{2}, T{103}, T{3}};
  std::array<T, 5> y{T{9}, T{201}, T{9}, T{203}, T{9}};
  auto x_view = TensorView<const T, ExtentsType, Mapping>::Create(
      x.data(), mapping.value(), x.size());
  auto y_view = TensorView<T, ExtentsType, Mapping>::Create(
      y.data(), mapping.value(), y.size());
  ASSERT_TRUE(x_view.ok());
  ASSERT_TRUE(y_view.ok());
  const ExecutionContext context = ExecutionContext::Serial();

  ASSERT_TRUE(Copy(context, x_view.value(), y_view.value()).ok());
  EXPECT_EQ(y, (std::array<T, 5>{T{1}, T{201}, T{2}, T{203}, T{3}}));
  ASSERT_TRUE(Scal(context, T{3}, y_view.value()).ok());
  EXPECT_EQ(y, (std::array<T, 5>{T{3}, T{201}, T{6}, T{203}, T{9}}));
  Result<T> dot = Dot(context, x_view.value(), y_view.value());
  ASSERT_TRUE(dot.ok());
  EXPECT_EQ(dot.value(), T{42});
}

TYPED_TEST(Blas1Test, DefinesEmptyResultsAndPositiveZero) {
  using T = TypeParam;
  using ExtentsType = Extents<0>;
  auto mapping = LayoutLeftMapping<ExtentsType>::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  auto readable = TensorView<const T, ExtentsType>::Create(
      nullptr, mapping.value(), 0);
  auto writable = TensorView<T, ExtentsType>::Create(
      nullptr, mapping.value(), 0);
  ASSERT_TRUE(readable.ok());
  ASSERT_TRUE(writable.ok());
  const ExecutionContext context = ExecutionContext::Serial();

  EXPECT_TRUE(Copy(context, readable.value(), writable.value()).ok());
  EXPECT_TRUE(Scal(context, T{7}, writable.value()).ok());
  EXPECT_TRUE(
      Axpy(context, T{7}, readable.value(), writable.value()).ok());
  Result<T> dot = Dot(context, readable.value(), readable.value());
  Result<T> norm = Nrm2(context, readable.value());
  ASSERT_TRUE(dot.ok());
  ASSERT_TRUE(norm.ok());
  EXPECT_EQ(dot.value(), T{0});
  EXPECT_EQ(norm.value(), T{0});
  EXPECT_FALSE(std::signbit(dot.value()));
  EXPECT_FALSE(std::signbit(norm.value()));
}

TYPED_TEST(Blas1Test, SupportsOnlyDocumentedExactAliases) {
  using T = TypeParam;
  using ExtentsType = Extents<4>;
  auto mapping = LayoutLeftMapping<ExtentsType>::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::array<T, 5> data{T{1}, T{2}, T{3}, T{4}, T{5}};
  auto readable = TensorView<const T, ExtentsType>::Create(
      data.data(), mapping.value(), 4);
  auto exact = TensorView<T, ExtentsType>::Create(
      data.data(), mapping.value(), 4);
  auto partial = TensorView<T, ExtentsType>::Create(
      data.data() + 1, mapping.value(), 4);
  ASSERT_TRUE(readable.ok());
  ASSERT_TRUE(exact.ok());
  ASSERT_TRUE(partial.ok());
  const ExecutionContext context = ExecutionContext::Serial();

  ASSERT_TRUE(Copy(context, readable.value(), exact.value()).ok());
  EXPECT_EQ(data, (std::array<T, 5>{T{1}, T{2}, T{3}, T{4}, T{5}}));
  ASSERT_TRUE(
      Axpy(context, T{2}, readable.value(), exact.value()).ok());
  EXPECT_EQ(data, (std::array<T, 5>{T{3}, T{6}, T{9}, T{12}, T{5}}));

  const std::array<T, 5> before = data;
  Status copy = Copy(context, readable.value(), partial.value());
  ASSERT_FALSE(copy.ok());
  EXPECT_EQ(copy.code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(data, before);
  Status axpy = Axpy(context, T{2}, readable.value(), partial.value());
  ASSERT_FALSE(axpy.ok());
  EXPECT_EQ(axpy.code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(data, before);

  Result<T> dot = Dot(context, readable.value(), partial.value());
  ASSERT_TRUE(dot.ok());
  EXPECT_EQ(dot.value(), T{240});
}

TYPED_TEST(Blas1Test, ShapeFailureIsTransactional) {
  using T = TypeParam;
  using InputExtents = Extents<2>;
  using OutputExtents = Extents<3>;
  auto input_mapping =
      LayoutLeftMapping<InputExtents>::Create(InputExtents());
  auto output_mapping =
      LayoutLeftMapping<OutputExtents>::Create(OutputExtents());
  ASSERT_TRUE(input_mapping.ok());
  ASSERT_TRUE(output_mapping.ok());
  std::array<T, 2> input{T{1}, T{2}};
  std::array<T, 3> output{T{3}, T{4}, T{5}};
  auto input_view = TensorView<const T, InputExtents>::Create(
      input.data(), input_mapping.value(), input.size());
  auto output_view = TensorView<T, OutputExtents>::Create(
      output.data(), output_mapping.value(), output.size());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_view.ok());
  const std::array<T, 3> before = output;
  const ExecutionContext context = ExecutionContext::Serial();

  Status copy = Copy(context, input_view.value(), output_view.value());
  Status axpy =
      Axpy(context, T{2}, input_view.value(), output_view.value());
  Result<T> dot = Dot(context, input_view.value(), output_view.value());
  ASSERT_FALSE(copy.ok());
  ASSERT_FALSE(axpy.ok());
  ASSERT_FALSE(dot.ok());
  EXPECT_EQ(copy.code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(axpy.code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(dot.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(output, before);
}

TYPED_TEST(Blas1Test, Nrm2UsesScaledSumOfSquaresAtFiniteExtremes) {
  using T = TypeParam;
  using ExtentsType = Extents<2>;
  auto mapping = LayoutLeftMapping<ExtentsType>::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  const T high = std::numeric_limits<T>::max() / T{4};
  const T low = std::numeric_limits<T>::min();
  std::array<T, 2> high_values{high, high};
  std::array<T, 2> low_values{low, low};
  auto high_view = TensorView<const T, ExtentsType>::Create(
      high_values.data(), mapping.value(), high_values.size());
  auto low_view = TensorView<const T, ExtentsType>::Create(
      low_values.data(), mapping.value(), low_values.size());
  ASSERT_TRUE(high_view.ok());
  ASSERT_TRUE(low_view.ok());

  Result<T> high_norm =
      Nrm2(ExecutionContext::Serial(), high_view.value());
  Result<T> low_norm =
      Nrm2(ExecutionContext::Serial(), low_view.value());
  ASSERT_TRUE(high_norm.ok());
  ASSERT_TRUE(low_norm.ok());
  EXPECT_TRUE(std::isfinite(high_norm.value()));
  EXPECT_GT(low_norm.value(), T{0});
  EXPECT_NEAR(high_norm.value() / high, std::sqrt(T{2}),
              TestTolerance<T>());
  EXPECT_NEAR(low_norm.value() / low, std::sqrt(T{2}),
              TestTolerance<T>());
}

TYPED_TEST(Blas1Test, DeterministicIdentitiesHold) {
  using T = TypeParam;
  using ExtentsType = Extents<5>;
  auto mapping = LayoutLeftMapping<ExtentsType>::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::array<T, 5> x{T{-3}, T{-1}, T{0.5}, T{2}, T{4}};
  std::array<T, 5> y{T{2}, T{1}, T{-2}, T{3}, T{-1}};
  auto x_view = TensorView<const T, ExtentsType>::Create(
      x.data(), mapping.value(), x.size());
  auto y_view = TensorView<const T, ExtentsType>::Create(
      y.data(), mapping.value(), y.size());
  ASSERT_TRUE(x_view.ok());
  ASSERT_TRUE(y_view.ok());
  const ExecutionContext context = ExecutionContext::Serial();

  Result<T> xy = Dot(context, x_view.value(), y_view.value());
  Result<T> yx = Dot(context, y_view.value(), x_view.value());
  Result<T> xx = Dot(context, x_view.value(), x_view.value());
  Result<T> norm = Nrm2(context, x_view.value());
  Result<T> repeated = Dot(context, x_view.value(), y_view.value());
  ASSERT_TRUE(xy.ok());
  ASSERT_TRUE(yx.ok());
  ASSERT_TRUE(xx.ok());
  ASSERT_TRUE(norm.ok());
  ASSERT_TRUE(repeated.ok());
  EXPECT_EQ(xy.value(), yx.value());
  EXPECT_EQ(xy.value(), repeated.value());
  EXPECT_NEAR(norm.value() * norm.value(), xx.value(),
              T{8} * TestTolerance<T>() * xx.value());
}

TEST(Blas1BoundaryTest, RevalidatesAccessibilitySpanAndNullData) {
  using ExtentsType = Extents<3>;
  using Mapping = LayoutLeftMapping<ExtentsType>;
  auto mapping = Mapping::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::array<double, 3> input{1.0, 2.0, 3.0};
  std::array<double, 3> output{4.0, 5.0, 6.0};
  test::LinalgTestOperand<const double, Mapping> inaccessible(
      input.data(), mapping.value(), input.size(), MemorySpace::kDevice);
  test::LinalgTestOperand<const double, Mapping> insufficient(
      input.data(), mapping.value(), 2);
  test::LinalgTestOperand<const double, Mapping> null_input(
      nullptr, mapping.value(), input.size());
  auto output_view = TensorView<double, ExtentsType>::Create(
      output.data(), mapping.value(), output.size());
  ASSERT_TRUE(output_view.ok());
  const std::array<double, 3> before = output;
  const ExecutionContext context = ExecutionContext::Serial();

  Status accessibility =
      Copy(context, inaccessible, output_view.value());
  Status span = Copy(context, insufficient, output_view.value());
  Status data = Copy(context, null_input, output_view.value());

  ASSERT_FALSE(accessibility.ok());
  ASSERT_FALSE(span.ok());
  ASSERT_FALSE(data.ok());
  EXPECT_EQ(accessibility.code(), StatusCode::kUnsupported);
  EXPECT_EQ(span.code(), StatusCode::kOverflow);
  EXPECT_EQ(data.code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(output, before);
}

TEST(Blas1BoundaryTest, RejectsForeignNonUniqueWritableMapping) {
  using ExtentsType = Extents<3>;
  using InputMapping = LayoutLeftMapping<ExtentsType>;
  using OutputMapping = LayoutStrideMapping<ExtentsType>;
  auto input_mapping = InputMapping::Create(ExtentsType());
  auto output_mapping = OutputMapping::Create(
      ExtentsType(), std::array<stride_t, 1>{0});
  ASSERT_TRUE(input_mapping.ok());
  ASSERT_TRUE(output_mapping.ok());
  std::array<double, 3> input{1.0, 2.0, 3.0};
  std::array<double, 1> output{7.0};
  auto input_view = TensorView<const double, ExtentsType>::Create(
      input.data(), input_mapping.value(), input.size());
  test::LinalgTestOperand<double, OutputMapping> non_unique(
      output.data(), output_mapping.value(), output.size());
  ASSERT_TRUE(input_view.ok());
  static_assert(WritableLinalgVector<decltype(non_unique)>);

  Status status = Copy(ExecutionContext::Serial(), input_view.value(),
                       non_unique);

  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(output[0], 7.0);
}

TEST(Blas1BoundaryTest, DetectsMetadataAndByteRangeOverflowBeforeAccess) {
  using ExtentsType = Extents<3>;
  using Mapping = LayoutLeftMapping<ExtentsType>;
  auto mapping = Mapping::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::array<double, 3> output{7.0, 8.0, 9.0};
  test::LinalgTestOperand<const double, Mapping> metadata(
      nullptr, mapping.value(), std::numeric_limits<extent_t>::max());
  metadata.SetExtent(0, std::numeric_limits<extent_t>::max());
  metadata.SetStride(0, 2);
  metadata.SetSize(std::numeric_limits<extent_t>::max());
  metadata.SetRequiredSpan(std::numeric_limits<extent_t>::max());
  const auto address = std::numeric_limits<std::uintptr_t>::max() - 1;
  auto* invalid_pointer = reinterpret_cast<const double*>(address);
  test::LinalgTestOperand<const double, Mapping> byte_range(
      invalid_pointer, mapping.value(), 3);
  auto output_view = TensorView<double, ExtentsType>::Create(
      output.data(), mapping.value(), output.size());
  ASSERT_TRUE(output_view.ok());
  const std::array<double, 3> before = output;
  const ExecutionContext context = ExecutionContext::Serial();

  Status metadata_status = Copy(context, metadata, output_view.value());
  Status byte_status = Copy(context, byte_range, output_view.value());

  ASSERT_FALSE(metadata_status.ok());
  ASSERT_FALSE(byte_status.ok());
  EXPECT_EQ(metadata_status.code(), StatusCode::kOverflow);
  EXPECT_EQ(byte_status.code(), StatusCode::kOverflow);
  EXPECT_EQ(output, before);
}

TEST(Blas1BoundaryTest, ShapeFailurePrecedesPoisonedDataAccess) {
  using InputExtents = Extents<2>;
  using OutputExtents = Extents<3>;
  using InputMapping = LayoutLeftMapping<InputExtents>;
  auto input_mapping = InputMapping::Create(InputExtents());
  auto output_mapping =
      LayoutLeftMapping<OutputExtents>::Create(OutputExtents());
  ASSERT_TRUE(input_mapping.ok());
  ASSERT_TRUE(output_mapping.ok());
  auto* poisoned_pointer = reinterpret_cast<const double*>(1);
  test::LinalgTestOperand<const double, InputMapping> input(
      poisoned_pointer, input_mapping.value(), 2);
  std::array<double, 3> output{11.0, 12.0, 13.0};
  auto output_view = TensorView<double, OutputExtents>::Create(
      output.data(), output_mapping.value(), output.size());
  ASSERT_TRUE(output_view.ok());

  Status status = Copy(ExecutionContext::Serial(), input,
                       output_view.value());

  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(output, (std::array<double, 3>{11.0, 12.0, 13.0}));
}

}  // namespace
}  // namespace asc
