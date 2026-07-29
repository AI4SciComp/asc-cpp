#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "../random_cuda/philox_oracle.h"
#include "../random_cuda/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/providers/dense_cuda.h"

namespace {

using asc_random_cuda_test::TestContext;

struct Fixture {
  asc::ExecutionContext execution;
  std::unique_ptr<asc::CudaMemoryResource> resource;
};

asc::Result<Fixture> MakeFixture() {
  auto execution =
      asc::CreateCudaExecutionContext(asc_random_cuda_test::kCudaDevice);
  if (!execution.ok()) {
    return execution.status();
  }
  auto resource = asc::CudaMemoryResource::Create(
      asc_random_cuda_test::kCudaDevice, asc::MemorySpace::kDevice);
  if (!resource.ok()) {
    return resource.status();
  }
  return Fixture{*execution, std::move(*resource)};
}

template <typename Element>
Element Expected(asc::RandomStream stream, asc::RandomSubsequence subsequence,
                 asc::RandomOffset offset, std::uint64_t ordinal) {
  if constexpr (std::same_as<Element, float>) {
    return asc_random_cuda_test::Uniform01FloatOracle(
        asc_random_cuda_test::PhiloxWordOracle(stream, subsequence,
                                               offset + ordinal));
  } else {
    return asc_random_cuda_test::Uniform01DoubleOracle(
        asc_random_cuda_test::PhiloxWordOracle(stream, subsequence,
                                               offset + 2U * ordinal),
        asc_random_cuda_test::PhiloxWordOracle(stream, subsequence,
                                               offset + 2U * ordinal + 1U));
  }
}

template <typename Element, std::size_t Rank>
void CheckLogical(const std::vector<Element>& physical,
                  const asc::DenseView<Element, Rank>& view,
                  asc::RandomStream stream, asc::RandomSubsequence subsequence,
                  asc::RandomOffset offset, TestContext& test) {
  for (std::uint64_t ordinal = 0;
       ordinal < static_cast<std::uint64_t>(view.logical_size()); ++ordinal) {
    std::uint64_t remaining = ordinal;
    std::uint64_t physical_offset = 0;
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      const std::uint64_t extent =
          static_cast<std::uint64_t>(view.extents()[dimension]);
      const std::uint64_t coordinate = remaining % extent;
      remaining /= extent;
      physical_offset +=
          coordinate * static_cast<std::uint64_t>(view.strides()[dimension]);
    }
    ASC_M7_CUDA_BIT_EQ(test,
                       physical[static_cast<std::size_t>(physical_offset)],
                       Expected<Element>(stream, subsequence, offset, ordinal));
  }
}

