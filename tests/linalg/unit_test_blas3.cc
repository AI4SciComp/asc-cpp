#include <asc/array.h>
#include <asc/linalg/blas1.h>
#include <asc/linalg/blas2.h>
#include <asc/linalg/blas3.h>

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <vector>

#include "allocation_counter.h"
#include "test_operands.h"

namespace asc {
namespace {

using FloatInputMatrixView = TensorView<const float, Extents<2, 2>>;
using FloatOutputMatrixView = TensorView<float, Extents<2, 2>>;

template <typename Alpha, typename Beta>
concept CanGemmFloatViews = requires(
    const ExecutionContext& context, Alpha alpha, Beta beta,
    const FloatInputMatrixView& input,
    const FloatOutputMatrixView& output) {
  Gemm(context, TransposeMode::kNoTranspose,
       TransposeMode::kNoTranspose, alpha, input, input, beta, output);
};

static_assert(CanGemmFloatViews<float, float>);
static_assert(!CanGemmFloatViews<double, float>);
static_assert(!CanGemmFloatViews<float, double>);

template <typename T>
class Blas3Test : public ::testing::Test {};

using FloatingTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(Blas3Test, FloatingTypes);

template <typename T, TransposeMode TransposeA, TransposeMode TransposeB>
void ExpectRectangularGoldenResult() {
  constexpr extent_t kARows =
      TransposeA == TransposeMode::kNoTranspose ? 2 : 3;
  constexpr extent_t kACols =
      TransposeA == TransposeMode::kNoTranspose ? 3 : 2;
  constexpr extent_t kBRows =
      TransposeB == TransposeMode::kNoTranspose ? 3 : 2;
  constexpr extent_t kBCols =
      TransposeB == TransposeMode::kNoTranspose ? 2 : 3;
  using AExtents = Extents<kARows, kACols>;
  using BExtents = Extents<kBRows, kBCols>;
  using CExtents = Extents<2, 2>;
  using AMapping = LayoutRightMapping<AExtents>;
  using BMapping = LayoutLeftMapping<BExtents>;
  using CMapping = LayoutRightMapping<CExtents>;
  auto a_mapping = AMapping::Create(AExtents());
  auto b_mapping = BMapping::Create(BExtents());
  auto c_mapping = CMapping::Create(CExtents());
  ASSERT_TRUE(a_mapping.ok());
  ASSERT_TRUE(b_mapping.ok());
  ASSERT_TRUE(c_mapping.ok());
  std::array<T, 6> a{};
  std::array<T, 6> b{};
  std::array<T, 4> c{T{2}, T{4}, T{6}, T{8}};
  auto a_mutable = TensorView<T, AExtents, AMapping>::Create(
      a.data(), a_mapping.value(), a.size());
  auto b_mutable = TensorView<T, BExtents, BMapping>::Create(
      b.data(), b_mapping.value(), b.size());
  auto c_view = TensorView<T, CExtents, CMapping>::Create(
      c.data(), c_mapping.value(), c.size());
  ASSERT_TRUE(a_mutable.ok());
  ASSERT_TRUE(b_mutable.ok());
  ASSERT_TRUE(c_view.ok());

  constexpr std::array<T, 6> kOpA{T{1}, T{2}, T{3}, T{4}, T{5}, T{6}};
  constexpr std::array<T, 6> kOpB{T{7}, T{8}, T{9}, T{10}, T{11}, T{12}};
  for (index_t row = 0; row < 2; ++row) {
    for (index_t inner = 0; inner < 3; ++inner) {
      if constexpr (TransposeA == TransposeMode::kNoTranspose) {
        a_mutable.value()(row, inner) = kOpA[row * 3 + inner];
      } else {
        a_mutable.value()(inner, row) = kOpA[row * 3 + inner];
      }
    }
  }
  for (index_t inner = 0; inner < 3; ++inner) {
    for (index_t column = 0; column < 2; ++column) {
      if constexpr (TransposeB == TransposeMode::kNoTranspose) {
        b_mutable.value()(inner, column) = kOpB[inner * 2 + column];
      } else {
        b_mutable.value()(column, inner) = kOpB[inner * 2 + column];
      }
    }
  }
  TensorView<const T, AExtents, AMapping> a_view = a_mutable.value();
  TensorView<const T, BExtents, BMapping> b_view = b_mutable.value();

  Status status = Gemm(ExecutionContext::Serial(), TransposeA, TransposeB,
                       T{2}, a_view, b_view, T{0.5}, c_view.value());

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(c_view.value()(0, 0), T{117});
  EXPECT_EQ(c_view.value()(0, 1), T{130});
  EXPECT_EQ(c_view.value()(1, 0), T{281});
  EXPECT_EQ(c_view.value()(1, 1), T{312});
}

TYPED_TEST(Blas3Test, SupportsEveryTransposeCombination) {
  using T = TypeParam;
  ExpectRectangularGoldenResult<T, TransposeMode::kNoTranspose,
                                TransposeMode::kNoTranspose>();
  ExpectRectangularGoldenResult<T, TransposeMode::kNoTranspose,
                                TransposeMode::kTranspose>();
  ExpectRectangularGoldenResult<T, TransposeMode::kTranspose,
                                TransposeMode::kNoTranspose>();
  ExpectRectangularGoldenResult<T, TransposeMode::kTranspose,
                                TransposeMode::kTranspose>();
}

TYPED_TEST(Blas3Test, UsesUniqueStridesAndPreservesPaddedHoles) {
  using T = TypeParam;
  using ExtentsType = Extents<2, 2>;
  using Mapping = LayoutStrideMapping<ExtentsType>;
  auto a_mapping = Mapping::Create(
      ExtentsType(), std::array<stride_t, 2>{1, 3});
  auto b_mapping = Mapping::Create(
      ExtentsType(), std::array<stride_t, 2>{3, 1});
  auto c_mapping = Mapping::Create(
      ExtentsType(), std::array<stride_t, 2>{1, 4});
  ASSERT_TRUE(a_mapping.ok());
  ASSERT_TRUE(b_mapping.ok());
  ASSERT_TRUE(c_mapping.ok());
  std::array<T, 5> a{T{1}, T{3}, T{101}, T{2}, T{4}};
  std::array<T, 5> b{T{5}, T{6}, T{102}, T{7}, T{8}};
  std::array<T, 6> c{T{0}, T{0}, T{201}, T{202}, T{0}, T{0}};
  auto a_view = TensorView<const T, ExtentsType, Mapping>::Create(
      a.data(), a_mapping.value(), a.size());
  auto b_view = TensorView<const T, ExtentsType, Mapping>::Create(
      b.data(), b_mapping.value(), b.size());
  auto c_view = TensorView<T, ExtentsType, Mapping>::Create(
      c.data(), c_mapping.value(), c.size());
  ASSERT_TRUE(a_view.ok());
  ASSERT_TRUE(b_view.ok());
  ASSERT_TRUE(c_view.ok());

  Status status = Gemm(ExecutionContext::Serial(),
                       TransposeMode::kNoTranspose,
                       TransposeMode::kNoTranspose, T{1}, a_view.value(),
                       b_view.value(), T{0}, c_view.value());

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(c, (std::array<T, 6>{T{19}, T{43}, T{201}, T{202},
                                 T{22}, T{50}}));
}

TYPED_TEST(Blas3Test, BetaZeroDoesNotReadNanDestination) {
  using T = TypeParam;
  using ExtentsType = Extents<2, 2>;
  auto mapping = LayoutLeftMapping<ExtentsType>::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::array<T, 4> a{T{1}, T{3}, T{2}, T{4}};
  std::array<T, 4> b{T{5}, T{7}, T{6}, T{8}};
  const T nan = std::numeric_limits<T>::quiet_NaN();
  std::array<T, 4> c{nan, nan, nan, nan};
  auto a_view = TensorView<const T, ExtentsType>::Create(
      a.data(), mapping.value(), a.size());
  auto b_view = TensorView<const T, ExtentsType>::Create(
      b.data(), mapping.value(), b.size());
  auto c_view = TensorView<T, ExtentsType>::Create(
      c.data(), mapping.value(), c.size());
  ASSERT_TRUE(a_view.ok());
  ASSERT_TRUE(b_view.ok());
  ASSERT_TRUE(c_view.ok());

  Status status = Gemm(ExecutionContext::Serial(),
                       TransposeMode::kNoTranspose,
                       TransposeMode::kNoTranspose, T{1}, a_view.value(),
                       b_view.value(), T{0}, c_view.value());

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(c, (std::array<T, 4>{T{19}, T{43}, T{22}, T{50}}));
}

TYPED_TEST(Blas3Test, ZeroInnerDimensionStillAppliesBeta) {
  using T = TypeParam;
  using AExtents = Extents<2, 0>;
  using BExtents = Extents<0, 3>;
  using CExtents = Extents<2, 3>;
  auto a_mapping = LayoutLeftMapping<AExtents>::Create(AExtents());
  auto b_mapping = LayoutLeftMapping<BExtents>::Create(BExtents());
  auto c_mapping = LayoutLeftMapping<CExtents>::Create(CExtents());
  ASSERT_TRUE(a_mapping.ok());
  ASSERT_TRUE(b_mapping.ok());
  ASSERT_TRUE(c_mapping.ok());
  std::array<T, 6> c{T{1}, T{2}, T{3}, T{4}, T{5}, T{6}};
  auto a_view = TensorView<const T, AExtents>::Create(
      nullptr, a_mapping.value(), 0);
  auto b_view = TensorView<const T, BExtents>::Create(
      nullptr, b_mapping.value(), 0);
  auto c_view = TensorView<T, CExtents>::Create(
      c.data(), c_mapping.value(), c.size());
  ASSERT_TRUE(a_view.ok());
  ASSERT_TRUE(b_view.ok());
  ASSERT_TRUE(c_view.ok());

  Status status = Gemm(ExecutionContext::Serial(),
                       TransposeMode::kNoTranspose,
                       TransposeMode::kNoTranspose, T{7}, a_view.value(),
                       b_view.value(), T{-2}, c_view.value());

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(c, (std::array<T, 6>{T{-2}, T{-4}, T{-6}, T{-8},
                                 T{-10}, T{-12}}));
}

TYPED_TEST(Blas3Test, ZeroOutputDimensionIsNoOp) {
  using T = TypeParam;
  using AExtents = Extents<0, 2>;
  using BExtents = Extents<2, 3>;
  using CExtents = Extents<0, 3>;
  auto a_mapping = LayoutLeftMapping<AExtents>::Create(AExtents());
  auto b_mapping = LayoutLeftMapping<BExtents>::Create(BExtents());
  auto c_mapping = LayoutLeftMapping<CExtents>::Create(CExtents());
  ASSERT_TRUE(a_mapping.ok());
  ASSERT_TRUE(b_mapping.ok());
  ASSERT_TRUE(c_mapping.ok());
  std::array<T, 6> b{T{1}, T{2}, T{3}, T{4}, T{5}, T{6}};
  auto a_view = TensorView<const T, AExtents>::Create(
      nullptr, a_mapping.value(), 0);
  auto b_view = TensorView<const T, BExtents>::Create(
      b.data(), b_mapping.value(), b.size());
  auto c_view = TensorView<T, CExtents>::Create(
      nullptr, c_mapping.value(), 0);
  ASSERT_TRUE(a_view.ok());
  ASSERT_TRUE(b_view.ok());
  ASSERT_TRUE(c_view.ok());

  EXPECT_TRUE(Gemm(ExecutionContext::Serial(),
                   TransposeMode::kNoTranspose,
                   TransposeMode::kNoTranspose, T{1}, a_view.value(),
                   b_view.value(), T{1}, c_view.value())
                  .ok());
}

TYPED_TEST(Blas3Test, RejectsInvalidModesAndShapesTransactionally) {
  using T = TypeParam;
  using AExtents = Extents<2, 3>;
  using BExtents = Extents<2, 2>;
  using CExtents = Extents<2, 2>;
  auto a_mapping = LayoutLeftMapping<AExtents>::Create(AExtents());
  auto b_mapping = LayoutLeftMapping<BExtents>::Create(BExtents());
  auto c_mapping = LayoutLeftMapping<CExtents>::Create(CExtents());
  ASSERT_TRUE(a_mapping.ok());
  ASSERT_TRUE(b_mapping.ok());
  ASSERT_TRUE(c_mapping.ok());
  std::array<T, 6> a{T{1}, T{2}, T{3}, T{4}, T{5}, T{6}};
  std::array<T, 4> b{T{1}, T{2}, T{3}, T{4}};
  std::array<T, 4> c{T{5}, T{6}, T{7}, T{8}};
  auto a_view = TensorView<const T, AExtents>::Create(
      a.data(), a_mapping.value(), a.size());
  auto b_view = TensorView<const T, BExtents>::Create(
      b.data(), b_mapping.value(), b.size());
  auto c_view = TensorView<T, CExtents>::Create(
      c.data(), c_mapping.value(), c.size());
  ASSERT_TRUE(a_view.ok());
  ASSERT_TRUE(b_view.ok());
  ASSERT_TRUE(c_view.ok());
  const std::array<T, 4> before = c;
  const ExecutionContext context = ExecutionContext::Serial();

  Status bad_shape = Gemm(context, TransposeMode::kNoTranspose,
                          TransposeMode::kNoTranspose, T{1}, a_view.value(),
                          b_view.value(), T{1}, c_view.value());
  Status bad_a_mode = Gemm(
      context, static_cast<TransposeMode>(255),
      TransposeMode::kNoTranspose, T{1}, a_view.value(), b_view.value(),
      T{1}, c_view.value());
  Status bad_b_mode = Gemm(
      context, TransposeMode::kTranspose,
      static_cast<TransposeMode>(255), T{1}, a_view.value(), b_view.value(),
      T{1}, c_view.value());
  ASSERT_FALSE(bad_shape.ok());
  ASSERT_FALSE(bad_a_mode.ok());
  ASSERT_FALSE(bad_b_mode.ok());
  EXPECT_EQ(bad_shape.code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(bad_a_mode.code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(bad_b_mode.code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(c, before);
}

TYPED_TEST(Blas3Test, RejectsOutputOverlapButAllowsInputOverlap) {
  using T = TypeParam;
  using ExtentsType = Extents<2, 2>;
  auto mapping = LayoutLeftMapping<ExtentsType>::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::array<T, 12> storage{T{1}, T{3}, T{2}, T{4}, T{5}, T{7},
                            T{6}, T{8}, T{0}, T{0}, T{0}, T{0}};
  auto a_view = TensorView<const T, ExtentsType>::Create(
      storage.data(), mapping.value(), 4);
  auto b_view = TensorView<const T, ExtentsType>::Create(
      storage.data() + 4, mapping.value(), 4);
  auto c_overlaps_a = TensorView<T, ExtentsType>::Create(
      storage.data() + 1, mapping.value(), 4);
  auto c_overlaps_b = TensorView<T, ExtentsType>::Create(
      storage.data() + 5, mapping.value(), 4);
  auto c_disjoint = TensorView<T, ExtentsType>::Create(
      storage.data() + 8, mapping.value(), 4);
  ASSERT_TRUE(a_view.ok());
  ASSERT_TRUE(b_view.ok());
  ASSERT_TRUE(c_overlaps_a.ok());
  ASSERT_TRUE(c_overlaps_b.ok());
  ASSERT_TRUE(c_disjoint.ok());
  const std::array<T, 12> before = storage;
  const ExecutionContext context = ExecutionContext::Serial();

  Status overlap_a = Gemm(
      context, TransposeMode::kNoTranspose,
      TransposeMode::kNoTranspose, T{1}, a_view.value(), b_view.value(),
      T{0}, c_overlaps_a.value());
  ASSERT_FALSE(overlap_a.ok());
  EXPECT_EQ(overlap_a.code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(storage, before);

  Status overlap_b = Gemm(
      context, TransposeMode::kNoTranspose,
      TransposeMode::kNoTranspose, T{1}, a_view.value(), b_view.value(),
      T{0}, c_overlaps_b.value());
  ASSERT_FALSE(overlap_b.ok());
  EXPECT_EQ(overlap_b.code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(storage, before);

  Status shared_inputs = Gemm(
      context, TransposeMode::kNoTranspose,
      TransposeMode::kNoTranspose, T{1}, a_view.value(), a_view.value(),
      T{0}, c_disjoint.value());
  ASSERT_TRUE(shared_inputs.ok());
  EXPECT_EQ(storage[8], T{7});
  EXPECT_EQ(storage[9], T{15});
  EXPECT_EQ(storage[10], T{10});
  EXPECT_EQ(storage[11], T{22});
}

TYPED_TEST(Blas3Test, ModerateIdentityProductNeedsNoOutputAllocation) {
  using T = TypeParam;
  constexpr extent_t kSize = 24;
  using ExtentsType = Extents<kSize, kSize>;
  auto mapping = LayoutLeftMapping<ExtentsType>::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::vector<T> identity(kSize * kSize, T{0});
  std::vector<T> input(kSize * kSize, T{0});
  std::vector<T> output(kSize * kSize, T{-1});
  std::vector<T> vector_input(kSize, T{2});
  std::vector<T> vector_output(kSize, T{3});
  for (index_t column = 0; column < kSize; ++column) {
    for (index_t row = 0; row < kSize; ++row) {
      identity[mapping.value()(row, column)] = row == column ? T{1} : T{0};
      input[mapping.value()(row, column)] =
          static_cast<T>(row - 2 * column);
    }
  }
  auto identity_view = TensorView<const T, ExtentsType>::Create(
      identity.data(), mapping.value(), identity.size());
  auto input_view = TensorView<const T, ExtentsType>::Create(
      input.data(), mapping.value(), input.size());
  auto output_view = TensorView<T, ExtentsType>::Create(
      output.data(), mapping.value(), output.size());
  using VectorExtents = Extents<kSize>;
  auto vector_mapping =
      LayoutLeftMapping<VectorExtents>::Create(VectorExtents());
  ASSERT_TRUE(vector_mapping.ok());
  auto vector_input_view = TensorView<const T, VectorExtents>::Create(
      vector_input.data(), vector_mapping.value(), vector_input.size());
  auto vector_output_view = TensorView<T, VectorExtents>::Create(
      vector_output.data(), vector_mapping.value(), vector_output.size());
  ASSERT_TRUE(identity_view.ok());
  ASSERT_TRUE(input_view.ok());
  ASSERT_TRUE(output_view.ok());
  ASSERT_TRUE(vector_input_view.ok());
  ASSERT_TRUE(vector_output_view.ok());
  T* const output_pointer = output_view.value().Data();

  const ExecutionContext context = ExecutionContext::Serial();
  test::BeginAllocationCount();
  Status copy = Copy(context, vector_input_view.value(),
                     vector_output_view.value());
  Status scale = Scal(context, T{2}, vector_output_view.value());
  Status axpy = Axpy(context, T{-1}, vector_input_view.value(),
                     vector_output_view.value());
  Result<T> dot = Dot(context, vector_input_view.value(),
                      vector_output_view.value());
  Result<T> norm = Nrm2(context, vector_input_view.value());
  Status gemv = Gemv(context, TransposeMode::kNoTranspose, T{1},
                     identity_view.value(), vector_input_view.value(), T{0},
                     vector_output_view.value());
  Status gemm = Gemm(context, TransposeMode::kNoTranspose,
                     TransposeMode::kNoTranspose, T{1},
                     identity_view.value(), input_view.value(), T{0},
                     output_view.value());
  const std::size_t allocation_count = test::EndAllocationCount();

  ASSERT_TRUE(copy.ok());
  ASSERT_TRUE(scale.ok());
  ASSERT_TRUE(axpy.ok());
  ASSERT_TRUE(dot.ok());
  ASSERT_TRUE(norm.ok());
  ASSERT_TRUE(gemv.ok());
  ASSERT_TRUE(gemm.ok());
  EXPECT_EQ(allocation_count, 0U);
  EXPECT_EQ(output_view.value().Data(), output_pointer);
  EXPECT_EQ(output, input);
}

TEST(Blas3BoundaryTest, RevalidatesForeignOperandsTransactionally) {
  using ExtentsType = Extents<2, 2>;
  using Mapping = LayoutLeftMapping<ExtentsType>;
  using NonUniqueMapping = LayoutStrideMapping<ExtentsType>;
  auto mapping = Mapping::Create(ExtentsType());
  auto non_unique_mapping = NonUniqueMapping::Create(
      ExtentsType(), std::array<stride_t, 2>{0, 1});
  ASSERT_TRUE(mapping.ok());
  ASSERT_TRUE(non_unique_mapping.ok());
  std::array<double, 4> a{1.0, 3.0, 2.0, 4.0};
  std::array<double, 4> b{5.0, 7.0, 6.0, 8.0};
  std::array<double, 4> c{9.0, 10.0, 11.0, 12.0};
  test::LinalgTestOperand<const double, Mapping> inaccessible(
      a.data(), mapping.value(), a.size(), MemorySpace::kDevice);
  test::LinalgTestOperand<const double, Mapping> insufficient(
      a.data(), mapping.value(), 3);
  test::LinalgTestOperand<double, NonUniqueMapping> non_unique(
      c.data(), non_unique_mapping.value(),
      non_unique_mapping.value().GetRequiredSpan());
  auto a_view = TensorView<const double, ExtentsType>::Create(
      a.data(), mapping.value(), a.size());
  auto b_view = TensorView<const double, ExtentsType>::Create(
      b.data(), mapping.value(), b.size());
  auto c_view = TensorView<double, ExtentsType>::Create(
      c.data(), mapping.value(), c.size());
  ASSERT_TRUE(a_view.ok());
  ASSERT_TRUE(b_view.ok());
  ASSERT_TRUE(c_view.ok());
  static_assert(WritableLinalgMatrix<decltype(non_unique)>);
  const std::array<double, 4> before = c;
  const ExecutionContext context = ExecutionContext::Serial();

  Status accessibility = Gemm(
      context, TransposeMode::kNoTranspose,
      TransposeMode::kNoTranspose, 1.0, inaccessible, b_view.value(), 0.0,
      c_view.value());
  Status span = Gemm(context, TransposeMode::kNoTranspose,
                     TransposeMode::kNoTranspose, 1.0, insufficient,
                     b_view.value(), 0.0, c_view.value());
  Status uniqueness = Gemm(
      context, TransposeMode::kNoTranspose,
      TransposeMode::kNoTranspose, 1.0, a_view.value(), b_view.value(), 0.0,
      non_unique);

  ASSERT_FALSE(accessibility.ok());
  ASSERT_FALSE(span.ok());
  ASSERT_FALSE(uniqueness.ok());
  EXPECT_EQ(accessibility.code(), StatusCode::kUnsupported);
  EXPECT_EQ(span.code(), StatusCode::kOverflow);
  EXPECT_EQ(uniqueness.code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(c, before);
}

}  // namespace
}  // namespace asc
