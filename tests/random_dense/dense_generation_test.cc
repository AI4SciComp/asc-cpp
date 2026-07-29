#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "test_support.h"

namespace {

using asc_random_dense_test::TestContext;

template <typename Element>
Element ExpectedValue(asc::RandomStream stream,
                      asc::RandomSubsequence subsequence,
                      asc::RandomOffset offset, std::uint64_t ordinal) {
  if constexpr (std::same_as<Element, float>) {
    return asc::Uniform01<float>(
        asc::GeneratePhilox4x32Word(stream, subsequence, offset + ordinal));
  } else {
    const std::uint64_t first = 2 * ordinal;
    return asc::Uniform01<double>(
        asc::GeneratePhilox4x32Word(stream, subsequence, offset + first),
        asc::GeneratePhilox4x32Word(stream, subsequence, offset + first + 1));
  }
}

template <typename Element, std::size_t Rank>
void CheckLogicalValues(TestContext& test,
                        const asc::DenseView<Element, Rank>& view,
                        asc::RandomStream stream,
                        asc::RandomSubsequence subsequence,
                        asc::RandomOffset offset) {
  std::array<asc::index_t, Rank> coordinate{};
  for (std::uint64_t ordinal = 0;
       ordinal < static_cast<std::uint64_t>(view.logical_size()); ++ordinal) {
    std::uint64_t remaining = ordinal;
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      const auto extent = static_cast<std::uint64_t>(view.extents()[dimension]);
      coordinate[dimension] = static_cast<asc::index_t>(remaining % extent);
      remaining /= extent;
    }
    auto value = view.At(std::span<const asc::index_t, Rank>(coordinate));
    ASC_RANDOM_DENSE_TEST_CHECK(test, value.ok());
    if (value.ok()) {
      ASC_RANDOM_DENSE_TEST_EQ(
          test, **value,
          ExpectedValue<Element>(stream, subsequence, offset, ordinal));
    }
  }
}

template <typename Element>
constexpr asc::RandomOffset WordCount(asc::extent_t logical_size) {
  return static_cast<asc::RandomOffset>(logical_size) *
         (std::same_as<Element, float> ? 1U : 2U);
}

template <typename Element, typename Layout>
void CheckRankTwoLayout(TestContext& test, Layout layout) {
  constexpr asc::RandomStream kStream = 0x0123456789ABCDEFULL;
  constexpr asc::RandomSubsequence kSubsequence = 0xA1B2C3D4E5F60718ULL;
  constexpr asc::RandomOffset kOffset = 37;
  constexpr std::array<asc::extent_t, 2> kExtents{2, 3};

  auto mapping = asc::DenseLayout<2>::Create(
      std::span<const asc::extent_t, 2>(kExtents), layout);
  ASC_RANDOM_DENSE_TEST_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  std::array<Element, 10> storage{};
  storage.fill(static_cast<Element>(-3));
  auto view = asc::DenseView<Element, 2>::Create(storage.data(), *mapping,
                                                 asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  auto next = asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *view,
                                      kStream, kSubsequence, kOffset);
  ASC_RANDOM_DENSE_TEST_CHECK(test, next.ok());
  if (next.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(test, *next, kOffset + WordCount<Element>(6));
  }
  CheckLogicalValues(test, *view, kStream, kSubsequence, kOffset);
}

void CheckLayoutsAndPadding(TestContext& test) {
  CheckRankTwoLayout<float>(test, asc::LayoutLeft{});
  CheckRankTwoLayout<double>(test, asc::LayoutRight{});

  constexpr asc::RandomStream kStream = 9;
  constexpr asc::RandomSubsequence kSubsequence = 11;
  constexpr asc::RandomOffset kOffset = 13;
  constexpr std::array<asc::extent_t, 2> kExtents{2, 3};
  auto mapping =
      asc::DenseLayout<2>::Create(std::span<const asc::extent_t, 2>(kExtents),
                                  asc::LayoutStride<2>{.strides = {1, 4}});
  ASC_RANDOM_DENSE_TEST_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  std::array<float, 10> storage{};
  storage.fill(-7.0F);
  auto view = asc::DenseView<float, 2>::Create(storage.data(), *mapping,
                                               asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  auto next = asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *view,
                                      kStream, kSubsequence, kOffset);
  ASC_RANDOM_DENSE_TEST_CHECK(test, next.ok());
  CheckLogicalValues(test, *view, kStream, kSubsequence, kOffset);
  constexpr std::array<std::size_t, 4> kHoles{2, 3, 6, 7};
  for (std::size_t hole : kHoles) {
    ASC_RANDOM_DENSE_TEST_EQ(test, storage[hole], -7.0F);
  }
}

