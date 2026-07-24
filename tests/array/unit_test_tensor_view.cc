#include <asc/array/tensor_concepts.h>
#include <asc/array/tensor_view.h>

#include <gtest/gtest.h>

#include <concepts>
#include <array>
#include <type_traits>
#include <utility>

namespace asc {
namespace {

using MatrixExtents = Extents<2, 3>;
using MatrixMapping = LayoutLeftMapping<MatrixExtents>;
using MutableMatrixView = TensorView<int, MatrixExtents>;
using ConstMatrixView = TensorView<const int, MatrixExtents>;

struct ThirdPartyWritableMatrix {
  using ElementType = double;
  using ValueType = double;
  using ExtentsType = MatrixExtents;
  using MappingType = MatrixMapping;
  using DataHandle = double*;

  static constexpr std::size_t Rank() noexcept { return 2; }
  const ExtentsType& GetExtents() const;
  const MappingType& GetMapping() const;
  extent_t GetExtent(std::size_t dimension) const;
  stride_t GetStride(std::size_t dimension) const;
  extent_t GetSize() const;
  DataHandle Data() const;
};

static_assert(std::is_empty_v<DefaultAccessor<int>>);
static_assert(std::is_trivially_copyable_v<DefaultAccessor<int>>);
static_assert(std::is_trivially_copyable_v<MutableMatrixView>);
static_assert(std::is_trivially_copyable_v<ConstMatrixView>);
static_assert(std::is_convertible_v<MutableMatrixView, ConstMatrixView>);
static_assert(!std::is_convertible_v<ConstMatrixView, MutableMatrixView>);
static_assert(!std::is_convertible_v<MutableMatrixView, int*>);
static_assert(!std::is_convertible_v<ConstMatrixView, const int*>);
static_assert(std::same_as<
              decltype(std::declval<const MutableMatrixView&>()(0, 0)),
              int&>);
static_assert(std::same_as<
              decltype(std::declval<const ConstMatrixView&>()(0, 0)),
              const int&>);
static_assert(TensorMapping<MatrixMapping>);
static_assert(TensorDescriptor<ThirdPartyWritableMatrix>);
static_assert(ReadableTensor<ThirdPartyWritableMatrix>);
static_assert(WritableTensor<ThirdPartyWritableMatrix>);
static_assert(ContiguousTensor<ThirdPartyWritableMatrix>);
static_assert(Matrix<ThirdPartyWritableMatrix>);
static_assert(!Vector<ThirdPartyWritableMatrix>);
static_assert(Vector<TensorView<int, Extents<3>>>);
static_assert(!Matrix<TensorView<int, Extents<3>>>);
static_assert(!Vector<TensorView<int, Extents<>>>);
static_assert(!Matrix<TensorView<int, Extents<>>>);
static_assert(ReadableTensor<ConstMatrixView>);
static_assert(!WritableTensor<ConstMatrixView>);

TEST(TensorViewTest, DefaultAccessorUsesDirectPointerSemantics) {
  std::array<int, 4> data{2, 3, 5, 7};
  const DefaultAccessor<int> accessor;

  EXPECT_EQ(accessor.Access(data.data(), 2), 5);
  accessor.Access(data.data(), 2) = 11;
  EXPECT_EQ(data[2], 11);
  EXPECT_EQ(accessor.Offset(data.data(), 3), data.data() + 3);

  const DefaultAccessor<const int> const_accessor(accessor);
  EXPECT_EQ(const_accessor.Access(data.data(), 2), 11);
}

TEST(TensorViewTest, ExposesDescriptorAndExternalStorage) {
  Result<MatrixMapping> mapping =
      MatrixMapping::Create(MatrixExtents());
  ASSERT_TRUE(mapping.ok());
  std::array<int, 6> data{0, 1, 2, 3, 4, 5};
  Result<MutableMatrixView> result = MutableMatrixView::Create(
      data.data(), mapping.value(), data.size());

  ASSERT_TRUE(result.ok());
  const MutableMatrixView view = result.value();
  EXPECT_EQ(view.Data(), data.data());
  EXPECT_EQ(view.GetExtents(), MatrixExtents());
  EXPECT_EQ(view.GetExtent(0), 2);
  EXPECT_EQ(view.GetExtent(1), 3);
  EXPECT_EQ(view.GetStride(0), 1);
  EXPECT_EQ(view.GetStride(1), 2);
  EXPECT_EQ(view.GetSize(), 6);
  EXPECT_EQ(view.GetRequiredSpan(), 6);
  EXPECT_EQ(view.GetAvailableSpan(), 6);
  EXPECT_EQ(view.GetMemorySpace(), MemorySpace::kHost);
  EXPECT_TRUE(view.IsContiguous());
  EXPECT_EQ(view(1, 2), 5);
  EXPECT_EQ(view.UncheckedAt(1, 1), 3);

  view(1, 2) = 17;
  EXPECT_EQ(data[5], 17);
}

TEST(TensorViewTest, ConstViewHandleRetainsMutableElementAuthority) {
  Result<MatrixMapping> mapping =
      MatrixMapping::Create(MatrixExtents());
  ASSERT_TRUE(mapping.ok());
  std::array<int, 6> data{};
  Result<MutableMatrixView> result = MutableMatrixView::Create(
      data.data(), mapping.value(), data.size());
  ASSERT_TRUE(result.ok());

  const MutableMatrixView view = result.value();
  view(1, 1) = 23;
  EXPECT_EQ(data[3], 23);
}

TEST(TensorViewTest, ConvertsMutableElementsToConstElements) {
  Result<MatrixMapping> mapping =
      MatrixMapping::Create(MatrixExtents());
  ASSERT_TRUE(mapping.ok());
  std::array<int, 6> data{0, 1, 2, 3, 4, 5};
  Result<MutableMatrixView> mutable_result = MutableMatrixView::Create(
      data.data(), mapping.value(), data.size());
  ASSERT_TRUE(mutable_result.ok());

  ConstMatrixView const_view = mutable_result.value();
  EXPECT_EQ(const_view.Data(), data.data());
  EXPECT_EQ(const_view.GetAvailableSpan(), 6);
  EXPECT_EQ(const_view.GetMemorySpace(), MemorySpace::kHost);
  EXPECT_EQ(const_view(1, 2), 5);

  mutable_result.value()(1, 2) = 29;
  EXPECT_EQ(const_view(1, 2), 29);
}

TEST(TensorViewTest, AllowsNonUniqueConstElementBroadcastView) {
  using StrideView = TensorView<const int, MatrixExtents,
                                LayoutStrideMapping<MatrixExtents>>;
  Result<LayoutStrideMapping<MatrixExtents>> mapping =
      LayoutStrideMapping<MatrixExtents>::Create(
          MatrixExtents(), std::array<stride_t, 2>{0, 1});
  ASSERT_TRUE(mapping.ok());
  ASSERT_FALSE(mapping.value().IsUnique());
  std::array<int, 3> data{5, 7, 11};
  Result<StrideView> result = StrideView::Create(
      data.data(), mapping.value(), data.size());

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value()(0, 0), 5);
  EXPECT_EQ(result.value()(1, 0), 5);
  EXPECT_EQ(result.value()(0, 2), 11);
  EXPECT_EQ(result.value()(1, 2), 11);
  EXPECT_FALSE(result.value().IsContiguous());
}

TEST(TensorViewTest, RejectsNonUniqueMutableElementView) {
  using StrideView = TensorView<int, MatrixExtents,
                                LayoutStrideMapping<MatrixExtents>>;
  Result<LayoutStrideMapping<MatrixExtents>> mapping =
      LayoutStrideMapping<MatrixExtents>::Create(
          MatrixExtents(), std::array<stride_t, 2>{0, 1});
  ASSERT_TRUE(mapping.ok());
  std::array<int, 3> data{};
  Result<StrideView> result = StrideView::Create(
      data.data(), mapping.value(), data.size());

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kFailedPrecondition);
}