template <typename Element, typename Layout>
void CheckRankTwo(Fixture& fixture, Layout layout, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kExtents = {3, 5};
  constexpr asc::RandomStream kStream = UINT64_C(0x0123456789abcdef);
  constexpr asc::RandomSubsequence kSubsequence = UINT64_C(0xfedcba9876543210);
  constexpr asc::RandomOffset kOffset = 11;
  auto mapping = asc::DenseLayout<2>::Create(kExtents, layout);
  ASC_M7_CUDA_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  std::vector<Element> sentinels(mapping->required_span_size(), Element{-7});
  auto device = asc_random_cuda_test::Upload<Element>(
      sentinels, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, device.ok());
  if (!device.ok()) {
    return;
  }
  auto view =
      asc::DenseView<Element, 2>::Create(static_cast<Element*>(device->data()),
                                         *mapping, asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  auto generated = asc::CudaFillDenseUniform01(fixture.execution, *view,
                                               kStream, kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, generated.ok());
  if (!generated.ok()) {
    return;
  }
  constexpr asc::RandomOffset kWords = std::same_as<Element, float> ? 15U : 30U;
  ASC_M7_CUDA_EQ(test, generated->next_offset, kOffset + kWords);
  ASC_M7_CUDA_CHECK(test, generated->completion.Wait().ok());
  auto actual =
      asc_random_cuda_test::Download<Element>(*device, fixture.execution);
  ASC_M7_CUDA_CHECK(test, actual.ok());
  if (actual.ok()) {
    CheckLogical(*actual, *view, kStream, kSubsequence, kOffset, test);
  }
}

void CheckPadding(Fixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kExtents = {2, 3};
  constexpr std::array<asc::stride_t, 2> kStrides = {1, 4};
  auto mapping = asc::DenseLayout<2>::Create(
      kExtents, asc::LayoutStride<2>{.strides = kStrides});
  ASC_M7_CUDA_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  std::vector<float> sentinels(mapping->required_span_size(), -91.0F);
  auto device = asc_random_cuda_test::Upload<float>(
      sentinels, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, device.ok());
  if (!device.ok()) {
    return;
  }
  auto view = asc::DenseView<float, 2>::Create(
      static_cast<float*>(device->data()), *mapping, asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  auto generated =
      asc::CudaFillDenseUniform01(fixture.execution, *view, 3, 5, 7);
  ASC_M7_CUDA_CHECK(test, generated.ok());
  if (generated.ok()) {
    ASC_M7_CUDA_CHECK(test, generated->completion.Wait().ok());
  }
  auto actual =
      asc_random_cuda_test::Download<float>(*device, fixture.execution);
  ASC_M7_CUDA_CHECK(test, actual.ok());
  if (!actual.ok()) {
    return;
  }
  CheckLogical(*actual, *view, 3, 5, 7, test);
  for (std::size_t hole :
       {std::size_t{2}, std::size_t{3}, std::size_t{6}, std::size_t{7}}) {
    ASC_M7_CUDA_EQ(test, (*actual)[hole], -91.0F);
  }
}

void CheckRankZeroAndEmpty(Fixture& fixture, TestContext& test) {
  const std::array<asc::extent_t, 0> scalar_extents{};
  auto scalar_mapping =
      asc::DenseLayout<0>::Create(scalar_extents, asc::LayoutLeft{});
  std::array<double, 1> sentinel = {-1.0};
  auto scalar_device = asc_random_cuda_test::Upload<double>(
      sentinel, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, scalar_mapping.ok());
  ASC_M7_CUDA_CHECK(test, scalar_device.ok());
  if (scalar_mapping.ok() && scalar_device.ok()) {
    auto scalar = asc::DenseView<double, 0>::Create(
        static_cast<double*>(scalar_device->data()), *scalar_mapping,
        asc::MemorySpace::kDevice);
    ASC_M7_CUDA_CHECK(test, scalar.ok());
    if (scalar.ok()) {
      auto generated =
          asc::CudaFillDenseUniform01(fixture.execution, *scalar, 13, 17, 19);
      ASC_M7_CUDA_CHECK(test, generated.ok());
      if (generated.ok()) {
        ASC_M7_CUDA_EQ(test, generated->next_offset, asc::RandomOffset{21});
        ASC_M7_CUDA_CHECK(test, generated->completion.Wait().ok());
      }
      auto actual = asc_random_cuda_test::Download<double>(*scalar_device,
                                                           fixture.execution);
      ASC_M7_CUDA_CHECK(test, actual.ok());
      if (actual.ok()) {
        ASC_M7_CUDA_BIT_EQ(test, actual->front(),
                           Expected<double>(13, 17, 19, 0));
      }
    }
  }

  constexpr std::array<asc::extent_t, 3> kEmptyExtents = {2, 0, 3};
  auto empty_mapping =
      asc::DenseLayout<3>::Create(kEmptyExtents, asc::LayoutRight{});
  ASC_M7_CUDA_CHECK(test, empty_mapping.ok());
  if (empty_mapping.ok()) {
    auto empty = asc::DenseView<float, 3>::Create(nullptr, *empty_mapping,
                                                  asc::MemorySpace::kDevice);
    ASC_M7_CUDA_CHECK(test, empty.ok());
    if (empty.ok()) {
      auto generated =
          asc::CudaFillDenseUniform01(fixture.execution, *empty, 23, 29, 31);
      ASC_M7_CUDA_CHECK(test, generated.ok());
      if (generated.ok()) {
        ASC_M7_CUDA_EQ(test, generated->next_offset, asc::RandomOffset{31});
        ASC_M7_CUDA_CHECK(test, generated->completion.Wait().ok());
      }
    }
  }
}

void CheckPartitionAndFailures(Fixture& fixture, TestContext& test) {
  constexpr std::size_t kFirst = 257;
  constexpr std::size_t kSecond = 773;
  constexpr std::size_t kCount = kFirst + kSecond;
  constexpr asc::RandomOffset kOffset = 37;
  std::vector<float> zeros(kCount, 0.0F);
  auto whole = asc_random_cuda_test::Upload<float>(zeros, *fixture.resource,
                                                   fixture.execution);
  auto partitioned = asc_random_cuda_test::Upload<float>(
      zeros, *fixture.resource, fixture.execution);
  auto second_fixture = MakeFixture();
  ASC_M7_CUDA_CHECK(test, whole.ok());
  ASC_M7_CUDA_CHECK(test, partitioned.ok());
  ASC_M7_CUDA_CHECK(test, second_fixture.ok());
  if (!whole.ok() || !partitioned.ok() || !second_fixture.ok()) {
    return;
  }
  const std::array<asc::extent_t, 1> whole_extents = {
      static_cast<asc::extent_t>(kCount)};
  const std::array<asc::extent_t, 1> first_extents = {
      static_cast<asc::extent_t>(kFirst)};
  const std::array<asc::extent_t, 1> second_extents = {
      static_cast<asc::extent_t>(kSecond)};
  auto whole_mapping =
      asc::DenseLayout<1>::Create(whole_extents, asc::LayoutLeft{});
  auto first_mapping =
      asc::DenseLayout<1>::Create(first_extents, asc::LayoutLeft{});
  auto second_mapping =
      asc::DenseLayout<1>::Create(second_extents, asc::LayoutLeft{});
  if (!whole_mapping.ok() || !first_mapping.ok() || !second_mapping.ok()) {
    ASC_M7_CUDA_CHECK(test, false);
    return;
  }
  auto whole_view = asc::DenseView<float, 1>::Create(
      static_cast<float*>(whole->data()), *whole_mapping,
      asc::MemorySpace::kDevice);
  auto first_view = asc::DenseView<float, 1>::Create(
      static_cast<float*>(partitioned->data()), *first_mapping,
      asc::MemorySpace::kDevice);
  auto second_view = asc::DenseView<float, 1>::Create(
      static_cast<float*>(partitioned->data()) + kFirst, *second_mapping,
      asc::MemorySpace::kDevice);
  if (!whole_view.ok() || !first_view.ok() || !second_view.ok()) {
    ASC_M7_CUDA_CHECK(test, false);
    return;
  }
  auto whole_generated = asc::CudaFillDenseUniform01(
      fixture.execution, *whole_view, 41, 43, kOffset);
  auto first_generated = asc::CudaFillDenseUniform01(
      fixture.execution, *first_view, 41, 43, kOffset);
  auto second_generated = asc::CudaFillDenseUniform01(
      second_fixture->execution, *second_view, 41, 43, kOffset + kFirst);
  ASC_M7_CUDA_CHECK(test, whole_generated.ok());
  ASC_M7_CUDA_CHECK(test, first_generated.ok());
  ASC_M7_CUDA_CHECK(test, second_generated.ok());
  if (whole_generated.ok() && first_generated.ok() && second_generated.ok()) {
    ASC_M7_CUDA_CHECK(test, second_generated->completion.Wait().ok());
    ASC_M7_CUDA_CHECK(test, first_generated->completion.Wait().ok());
    ASC_M7_CUDA_CHECK(test, whole_generated->completion.Wait().ok());
  }
  auto whole_result =
      asc_random_cuda_test::Download<float>(*whole, fixture.execution);
  auto partitioned_result =
      asc_random_cuda_test::Download<float>(*partitioned, fixture.execution);
  ASC_M7_CUDA_CHECK(test, whole_result.ok());
  ASC_M7_CUDA_CHECK(test, partitioned_result.ok());
  if (whole_result.ok() && partitioned_result.ok()) {
    ASC_M7_CUDA_EQ(test, *whole_result, *partitioned_result);
  }

  const std::array<asc::extent_t, 1> four_extents = {4};
  auto four_mapping =
      asc::DenseLayout<1>::Create(four_extents, asc::LayoutLeft{});
  ASC_M7_CUDA_CHECK(test, four_mapping.ok());
  std::array<float, 4> host{};
  if (four_mapping.ok()) {
    auto host_view = asc::DenseView<float, 1>::Create(
        host.data(), *four_mapping, asc::MemorySpace::kHost);
    ASC_M7_CUDA_CHECK(test, host_view.ok());
    if (host_view.ok()) {
      const auto rejected = asc::CudaFillDenseUniform01(fixture.execution,
                                                        *host_view, 61, 67, 71);
      ASC_M7_CUDA_CHECK(test, !rejected.ok());
    }
  }

  const auto overflow = asc::CudaFillDenseUniform01(
      fixture.execution, *whole_view, 73, 79,
      std::numeric_limits<asc::RandomOffset>::max() - kCount + 2U);
  ASC_M7_CUDA_CHECK(test, !overflow.ok());
}

void CheckIrregularDoublePartitions(Fixture& fixture, TestContext& test) {
  constexpr std::size_t kFirst = 17;
  constexpr std::size_t kSecond = 1;
  constexpr std::size_t kThird = 509;
  constexpr std::size_t kFourth = 500;
  constexpr std::size_t kCount = kFirst + kSecond + kThird + kFourth;
  constexpr asc::RandomStream kStream = 83;
  constexpr asc::RandomSubsequence kSubsequence = 89;
  constexpr asc::RandomOffset kOffset = 97;
  constexpr asc::RandomOffset kWordsPerElement = 2;

  std::vector<double> zeros(kCount, 0.0);
  auto whole = asc_random_cuda_test::Upload<double>(zeros, *fixture.resource,
                                                    fixture.execution);
  auto partitioned = asc_random_cuda_test::Upload<double>(
      zeros, *fixture.resource, fixture.execution);
  auto second_fixture = MakeFixture();
  ASC_M7_CUDA_CHECK(test, whole.ok());
  ASC_M7_CUDA_CHECK(test, partitioned.ok());
  ASC_M7_CUDA_CHECK(test, second_fixture.ok());
  if (!whole.ok() || !partitioned.ok() || !second_fixture.ok()) {
    return;
  }

  const auto make_mapping = [&](std::size_t size) {
    return asc::DenseLayout<1>::Create(
        std::array<asc::extent_t, 1>{static_cast<asc::extent_t>(size)},
        asc::LayoutLeft{});
  };
  auto whole_mapping = make_mapping(kCount);
  auto first_mapping = make_mapping(kFirst);
  auto second_mapping = make_mapping(kSecond);
  auto third_mapping = make_mapping(kThird);
  auto fourth_mapping = make_mapping(kFourth);
  auto empty_mapping = make_mapping(0);
  ASC_M7_CUDA_CHECK(test, whole_mapping.ok());
  ASC_M7_CUDA_CHECK(test, first_mapping.ok());
  ASC_M7_CUDA_CHECK(test, second_mapping.ok());
  ASC_M7_CUDA_CHECK(test, third_mapping.ok());
  ASC_M7_CUDA_CHECK(test, fourth_mapping.ok());
  ASC_M7_CUDA_CHECK(test, empty_mapping.ok());
  if (!whole_mapping.ok() || !first_mapping.ok() || !second_mapping.ok() ||
      !third_mapping.ok() || !fourth_mapping.ok() || !empty_mapping.ok()) {
    return;
  }

  auto* base = static_cast<double*>(partitioned->data());
  auto whole_view = asc::DenseView<double, 1>::Create(
      static_cast<double*>(whole->data()), *whole_mapping,
      asc::MemorySpace::kDevice);
  auto first_view = asc::DenseView<double, 1>::Create(
      base, *first_mapping, asc::MemorySpace::kDevice);
  auto second_view = asc::DenseView<double, 1>::Create(
      base + kFirst, *second_mapping, asc::MemorySpace::kDevice);
  auto third_view = asc::DenseView<double, 1>::Create(
      base + kFirst + kSecond, *third_mapping, asc::MemorySpace::kDevice);
  auto fourth_view = asc::DenseView<double, 1>::Create(
      base + kFirst + kSecond + kThird, *fourth_mapping,
      asc::MemorySpace::kDevice);
  auto empty_view = asc::DenseView<double, 1>::Create(
      base + kFirst, *empty_mapping, asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, whole_view.ok());
  ASC_M7_CUDA_CHECK(test, first_view.ok());
  ASC_M7_CUDA_CHECK(test, second_view.ok());
  ASC_M7_CUDA_CHECK(test, third_view.ok());
  ASC_M7_CUDA_CHECK(test, fourth_view.ok());
  ASC_M7_CUDA_CHECK(test, empty_view.ok());
  if (!whole_view.ok() || !first_view.ok() || !second_view.ok() ||
      !third_view.ok() || !fourth_view.ok() || !empty_view.ok()) {
    return;
  }

  auto whole_generation = asc::CudaFillDenseUniform01(
      fixture.execution, *whole_view, kStream, kSubsequence, kOffset);
  auto fourth_generation = asc::CudaFillDenseUniform01(
      second_fixture->execution, *fourth_view, kStream, kSubsequence,
      kOffset + (kFirst + kSecond + kThird) * kWordsPerElement);
  auto empty_generation = asc::CudaFillDenseUniform01(
      fixture.execution, *empty_view, kStream, kSubsequence,
      kOffset + kFirst * kWordsPerElement);
  auto second_generation = asc::CudaFillDenseUniform01(
      second_fixture->execution, *second_view, kStream, kSubsequence,
      kOffset + kFirst * kWordsPerElement);
  auto first_generation = asc::CudaFillDenseUniform01(
      fixture.execution, *first_view, kStream, kSubsequence, kOffset);
  auto third_generation = asc::CudaFillDenseUniform01(
      fixture.execution, *third_view, kStream, kSubsequence,
      kOffset + (kFirst + kSecond) * kWordsPerElement);
  ASC_M7_CUDA_CHECK(test, whole_generation.ok());
  ASC_M7_CUDA_CHECK(test, fourth_generation.ok());
  ASC_M7_CUDA_CHECK(test, empty_generation.ok());
  ASC_M7_CUDA_CHECK(test, second_generation.ok());
  ASC_M7_CUDA_CHECK(test, first_generation.ok());
  ASC_M7_CUDA_CHECK(test, third_generation.ok());
  if (!whole_generation.ok() || !fourth_generation.ok() ||
      !empty_generation.ok() || !second_generation.ok() ||
      !first_generation.ok() || !third_generation.ok()) {
    return;
  }
  ASC_M7_CUDA_EQ(test, empty_generation->next_offset,
                 kOffset + kFirst * kWordsPerElement);
  ASC_M7_CUDA_CHECK(test, third_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, first_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, second_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, empty_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, fourth_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, whole_generation->completion.Wait().ok());

  auto whole_result =
      asc_random_cuda_test::Download<double>(*whole, fixture.execution);
  auto partitioned_result =
      asc_random_cuda_test::Download<double>(*partitioned, fixture.execution);
  ASC_M7_CUDA_CHECK(test, whole_result.ok());
  ASC_M7_CUDA_CHECK(test, partitioned_result.ok());
  if (whole_result.ok() && partitioned_result.ok()) {
    ASC_M7_CUDA_EQ(test, *partitioned_result, *whole_result);
  }
}

}  // namespace

int main() {
  if (!asc_random_cuda_test::HasCudaDevice()) {
    return asc_random_cuda_test::kSkipReturnCode;
  }
  TestContext test;
  auto fixture = MakeFixture();
  ASC_M7_CUDA_CHECK(test, fixture.ok());
  if (!fixture.ok()) {
    return test.Finish();
  }
  CheckRankTwo<float>(*fixture, asc::LayoutLeft{}, test);
  CheckRankTwo<double>(*fixture, asc::LayoutRight{}, test);
  CheckPadding(*fixture, test);
  CheckRankZeroAndEmpty(*fixture, test);
  CheckPartitionAndFailures(*fixture, test);
  CheckIrregularDoublePartitions(*fixture, test);
  return test.Finish();
}