void CheckRankZeroAndZeroExtent(TestContext& test) {
  constexpr asc::RandomStream kStream = 3;
  constexpr asc::RandomSubsequence kSubsequence = 5;
  constexpr asc::RandomOffset kOffset = 7;

  constexpr std::array<asc::extent_t, 0> kScalarExtents{};
  auto scalar_mapping = asc::DenseLayout<0>::Create(
      std::span<const asc::extent_t, 0>(kScalarExtents), asc::LayoutLeft{});
  ASC_RANDOM_DENSE_TEST_CHECK(test, scalar_mapping.ok());
  double scalar = -1.0;
  if (scalar_mapping.ok()) {
    auto scalar_view = asc::DenseView<double, 0>::Create(
        &scalar, *scalar_mapping, asc::MemorySpace::kHost);
    ASC_RANDOM_DENSE_TEST_CHECK(test, scalar_view.ok());
    if (scalar_view.ok()) {
      auto next =
          asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *scalar_view,
                                  kStream, kSubsequence, kOffset);
      ASC_RANDOM_DENSE_TEST_CHECK(test, next.ok());
      if (next.ok()) {
        ASC_RANDOM_DENSE_TEST_EQ(test, *next, kOffset + 2);
      }
      ASC_RANDOM_DENSE_TEST_EQ(
          test, scalar,
          ExpectedValue<double>(kStream, kSubsequence, kOffset, 0));
    }
  }

  constexpr std::array<asc::extent_t, 3> kEmptyExtents{2, 0, 3};
  auto empty_mapping = asc::DenseLayout<3>::Create(
      std::span<const asc::extent_t, 3>(kEmptyExtents), asc::LayoutRight{});
  ASC_RANDOM_DENSE_TEST_CHECK(test, empty_mapping.ok());
  if (empty_mapping.ok()) {
    auto empty_view = asc::DenseView<float, 3>::Create(nullptr, *empty_mapping,
                                                       asc::MemorySpace::kHost);
    ASC_RANDOM_DENSE_TEST_CHECK(test, empty_view.ok());
    if (empty_view.ok()) {
      auto next =
          asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *empty_view,
                                  kStream, kSubsequence, kOffset);
      ASC_RANDOM_DENSE_TEST_CHECK(test, next.ok());
      if (next.ok()) {
        ASC_RANDOM_DENSE_TEST_EQ(test, *next, kOffset);
      }
    }
  }
}

