#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "asc/core/extents.h"
#include "asc/core/types.h"
#include "test_support.h"

namespace {

static_assert(std::is_same_v<asc::index_t, std::int64_t>);
static_assert(std::is_same_v<asc::extent_t, std::int64_t>);
static_assert(std::is_same_v<asc::stride_t, std::int64_t>);
static_assert(std::is_same_v<asc::nnz_t, std::int64_t>);
static_assert(std::is_same_v<asc::rank_t, std::uint32_t>);
static_assert(asc::kDynamicExtent == -1);

template <typename... Values>
concept CanCreateOneDynamicExtent = requires(Values... values) {
  asc::Extents<asc::kDynamicExtent>::Create(values...);
};

static_assert(CanCreateOneDynamicExtent<asc::extent_t>);
static_assert(CanCreateOneDynamicExtent<std::uint64_t>);
static_assert(!CanCreateOneDynamicExtent<double>);

void CheckCasts(asc_core_test::TestContext& context) {
  const auto signed_value = asc::CheckedCast<std::int32_t>(std::int64_t{42});
  ASC_TEST_CHECK(context, signed_value.ok());
  ASC_TEST_EQ(context, *signed_value, 42);

  const auto negative_to_unsigned =
      asc::CheckedCast<std::uint64_t>(std::int64_t{-1});
  ASC_TEST_EQ(context, negative_to_unsigned.status().code(),
              asc::ErrorCode::kOverflow);

  const auto positive_overflow =
      asc::CheckedCast<std::int32_t>(std::numeric_limits<std::int64_t>::max());
  ASC_TEST_EQ(context, positive_overflow.status().code(),
              asc::ErrorCode::kOverflow);
  ASC_TEST_CHECK(context, !asc::CheckedCast<std::int64_t>(
                               std::numeric_limits<std::uint64_t>::max())
                               .ok());
}

void CheckArithmetic(asc_core_test::TestContext& context) {
  ASC_TEST_EQ(context, *asc::CheckedAdd(std::int64_t{17}, std::int64_t{-8}), 9);
  ASC_TEST_CHECK(
      context,
      asc::CheckedAdd(std::numeric_limits<std::int64_t>::max(), std::int64_t{0})
          .ok());
  ASC_TEST_CHECK(context,
                 !asc::CheckedAdd(std::numeric_limits<std::int64_t>::max(),
                                  std::int64_t{1})
                      .ok());
  ASC_TEST_CHECK(context,
                 !asc::CheckedAdd(std::numeric_limits<std::int64_t>::min(),
                                  std::int64_t{-1})
                      .ok());
  ASC_TEST_CHECK(context,
                 !asc::CheckedAdd(std::numeric_limits<std::uint64_t>::max(),
                                  std::uint64_t{1})
                      .ok());

  ASC_TEST_EQ(context,
              *asc::CheckedMultiply(std::int64_t{-7}, std::int64_t{-9}), 63);
  ASC_TEST_EQ(context,
              *asc::CheckedMultiply(std::int64_t{0},
                                    std::numeric_limits<std::int64_t>::min()),
              0);
  ASC_TEST_CHECK(context,
                 !asc::CheckedMultiply(std::numeric_limits<std::int64_t>::max(),
                                       std::int64_t{2})
                      .ok());
  ASC_TEST_CHECK(context,
                 !asc::CheckedMultiply(std::numeric_limits<std::int64_t>::min(),
                                       std::int64_t{-1})
                      .ok());

  for (int left = std::numeric_limits<std::int8_t>::min();
       left <= std::numeric_limits<std::int8_t>::max(); ++left) {
    for (int right = std::numeric_limits<std::int8_t>::min();
         right <= std::numeric_limits<std::int8_t>::max(); ++right) {
      const auto checked_sum = asc::CheckedAdd(static_cast<std::int8_t>(left),
                                               static_cast<std::int8_t>(right));
      const int reference_sum = left + right;
      const bool sum_fits =
          reference_sum >= std::numeric_limits<std::int8_t>::min() &&
          reference_sum <= std::numeric_limits<std::int8_t>::max();
      ASC_TEST_EQ(context, checked_sum.ok(), sum_fits);

      const auto checked_product = asc::CheckedMultiply(
          static_cast<std::int8_t>(left), static_cast<std::int8_t>(right));
      const int reference_product = left * right;
      const bool product_fits =
          reference_product >= std::numeric_limits<std::int8_t>::min() &&
          reference_product <= std::numeric_limits<std::int8_t>::max();
      ASC_TEST_EQ(context, checked_product.ok(), product_fits);
    }
  }
}

void CheckByteCounts(asc_core_test::TestContext& context) {
  ASC_TEST_EQ(context, *asc::CheckedByteCount(11, sizeof(std::uint32_t)),
              std::size_t{44});
  ASC_TEST_EQ(context, *asc::CheckedByteCount(0, 4096), std::size_t{0});
  ASC_TEST_EQ(context, asc::CheckedByteCount(-1, 1).status().code(),
              asc::ErrorCode::kInvalidArgument);
  const auto overflow =
      asc::CheckedByteCount(std::numeric_limits<asc::extent_t>::max(),
                            std::numeric_limits<std::size_t>::max());
  ASC_TEST_EQ(context, overflow.status().code(), asc::ErrorCode::kOverflow);
}

void CheckExtents(asc_core_test::TestContext& context) {
  using ScalarExtents = asc::Extents<>;
  static_assert(ScalarExtents::rank() == 0);
  static_assert(ScalarExtents::dynamic_rank() == 0);
  const auto scalar = ScalarExtents::Create();
  ASC_TEST_EQ(context, scalar->logical_size(), 1);
  ASC_TEST_CHECK(context, scalar->values().empty());
  ASC_TEST_EQ(context, scalar->extent(0).status().code(),
              asc::ErrorCode::kIndex);

  using MixedExtents = asc::Extents<2, asc::kDynamicExtent, 4>;
  static_assert(MixedExtents::rank() == 3);
  static_assert(MixedExtents::dynamic_rank() == 1);
  static_assert(MixedExtents::static_extent<0>() == 2);
  const auto mixed = MixedExtents::Create(3);
  ASC_TEST_EQ(context, mixed->logical_size(), 24);
  ASC_TEST_EQ(context, *mixed->extent(0), 2);
  ASC_TEST_EQ(context, *mixed->extent(1), 3);
  ASC_TEST_EQ(context, *mixed->extent(2), 4);

  using ZeroExtents = asc::Extents<asc::kDynamicExtent, 0, asc::kDynamicExtent>;
  const auto zero =
      ZeroExtents::Create(std::numeric_limits<asc::extent_t>::max(),
                          std::numeric_limits<asc::extent_t>::max());
  ASC_TEST_CHECK(context, zero.ok());
  ASC_TEST_EQ(context, zero->logical_size(), 0);
  ASC_TEST_EQ(context, MixedExtents::Create(-1).status().code(),
              asc::ErrorCode::kShape);

  using OneDynamic = asc::Extents<asc::kDynamicExtent>;
  ASC_TEST_EQ(context,
              OneDynamic::Create(std::numeric_limits<std::uint64_t>::max())
                  .status()
                  .code(),
              asc::ErrorCode::kOverflow);
  using Product = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  ASC_TEST_EQ(context,
              Product::Create(std::numeric_limits<asc::extent_t>::max(), 2)
                  .status()
                  .code(),
              asc::ErrorCode::kOverflow);

  const std::array<asc::extent_t, 1> wrong_count = {5};
  ASC_TEST_EQ(context,
              Product::Create(std::span<const asc::extent_t>(wrong_count))
                  .status()
                  .code(),
              asc::ErrorCode::kShape);
}

}  // namespace

int main() {
  asc_core_test::TestContext context;
  CheckCasts(context);
  CheckArithmetic(context);
  CheckByteCounts(context);
  CheckExtents(context);
  return context.Finish();
}
