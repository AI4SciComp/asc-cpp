#include <asc/array.h>
#include <asc/linalg/blas2.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cmath>
#include <limits>

#include "test_operands.h"

namespace asc {
namespace {

using FloatMatrixView = TensorView<const float, Extents<2, 2>>;
using FloatInputView = TensorView<const float, Extents<2>>;
using FloatOutputView = TensorView<float, Extents<2>>;

template <typename Alpha, typename Beta>
concept CanGemvFloatViews = requires(
    const ExecutionContext& context, Alpha alpha, Beta beta,
    const FloatMatrixView& matrix, const FloatInputView& input,
    const FloatOutputView& output) {
  Gemv(context, TransposeMode::kNoTranspose, alpha, matrix, input, beta,
       output);
};

static_assert(CanGemvFloatViews<float, float>);
static_assert(!CanGemvFloatViews<double, float>);
static_assert(!CanGemvFloatViews<float, double>);

template <typename T>
class Blas2Test : public ::testing::Test {};

using FloatingTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(Blas2Test, FloatingTypes);

template <typename T>
constexpr T TestTolerance() {
  return T{64} * std::numeric_limits<T>::epsilon();
}

TYPED_TEST(Blas2Test, ComputesNoTransposeRectangularGoldenResult) {
  using T = TypeParam;
  using MatrixExtents = Extents<2, 3>;
  using InputExtents = Extents<3>;
  using OutputExtents = Extents<2>;
  auto matrix_mapping =
      LayoutLeftMapping<MatrixExtents>::Create(MatrixExtents());
  auto input_mapping =
      LayoutLeftMapping<InputExtents>::Create(InputExtents());
  auto output_mapping =
      LayoutLeftMapping<OutputExtents>::Create(OutputExtents());
  ASSERT_TRUE(matrix_mapping.ok());
  ASSERT_TRUE(input_mapping.ok());
  ASSERT_TRUE(output_mapping.ok());
  std::array<T, 6> matrix{T{1}, T{4}, T{2}, T{5}, T{3}, T{6}};
  std::array<T, 3> input{T{1}, T{-1}, T{2}};
  std::array<T, 2> output{T{10}, T{20}};
  auto matrix_view = TensorView<const T, MatrixExtents>::Create(
      matrix.data(), matrix_mapping.value(), matrix.size());
  auto input_view = TensorView<const T, InputExtents>::Create(
      input.data(), input_mapping.value(), input.size());
  auto output_view = TensorView<T, OutputExtents>::Create(
      output.data(), output_mapping.value(), output.size());
  ASSERT_TRUE(matrix_view.ok());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_view.ok());

  Status status = Gemv(ExecutionContext::Serial(),
                       TransposeMode::kNoTranspose, T{2},
                       matrix_view.value(), input_view.value(), T{0.5},
                       output_view.value());

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(output, (std::array<T, 2>{T{15}, T{32}}));
}

TYPED_TEST(Blas2Test, ComputesTransposeRectangularGoldenResult) {
  using T = TypeParam;
  using MatrixExtents = Extents<2, 3>;
  using InputExtents = Extents<2>;
  using OutputExtents = Extents<3>;
  using MatrixMapping = LayoutRightMapping<MatrixExtents>;
  using InputMapping = LayoutRightMapping<InputExtents>;
  using OutputMapping = LayoutRightMapping<OutputExtents>;
  auto matrix_mapping = MatrixMapping::Create(MatrixExtents());
  auto input_mapping = InputMapping::Create(InputExtents());
  auto output_mapping = OutputMapping::Create(OutputExtents());
  ASSERT_TRUE(matrix_mapping.ok());
  ASSERT_TRUE(input_mapping.ok());
  ASSERT_TRUE(output_mapping.ok());
  std::array<T, 6> matrix{T{1}, T{2}, T{3}, T{4}, T{5}, T{6}};
  std::array<T, 2> input{T{2}, T{-1}};
  std::array<T, 3> output{T{1}, T{2}, T{3}};
  auto matrix_view = TensorView<const T, MatrixExtents, MatrixMapping>::Create(
      matrix.data(), matrix_mapping.value(), matrix.size());
  auto input_view = TensorView<const T, InputExtents, InputMapping>::Create(
      input.data(), input_mapping.value(), input.size());
  auto output_view = TensorView<T, OutputExtents, OutputMapping>::Create(
      output.data(), output_mapping.value(), output.size());
  ASSERT_TRUE(matrix_view.ok());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_view.ok());