void CheckPartitionAndRerun(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kExtents{2, 4};
  constexpr std::array<asc::extent_t, 2> kPartitionExtents{2, 2};
  constexpr std::array<asc::index_t, 2> kFirstOffset{0, 0};
  constexpr std::array<asc::index_t, 2> kSecondOffset{0, 2};
  constexpr asc::RandomStream kStream = 101;
  constexpr asc::RandomSubsequence kSubsequence = 103;
  constexpr asc::RandomOffset kOffset = 107;

  auto mapping = asc::DenseLayout<2>::Create(
      std::span<const asc::extent_t, 2>(kExtents), asc::LayoutLeft{});
  ASC_RANDOM_DENSE_TEST_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  std::array<float, 8> whole{};
  std::array<float, 8> partitioned{};
  std::array<float, 8> rerun{};
  auto whole_view = asc::DenseView<float, 2>::Create(whole.data(), *mapping,
                                                     asc::MemorySpace::kHost);
  auto partitioned_view = asc::DenseView<float, 2>::Create(
      partitioned.data(), *mapping, asc::MemorySpace::kHost);
  auto rerun_view = asc::DenseView<float, 2>::Create(rerun.data(), *mapping,
                                                     asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(test, whole_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, partitioned_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, rerun_view.ok());
  if (!whole_view.ok() || !partitioned_view.ok() || !rerun_view.ok()) {
    return;
  }
  auto whole_next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *whole_view,
                              kStream, kSubsequence, kOffset);
  auto rerun_next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *rerun_view,
                              kStream, kSubsequence, kOffset);
  auto first = partitioned_view->Subview(
      std::span<const asc::index_t, 2>(kFirstOffset),
      std::span<const asc::extent_t, 2>(kPartitionExtents));
  auto second = partitioned_view->Subview(
      std::span<const asc::index_t, 2>(kSecondOffset),
      std::span<const asc::extent_t, 2>(kPartitionExtents));
  ASC_RANDOM_DENSE_TEST_CHECK(test, first.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, second.ok());
  if (!first.ok() || !second.ok()) {
    return;
  }
  auto first_next = asc::FillDenseUniform01(
      asc::ExecutionContext::Serial(), *first, kStream, kSubsequence, kOffset);
  auto second_next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *second, kStream,
                              kSubsequence, kOffset + 4);
  ASC_RANDOM_DENSE_TEST_CHECK(test, whole_next.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, rerun_next.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, first_next.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, second_next.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, whole, rerun);
  ASC_RANDOM_DENSE_TEST_EQ(test, whole, partitioned);
  if (first_next.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(test, *first_next, kOffset + 4);
  }
  if (second_next.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(test, *second_next, kOffset + 8);
  }
}

template <typename Element>
void CheckIrregularReorderedPartitions(TestContext& test) {
  constexpr asc::extent_t kLogicalSize = 11;
  constexpr asc::RandomStream kStream = 211;
  constexpr asc::RandomSubsequence kSubsequence = 223;
  constexpr asc::RandomOffset kOffset = 227;
  constexpr asc::RandomOffset kWordsPerElement =
      std::same_as<Element, float> ? 1U : 2U;
  constexpr std::array<asc::extent_t, 1> kWholeExtents{kLogicalSize};
  constexpr std::array<asc::extent_t, 1> kFirstExtents{3};
  constexpr std::array<asc::extent_t, 1> kMiddleExtents{5};
  constexpr std::array<asc::extent_t, 1> kLastExtents{3};
  constexpr std::array<asc::extent_t, 1> kEmptyExtents{0};

  const auto whole_mapping =
      asc::DenseLayout<1>::Create(kWholeExtents, asc::LayoutLeft{});
  const auto first_mapping =
      asc::DenseLayout<1>::Create(kFirstExtents, asc::LayoutLeft{});
  const auto middle_mapping =
      asc::DenseLayout<1>::Create(kMiddleExtents, asc::LayoutLeft{});
  const auto last_mapping =
      asc::DenseLayout<1>::Create(kLastExtents, asc::LayoutLeft{});
  const auto empty_mapping =
      asc::DenseLayout<1>::Create(kEmptyExtents, asc::LayoutLeft{});
  ASC_RANDOM_DENSE_TEST_CHECK(test, whole_mapping.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, first_mapping.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, middle_mapping.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, last_mapping.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, empty_mapping.ok());
  if (!whole_mapping.ok() || !first_mapping.ok() || !middle_mapping.ok() ||
      !last_mapping.ok() || !empty_mapping.ok()) {
    return;
  }

  std::array<Element, kLogicalSize> whole{};
  std::array<Element, kLogicalSize> partitioned{};
  auto whole_view = asc::DenseView<Element, 1>::Create(
      whole.data(), *whole_mapping, asc::MemorySpace::kHost);
  auto first_view = asc::DenseView<Element, 1>::Create(
      partitioned.data(), *first_mapping, asc::MemorySpace::kHost);
  auto middle_view = asc::DenseView<Element, 1>::Create(
      partitioned.data() + 3, *middle_mapping, asc::MemorySpace::kHost);
  auto last_view = asc::DenseView<Element, 1>::Create(
      partitioned.data() + 8, *last_mapping, asc::MemorySpace::kHost);
  auto empty_view = asc::DenseView<Element, 1>::Create(
      partitioned.data() + 3, *empty_mapping, asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(test, whole_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, first_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, middle_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, last_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, empty_view.ok());
  if (!whole_view.ok() || !first_view.ok() || !middle_view.ok() ||
      !last_view.ok() || !empty_view.ok()) {
    return;
  }

  const auto whole_next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *whole_view,
                              kStream, kSubsequence, kOffset);
  // Deliberately submit the disjoint partitions in reverse/noncontiguous
  // order, including a zero-length partition between populated calls.
  const auto last_next = asc::FillDenseUniform01(
      asc::ExecutionContext::Serial(), *last_view, kStream, kSubsequence,
      kOffset + 8U * kWordsPerElement);
  const auto empty_next = asc::FillDenseUniform01(
      asc::ExecutionContext::Serial(), *empty_view, kStream, kSubsequence,
      kOffset + 3U * kWordsPerElement);
  const auto first_next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *first_view,
                              kStream, kSubsequence, kOffset);
  const auto middle_next = asc::FillDenseUniform01(
      asc::ExecutionContext::Serial(), *middle_view, kStream, kSubsequence,
      kOffset + 3U * kWordsPerElement);
  ASC_RANDOM_DENSE_TEST_CHECK(test, whole_next.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, last_next.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, empty_next.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, first_next.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, middle_next.ok());
  ASC_RANDOM_DENSE_TEST_EQ(test, partitioned, whole);
  if (whole_next.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(
        test, *whole_next,
        kOffset +
            static_cast<asc::RandomOffset>(kLogicalSize) * kWordsPerElement);
  }
  if (empty_next.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(test, *empty_next,
                             kOffset + 3U * kWordsPerElement);
  }
}

