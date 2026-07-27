#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <thread>
#include <type_traits>

#include "allocation_counter.h"
#include "asc/random/dense.h"
#include "test_support.h"

namespace {

constexpr asc::RandomStream kStream = 0x0123456789abcdefULL;
constexpr asc::RandomSubsequence kSubsequence = 0xfedcba9876543210ULL;
constexpr asc::RandomOffset kOffset = 37;

template <typename Element>
constexpr std::uint64_t kWordsPerElement =
    std::same_as<Element, float> ? 1U : 2U;

template <typename Element>
Element ExpectedValue(std::uint64_t ordinal, asc::RandomStream stream = kStream,
                      asc::RandomSubsequence subsequence = kSubsequence,
                      asc::RandomOffset offset = kOffset) {
  const auto address = offset + ordinal * static_cast<asc::RandomOffset>(
                                              kWordsPerElement<Element>);
  if constexpr (std::same_as<Element, float>) {
    return asc::Uniform01<float>(
        asc::Philox4x32Word(stream, subsequence, address));
  } else {
    return asc::Uniform01<double>(
        asc::Philox4x32Word(stream, subsequence, address),
        asc::Philox4x32Word(stream, subsequence, address + 1U));
  }
}

template <typename Element>
void CheckLogicalValues(asc_random_dense_test::TestContext& context,
                        asc::DenseView<Element, 2> view,
                        asc::RandomStream stream = kStream,
                        asc::RandomSubsequence subsequence = kSubsequence,
                        asc::RandomOffset offset = kOffset) {
  std::uint64_t ordinal = 0;
  for (asc::index_t column = 0; column < view.shape()[1]; ++column) {
    for (asc::index_t row = 0; row < view.shape()[0]; ++row) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto value = view.At(coordinate);
      ASC_RANDOM_DENSE_TEST_CHECK(context, value.ok());
      if (value.ok()) {
        ASC_RANDOM_DENSE_TEST_EQ(context, **value,
                                 ExpectedValue<std::remove_const_t<Element>>(
                                     ordinal, stream, subsequence, offset));
      }
      ++ordinal;
    }
  }
}

