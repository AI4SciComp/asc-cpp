#include <asc/core/types.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace asc {
namespace {

static_assert(std::is_same_v<index_t, std::int64_t>);
static_assert(std::is_same_v<extent_t, std::int64_t>);
static_assert(std::is_same_v<stride_t, std::int64_t>);
static_assert(std::is_same_v<nnz_t, std::int64_t>);
static_assert(std::is_signed_v<index_t>);
static_assert(std::is_signed_v<extent_t>);
static_assert(std::is_signed_v<stride_t>);
static_assert(std::is_signed_v<nnz_t>);

TEST(CoreTypesTest, MetadataTypesHaveSigned64BitRange) {
  EXPECT_EQ(sizeof(index_t), sizeof(std::int64_t));
  EXPECT_EQ(sizeof(extent_t), sizeof(std::int64_t));
  EXPECT_EQ(sizeof(stride_t), sizeof(std::int64_t));
  EXPECT_EQ(sizeof(nnz_t), sizeof(std::int64_t));
  EXPECT_LT(std::numeric_limits<index_t>::lowest(), 0);
}

TEST(CoreTypesTest, DynamicExtentIsNegativeOne) {
  EXPECT_EQ(dynamic_extent, extent_t{-1});
}

}  // namespace
}  // namespace asc