TEST(TensorViewTest, SupportsUniqueStridedMutableViewWithHoles) {
  using StrideView = TensorView<int, MatrixExtents,
                                LayoutStrideMapping<MatrixExtents>>;
  Result<LayoutStrideMapping<MatrixExtents>> mapping =
      LayoutStrideMapping<MatrixExtents>::Create(
          MatrixExtents(), std::array<stride_t, 2>{1, 4});
  ASSERT_TRUE(mapping.ok());
  std::array<int, 10> data{};
  Result<StrideView> result = StrideView::Create(
      data.data(), mapping.value(), data.size());

  ASSERT_TRUE(result.ok());
  result.value()(1, 2) = 31;
  EXPECT_EQ(data[9], 31);
  EXPECT_EQ(result.value().GetRequiredSpan(), 10);
  EXPECT_FALSE(result.value().IsContiguous());
}

TEST(TensorViewTest, ValidatesAvailableSpanAndDataHandle) {
  Result<MatrixMapping> mapping =
      MatrixMapping::Create(MatrixExtents());
  ASSERT_TRUE(mapping.ok());
  std::array<int, 6> data{};

  Result<MutableMatrixView> negative = MutableMatrixView::Create(
      data.data(), mapping.value(), -1);
  Result<MutableMatrixView> short_span = MutableMatrixView::Create(
      data.data(), mapping.value(), 5);
  Result<MutableMatrixView> null_data = MutableMatrixView::Create(
      nullptr, mapping.value(), 6);

  ASSERT_FALSE(negative.ok());
  ASSERT_FALSE(short_span.ok());
  ASSERT_FALSE(null_data.ok());
  EXPECT_EQ(negative.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(short_span.status().code(), StatusCode::kOutOfRange);
  EXPECT_EQ(null_data.status().code(), StatusCode::kInvalidArgument);
}

TEST(TensorViewTest, AllowsNullDataForZeroRequiredSpan) {
  using EmptyExtents = Extents<2, 0>;
  using EmptyMapping = LayoutLeftMapping<EmptyExtents>;
  using EmptyView = TensorView<int, EmptyExtents>;
  Result<EmptyMapping> mapping = EmptyMapping::Create(EmptyExtents());
  ASSERT_TRUE(mapping.ok());

  Result<EmptyView> result =
      EmptyView::Create(nullptr, mapping.value(), 0);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().Data(), nullptr);
  EXPECT_EQ(result.value().GetSize(), 0);
  EXPECT_EQ(result.value().GetRequiredSpan(), 0);
}