void CheckRankZeroAndZeroExtent(asc_random_dense_test::TestContext& context) {
  const std::array<asc::extent_t, 0> scalar_shape{};
  auto scalar_mapping =
      asc::DenseLayoutMapping<0>::Create(asc::LayoutLeft{}, scalar_shape);
  ASC_RANDOM_DENSE_TEST_CHECK(context, scalar_mapping.ok());
  if (!scalar_mapping.ok()) {
    return;
  }
  double scalar = -1.0;
  auto scalar_view = asc::DenseView<double, 0>::Create(&scalar, *scalar_mapping,
                                                       asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(context, scalar_view.ok());
  if (!scalar_view.ok()) {
    return;
  }
  const auto scalar_next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *scalar_view,
                              kStream, kSubsequence, kOffset);
  ASC_RANDOM_DENSE_TEST_CHECK(context, scalar_next.ok());
  if (scalar_next.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(context, *scalar_next, kOffset + 2U);
  }
  ASC_RANDOM_DENSE_TEST_EQ(context, scalar, ExpectedValue<double>(0));

  const std::array<asc::extent_t, 3> empty_shape{2, 0, 3};
  auto empty_mapping =
      asc::DenseLayoutMapping<3>::Create(asc::LayoutLeft{}, empty_shape);
  ASC_RANDOM_DENSE_TEST_CHECK(context, empty_mapping.ok());
  if (!empty_mapping.ok()) {
    return;
  }
  auto empty_view = asc::DenseView<float, 3>::Create(nullptr, *empty_mapping,
                                                     asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(context, empty_view.ok());
  if (!empty_view.ok()) {
    return;
  }
  const auto empty_next = asc::FillDenseUniform01(
      asc::ExecutionContext::Serial(), *empty_view, kStream, kSubsequence,
      std::numeric_limits<asc::RandomOffset>::max());
  ASC_RANDOM_DENSE_TEST_CHECK(context, empty_next.ok());
  if (empty_next.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(context, *empty_next,
                             std::numeric_limits<asc::RandomOffset>::max());
  }
}

template <typename Element>
void CheckLayoutsAndPadding(asc_random_dense_test::TestContext& context) {
  constexpr std::array<asc::extent_t, 2> kShape{2, 3};
  constexpr std::array<asc::stride_t, 2> kPaddedStrides{1, 4};
  auto left = asc::DenseLayoutMapping<2>::Create(asc::LayoutLeft{}, kShape);
  auto right = asc::DenseLayoutMapping<2>::Create(asc::LayoutRight{}, kShape);
  auto padded = asc::DenseLayoutMapping<2>::Create(asc::LayoutStride{}, kShape,
                                                   kPaddedStrides);
  ASC_RANDOM_DENSE_TEST_CHECK(context, left.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(context, right.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(context, padded.ok());
  if (!left.ok() || !right.ok() || !padded.ok()) {
    return;
  }

  std::array<Element, 6> left_storage{};
  std::array<Element, 6> right_storage{};
  std::array<Element, 10> padded_storage{};
  padded_storage.fill(static_cast<Element>(-7));
  auto left_view = asc::DenseView<Element, 2>::Create(
      left_storage.data(), *left, asc::MemorySpace::kHost);
  auto right_view = asc::DenseView<Element, 2>::Create(
      right_storage.data(), *right, asc::MemorySpace::kHost);
  auto padded_view = asc::DenseView<Element, 2>::Create(
      padded_storage.data(), *padded, asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(context, left_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(context, right_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(context, padded_view.ok());
  if (!left_view.ok() || !right_view.ok() || !padded_view.ok()) {
    return;
  }

  const auto left_next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *left_view,
                              kStream, kSubsequence, kOffset);
  const auto right_next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *right_view,
                              kStream, kSubsequence, kOffset);
  const auto padded_next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *padded_view,
                              kStream, kSubsequence, kOffset);
  ASC_RANDOM_DENSE_TEST_CHECK(context, left_next.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(context, right_next.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(context, padded_next.ok());
  const auto expected_next =
      kOffset + 6U * static_cast<asc::RandomOffset>(kWordsPerElement<Element>);
  if (left_next.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(context, *left_next, expected_next);
  }
  if (right_next.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(context, *right_next, expected_next);
  }
  if (padded_next.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(context, *padded_next, expected_next);
  }

  CheckLogicalValues(context, *left_view);
  CheckLogicalValues(context, *right_view);
  CheckLogicalValues(context, *padded_view);

  std::array<bool, 10> used{};
  for (asc::index_t column = 0; column < kShape[1]; ++column) {
    for (asc::index_t row = 0; row < kShape[0]; ++row) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto physical = padded->Offset(coordinate);
      ASC_RANDOM_DENSE_TEST_CHECK(context, physical.ok());
      if (physical.ok()) {
        used[static_cast<std::size_t>(*physical)] = true;
      }
    }
  }
  for (std::size_t index = 0; index < padded_storage.size(); ++index) {
    if (!used[index]) {
      ASC_RANDOM_DENSE_TEST_EQ(context, padded_storage[index],
                               static_cast<Element>(-7));
    }
  }
}

template <typename Element>
void CheckPartitionEquivalence(asc_random_dense_test::TestContext& context) {
  constexpr std::array<asc::extent_t, 2> kShape{2, 3};
  constexpr std::array<asc::stride_t, 2> kStrides{1, 4};
  auto mapping =
      asc::DenseLayoutMapping<2>::Create(asc::LayoutStride{}, kShape, kStrides);
  ASC_RANDOM_DENSE_TEST_CHECK(context, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  std::array<Element, 10> whole_storage{};
  std::array<Element, 10> partitioned_storage{};
  whole_storage.fill(static_cast<Element>(-11));
  partitioned_storage.fill(static_cast<Element>(-11));
  auto whole = asc::DenseView<Element, 2>::Create(
      whole_storage.data(), *mapping, asc::MemorySpace::kHost);
  auto partitioned = asc::DenseView<Element, 2>::Create(
      partitioned_storage.data(), *mapping, asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(context, whole.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(context, partitioned.ok());
  if (!whole.ok() || !partitioned.ok()) {
    return;
  }
  const auto whole_next = asc::FillDenseUniform01(
      asc::ExecutionContext::Serial(), *whole, kStream, kSubsequence, kOffset);
  ASC_RANDOM_DENSE_TEST_CHECK(context, whole_next.ok());

  for (asc::index_t column = 0; column < kShape[1]; ++column) {
    const std::array<asc::index_t, 2> offsets{0, column};
    const std::array<asc::extent_t, 2> extents{2, 1};
    auto part = partitioned->Subview(offsets, extents);
    ASC_RANDOM_DENSE_TEST_CHECK(context, part.ok());
    if (!part.ok()) {
      return;
    }
    const auto part_offset =
        kOffset + static_cast<asc::RandomOffset>(column * 2) *
                      static_cast<asc::RandomOffset>(kWordsPerElement<Element>);
    const auto part_next =
        asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *part, kStream,
                                kSubsequence, part_offset);
    ASC_RANDOM_DENSE_TEST_CHECK(context, part_next.ok());
    if (part_next.ok()) {
      ASC_RANDOM_DENSE_TEST_EQ(
          context, *part_next,
          part_offset +
              2U * static_cast<asc::RandomOffset>(kWordsPerElement<Element>));
    }
  }
  ASC_RANDOM_DENSE_TEST_EQ(context, partitioned_storage, whole_storage);
}

void CheckFailuresAndAllocations(asc_random_dense_test::TestContext& context) {
  constexpr std::array<asc::extent_t, 1> kShape{2};
  auto mapping = asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kShape);
  ASC_RANDOM_DENSE_TEST_CHECK(context, mapping.ok());
  if (!mapping.ok()) {
    return;
  }

  std::array<double, 2> values{9.0, 10.0};
  auto host_view = asc::DenseView<double, 1>::Create(values.data(), *mapping,
                                                     asc::MemorySpace::kHost);
  auto device_view = asc::DenseView<double, 1>::Create(
      values.data(), *mapping, asc::MemorySpace::kDevice);
  ASC_RANDOM_DENSE_TEST_CHECK(context, host_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(context, device_view.ok());
  if (!host_view.ok() || !device_view.ok()) {
    return;
  }

  const auto before_device = values;
  const auto device_status =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *device_view,
                              kStream, kSubsequence, kOffset);
  ASC_RANDOM_DENSE_TEST_CHECK(context, !device_status.ok());
  if (!device_status.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(context, device_status.status().code(),
                             asc::ErrorCode::kUnsupported);
  }
  ASC_RANDOM_DENSE_TEST_EQ(context, values, before_device);

  const auto maximum = std::numeric_limits<asc::RandomOffset>::max();
  const auto before_overflow = values;
  const auto overflow =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *host_view,
                              kStream, kSubsequence, maximum - 3U);
  ASC_RANDOM_DENSE_TEST_CHECK(context, !overflow.ok());
  if (!overflow.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(context, overflow.status().code(),
                             asc::ErrorCode::kOverflow);
  }
  ASC_RANDOM_DENSE_TEST_EQ(context, values, before_overflow);

  bool allocation_call_ok = false;
  asc::RandomOffset allocation_next = 0;
  std::size_t allocations = 0;
  {
    asc_random_dense_test::AllocationCountScope scope;
    const auto generated =
        asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *host_view,
                                kStream, kSubsequence, kOffset);
    allocation_call_ok = generated.ok();
    if (generated.ok()) {
      allocation_next = *generated;
    }
    allocations = scope.count();
  }
  ASC_RANDOM_DENSE_TEST_CHECK(context, allocation_call_ok);
  ASC_RANDOM_DENSE_TEST_EQ(context, allocation_next, kOffset + 4U);
  ASC_RANDOM_DENSE_TEST_EQ(context, allocations, std::size_t{0});

  const auto unavailable_context = asc::ExecutionContext::Create(
      asc::Backend::kCuda, asc::Device{asc::Backend::kCuda, 0});
  ASC_RANDOM_DENSE_TEST_CHECK(context, !unavailable_context.ok());
}

void CheckDeterminismAndConcurrency(
    asc_random_dense_test::TestContext& context) {
  constexpr std::array<asc::extent_t, 2> kShape{4, 3};
  auto mapping = asc::DenseLayoutMapping<2>::Create(asc::LayoutRight{}, kShape);
  ASC_RANDOM_DENSE_TEST_CHECK(context, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  std::array<float, 12> first{};
  std::array<float, 12> second{};
  auto first_view = asc::DenseView<float, 2>::Create(first.data(), *mapping,
                                                     asc::MemorySpace::kHost);
  auto second_view = asc::DenseView<float, 2>::Create(second.data(), *mapping,
                                                      asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(context, first_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(context, second_view.ok());
  if (!first_view.ok() || !second_view.ok()) {
    return;
  }
  const auto first_next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *first_view,
                              kStream, kSubsequence, kOffset);
  const auto second_next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *second_view,
                              kStream, kSubsequence, kOffset);
  ASC_RANDOM_DENSE_TEST_CHECK(context, first_next.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(context, second_next.ok());
  ASC_RANDOM_DENSE_TEST_EQ(context, first, second);

  std::array<float, 12> concurrent_a{};
  std::array<float, 12> concurrent_b{};
  auto view_a = asc::DenseView<float, 2>::Create(concurrent_a.data(), *mapping,
                                                 asc::MemorySpace::kHost);
  auto view_b = asc::DenseView<float, 2>::Create(concurrent_b.data(), *mapping,
                                                 asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(context, view_a.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(context, view_b.ok());
  if (!view_a.ok() || !view_b.ok()) {
    return;
  }
  bool ok_a = false;
  bool ok_b = false;
  asc::RandomOffset next_a = 0;
  asc::RandomOffset next_b = 0;
  std::thread thread_a([&] {
    auto result = asc::FillDenseUniform01(asc::ExecutionContext::Serial(),
                                          *view_a, 11, 12, 13);
    ok_a = result.ok();
    if (result.ok()) {
      next_a = *result;
    }
  });
  std::thread thread_b([&] {
    auto result = asc::FillDenseUniform01(asc::ExecutionContext::Serial(),
                                          *view_b, 21, 22, 23);
    ok_b = result.ok();
    if (result.ok()) {
      next_b = *result;
    }
  });
  thread_a.join();
  thread_b.join();
  ASC_RANDOM_DENSE_TEST_CHECK(context, ok_a);
  ASC_RANDOM_DENSE_TEST_CHECK(context, ok_b);
  ASC_RANDOM_DENSE_TEST_EQ(context, next_a, asc::RandomOffset{25});
  ASC_RANDOM_DENSE_TEST_EQ(context, next_b, asc::RandomOffset{35});
  CheckLogicalValues(context, *view_a, 11, 12, 13);
  CheckLogicalValues(context, *view_b, 21, 22, 23);
}

}  // namespace

int main() {
  asc_random_dense_test::TestContext context;
  CheckRankZeroAndZeroExtent(context);
  CheckLayoutsAndPadding<float>(context);
  CheckLayoutsAndPadding<double>(context);
  CheckPartitionEquivalence<float>(context);
  CheckPartitionEquivalence<double>(context);
  CheckFailuresAndAllocations(context);
  CheckDeterminismAndConcurrency(context);
  return context.Finish();
}
