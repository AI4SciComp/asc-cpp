#include <asc/array/extents.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace asc {
namespace {

template <typename ExtentsType>
concept CreatableWithNoDynamicExtents = requires {
  ExtentsType::Create();
};

template <typename ExtentsType>
concept CreatableWithOneDynamicExtent = requires {
  ExtentsType::Create(1);
};

template <typename ExtentsType>
concept CreatableWithTwoDynamicExtents = requires {
  ExtentsType::Create(1, 2);
};

template <typename ExtentsType>
concept CreatableWithBooleanExtent = requires {
  ExtentsType::Create(true);
};

using MixedExtents = Extents<2, dynamic_extent, 4, dynamic_extent>;

static_assert(Extents<>::Rank() == 0);
static_assert(Extents<>::DynamicRank() == 0);
static_assert(Extents<2, 3>::Rank() == 2);
static_assert(Extents<2, 3>::DynamicRank() == 0);
static_assert(MixedExtents::Rank() == 4);
static_assert(MixedExtents::DynamicRank() == 2);
static_assert(MixedExtents::StaticExtent(0) == 2);
static_assert(MixedExtents::StaticExtent(1) == dynamic_extent);
static_assert(MixedExtents::StaticExtent(2) == 4);
static_assert(MixedExtents::StaticExtent(3) == dynamic_extent);
static_assert(DynamicTensorExtents<3>::Rank() == 3);
static_assert(DynamicTensorExtents<3>::DynamicRank() == 3);
static_assert(std::is_same_v<DynamicTensorExtents<0>, Extents<>>);
static_assert(std::is_same_v<DynamicTensorExtents<2>,
                             Extents<dynamic_extent, dynamic_extent>>);
static_assert(CreatableWithNoDynamicExtents<Extents<2, 3>>);
static_assert(!CreatableWithOneDynamicExtent<Extents<2, 3>>);
static_assert(CreatableWithOneDynamicExtent<
              Extents<2, dynamic_extent, 4>>);
static_assert(!CreatableWithBooleanExtent<
              Extents<2, dynamic_extent, 4>>);
static_assert(!CreatableWithTwoDynamicExtents<
              Extents<2, dynamic_extent, 4>>);
static_assert(CreatableWithTwoDynamicExtents<MixedExtents>);
static_assert(std::is_trivially_copyable_v<MixedExtents>);
static_assert(sizeof(MixedExtents) == 2 * sizeof(extent_t));

TEST(ExtentsTest, RankZeroIsScalarDescriptor) {
  Result<Extents<>> result = Extents<>::Create();

  ASSERT_TRUE(result.ok());
  Result<extent_t> size = result.value().GetSize();
  ASSERT_TRUE(size.ok());
  EXPECT_EQ(size.value(), 1);
  EXPECT_EQ(result.value(), Extents<>());
}

TEST(ExtentsTest, StaticExtentsExposeCompileTimeValues) {
  Result<Extents<2, 3, 4>> result = Extents<2, 3, 4>::Create();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().GetExtent(0), 2);
  EXPECT_EQ(result.value().GetExtent(1), 3);
  EXPECT_EQ(result.value().GetExtent(2), 4);
  Result<extent_t> size = result.value().GetSize();
  ASSERT_TRUE(size.ok());
  EXPECT_EQ(size.value(), 24);
}

TEST(ExtentsTest, DefaultDynamicValuesAreZero) {
  const DynamicTensorExtents<3> extents;

  EXPECT_EQ(extents.GetExtent(0), 0);
  EXPECT_EQ(extents.GetExtent(1), 0);
  EXPECT_EQ(extents.GetExtent(2), 0);
  Result<extent_t> size = extents.GetSize();
  ASSERT_TRUE(size.ok());
  EXPECT_EQ(size.value(), 0);
}

TEST(ExtentsTest, MixedExtentsConsumeRuntimeValuesInDimensionOrder) {
  Result<MixedExtents> result = MixedExtents::Create(3, 5);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().GetExtent(0), 2);
  EXPECT_EQ(result.value().GetExtent(1), 3);
  EXPECT_EQ(result.value().GetExtent(2), 4);
  EXPECT_EQ(result.value().GetExtent(3), 5);
  Result<extent_t> size = result.value().GetSize();
  ASSERT_TRUE(size.ok());
  EXPECT_EQ(size.value(), 120);
}

TEST(ExtentsTest, ZeroExtentMakesLogicalSizeZero) {
  using ZeroExtents = Extents<dynamic_extent, 7, dynamic_extent>;
  Result<ZeroExtents> result = ZeroExtents::Create(
      std::numeric_limits<extent_t>::max(), 0);

  ASSERT_TRUE(result.ok());
  Result<extent_t> size = result.value().GetSize();
  ASSERT_TRUE(size.ok());
  EXPECT_EQ(size.value(), 0);
}

TEST(ExtentsTest, RejectsNegativeDynamicExtent) {
  Result<DynamicTensorExtents<2>> result =
      DynamicTensorExtents<2>::Create(3, -1);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kInvalidArgument);
}

TEST(ExtentsTest, RejectsUnsignedValueBeyondCanonicalRange) {
  Result<DynamicTensorExtents<1>> result =
      DynamicTensorExtents<1>::Create(
          std::numeric_limits<std::uint64_t>::max());

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kOverflow);
}

TEST(ExtentsTest, DetectsLogicalSizeOverflow) {
  Result<DynamicTensorExtents<2>> result =
      DynamicTensorExtents<2>::Create(
          std::numeric_limits<extent_t>::max(), 2);

  ASSERT_TRUE(result.ok());
  Result<extent_t> size = result.value().GetSize();
  ASSERT_FALSE(size.ok());
  EXPECT_EQ(size.status().code(), StatusCode::kOverflow);
}

TEST(ExtentsTest, RepresentsMetadataBeyondIntRangeWithoutAllocation) {
  constexpr extent_t kBeyondInt =
      static_cast<extent_t>(std::numeric_limits<int>::max()) + 17;
  using LargeExtents = Extents<dynamic_extent, 2>;
  Result<LargeExtents> result = LargeExtents::Create(kBeyondInt);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().GetExtent(0), kBeyondInt);
  EXPECT_EQ(result.value().GetExtent(1), 2);
  Result<extent_t> size = result.value().GetSize();
  ASSERT_TRUE(size.ok());
  EXPECT_EQ(size.value(), 2 * kBeyondInt);
}

TEST(ExtentsTest, EqualityComparesLogicalExtents) {
  using TestExtents = Extents<2, dynamic_extent>;
  Result<TestExtents> first = TestExtents::Create(3);
  Result<TestExtents> same = TestExtents::Create(3);
  Result<TestExtents> different = TestExtents::Create(4);

  ASSERT_TRUE(first.ok());
  ASSERT_TRUE(same.ok());
  ASSERT_TRUE(different.ok());
  EXPECT_EQ(first.value(), same.value());
  EXPECT_NE(first.value(), different.value());
}

}  // namespace
}  // namespace asc