  Status status = Gemv(ExecutionContext::Serial(), TransposeMode::kTranspose,
                       T{1}, matrix_view.value(), input_view.value(), T{2},
                       output_view.value());

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(output, (std::array<T, 3>{T{0}, T{3}, T{6}}));
}

TYPED_TEST(Blas2Test, UsesUniqueStridesAndPreservesPaddedHoles) {
  using T = TypeParam;
  using MatrixExtents = Extents<2, 3>;
  using VectorExtents = Extents<3>;
  using OutputExtents = Extents<2>;
  using MatrixMapping = LayoutStrideMapping<MatrixExtents>;
  using VectorMapping = LayoutStrideMapping<VectorExtents>;
  using OutputMapping = LayoutStrideMapping<OutputExtents>;
  auto matrix_mapping = MatrixMapping::Create(
      MatrixExtents(), std::array<stride_t, 2>{1, 4});
  auto vector_mapping = VectorMapping::Create(
      VectorExtents(), std::array<stride_t, 1>{2});
  auto output_mapping = OutputMapping::Create(
      OutputExtents(), std::array<stride_t, 1>{3});
  ASSERT_TRUE(matrix_mapping.ok());
  ASSERT_TRUE(vector_mapping.ok());
  ASSERT_TRUE(output_mapping.ok());
  std::array<T, 10> matrix{T{1}, T{4}, T{101}, T{102}, T{2},
                           T{5}, T{105}, T{106}, T{3}, T{6}};
  std::array<T, 5> input{T{1}, T{201}, T{-1}, T{203}, T{2}};
  std::array<T, 4> output{T{10}, T{301}, T{302}, T{20}};
  auto matrix_view = TensorView<const T, MatrixExtents, MatrixMapping>::Create(
      matrix.data(), matrix_mapping.value(), matrix.size());
  auto input_view = TensorView<const T, VectorExtents, VectorMapping>::Create(
      input.data(), vector_mapping.value(), input.size());
  auto output_view = TensorView<T, OutputExtents, OutputMapping>::Create(
      output.data(), output_mapping.value(), output.size());
  ASSERT_TRUE(matrix_view.ok());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_view.ok());

  Status status = Gemv(ExecutionContext::Serial(),
                       TransposeMode::kNoTranspose, T{1},
                       matrix_view.value(), input_view.value(), T{1},
                       output_view.value());

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(output,
            (std::array<T, 4>{T{15}, T{301}, T{302}, T{31}}));
}

TYPED_TEST(Blas2Test, BetaZeroDoesNotReadNanDestination) {
  using T = TypeParam;
  using MatrixExtents = Extents<2, 2>;
  using VectorExtents = Extents<2>;
  auto matrix_mapping =
      LayoutLeftMapping<MatrixExtents>::Create(MatrixExtents());
  auto vector_mapping =
      LayoutLeftMapping<VectorExtents>::Create(VectorExtents());
  ASSERT_TRUE(matrix_mapping.ok());
  ASSERT_TRUE(vector_mapping.ok());
  std::array<T, 4> matrix{T{2}, T{0}, T{0}, T{3}};
  std::array<T, 2> input{T{4}, T{5}};
  std::array<T, 2> output{
      std::numeric_limits<T>::quiet_NaN(),
      std::numeric_limits<T>::quiet_NaN()};
  auto matrix_view = TensorView<const T, MatrixExtents>::Create(
      matrix.data(), matrix_mapping.value(), matrix.size());
  auto input_view = TensorView<const T, VectorExtents>::Create(
      input.data(), vector_mapping.value(), input.size());
  auto output_view = TensorView<T, VectorExtents>::Create(
      output.data(), vector_mapping.value(), output.size());
  ASSERT_TRUE(matrix_view.ok());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_view.ok());

  Status status = Gemv(ExecutionContext::Serial(),
                       TransposeMode::kNoTranspose, T{1},
                       matrix_view.value(), input_view.value(), T{0},
                       output_view.value());

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(output, (std::array<T, 2>{T{8}, T{15}}));
}

