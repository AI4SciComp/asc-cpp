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
#include "asc/core/status.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/providers/dense_cuda.h"

namespace {

static_assert(!std::is_copy_constructible_v<asc::CudaDenseUniform01Generation>);
static_assert(
    std::is_nothrow_move_constructible_v<asc::CudaDenseUniform01Generation>);

constexpr asc::RandomStream kStream = UINT64_C(0x1020304050607080);
constexpr asc::RandomSubsequence kSubsequence = UINT64_C(0x8070605040302010);
constexpr asc::RandomOffset kOffset = 19;

using asc_random_cuda_test::TestContext;

struct CudaFixture {
  asc::ExecutionContext execution;
  std::unique_ptr<asc::CudaMemoryResource> resource;
};

asc::Result<CudaFixture> MakeFixture() {
  auto execution =
      asc::CreateCudaExecutionContext(asc_random_cuda_test::CudaDevice());
  if (!execution.ok()) {
    return execution.status();
  }
  auto resource = asc::CudaMemoryResource::Create(
      asc_random_cuda_test::CudaDevice(), asc::MemorySpace::kDevice);
  if (!resource.ok()) {
    return resource.status();
  }
  return CudaFixture{*execution, std::move(*resource)};
}

template <typename Element>
inline constexpr asc::RandomOffset kWordsPerElement =
    std::same_as<Element, float> ? 1U : 2U;

template <typename Element>
Element Expected(std::uint64_t ordinal, asc::RandomOffset offset = kOffset) {
  const asc::RandomOffset address =
      offset + ordinal * kWordsPerElement<Element>;
  if constexpr (std::same_as<Element, float>) {
    return asc_random_cuda_test::Uniform01FloatOracle(
        asc_random_cuda_test::PhiloxWordOracle(kStream, kSubsequence, address));
  } else {
    return asc_random_cuda_test::Uniform01DoubleOracle(
        asc_random_cuda_test::PhiloxWordOracle(kStream, kSubsequence, address),
        asc_random_cuda_test::PhiloxWordOracle(kStream, kSubsequence,
                                               address + 1U));
  }
}

template <typename Element>
void CheckRankTwoLayout(const asc::DenseLayoutMapping<2>& mapping,
                        CudaFixture& fixture, TestContext& test) {
  const auto span_size = static_cast<std::size_t>(mapping.required_span_size());
  constexpr Element kSentinel = static_cast<Element>(-7.25);
  std::vector<Element> initial(span_size, kSentinel);
  auto device = asc_random_cuda_test::Upload<Element>(
      initial, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, device.ok());
  if (!device.ok()) {
    return;
  }
  auto device_view =
      asc::DenseView<Element, 2>::Create(static_cast<Element*>(device->data()),
                                         mapping, asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, device_view.ok());
  if (!device_view.ok()) {
    return;
  }

  auto generation = asc::CudaFillDenseUniform01(fixture.execution, *device_view,
                                                kStream, kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, generation.ok());
  if (!generation.ok()) {
    return;
  }
  const auto logical_size =
      static_cast<asc::RandomOffset>(mapping.logical_size());
  ASC_M7_CUDA_EQ(test, generation->next_offset,
                 kOffset + logical_size * kWordsPerElement<Element>);
  ASC_M7_CUDA_CHECK(test, generation->completion.Wait().ok());

  auto actual =
      asc_random_cuda_test::Download<Element>(*device, fixture.execution);
  ASC_M7_CUDA_CHECK(test, actual.ok());
  if (!actual.ok()) {
    return;
  }

  std::vector<Element> cpu(span_size, kSentinel);
  auto cpu_view = asc::DenseView<Element, 2>::Create(cpu.data(), mapping,
                                                     asc::MemorySpace::kHost);
  ASC_M7_CUDA_CHECK(test, cpu_view.ok());
  if (cpu_view.ok()) {
    const auto next =
        asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *cpu_view,
                                kStream, kSubsequence, kOffset);
    ASC_M7_CUDA_CHECK(test, next.ok());
    if (next.ok()) {
      ASC_M7_CUDA_EQ(test, *next, generation->next_offset);
    }
  }

  std::vector<bool> used(span_size, false);
  std::uint64_t ordinal = 0;
  for (asc::index_t column = 0; column < mapping.shape()[1]; ++column) {
    for (asc::index_t row = 0; row < mapping.shape()[0]; ++row) {
      const std::array<asc::index_t, 2> coordinate = {row, column};
      auto physical = mapping.Offset(coordinate);
      ASC_M7_CUDA_CHECK(test, physical.ok());
      if (physical.ok()) {
        const std::size_t index = static_cast<std::size_t>(*physical);
        used[index] = true;
        ASC_M7_CUDA_BIT_EQ(test, (*actual)[index], Expected<Element>(ordinal));
        ASC_M7_CUDA_BIT_EQ(test, (*actual)[index], cpu[index]);
      }
      ++ordinal;
    }
  }
  for (std::size_t index = 0; index < span_size; ++index) {
    if (!used[index]) {
      ASC_M7_CUDA_BIT_EQ(test, (*actual)[index], kSentinel);
      ASC_M7_CUDA_BIT_EQ(test, cpu[index], kSentinel);
    }
  }
}