TEST(TensorViewTest, RejectsNonHostAccessibleMemorySpace) {
  Result<MatrixMapping> mapping =
      MatrixMapping::Create(MatrixExtents());
  ASSERT_TRUE(mapping.ok());
  std::array<int, 6> data{};

  Result<MutableMatrixView> result = MutableMatrixView::Create(
      data.data(), mapping.value(), data.size(), MemorySpace::kDevice);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kUnsupported);
}

TEST(TensorViewTest, TryAtReturnsCheckedPointers) {
  Result<MatrixMapping> mapping =
      MatrixMapping::Create(MatrixExtents());
  ASSERT_TRUE(mapping.ok());
  std::array<int, 6> data{2, 3, 5, 7, 11, 13};
  Result<MutableMatrixView> result = MutableMatrixView::Create(
      data.data(), mapping.value(), data.size());
  ASSERT_TRUE(result.ok());
  const auto& view = result.value();

  Result<int*> valid = view.TryAt(1, 2);
  Result<int*> negative = view.TryAt(-1, 0);
  Result<int*> past_end = view.TryAt(0, 3);
  ASSERT_TRUE(valid.ok());
  ASSERT_FALSE(negative.ok());
  ASSERT_FALSE(past_end.ok());
  EXPECT_EQ(valid.value(), data.data() + 5);
  EXPECT_EQ(*valid.value(), 13);
  EXPECT_EQ(negative.status().code(), StatusCode::kOutOfRange);
  EXPECT_EQ(past_end.status().code(), StatusCode::kOutOfRange);
}

TEST(TensorViewTest, RightLayoutPreservesLogicalCoordinateAccess) {
  using RightMapping = LayoutRightMapping<MatrixExtents>;
  using RightView = TensorView<int, MatrixExtents, RightMapping>;
  Result<RightMapping> mapping = RightMapping::Create(MatrixExtents());
  ASSERT_TRUE(mapping.ok());
  std::array<int, 6> data{0, 1, 2, 3, 4, 5};
  Result<RightView> result = RightView::Create(
      data.data(), mapping.value(), data.size());

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value()(0, 2), 2);
  EXPECT_EQ(result.value()(1, 0), 3);
  result.value()(1, 2) = 37;
  EXPECT_EQ(data[5], 37);
}

}  // namespace
}  // namespace asc
