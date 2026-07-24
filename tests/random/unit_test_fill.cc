#include <asc/array.h>
#include <asc/random.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <thread>
#include <type_traits>

#include "allocation_counter.h"
#include "test_operands.h"

namespace asc {
namespace {

using FloatExtents = Extents<2>;
using FloatView = TensorView<float, FloatExtents>;
using ConstFloatView = TensorView<const float, FloatExtents>;
using VolatileFloatView = TensorView<volatile float, FloatExtents>;
using IntView = TensorView<int, FloatExtents>;

template <typename Destination, typename Distribution>
concept CanFillRandom = requires(const ExecutionContext& context,
                                 const Destination& destination,
                                 Distribution distribution) {
  FillRandom(context, destination, distribution, RandomKey{1},
             RandomCounter{2, 3});
};

static_assert(CanFillRandom<FloatView, Uniform01<float>>);
static_assert(!CanFillRandom<FloatView, Uniform01<double>>);
static_assert(!CanFillRandom<ConstFloatView, Uniform01<float>>);
static_assert(!CanFillRandom<VolatileFloatView, Uniform01<float>>);
static_assert(!CanFillRandom<IntView, Uniform01<float>>);

template <typename T>
constexpr std::array<T, 12> KnownUniformValues();

template <>
constexpr std::array<float, 12> KnownUniformValues<float>() {
  return {0x1.989fa0p-2F, 0x1.f1c998p-1F, 0x1.3ea8c0p-6F,
          0x1.9321dep-1F, 0x1.de7b86p-1F, 0x1.cd224cp-2F,
          0x1.6d5e96p-1F, 0x1.51663ap-1F, 0x1.fe3548p-3F,
          0x1.2dd202p-1F, 0x1.aeef9cp-1F, 0x1.41ebd4p-1F};
}

template <>
constexpr std::array<double, 12> KnownUniformValues<double>() {
  return {0x1.989fa35785a70p-2, 0x1.f1c99948b9640p-1,
          0x1.3ea8ca5471cc0p-6, 0x1.9321de52d488ep-1,
          0x1.de7b86a96327ep-1, 0x1.cd224fec1c6a4p-2,
          0x1.6d5e97f19d611p-1, 0x1.51663a6215c8bp-1,
          0x1.fe354b2504700p-3, 0x1.2dd2037855b46p-1,
          0x1.aeef9cd5a1a6bp-1, 0x1.41ebd5041361dp-1};
}

template <typename T>
class RandomFillTest : public ::testing::Test {};

using FloatingTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(RandomFillTest, FloatingTypes);

TYPED_TEST(RandomFillTest, MatchesIndependentGoldenVector) {
  using T = TypeParam;
  using ExtentsType = Extents<12>;
  auto mapping = LayoutLeftMapping<ExtentsType>::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::array<T, 12> output{};
  auto view = TensorView<T, ExtentsType>::Create(
      output.data(), mapping.value(), output.size());
  ASSERT_TRUE(view.ok());

  Status status = FillRandom(ExecutionContext::Serial(), view.value(),
                             Uniform01<T>(), RandomKey{0},
                             RandomCounter{0, 0});

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(output, KnownUniformValues<T>());
}

TYPED_TEST(RandomFillTest, UsesLogicalOrderAcrossLayoutsAndPreservesHoles) {
  using T = TypeParam;
  using ExtentsType = Extents<2, 3>;
  using LeftMapping = LayoutLeftMapping<ExtentsType>;
  using RightMapping = LayoutRightMapping<ExtentsType>;
  using StrideMapping = LayoutStrideMapping<ExtentsType>;
  auto left_mapping = LeftMapping::Create(ExtentsType());
  auto right_mapping = RightMapping::Create(ExtentsType());
  auto stride_mapping = StrideMapping::Create(
      ExtentsType(), std::array<stride_t, 2>{5, 1});
  ASSERT_TRUE(left_mapping.ok());
  ASSERT_TRUE(right_mapping.ok());
  ASSERT_TRUE(stride_mapping.ok());
  std::array<T, 6> left{};
  std::array<T, 6> right{};
  constexpr T kHole = T{-7};
  std::array<T, 8> strided{kHole, kHole, kHole, kHole,
                           kHole, kHole, kHole, kHole};
  auto left_view = TensorView<T, ExtentsType, LeftMapping>::Create(
      left.data(), left_mapping.value(), left.size());
  auto right_view = TensorView<T, ExtentsType, RightMapping>::Create(
      right.data(), right_mapping.value(), right.size());
  auto stride_view = TensorView<T, ExtentsType, StrideMapping>::Create(
      strided.data(), stride_mapping.value(), strided.size());
  ASSERT_TRUE(left_view.ok());
  ASSERT_TRUE(right_view.ok());
  ASSERT_TRUE(stride_view.ok());
  const ExecutionContext context = ExecutionContext::Serial();

  ASSERT_TRUE(FillRandom(context, left_view.value(), Uniform01<T>(),
                         RandomKey{0}, RandomCounter{0, 0})
                  .ok());
  ASSERT_TRUE(FillRandom(context, right_view.value(), Uniform01<T>(),
                         RandomKey{0}, RandomCounter{0, 0})
                  .ok());
  ASSERT_TRUE(FillRandom(context, stride_view.value(), Uniform01<T>(),
                         RandomKey{0}, RandomCounter{0, 0})
                  .ok());

  const auto expected = KnownUniformValues<T>();
  for (index_t row = 0; row < 2; ++row) {
    for (index_t column = 0; column < 3; ++column) {
      const std::size_t logical = static_cast<std::size_t>(row * 3 + column);
      EXPECT_EQ(left_view.value()(row, column), expected[logical]);
      EXPECT_EQ(right_view.value()(row, column), expected[logical]);
      EXPECT_EQ(stride_view.value()(row, column), expected[logical]);
    }
  }
  EXPECT_EQ(strided[3], kHole);
  EXPECT_EQ(strided[4], kHole);
}

TYPED_TEST(RandomFillTest, SupportsRankZeroAndZeroDimensions) {
  using T = TypeParam;
  using ScalarExtents = Extents<>;
  using EmptyExtents = Extents<2, 0, 3>;
  auto scalar_mapping =
      LayoutLeftMapping<ScalarExtents>::Create(ScalarExtents());
  auto empty_mapping = LayoutRightMapping<EmptyExtents>::Create(EmptyExtents());
  ASSERT_TRUE(scalar_mapping.ok());
  ASSERT_TRUE(empty_mapping.ok());
  T scalar = T{-1};
  auto scalar_view = TensorView<T, ScalarExtents>::Create(
      &scalar, scalar_mapping.value(), 1);
  auto empty_view = TensorView<T, EmptyExtents,
                               LayoutRightMapping<EmptyExtents>>::Create(
      nullptr, empty_mapping.value(), 0);
  ASSERT_TRUE(scalar_view.ok());
  ASSERT_TRUE(empty_view.ok());

  EXPECT_TRUE(FillRandom(ExecutionContext::Serial(), scalar_view.value(),
                         Uniform01<T>(), RandomKey{0}, RandomCounter{0, 0})
                  .ok());
  EXPECT_EQ(scalar, KnownUniformValues<T>()[0]);
  EXPECT_TRUE(FillRandom(
                  ExecutionContext::Serial(), empty_view.value(),
                  Uniform01<T>(), RandomKey{0},
                  RandomCounter{0, std::numeric_limits<std::uint64_t>::max()})
                  .ok());
}

TYPED_TEST(RandomFillTest, WholeAndReorderedPartitionsAreBitIdentical) {
  using T = TypeParam;
  using ExtentsType = DynamicTensorExtents<1>;
  constexpr extent_t kSize = 12;
  auto full_extents = ExtentsType::Create(kSize);
  ASSERT_TRUE(full_extents.ok());
  auto full_mapping = LayoutRightMapping<ExtentsType>::Create(
      full_extents.value());
  ASSERT_TRUE(full_mapping.ok());
  std::array<T, kSize> whole{};
  std::array<T, kSize> two_way{};
  std::array<T, kSize> three_way{};
  auto whole_view = TensorView<T, ExtentsType,
                               LayoutRightMapping<ExtentsType>>::Create(
      whole.data(), full_mapping.value(), whole.size());
  ASSERT_TRUE(whole_view.ok());
  const ExecutionContext context = ExecutionContext::Serial();
  ASSERT_TRUE(FillRandom(context, whole_view.value(), Uniform01<T>(),
                         RandomKey{0}, RandomCounter{0, 0})
                  .ok());

  const auto fill_partition = [&](auto& destination, extent_t begin,
                                  extent_t size) {
    auto extents = ExtentsType::Create(size);
    if (!extents.ok()) {
      return extents.status();
    }
    auto mapping = LayoutRightMapping<ExtentsType>::Create(extents.value());
    if (!mapping.ok()) {
      return mapping.status();
    }
    auto view = TensorView<T, ExtentsType,
                           LayoutRightMapping<ExtentsType>>::Create(
        destination.data() + begin, mapping.value(), size);
    if (!view.ok()) {
      return view.status();
    }
    return FillRandom(context, view.value(), Uniform01<T>(), RandomKey{0},
                      RandomCounter{0, static_cast<std::uint64_t>(begin)});
  };

  ASSERT_TRUE(fill_partition(two_way, 0, 5).ok());
  ASSERT_TRUE(fill_partition(two_way, 5, 7).ok());
  ASSERT_TRUE(fill_partition(three_way, 5, 7).ok());
  ASSERT_TRUE(fill_partition(three_way, 2, 3).ok());
  ASSERT_TRUE(fill_partition(three_way, 0, 2).ok());
  EXPECT_EQ(two_way, whole);
  EXPECT_EQ(three_way, whole);
}

TYPED_TEST(RandomFillTest, ConcurrentIndependentFillsMatchSerial) {
  using T = TypeParam;
  using ExtentsType = Extents<12>;
  auto mapping = LayoutLeftMapping<ExtentsType>::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::array<T, 12> serial{};
  std::array<T, 12> first{};
  std::array<T, 12> second{};
  auto serial_view = TensorView<T, ExtentsType>::Create(
      serial.data(), mapping.value(), serial.size());
  auto first_view = TensorView<T, ExtentsType>::Create(
      first.data(), mapping.value(), first.size());
  auto second_view = TensorView<T, ExtentsType>::Create(
      second.data(), mapping.value(), second.size());
  ASSERT_TRUE(serial_view.ok());
  ASSERT_TRUE(first_view.ok());
  ASSERT_TRUE(second_view.ok());
  const ExecutionContext context = ExecutionContext::Serial();
  ASSERT_TRUE(FillRandom(context, serial_view.value(), Uniform01<T>(),
                         RandomKey{73}, RandomCounter{19, 23})
                  .ok());
  Status first_status;
  Status second_status;

  std::thread first_thread([&] {
    first_status = FillRandom(context, first_view.value(), Uniform01<T>(),
                              RandomKey{73}, RandomCounter{19, 23});
  });
  std::thread second_thread([&] {
    second_status = FillRandom(context, second_view.value(), Uniform01<T>(),
                               RandomKey{73}, RandomCounter{19, 23});
  });
  first_thread.join();
  second_thread.join();

  ASSERT_TRUE(first_status.ok());
  ASSERT_TRUE(second_status.ok());
  EXPECT_EQ(first, serial);
  EXPECT_EQ(second, serial);
}

TYPED_TEST(RandomFillTest, ChecksCounterRangeBeforeMutation) {
  using T = TypeParam;
  using OneExtents = Extents<1>;
  using TwoExtents = Extents<2>;
  auto one_mapping = LayoutLeftMapping<OneExtents>::Create(OneExtents());
  auto two_mapping = LayoutLeftMapping<TwoExtents>::Create(TwoExtents());
  ASSERT_TRUE(one_mapping.ok());
  ASSERT_TRUE(two_mapping.ok());
  std::array<T, 2> storage{T{-3}, T{-5}};
  auto one_view = TensorView<T, OneExtents>::Create(
      storage.data(), one_mapping.value(), storage.size());
  auto two_view = TensorView<T, TwoExtents>::Create(
      storage.data(), two_mapping.value(), storage.size());
  ASSERT_TRUE(one_view.ok());
  ASSERT_TRUE(two_view.ok());
  const ExecutionContext context = ExecutionContext::Serial();
  const RandomCounter last{0, std::numeric_limits<std::uint64_t>::max()};

  ASSERT_TRUE(FillRandom(context, one_view.value(), Uniform01<T>(),
                         RandomKey{0}, last)
                  .ok());
  const std::array<T, 2> before_overflow = storage;
  Status overflow = FillRandom(context, two_view.value(), Uniform01<T>(),
                               RandomKey{0}, last);
  ASSERT_FALSE(overflow.ok());
  EXPECT_EQ(overflow.code(), StatusCode::kOverflow);
  EXPECT_EQ(storage, before_overflow);
}

TYPED_TEST(RandomFillTest, RejectsForeignMetadataTransactionally) {
  using T = TypeParam;
  using ExtentsType = Extents<2, 2>;
  using Mapping = LayoutLeftMapping<ExtentsType>;
  auto mapping = Mapping::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::array<T, 4> storage{T{1}, T{2}, T{3}, T{4}};
  const std::array<T, 4> original = storage;
  const ExecutionContext context = ExecutionContext::Serial();
  const auto fill = [&](const auto& operand) {
    const Status status = FillRandom(context, operand, Uniform01<T>(),
                                     RandomKey{0}, RandomCounter{0, 0});
    EXPECT_EQ(storage, original);
    return status;
  };

  test::RandomTestOperand<T, Mapping> null_operand(nullptr, mapping.value(), 4);
  EXPECT_EQ(fill(null_operand).code(), StatusCode::kInvalidArgument);

  test::RandomTestOperand<T, Mapping> insufficient(
      storage.data(), mapping.value(), 3);
  EXPECT_EQ(fill(insufficient).code(), StatusCode::kOutOfRange);

  test::RandomTestOperand<T, Mapping> negative_available(
      reinterpret_cast<T*>(static_cast<std::uintptr_t>(1)), mapping.value(),
      -1);
  EXPECT_EQ(fill(negative_available).code(), StatusCode::kInvalidArgument);

  test::RandomTestOperand<T, Mapping> device(
      reinterpret_cast<T*>(static_cast<std::uintptr_t>(1)), mapping.value(),
      4, MemorySpace::kDevice);
  EXPECT_EQ(fill(device).code(), StatusCode::kUnsupported);

  test::RandomTestOperand<T, Mapping> negative_extent(
      reinterpret_cast<T*>(static_cast<std::uintptr_t>(1)), mapping.value(),
      4);
  negative_extent.SetExtent(0, -1);
  EXPECT_EQ(fill(negative_extent).code(), StatusCode::kInvalidArgument);

  test::RandomTestOperand<T, Mapping> inconsistent_size(
      reinterpret_cast<T*>(static_cast<std::uintptr_t>(1)), mapping.value(),
      4);
  inconsistent_size.SetSize(3);
  EXPECT_EQ(fill(inconsistent_size).code(), StatusCode::kInvalidArgument);

  test::RandomTestOperand<T, Mapping> negative_stride(
      reinterpret_cast<T*>(static_cast<std::uintptr_t>(1)), mapping.value(),
      4);
  negative_stride.SetStride(0, -1);
  EXPECT_EQ(fill(negative_stride).code(), StatusCode::kInvalidArgument);

  test::RandomTestOperand<T, Mapping> inconsistent_span(
      reinterpret_cast<T*>(static_cast<std::uintptr_t>(1)), mapping.value(),
      4);
  inconsistent_span.SetRequiredSpan(3);
  EXPECT_EQ(fill(inconsistent_span).code(), StatusCode::kInvalidArgument);

  test::RandomTestOperand<T, Mapping> extent_overflow(
      reinterpret_cast<T*>(static_cast<std::uintptr_t>(1)), mapping.value(),
      std::numeric_limits<extent_t>::max());
  extent_overflow.SetExtent(0, std::numeric_limits<extent_t>::max());
  extent_overflow.SetExtent(1, 2);
  extent_overflow.SetSize(std::numeric_limits<extent_t>::max());
  extent_overflow.SetRequiredSpan(std::numeric_limits<extent_t>::max());
  EXPECT_EQ(fill(extent_overflow).code(), StatusCode::kOverflow);

  test::RandomTestOperand<T, Mapping> span_overflow(
      reinterpret_cast<T*>(static_cast<std::uintptr_t>(1)), mapping.value(),
      std::numeric_limits<extent_t>::max());
  span_overflow.SetStride(0, std::numeric_limits<stride_t>::max());
  span_overflow.SetRequiredSpan(std::numeric_limits<extent_t>::max());
  EXPECT_EQ(fill(span_overflow).code(), StatusCode::kOverflow);
}

TYPED_TEST(RandomFillTest, RejectsWrapRiskingByteSpanBeforeDataAccess) {
  using T = TypeParam;
  using ExtentsType = Extents<1, 2>;
  using Mapping = LayoutRightMapping<ExtentsType>;
  auto mapping = Mapping::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  constexpr stride_t kHugeInnerStride =
      std::numeric_limits<stride_t>::max() / 2 + 1;
  constexpr extent_t kRequiredSpan = kHugeInnerStride + 1;
  test::RandomTestOperand<T, Mapping> operand(
      reinterpret_cast<T*>(static_cast<std::uintptr_t>(1)), mapping.value(),
      kRequiredSpan);
  operand.SetStride(0, 0);
  operand.SetStride(1, kHugeInnerStride);
  operand.SetRequiredSpan(kRequiredSpan);

  Status status = FillRandom(ExecutionContext::Serial(), operand,
                             Uniform01<T>(), RandomKey{0},
                             RandomCounter{0, 0});

  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kOverflow);
}

TYPED_TEST(RandomFillTest, RejectsNonUniqueForeignMapping) {
  using T = TypeParam;
  using ExtentsType = Extents<2, 2>;
  using Mapping = LayoutStrideMapping<ExtentsType>;
  auto mapping = Mapping::Create(
      ExtentsType(), std::array<stride_t, 2>{0, 1});
  ASSERT_TRUE(mapping.ok());
  std::array<T, 2> storage{T{7}, T{11}};
  const std::array<T, 2> original = storage;
  test::RandomTestOperand<T, Mapping> operand(storage.data(), mapping.value(),
                                               storage.size());

  Status status = FillRandom(ExecutionContext::Serial(), operand,
                             Uniform01<T>(), RandomKey{0},
                             RandomCounter{0, 0});

  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(storage, original);
}

TYPED_TEST(RandomFillTest, SuccessfulOperationsDoNotAllocate) {
  using T = TypeParam;
  using ExtentsType = Extents<12>;
  auto mapping = LayoutLeftMapping<ExtentsType>::Create(ExtentsType());
  ASSERT_TRUE(mapping.ok());
  std::array<T, 12> output{};
  auto view = TensorView<T, ExtentsType>::Create(
      output.data(), mapping.value(), output.size());
  ASSERT_TRUE(view.ok());
  const ExecutionContext context = ExecutionContext::Serial();
  constexpr Uniform01<T> distribution;

  test::BeginRandomAllocationCount();
  const auto words = Philox4x32_10::Generate(RandomKey{0},
                                             RandomCounter{0, 0});
  const T scalar = distribution(0x6627e8d5e169c58dULL);
  const Status status = FillRandom(context, view.value(), distribution,
                                   RandomKey{0}, RandomCounter{0, 0});
  const std::size_t allocations = test::EndRandomAllocationCount();

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(words[0], 0x6627e8d5U);
  EXPECT_EQ(scalar, KnownUniformValues<T>()[0]);
  EXPECT_EQ(allocations, 0U);
}

}  // namespace
}  // namespace asc
