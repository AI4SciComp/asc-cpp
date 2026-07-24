#include <asc/array/layout.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace asc {
namespace {

template <typename Mapping>
concept HasOneCoordinateOffset = requires(const Mapping& mapping) {
  mapping.TryOffset(0);
};

template <typename Mapping>
concept HasTwoCoordinateOffset = requires(const Mapping& mapping) {
  mapping.TryOffset(0, 0);
};

template <typename Mapping>
concept HasBooleanCoordinateOffset = requires(const Mapping& mapping) {
  mapping.TryOffset(true, 0);
};

using StaticMatrixExtents = Extents<2, 3>;
using LeftMatrixMapping = LayoutLeftMapping<StaticMatrixExtents>;
using RightMatrixMapping = LayoutRightMapping<StaticMatrixExtents>;

static_assert(LeftMatrixMapping::Rank() == 2);
static_assert(LeftMatrixMapping::IsAlwaysUnique());
static_assert(LeftMatrixMapping::IsAlwaysExhaustive());
static_assert(LeftMatrixMapping::IsAlwaysContiguous());
static_assert(RightMatrixMapping::IsAlwaysUnique());
static_assert(RightMatrixMapping::IsAlwaysExhaustive());
static_assert(RightMatrixMapping::IsAlwaysContiguous());
static_assert(!LayoutStrideMapping<StaticMatrixExtents>::IsAlwaysUnique());
static_assert(
    !LayoutStrideMapping<StaticMatrixExtents>::IsAlwaysExhaustive());
static_assert(
    !LayoutStrideMapping<StaticMatrixExtents>::IsAlwaysContiguous());
static_assert(!HasOneCoordinateOffset<LeftMatrixMapping>);
static_assert(HasTwoCoordinateOffset<LeftMatrixMapping>);
static_assert(!HasBooleanCoordinateOffset<LeftMatrixMapping>);
static_assert(std::is_same_v<typename LeftMatrixMapping::ExtentsType,
                             StaticMatrixExtents>);

TEST(LayoutTest, RankZeroMappingsDescribeOneScalar) {
  const Extents<> extents;
  Result<LayoutLeftMapping<Extents<>>> left =
      LayoutLeftMapping<Extents<>>::Create(extents);
  Result<LayoutRightMapping<Extents<>>> right =
      LayoutRightMapping<Extents<>>::Create(extents);
  Result<LayoutStrideMapping<Extents<>>> stride =
      LayoutStrideMapping<Extents<>>::Create(extents, {});

  ASSERT_TRUE(left.ok());
  ASSERT_TRUE(right.ok());
  ASSERT_TRUE(stride.ok());
  EXPECT_EQ(left.value().GetSize(), 1);
  EXPECT_EQ(left.value().GetRequiredSpan(), 1);
  EXPECT_EQ(right.value().GetSize(), 1);
  EXPECT_EQ(right.value().GetRequiredSpan(), 1);
  EXPECT_EQ(stride.value().GetSize(), 1);
  EXPECT_EQ(stride.value().GetRequiredSpan(), 1);
  EXPECT_TRUE(stride.value().IsUnique());
  EXPECT_TRUE(stride.value().IsExhaustive());
  EXPECT_TRUE(stride.value().IsContiguous());
  EXPECT_EQ(left.value()(), 0);
  EXPECT_EQ(right.value().UncheckedOffset(), 0);
  ASSERT_TRUE(stride.value().TryOffset().ok());
  EXPECT_EQ(stride.value().TryOffset().value(), 0);
}

TEST(LayoutTest, LeftMappingUsesColumnMajorFormula) {
  using TestExtents = Extents<2, 3, 4>;
  Result<LayoutLeftMapping<TestExtents>> result =
      LayoutLeftMapping<TestExtents>::Create(TestExtents());

  ASSERT_TRUE(result.ok());
  const auto& mapping = result.value();
  EXPECT_EQ(mapping.GetExtent(0), 2);
  EXPECT_EQ(mapping.GetExtent(1), 3);
  EXPECT_EQ(mapping.GetExtent(2), 4);
  EXPECT_EQ(mapping.GetStride(0), 1);
  EXPECT_EQ(mapping.GetStride(1), 2);
  EXPECT_EQ(mapping.GetStride(2), 6);
  EXPECT_EQ(mapping.GetSize(), 24);
  EXPECT_EQ(mapping.GetRequiredSpan(), 24);
  EXPECT_TRUE(mapping.IsUnique());
  EXPECT_TRUE(mapping.IsExhaustive());
  EXPECT_TRUE(mapping.IsContiguous());
  EXPECT_EQ(mapping(1, 2, 3), 23);
  EXPECT_EQ(mapping.UncheckedOffset(1, 2, 3), 23);
}

TEST(LayoutTest, RightMappingUsesRowMajorFormula) {
  using TestExtents = Extents<2, 3, 4>;
  Result<LayoutRightMapping<TestExtents>> result =
      LayoutRightMapping<TestExtents>::Create(TestExtents());

  ASSERT_TRUE(result.ok());
  const auto& mapping = result.value();
  EXPECT_EQ(mapping.GetStride(0), 12);
  EXPECT_EQ(mapping.GetStride(1), 4);
  EXPECT_EQ(mapping.GetStride(2), 1);
  EXPECT_EQ(mapping.GetSize(), 24);
  EXPECT_EQ(mapping.GetRequiredSpan(), 24);
  EXPECT_EQ(mapping(1, 2, 3), 23);
  EXPECT_EQ(mapping.UncheckedOffset(1, 2, 3), 23);
}

TEST(LayoutTest, LeftAndRightCoverEveryLogicalCoordinateExactlyOnce) {
  using TestExtents = Extents<2, 3, 4>;
  auto left = LayoutLeftMapping<TestExtents>::Create(TestExtents());
  auto right = LayoutRightMapping<TestExtents>::Create(TestExtents());
  ASSERT_TRUE(left.ok());
  ASSERT_TRUE(right.ok());
  std::vector<bool> left_offsets(24, false);
  std::vector<bool> right_offsets(24, false);

  for (index_t first = 0; first < 2; ++first) {
    for (index_t second = 0; second < 3; ++second) {
      for (index_t third = 0; third < 4; ++third) {
        const index_t left_offset = left.value()(first, second, third);
        const index_t right_offset = right.value()(first, second, third);
        EXPECT_FALSE(left_offsets.at(left_offset));
        EXPECT_FALSE(right_offsets.at(right_offset));
        left_offsets.at(left_offset) = true;
        right_offsets.at(right_offset) = true;
      }
    }
  }
  for (bool visited : left_offsets) {
    EXPECT_TRUE(visited);
  }
  for (bool visited : right_offsets) {
    EXPECT_TRUE(visited);
  }
}

TEST(LayoutTest, EmptyMappingsAreVacuouslyContiguousAndUnique) {
  using EmptyExtents = Extents<2, dynamic_extent, 4>;
  Result<EmptyExtents> extents = EmptyExtents::Create(0);
  ASSERT_TRUE(extents.ok());
  auto left = LayoutLeftMapping<EmptyExtents>::Create(extents.value());
  auto right = LayoutRightMapping<EmptyExtents>::Create(extents.value());
  auto stride = LayoutStrideMapping<EmptyExtents>::Create(
      extents.value(), std::array<stride_t, 3>{7, 0, 99});

  ASSERT_TRUE(left.ok());
  ASSERT_TRUE(right.ok());
  ASSERT_TRUE(stride.ok());
  EXPECT_EQ(left.value().GetSize(), 0);
  EXPECT_EQ(left.value().GetRequiredSpan(), 0);
  EXPECT_EQ(right.value().GetRequiredSpan(), 0);
  EXPECT_EQ(stride.value().GetRequiredSpan(), 0);
  EXPECT_TRUE(stride.value().IsUnique());
  EXPECT_TRUE(stride.value().IsExhaustive());
  EXPECT_TRUE(stride.value().IsContiguous());
}

TEST(LayoutTest, StrideMappingSupportsHoles) {
  Result<LayoutStrideMapping<StaticMatrixExtents>> result =
      LayoutStrideMapping<StaticMatrixExtents>::Create(
          StaticMatrixExtents(), std::array<stride_t, 2>{1, 4});

  ASSERT_TRUE(result.ok());
  const auto& mapping = result.value();
  EXPECT_EQ(mapping.GetStride(0), 1);
  EXPECT_EQ(mapping.GetStride(1), 4);
  EXPECT_EQ(mapping.GetSize(), 6);
  EXPECT_EQ(mapping.GetRequiredSpan(), 10);
  EXPECT_TRUE(mapping.IsUnique());
  EXPECT_FALSE(mapping.IsExhaustive());
  EXPECT_FALSE(mapping.IsContiguous());
  EXPECT_EQ(mapping(1, 2), 9);
}

TEST(LayoutTest, ZeroStrideRepresentsBroadcastAliasing) {
  Result<LayoutStrideMapping<StaticMatrixExtents>> result =
      LayoutStrideMapping<StaticMatrixExtents>::Create(
          StaticMatrixExtents(), std::array<stride_t, 2>{0, 1});

  ASSERT_TRUE(result.ok());
  const auto& mapping = result.value();
  EXPECT_EQ(mapping.GetRequiredSpan(), 3);
  EXPECT_FALSE(mapping.IsUnique());
  EXPECT_FALSE(mapping.IsExhaustive());
  EXPECT_FALSE(mapping.IsContiguous());
  EXPECT_EQ(mapping(0, 2), mapping(1, 2));
}

TEST(LayoutTest, CanonicalStrideMappingsAreContiguous) {
  using TestExtents = Extents<2, 1, 3>;
  auto left = LayoutStrideMapping<TestExtents>::Create(
      TestExtents(), std::array<stride_t, 3>{1, 501, 2});
  auto right = LayoutStrideMapping<TestExtents>::Create(
      TestExtents(), std::array<stride_t, 3>{3, 501, 1});

  ASSERT_TRUE(left.ok());
  ASSERT_TRUE(right.ok());
  EXPECT_EQ(left.value().GetRequiredSpan(), 6);
  EXPECT_TRUE(left.value().IsUnique());
  EXPECT_TRUE(left.value().IsExhaustive());
  EXPECT_TRUE(left.value().IsContiguous());
  EXPECT_EQ(right.value().GetRequiredSpan(), 6);
  EXPECT_TRUE(right.value().IsUnique());
  EXPECT_TRUE(right.value().IsExhaustive());
  EXPECT_TRUE(right.value().IsContiguous());
}

TEST(LayoutTest, UniquenessProofHandlesPermutedNonOverlappingDimensions) {
  using TestExtents = Extents<2, 2>;
  auto unique = LayoutStrideMapping<TestExtents>::Create(
      TestExtents(), std::array<stride_t, 2>{3, 1});
  auto overlapping = LayoutStrideMapping<TestExtents>::Create(
      TestExtents(), std::array<stride_t, 2>{2, 2});

  ASSERT_TRUE(unique.ok());
  ASSERT_TRUE(overlapping.ok());
  EXPECT_TRUE(unique.value().IsUnique());
  EXPECT_FALSE(overlapping.value().IsUnique());
}

TEST(LayoutTest, RejectsNegativeStride) {
  Result<LayoutStrideMapping<StaticMatrixExtents>> result =
      LayoutStrideMapping<StaticMatrixExtents>::Create(
          StaticMatrixExtents(), std::array<stride_t, 2>{1, -1});

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kInvalidArgument);
}