template <typename Element>
void CheckLayouts(CudaFixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape = {2, 3};
  constexpr std::array<asc::stride_t, 2> kPaddedStrides = {1, 4};
  auto left = asc::DenseLayoutMapping<2>::Create(asc::LayoutLeft{}, kShape);
  auto right = asc::DenseLayoutMapping<2>::Create(asc::LayoutRight{}, kShape);
  auto padded = asc::DenseLayoutMapping<2>::Create(asc::LayoutStride{}, kShape,
                                                   kPaddedStrides);
  ASC_M7_CUDA_CHECK(test, left.ok());
  ASC_M7_CUDA_CHECK(test, right.ok());
  ASC_M7_CUDA_CHECK(test, padded.ok());
  if (left.ok()) {
    CheckRankTwoLayout<Element>(*left, fixture, test);
  }
  if (right.ok()) {
    CheckRankTwoLayout<Element>(*right, fixture, test);
  }
  if (padded.ok()) {
    CheckRankTwoLayout<Element>(*padded, fixture, test);
  }
}

void CheckRankZeroAndZeroExtent(CudaFixture& fixture, TestContext& test) {
  const std::array<asc::extent_t, 0> rank_zero_shape{};
  auto rank_zero_mapping =
      asc::DenseLayoutMapping<0>::Create(asc::LayoutLeft{}, rank_zero_shape);
  ASC_M7_CUDA_CHECK(test, rank_zero_mapping.ok());
  if (rank_zero_mapping.ok()) {
    const std::array<double, 1> initial = {-1.0};
    auto device = asc_random_cuda_test::Upload<double>(
        initial, *fixture.resource, fixture.execution);
    ASC_M7_CUDA_CHECK(test, device.ok());
    if (device.ok()) {
      auto view = asc::DenseView<double, 0>::Create(
          static_cast<double*>(device->data()), *rank_zero_mapping,
          asc::MemorySpace::kDevice);
      ASC_M7_CUDA_CHECK(test, view.ok());
      if (view.ok()) {
        auto generation = asc::CudaFillDenseUniform01(
            fixture.execution, *view, kStream, kSubsequence, kOffset);
        ASC_M7_CUDA_CHECK(test, generation.ok());
        if (generation.ok()) {
          ASC_M7_CUDA_EQ(test, generation->next_offset, kOffset + 2U);
          ASC_M7_CUDA_CHECK(test, generation->completion.Wait().ok());
          auto actual = asc_random_cuda_test::Download<double>(
              *device, fixture.execution);
          ASC_M7_CUDA_CHECK(test, actual.ok());
          if (actual.ok()) {
            ASC_M7_CUDA_BIT_EQ(test, actual->front(), Expected<double>(0));
          }
        }
      }
    }
  }

  constexpr std::array<asc::extent_t, 3> kEmptyShape = {2, 0, 3};
  auto empty_mapping =
      asc::DenseLayoutMapping<3>::Create(asc::LayoutRight{}, kEmptyShape);
  ASC_M7_CUDA_CHECK(test, empty_mapping.ok());
  if (!empty_mapping.ok()) {
    return;
  }
  auto empty_view = asc::DenseView<float, 3>::Create(nullptr, *empty_mapping,
                                                     asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, empty_view.ok());
  if (!empty_view.ok()) {
    return;
  }
  const auto maximum = std::numeric_limits<asc::RandomOffset>::max();
  auto empty = asc::CudaFillDenseUniform01(fixture.execution, *empty_view,
                                           kStream, kSubsequence, maximum);
  ASC_M7_CUDA_CHECK(test, empty.ok());
  if (empty.ok()) {
    ASC_M7_CUDA_EQ(test, empty->next_offset, maximum);
    ASC_M7_CUDA_CHECK(test, empty->completion.Wait().ok());
  }
}