TYPED_TEST(Blas2Test, ZeroInnerDimensionStillAppliesBeta) {
  using T = TypeParam;
  using MatrixExtents = Extents<2, 0>;
  using InputExtents = Extents<0>;
  using OutputExtents = Extents<2>;
  auto matrix_mapping =
      LayoutLeftMapping<MatrixExtents>::Create(MatrixExtents());
  auto input_mapping =
      LayoutLeftMapping<InputExtents>::Create(InputExtents());
  auto output_mapping =
      LayoutLeftMapping<OutputExtents>::Create(OutputExtents());
  ASSERT_TRUE(matrix_mapping.ok());
  ASSERT_TRUE(input_mapping.ok());
  ASSERT_TRUE(output_mapping.ok());
  std::array<T, 2> output{T{2}, T{-3}};
  auto matrix_view = TensorView<const T, MatrixExtents>::Create(
      nullptr, matrix_mapping.value(), 0);
  auto input_view = TensorView<const T, InputExtents>::Create(
      nullptr, input_mapping.value(), 0);
  auto output_view = TensorView<T, OutputExtents>::Create(
      output.data(), output_mapping.value(), output.size());
  ASSERT_TRUE(matrix_view.ok());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_view.ok());

  Status status = Gemv(ExecutionContext::Serial(),
                       TransposeMode::kNoTranspose, T{7},
                       matrix_view.value(), input_view.value(), T{3},
                       output_view.value());

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(output, (std::array<T, 2>{T{6}, T{-9}}));
}

TYPED_TEST(Blas2Test, ZeroOutputDimensionIsNoOp) {
  using T = TypeParam;
  using MatrixExtents = Extents<0, 3>;
  using InputExtents = Extents<3>;
  using OutputExtents = Extents<0>;
  auto matrix_mapping =
      LayoutLeftMapping<MatrixExtents>::Create(MatrixExtents());
  auto input_mapping =
      LayoutLeftMapping<InputExtents>::Create(InputExtents());
  auto output_mapping =
      LayoutLeftMapping<OutputExtents>::Create(OutputExtents());
  ASSERT_TRUE(matrix_mapping.ok());
  ASSERT_TRUE(input_mapping.ok());
  ASSERT_TRUE(output_mapping.ok());
  std::array<T, 3> input{T{1}, T{2}, T{3}};
  auto matrix_view = TensorView<const T, MatrixExtents>::Create(
      nullptr, matrix_mapping.value(), 0);
  auto input_view = TensorView<const T, InputExtents>::Create(
      input.data(), input_mapping.value(), input.size());
  auto output_view = TensorView<T, OutputExtents>::Create(
      nullptr, output_mapping.value(), 0);
  ASSERT_TRUE(matrix_view.ok());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_view.ok());

  EXPECT_TRUE(Gemv(ExecutionContext::Serial(),
                   TransposeMode::kNoTranspose, T{1}, matrix_view.value(),
                   input_view.value(), T{1}, output_view.value())
                  .ok());
}

