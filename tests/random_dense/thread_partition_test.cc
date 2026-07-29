#include <array>
#include <cstdint>
#include <span>
#include <thread>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "test_support.h"

namespace {

template <typename Element>
void CheckParallelPartitions(asc_random_dense_test::TestContext& test) {
  constexpr std::size_t kFirst = 31;
  constexpr std::size_t kMiddle = 97;
  constexpr std::size_t kLast = 128;
  constexpr std::size_t kCount = kFirst + kMiddle + kLast;
  constexpr asc::RandomStream kStream = 17;
  constexpr asc::RandomSubsequence kSubsequence = 19;
  constexpr asc::RandomOffset kOffset = 23;
  constexpr asc::RandomOffset kWordsPerElement =
      std::same_as<Element, float> ? 1U : 2U;

  const auto make_mapping = [](std::size_t size) {
    return asc::DenseLayout<1>::Create(
        std::array<asc::extent_t, 1>{static_cast<asc::extent_t>(size)},
        asc::LayoutLeft{});
  };
  auto whole_mapping = make_mapping(kCount);
  auto first_mapping = make_mapping(kFirst);
  auto middle_mapping = make_mapping(kMiddle);
  auto last_mapping = make_mapping(kLast);
  auto empty_mapping = make_mapping(0);
  ASC_RANDOM_DENSE_TEST_CHECK(test, whole_mapping.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, first_mapping.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, middle_mapping.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, last_mapping.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, empty_mapping.ok());
  if (!whole_mapping.ok() || !first_mapping.ok() || !middle_mapping.ok() ||
      !last_mapping.ok() || !empty_mapping.ok()) {
    return;
  }

  std::array<Element, kCount> whole{};
  std::array<Element, kCount> partitioned{};
  auto whole_view = asc::DenseView<Element, 1>::Create(
      whole.data(), *whole_mapping, asc::MemorySpace::kHost);
  auto first_view = asc::DenseView<Element, 1>::Create(
      partitioned.data(), *first_mapping, asc::MemorySpace::kHost);
  auto middle_view = asc::DenseView<Element, 1>::Create(
      partitioned.data() + kFirst, *middle_mapping, asc::MemorySpace::kHost);
  auto last_view = asc::DenseView<Element, 1>::Create(
      partitioned.data() + kFirst + kMiddle, *last_mapping,
      asc::MemorySpace::kHost);
  auto empty_view = asc::DenseView<Element, 1>::Create(
      partitioned.data() + kFirst, *empty_mapping, asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(test, whole_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, first_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, middle_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, last_view.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, empty_view.ok());
  if (!whole_view.ok() || !first_view.ok() || !middle_view.ok() ||
      !last_view.ok() || !empty_view.ok()) {
    return;
  }

  const auto whole_result =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *whole_view,
                              kStream, kSubsequence, kOffset);
  ASC_RANDOM_DENSE_TEST_CHECK(test, whole_result.ok());

  bool first_ok = false;
  bool middle_ok = false;
  bool last_ok = false;
  bool empty_ok = false;
  std::thread last([&] {
    last_ok = asc::FillDenseUniform01(
                  asc::ExecutionContext::Serial(), *last_view, kStream,
                  kSubsequence, kOffset + (kFirst + kMiddle) * kWordsPerElement)
                  .ok();
  });
  std::thread empty([&] {
    auto generated = asc::FillDenseUniform01(
        asc::ExecutionContext::Serial(), *empty_view, kStream, kSubsequence,
        kOffset + kFirst * kWordsPerElement);
    empty_ok =
        generated.ok() && *generated == kOffset + kFirst * kWordsPerElement;
  });
  std::thread first([&] {
    first_ok =
        asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *first_view,
                                kStream, kSubsequence, kOffset)
            .ok();
  });
  std::thread middle([&] {
    middle_ok = asc::FillDenseUniform01(asc::ExecutionContext::Serial(),
                                        *middle_view, kStream, kSubsequence,
                                        kOffset + kFirst * kWordsPerElement)
                    .ok();
  });
  last.join();
  empty.join();
  first.join();
  middle.join();

  ASC_RANDOM_DENSE_TEST_CHECK(test, first_ok);
  ASC_RANDOM_DENSE_TEST_CHECK(test, middle_ok);
  ASC_RANDOM_DENSE_TEST_CHECK(test, last_ok);
  ASC_RANDOM_DENSE_TEST_CHECK(test, empty_ok);
  ASC_RANDOM_DENSE_TEST_EQ(test, partitioned, whole);
}

}  // namespace

int main() {
  asc_random_dense_test::TestContext test;
  CheckParallelPartitions<float>(test);
  CheckParallelPartitions<double>(test);
  return test.Finish();
}