template <typename Element>
void CheckPartition(CudaFixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape = {2, 3};
  constexpr std::array<asc::stride_t, 2> kStrides = {1, 4};
  auto mapping =
      asc::DenseLayoutMapping<2>::Create(asc::LayoutStride{}, kShape, kStrides);
  ASC_M7_CUDA_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  const std::size_t span_size =
      static_cast<std::size_t>(mapping->required_span_size());
  constexpr Element kSentinel = static_cast<Element>(-11.5);
  const std::vector<Element> initial(span_size, kSentinel);
  auto whole = asc_random_cuda_test::Upload<Element>(initial, *fixture.resource,
                                                     fixture.execution);
  auto partitioned = asc_random_cuda_test::Upload<Element>(
      initial, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, whole.ok());
  ASC_M7_CUDA_CHECK(test, partitioned.ok());
  if (!whole.ok() || !partitioned.ok()) {
    return;
  }
  auto whole_view =
      asc::DenseView<Element, 2>::Create(static_cast<Element*>(whole->data()),
                                         *mapping, asc::MemorySpace::kDevice);
  auto partitioned_view = asc::DenseView<Element, 2>::Create(
      static_cast<Element*>(partitioned->data()), *mapping,
      asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, whole_view.ok());
  ASC_M7_CUDA_CHECK(test, partitioned_view.ok());
  if (!whole_view.ok() || !partitioned_view.ok()) {
    return;
  }

  auto whole_generation = asc::CudaFillDenseUniform01(
      fixture.execution, *whole_view, kStream, kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, whole_generation.ok());
  if (!whole_generation.ok()) {
    return;
  }
  for (asc::index_t column = 0; column < kShape[1]; ++column) {
    const std::array<asc::index_t, 2> offsets = {0, column};
    const std::array<asc::extent_t, 2> extents = {2, 1};
    auto subview = partitioned_view->Subview(offsets, extents);
    ASC_M7_CUDA_CHECK(test, subview.ok());
    if (!subview.ok()) {
      return;
    }
    const asc::RandomOffset partition_offset =
        kOffset +
        static_cast<asc::RandomOffset>(2 * column) * kWordsPerElement<Element>;
    auto generated = asc::CudaFillDenseUniform01(
        fixture.execution, *subview, kStream, kSubsequence, partition_offset);
    ASC_M7_CUDA_CHECK(test, generated.ok());
    if (!generated.ok()) {
      return;
    }
    ASC_M7_CUDA_CHECK(test, generated->completion.Wait().ok());
  }
  ASC_M7_CUDA_CHECK(test, whole_generation->completion.Wait().ok());
  auto whole_result =
      asc_random_cuda_test::Download<Element>(*whole, fixture.execution);
  auto partitioned_result =
      asc_random_cuda_test::Download<Element>(*partitioned, fixture.execution);
  ASC_M7_CUDA_CHECK(test, whole_result.ok());
  ASC_M7_CUDA_CHECK(test, partitioned_result.ok());
  if (whole_result.ok() && partitioned_result.ok()) {
    for (std::size_t index = 0; index < whole_result->size(); ++index) {
      ASC_M7_CUDA_BIT_EQ(test, (*whole_result)[index],
                         (*partitioned_result)[index]);
    }
  }
}

void CheckFailures(CudaFixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 1> kShape = {2};
  auto mapping = asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kShape);
  ASC_M7_CUDA_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  const std::array<double, 2> initial = {9.0, 10.0};
  auto device = asc_random_cuda_test::Upload<double>(initial, *fixture.resource,
                                                     fixture.execution);
  ASC_M7_CUDA_CHECK(test, device.ok());
  if (!device.ok()) {
    return;
  }
  auto device_view =
      asc::DenseView<double, 1>::Create(static_cast<double*>(device->data()),
                                        *mapping, asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, device_view.ok());
  if (!device_view.ok()) {
    return;
  }

  const auto maximum = std::numeric_limits<asc::RandomOffset>::max();
  const auto overflow = asc::CudaFillDenseUniform01(
      fixture.execution, *device_view, kStream, kSubsequence, maximum - 2U);
  ASC_M7_CUDA_CHECK(test, !overflow.ok());
  if (!overflow.ok()) {
    ASC_M7_CUDA_EQ(test, overflow.status().code(), asc::ErrorCode::kOverflow);
  }
  const auto serial =
      asc::CudaFillDenseUniform01(asc::ExecutionContext::Serial(), *device_view,
                                  kStream, kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, !serial.ok());

  std::array<double, 2> host = initial;
  auto host_view = asc::DenseView<double, 1>::Create(host.data(), *mapping,
                                                     asc::MemorySpace::kHost);
  ASC_M7_CUDA_CHECK(test, host_view.ok());
  if (host_view.ok()) {
    const auto wrong_space = asc::CudaFillDenseUniform01(
        fixture.execution, *host_view, kStream, kSubsequence, kOffset);
    ASC_M7_CUDA_CHECK(test, !wrong_space.ok());
    ASC_M7_CUDA_CHECK(test, host == initial);
  }

  auto unchanged =
      asc_random_cuda_test::Download<double>(*device, fixture.execution);
  ASC_M7_CUDA_CHECK(test, unchanged.ok());
  if (unchanged.ok()) {
    ASC_M7_CUDA_CHECK(test, *unchanged == std::vector<double>(initial.begin(),
                                                              initial.end()));
  }

  constexpr std::array<asc::stride_t, 1> kAliasedStride = {0};
  const auto aliased_mapping = asc::DenseLayoutMapping<1>::Create(
      asc::LayoutStride{}, kShape, kAliasedStride);
  ASC_M7_CUDA_CHECK(test, aliased_mapping.ok());
  if (aliased_mapping.ok()) {
    const auto aliased = asc::DenseView<double, 1>::Create(
        static_cast<double*>(device->data()), *aliased_mapping,
        asc::MemorySpace::kDevice);
    ASC_M7_CUDA_CHECK(test, !aliased.ok());
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
  CheckRankZeroAndZeroExtent(*fixture, test);
  CheckLayouts<float>(*fixture, test);
  CheckLayouts<double>(*fixture, test);
  CheckPartition<float>(*fixture, test);
  CheckPartition<double>(*fixture, test);
  CheckFailures(*fixture, test);
  return test.Finish();
}