TEST(LayoutTest, DetectsContiguousMappingOverflow) {
  using TestExtents = DynamicTensorExtents<2>;
  Result<TestExtents> extents = TestExtents::Create(
      std::numeric_limits<extent_t>::max(), 2);
  ASSERT_TRUE(extents.ok());
  Result<LayoutLeftMapping<TestExtents>> result =
      LayoutLeftMapping<TestExtents>::Create(extents.value());

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kOverflow);
}

TEST(LayoutTest, DetectsRequiredSpanOverflow) {
  using TestExtents = DynamicTensorExtents<1>;
  Result<TestExtents> extents =
      TestExtents::Create(std::numeric_limits<extent_t>::max());
  ASSERT_TRUE(extents.ok());
  Result<LayoutStrideMapping<TestExtents>> result =
      LayoutStrideMapping<TestExtents>::Create(
          extents.value(), std::array<stride_t, 1>{2});

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kOverflow);
}

TEST(LayoutTest, TryOffsetRejectsBoundsAndWideUnsignedCoordinates) {
  Result<LeftMatrixMapping> result =
      LeftMatrixMapping::Create(StaticMatrixExtents());
  ASSERT_TRUE(result.ok());
  const auto& mapping = result.value();

  Result<index_t> negative = mapping.TryOffset(-1, 0);
  Result<index_t> past_end = mapping.TryOffset(0, 3);
  Result<index_t> too_wide = mapping.TryOffset(
      std::numeric_limits<std::uint64_t>::max(), 0);
  ASSERT_FALSE(negative.ok());
  ASSERT_FALSE(past_end.ok());
  ASSERT_FALSE(too_wide.ok());
  EXPECT_EQ(negative.status().code(), StatusCode::kOutOfRange);
  EXPECT_EQ(past_end.status().code(), StatusCode::kOutOfRange);
  EXPECT_EQ(too_wide.status().code(), StatusCode::kOutOfRange);
}

TEST(LayoutTest, LargeMetadataDoesNotRequireBackingAllocation) {
  constexpr extent_t kBeyondInt =
      static_cast<extent_t>(std::numeric_limits<int>::max()) + 31;
  using TestExtents = Extents<dynamic_extent, 2>;
  Result<TestExtents> extents = TestExtents::Create(kBeyondInt);
  ASSERT_TRUE(extents.ok());
  Result<LayoutRightMapping<TestExtents>> result =
      LayoutRightMapping<TestExtents>::Create(extents.value());

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().GetStride(0), 2);
  EXPECT_EQ(result.value().GetStride(1), 1);
  EXPECT_EQ(result.value().GetRequiredSpan(), 2 * kBeyondInt);
  Result<index_t> last = result.value().TryOffset(kBeyondInt - 1, 1);
  ASSERT_TRUE(last.ok());
  EXPECT_EQ(last.value(), 2 * kBeyondInt - 1);
}

}  // namespace
}  // namespace asc