TYPED_TEST(Blas2Test, RejectsInvalidModeAndShapeWithoutMutation) {
  using T = TypeParam;
  using MatrixExtents = Extents<2, 3>;
  using WrongInputExtents = Extents<2>;
  using OutputExtents = Extents<2>;
  auto matrix_mapping =
      LayoutLeftMapping<MatrixExtents>::Create(MatrixExtents());
  auto input_mapping =
      LayoutLeftMapping<WrongInputExtents>::Create(WrongInputExtents());
  auto output_mapping =
      LayoutLeftMapping<OutputExtents>::Create(OutputExtents());
  ASSERT_TRUE(matrix_mapping.ok());
  ASSERT_TRUE(input_mapping.ok());
  ASSERT_TRUE(output_mapping.ok());
  std::array<T, 6> matrix{T{1}, T{2}, T{3}, T{4}, T{5}, T{6}};
  std::array<T, 2> input{T{1}, T{2}};
  std::array<T, 2> output{T{7}, T{8}};
  auto matrix_view = TensorView<const T, MatrixExtents>::Create(
      matrix.data(), matrix_mapping.value(), matrix.size());
  auto input_view = TensorView<const T, WrongInputExtents>::Create(
      input.data(), input_mapping.value(), input.size());
  auto output_view = TensorView<T, OutputExtents>::Create(
      output.data(), output_mapping.value(), output.size());
  ASSERT_TRUE(matrix_view.ok());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_view.ok());
  const std::array<T, 2> before = output;
  const ExecutionContext context = ExecutionContext::Serial();

  Status bad_shape = Gemv(context, TransposeMode::kNoTranspose, T{1},
                          matrix_view.value(), input_view.value(), T{1},
                          output_view.value());
  Status bad_mode = Gemv(context, static_cast<TransposeMode>(255), T{1},
                         matrix_view.value(), input_view.value(), T{1},
                         output_view.value());
  ASSERT_FALSE(bad_shape.ok());
  ASSERT_FALSE(bad_mode.ok());
  EXPECT_EQ(bad_shape.code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(bad_mode.code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(output, before);
}

TYPED_TEST(Blas2Test, RejectsOutputOverlapWithInputsTransactionally) {
  using T = TypeParam;
  using MatrixExtents = Extents<2, 2>;
  using VectorExtents = Extents<2>;
  auto matrix_mapping =
      LayoutLeftMapping<MatrixExtents>::Create(MatrixExtents());
  auto vector_mapping =
      LayoutLeftMapping<VectorExtents>::Create(VectorExtents());
  ASSERT_TRUE(matrix_mapping.ok());
  ASSERT_TRUE(vector_mapping.ok());
  std::array<T, 6> storage{T{2}, T{0}, T{0}, T{3}, T{4}, T{5}};
  auto matrix_view = TensorView<const T, MatrixExtents>::Create(
      storage.data(), matrix_mapping.value(), 4);
  auto input_view = TensorView<const T, VectorExtents>::Create(
      storage.data() + 4, vector_mapping.value(), 2);
  auto output_overlaps_matrix = TensorView<T, VectorExtents>::Create(
      storage.data() + 1, vector_mapping.value(), 2);
  auto output_overlaps_input = TensorView<T, VectorExtents>::Create(
      storage.data() + 4, vector_mapping.value(), 2);
  ASSERT_TRUE(matrix_view.ok());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_overlaps_matrix.ok());
  ASSERT_TRUE(output_overlaps_input.ok());
  const std::array<T, 6> before = storage;
  const ExecutionContext context = ExecutionContext::Serial();

  Status matrix_overlap = Gemv(
      context, TransposeMode::kNoTranspose, T{1}, matrix_view.value(),
      input_view.value(), T{0}, output_overlaps_matrix.value());
  ASSERT_FALSE(matrix_overlap.ok());
  EXPECT_EQ(matrix_overlap.code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(storage, before);

  Status input_overlap = Gemv(
      context, TransposeMode::kNoTranspose, T{1}, matrix_view.value(),
      input_view.value(), T{0}, output_overlaps_input.value());
  ASSERT_FALSE(input_overlap.ok());
  EXPECT_EQ(input_overlap.code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(storage, before);
}

TYPED_TEST(Blas2Test, IdentityPropertyAndRepeatedOrderAreDeterministic) {
  using T = TypeParam;
  using MatrixExtents = Extents<3, 3>;
  using VectorExtents = Extents<3>;
  auto matrix_mapping =
      LayoutLeftMapping<MatrixExtents>::Create(MatrixExtents());
  auto vector_mapping =
      LayoutLeftMapping<VectorExtents>::Create(VectorExtents());
  ASSERT_TRUE(matrix_mapping.ok());
  ASSERT_TRUE(vector_mapping.ok());
  std::array<T, 9> identity{T{1}, T{0}, T{0}, T{0}, T{1},
                            T{0}, T{0}, T{0}, T{1}};
  std::array<T, 3> input{T{1.25}, T{-2.5}, T{3.75}};
  std::array<T, 3> first{};
  std::array<T, 3> second{};
  auto matrix_view = TensorView<const T, MatrixExtents>::Create(
      identity.data(), matrix_mapping.value(), identity.size());
  auto input_view = TensorView<const T, VectorExtents>::Create(
      input.data(), vector_mapping.value(), input.size());
  auto first_view = TensorView<T, VectorExtents>::Create(
      first.data(), vector_mapping.value(), first.size());
  auto second_view = TensorView<T, VectorExtents>::Create(
      second.data(), vector_mapping.value(), second.size());
  ASSERT_TRUE(matrix_view.ok());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(first_view.ok());
  ASSERT_TRUE(second_view.ok());
  const ExecutionContext context = ExecutionContext::Serial();

  ASSERT_TRUE(Gemv(context, TransposeMode::kNoTranspose, T{1},
                   matrix_view.value(), input_view.value(), T{0},
                   first_view.value())
                  .ok());
  ASSERT_TRUE(Gemv(context, TransposeMode::kNoTranspose, T{1},
                   matrix_view.value(), input_view.value(), T{0},
                   second_view.value())
                  .ok());
  EXPECT_EQ(first, input);
  EXPECT_EQ(second, first);
  for (std::size_t index = 0; index < first.size(); ++index) {
    EXPECT_NEAR(first[index], input[index], TestTolerance<T>());
  }
}

TEST(Blas2BoundaryTest, RevalidatesForeignOperandsTransactionally) {
  using MatrixExtents = Extents<2, 2>;
  using VectorExtents = Extents<2>;
  using MatrixMapping = LayoutLeftMapping<MatrixExtents>;
  using VectorMapping = LayoutLeftMapping<VectorExtents>;
  using NonUniqueMapping = LayoutStrideMapping<VectorExtents>;
  auto matrix_mapping = MatrixMapping::Create(MatrixExtents());
  auto vector_mapping = VectorMapping::Create(VectorExtents());
  auto non_unique_mapping = NonUniqueMapping::Create(
      VectorExtents(), std::array<stride_t, 1>{0});
  ASSERT_TRUE(matrix_mapping.ok());
  ASSERT_TRUE(vector_mapping.ok());
  ASSERT_TRUE(non_unique_mapping.ok());
  std::array<double, 4> matrix{2.0, 0.0, 0.0, 3.0};
  std::array<double, 2> input{4.0, 5.0};
  std::array<double, 2> output{7.0, 8.0};
  test::LinalgTestOperand<const double, MatrixMapping> inaccessible(
      matrix.data(), matrix_mapping.value(), matrix.size(),
      MemorySpace::kDevice);
  test::LinalgTestOperand<const double, MatrixMapping> insufficient(
      matrix.data(), matrix_mapping.value(), 3);
  test::LinalgTestOperand<double, NonUniqueMapping> non_unique(
      output.data(), non_unique_mapping.value(), 1);
  auto matrix_view = TensorView<const double, MatrixExtents>::Create(
      matrix.data(), matrix_mapping.value(), matrix.size());
  auto input_view = TensorView<const double, VectorExtents>::Create(
      input.data(), vector_mapping.value(), input.size());
  auto output_view = TensorView<double, VectorExtents>::Create(
      output.data(), vector_mapping.value(), output.size());
  ASSERT_TRUE(matrix_view.ok());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_view.ok());
  static_assert(WritableLinalgVector<decltype(non_unique)>);
  const std::array<double, 2> before = output;
  const ExecutionContext context = ExecutionContext::Serial();

  Status accessibility = Gemv(
      context, TransposeMode::kNoTranspose, 1.0, inaccessible,
      input_view.value(), 0.0, output_view.value());
  Status span = Gemv(context, TransposeMode::kNoTranspose, 1.0, insufficient,
                     input_view.value(), 0.0, output_view.value());
  Status uniqueness = Gemv(
      context, TransposeMode::kNoTranspose, 1.0, matrix_view.value(),
      input_view.value(), 0.0, non_unique);

  ASSERT_FALSE(accessibility.ok());
  ASSERT_FALSE(span.ok());
  ASSERT_FALSE(uniqueness.ok());
  EXPECT_EQ(accessibility.code(), StatusCode::kUnsupported);
  EXPECT_EQ(span.code(), StatusCode::kOverflow);
  EXPECT_EQ(uniqueness.code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(output, before);
}

TEST(Blas2BoundaryTest, AliasRangeOverflowFailsBeforePointerAccess) {
  using MatrixExtents = Extents<2, 2>;
  using VectorExtents = Extents<2>;
  using MatrixMapping = LayoutLeftMapping<MatrixExtents>;
  auto matrix_mapping = MatrixMapping::Create(MatrixExtents());
  auto vector_mapping =
      LayoutLeftMapping<VectorExtents>::Create(VectorExtents());
  ASSERT_TRUE(matrix_mapping.ok());
  ASSERT_TRUE(vector_mapping.ok());
  const auto address = std::numeric_limits<std::uintptr_t>::max() - 1;
  auto* invalid_pointer = reinterpret_cast<const double*>(address);
  test::LinalgTestOperand<const double, MatrixMapping> matrix(
      invalid_pointer, matrix_mapping.value(), 4);
  std::array<double, 2> input{1.0, 2.0};
  std::array<double, 2> output{3.0, 4.0};
  auto input_view = TensorView<const double, VectorExtents>::Create(
      input.data(), vector_mapping.value(), input.size());
  auto output_view = TensorView<double, VectorExtents>::Create(
      output.data(), vector_mapping.value(), output.size());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_view.ok());

  Status status = Gemv(
      ExecutionContext::Serial(), TransposeMode::kNoTranspose, 1.0, matrix,
      input_view.value(), 0.0, output_view.value());

  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kOverflow);
  EXPECT_EQ(output, (std::array<double, 2>{3.0, 4.0}));
}

}  // namespace
}  // namespace asc