void CheckFailureTransactions(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kExtents{2, 3};
  auto mapping = asc::DenseLayout<2>::Create(
      std::span<const asc::extent_t, 2>(kExtents), asc::LayoutLeft{});
  ASC_RANDOM_DENSE_TEST_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  std::array<float, 6> storage{};
  storage.fill(-11.0F);
  const auto original = storage;
  auto view = asc::DenseView<float, 2>::Create(storage.data(), *mapping,
                                               asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  auto overflow = asc::FillDenseUniform01(
      asc::ExecutionContext::Serial(), *view, 1, 2,
      std::numeric_limits<asc::RandomOffset>::max() - 4);
  ASC_RANDOM_DENSE_TEST_CHECK(test, !overflow.ok());
  if (!overflow.ok()) {
    ASC_RANDOM_DENSE_TEST_EQ(test, overflow.status().code(),
                             asc::ErrorCode::kOverflow);
  }
  ASC_RANDOM_DENSE_TEST_EQ(test, storage, original);

  auto device_view = asc::DenseView<float, 2>::Create(
      storage.data(), *mapping, asc::MemorySpace::kDevice);
  ASC_RANDOM_DENSE_TEST_CHECK(test, device_view.ok());
  if (device_view.ok()) {
    auto rejected = asc::FillDenseUniform01(asc::ExecutionContext::Serial(),
                                            *device_view, 1, 2, 3);
    ASC_RANDOM_DENSE_TEST_CHECK(test, !rejected.ok());
    if (!rejected.ok()) {
      ASC_RANDOM_DENSE_TEST_EQ(test, rejected.status().code(),
                               asc::ErrorCode::kMemoryAccess);
    }
  }
  ASC_RANDOM_DENSE_TEST_EQ(test, storage, original);
}

}  // namespace

int main() {
  TestContext test;
  CheckLayoutsAndPadding(test);
  CheckRankZeroAndZeroExtent(test);
  CheckPartitionAndRerun(test);
  CheckIrregularReorderedPartitions<float>(test);
  CheckIrregularReorderedPartitions<double>(test);
  CheckFailureTransactions(test);
  return test.Finish();
}
