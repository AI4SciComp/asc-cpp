#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/providers/cuda.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"
#include "counting_resource.h"
#include "device_test_helpers.h"
#include "test_support.h"

namespace m6_verifier {

struct ExternalExpression {
  std::array<asc::extent_t, 1> shape;
};

}  // namespace m6_verifier

namespace asc {

template <>
struct ExpressionAdapter<m6_verifier::ExternalExpression> {
  using value_type = float;
  static constexpr rank_t rank = 1;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;
  static constexpr ExpressionOperation operation =
      ExpressionOperation::kExternal;

  static constexpr std::array<extent_t, 1> Shape(
      const m6_verifier::ExternalExpression& expression) noexcept {
    return expression.shape;
  }

  static constexpr float Read(
      const m6_verifier::ExternalExpression& /*expression*/,
      std::span<const index_t, 1> /*coordinate*/) noexcept {
    return 0.0F;
  }

  static constexpr bool MayAlias(
      const m6_verifier::ExternalExpression& /*expression*/,
      AliasToken /*token*/) noexcept {
    return false;
  }
};

}  // namespace asc

namespace {

using asc_dense_cuda_test::CopyAndWait;
using asc_dense_cuda_test::Data;
using asc_dense_cuda_test::MakeLeftView;
using asc_dense_cuda_test::MakeView;

template <typename T, std::size_t Rank>
void TestTerminalRank(asc_dense_cuda_test::TestContext& test,
                      asc::MemoryResource& pinned_resource,
                      asc::MemoryResource& device_resource,
                      asc::DenseCudaContext& dense_context) {
  std::array<asc::extent_t, Rank> extents{};
  extents.fill(1);
  if constexpr (Rank != 0) {
    extents[0] = 2;
  }
  constexpr std::size_t kElements = Rank == 0 ? 1 : 2;
  const std::size_t bytes = kElements * sizeof(T);

  auto host_source = asc::Buffer::Allocate(pinned_resource, bytes, alignof(T));
  auto host_destination =
      asc::Buffer::Allocate(pinned_resource, bytes, alignof(T));
  auto device_source =
      asc::Buffer::Allocate(device_resource, bytes, alignof(T));
  auto device_destination =
      asc::Buffer::Allocate(device_resource, bytes, alignof(T));
  ASC_DENSE_CUDA_CHECK(test, host_source.ok());
  ASC_DENSE_CUDA_CHECK(test, host_destination.ok());
  ASC_DENSE_CUDA_CHECK(test, device_source.ok());
  ASC_DENSE_CUDA_CHECK(test, device_destination.ok());
  if (!host_source.ok() || !host_destination.ok() || !device_source.ok() ||
      !device_destination.ok()) {
    return;
  }

  for (std::size_t index = 0; index < kElements; ++index) {
    Data<T>(*host_source)[index] =
        static_cast<T>(std::int32_t{3} - static_cast<std::int32_t>(index * 5));
    Data<T>(*host_destination)[index] = static_cast<T>(-99);
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(dense_context.execution_context(),
                                         *device_source, *host_source));

  auto source_mutable =
      MakeLeftView(Data<T>(*device_source), extents, asc::MemorySpace::kDevice);
  asc::DenseView<const T, Rank> source = source_mutable;
  auto destination = MakeLeftView(Data<T>(*device_destination), extents,
                                  asc::MemorySpace::kDevice);
  auto event = asc::CudaEvaluate(dense_context, source, destination);
  ASC_DENSE_CUDA_CHECK(test, event.ok());
  if (!event.ok()) {
    std::cerr << "terminal CudaEvaluate failed: " << event.status().ToString()
              << '\n';
  }
  if (!event.ok()) {
    return;
  }
  auto early_query = event->Query();
  ASC_DENSE_CUDA_CHECK(test, early_query.ok());
  ASC_DENSE_CUDA_CHECK(test, event->Wait().ok());
  auto complete_query = event->Query();
  ASC_DENSE_CUDA_CHECK(test, complete_query.ok());
  if (complete_query.ok()) {
    ASC_DENSE_CUDA_CHECK(test, *complete_query);
  }

  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(dense_context.execution_context(), *host_destination,
                        *device_destination));
  for (std::size_t index = 0; index < kElements; ++index) {
    ASC_DENSE_CUDA_EQ(test, Data<T>(*host_destination)[index],
                      Data<T>(*host_source)[index]);
  }

  auto self = asc::CudaEvaluate(dense_context, source, source_mutable);
  ASC_DENSE_CUDA_CHECK(test, self.ok());
  if (self.ok()) {
    ASC_DENSE_CUDA_CHECK(test, self->Wait().ok());
  }
}

void TestAllTerminalRanks(asc_dense_cuda_test::TestContext& test,
                          asc::MemoryResource& pinned_resource,
                          asc::MemoryResource& device_resource,
                          asc::DenseCudaContext& dense_context) {
  TestTerminalRank<float, 0>(test, pinned_resource, device_resource,
                             dense_context);
  TestTerminalRank<double, 0>(test, pinned_resource, device_resource,
                              dense_context);
  TestTerminalRank<float, 1>(test, pinned_resource, device_resource,
                             dense_context);
  TestTerminalRank<double, 2>(test, pinned_resource, device_resource,
                              dense_context);
  TestTerminalRank<float, 3>(test, pinned_resource, device_resource,
                             dense_context);
  TestTerminalRank<double, 4>(test, pinned_resource, device_resource,
                              dense_context);
  TestTerminalRank<float, 5>(test, pinned_resource, device_resource,
                             dense_context);
  TestTerminalRank<double, 6>(test, pinned_resource, device_resource,
                              dense_context);
  TestTerminalRank<float, 7>(test, pinned_resource, device_resource,
                             dense_context);
  TestTerminalRank<double, 8>(test, pinned_resource, device_resource,
                              dense_context);
}

// Pointwise variants share storage and an independent element-wise oracle.
// NOLINTNEXTLINE(readability-function-size)
void TestScalarAndOneLevelOperations(
    asc_dense_cuda_test::TestContext& test,
    asc_dense_cuda_test::CountingResource& pinned_resource,
    asc_dense_cuda_test::CountingResource& device_resource,
    asc::DenseCudaContext& dense_context) {
  auto scalar_host =
      asc::Buffer::Allocate(pinned_resource, sizeof(double), alignof(double));
  auto scalar_device =
      asc::Buffer::Allocate(device_resource, sizeof(double), alignof(double));
  ASC_DENSE_CUDA_CHECK(test, scalar_host.ok());
  ASC_DENSE_CUDA_CHECK(test, scalar_device.ok());
  if (!scalar_host.ok() || !scalar_device.ok()) {
    return;
  }
  const std::array<asc::extent_t, 0> scalar_extents{};
  auto scalar_destination = MakeLeftView(
      Data<double>(*scalar_device), scalar_extents, asc::MemorySpace::kDevice);
  const std::size_t allocations_before_scalar =
      device_resource.allocation_calls();
  auto scalar_event =
      asc::CudaEvaluate(dense_context, -7.25, scalar_destination);
  ASC_DENSE_CUDA_CHECK(test, scalar_event.ok());
  if (!scalar_event.ok()) {
    std::cerr << "scalar CudaEvaluate failed: "
              << scalar_event.status().ToString() << '\n';
  }
  if (scalar_event.ok()) {
    ASC_DENSE_CUDA_CHECK(test, scalar_event->Wait().ok());
  }
  ASC_DENSE_CUDA_EQ(test, device_resource.allocation_calls(),
                    allocations_before_scalar);
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(dense_context.execution_context(),
                                         *scalar_host, *scalar_device));
  ASC_DENSE_CUDA_EQ(test, *Data<double>(*scalar_host), -7.25);

  constexpr std::size_t kElements = 6;
  constexpr std::size_t kBytes = kElements * sizeof(double);
  auto host_left =
      asc::Buffer::Allocate(pinned_resource, kBytes, alignof(double));
  auto host_right =
      asc::Buffer::Allocate(pinned_resource, kBytes, alignof(double));
  auto host_output =
      asc::Buffer::Allocate(pinned_resource, kBytes, alignof(double));
  auto device_left =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(double));
  auto device_right =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(double));
  auto device_output =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(double));
  ASC_DENSE_CUDA_CHECK(test, host_left.ok());
  ASC_DENSE_CUDA_CHECK(test, host_right.ok());
  ASC_DENSE_CUDA_CHECK(test, host_output.ok());
  ASC_DENSE_CUDA_CHECK(test, device_left.ok());
  ASC_DENSE_CUDA_CHECK(test, device_right.ok());
  ASC_DENSE_CUDA_CHECK(test, device_output.ok());
  if (!host_left.ok() || !host_right.ok() || !host_output.ok() ||
      !device_left.ok() || !device_right.ok() || !device_output.ok()) {
    return;
  }
  for (std::size_t index = 0; index < kElements; ++index) {
    Data<double>(*host_left)[index] =
        static_cast<double>(static_cast<std::int32_t>(index) - 2);
    Data<double>(*host_right)[index] =
        static_cast<double>(2 * static_cast<std::int32_t>(index) + 1);
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(dense_context.execution_context(),
                                         *device_left, *host_left));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(dense_context.execution_context(),
                                         *device_right, *host_right));

  const std::array<asc::extent_t, 2> extents{2, 3};
  auto left_mutable = MakeLeftView(Data<double>(*device_left), extents,
                                   asc::MemorySpace::kDevice);
  auto right_mutable = MakeLeftView(Data<double>(*device_right), extents,
                                    asc::MemorySpace::kDevice);
  asc::DenseView<const double, 2> left = left_mutable;
  asc::DenseView<const double, 2> right = right_mutable;
  auto output = MakeLeftView(Data<double>(*device_output), extents,
                             asc::MemorySpace::kDevice);

  const auto verify = [&](auto&& expression, auto expected) {
    const std::size_t allocations_before = device_resource.allocation_calls();
    auto event = asc::CudaEvaluate(dense_context, expression, output);
    ASC_DENSE_CUDA_CHECK(test, event.ok());
    if (!event.ok()) {
      std::cerr << "one-level CudaEvaluate failed: "
                << event.status().ToString() << '\n';
    }
    if (!event.ok()) {
      return;
    }
    auto query = event->Query();
    ASC_DENSE_CUDA_CHECK(test, query.ok());
    ASC_DENSE_CUDA_CHECK(test, event->Wait().ok());
    ASC_DENSE_CUDA_EQ(test, device_resource.allocation_calls(),
                      allocations_before);
    ASC_DENSE_CUDA_CHECK(test, CopyAndWait(dense_context.execution_context(),
                                           *host_output, *device_output));
    for (std::size_t index = 0; index < kElements; ++index) {
      ASC_DENSE_CUDA_EQ(test, Data<double>(*host_output)[index],
                        expected(index));
    }
  };

  auto negated = asc::MakeNegate(left);
  verify(negated,
         [&](std::size_t index) { return -Data<double>(*host_left)[index]; });
  auto added = asc::MakeAdd(left, right);
  ASC_DENSE_CUDA_CHECK(test, added.ok());
  if (added.ok()) {
    verify(*added, [&](std::size_t index) {
      return Data<double>(*host_left)[index] + Data<double>(*host_right)[index];
    });
  }
  auto subtracted = asc::MakeSubtract(left, right);
  ASC_DENSE_CUDA_CHECK(test, subtracted.ok());
  if (subtracted.ok()) {
    verify(*subtracted, [&](std::size_t index) {
      return Data<double>(*host_left)[index] - Data<double>(*host_right)[index];
    });
  }
  auto multiplied = asc::MakeMultiply(left, right);
  ASC_DENSE_CUDA_CHECK(test, multiplied.ok());
  if (multiplied.ok()) {
    verify(*multiplied, [&](std::size_t index) {
      return Data<double>(*host_left)[index] * Data<double>(*host_right)[index];
    });
  }
  auto scalar_added = asc::MakeAdd(left, 2.0);
  ASC_DENSE_CUDA_CHECK(test, scalar_added.ok());
  if (scalar_added.ok()) {
    verify(*scalar_added, [&](std::size_t index) {
      return Data<double>(*host_left)[index] + 2.0;
    });
  }
}

// Padded and negative cases share a descriptor fixture and sentinels.
// NOLINTNEXTLINE(readability-function-size)
void TestPaddedLayoutAndNegatives(asc_dense_cuda_test::TestContext& test,
                                  asc::MemoryResource& pinned_resource,
                                  asc::MemoryResource& device_resource,
                                  asc::DenseCudaContext& dense_context) {
  constexpr std::size_t kSpan = 10;
  constexpr std::size_t kBytes = kSpan * sizeof(float);
  auto host_source =
      asc::Buffer::Allocate(pinned_resource, kBytes, alignof(float));
  auto host_output =
      asc::Buffer::Allocate(pinned_resource, kBytes, alignof(float));
  auto device_source =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(float));
  auto device_output =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(float));
  ASC_DENSE_CUDA_CHECK(test, host_source.ok());
  ASC_DENSE_CUDA_CHECK(test, host_output.ok());
  ASC_DENSE_CUDA_CHECK(test, device_source.ok());
  ASC_DENSE_CUDA_CHECK(test, device_output.ok());
  if (!host_source.ok() || !host_output.ok() || !device_source.ok() ||
      !device_output.ok()) {
    return;
  }
  for (std::size_t index = 0; index < kSpan; ++index) {
    Data<float>(*host_source)[index] = -777.0F;
    Data<float>(*host_output)[index] = -999.0F;
  }
  constexpr std::array<std::size_t, 6> kOffsets{0, 1, 4, 5, 8, 9};
  for (std::size_t logical = 0; logical < kOffsets.size(); ++logical) {
    Data<float>(*host_source)[kOffsets[logical]] =
        static_cast<float>(static_cast<int>(logical) - 3);
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(dense_context.execution_context(),
                                         *device_source, *host_source));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(dense_context.execution_context(),
                                         *device_output, *host_output));

  const std::array<asc::extent_t, 2> extents{2, 3};
  const std::array<asc::stride_t, 2> strides{1, 4};
  auto source_mutable = MakeView(Data<float>(*device_source), extents, strides,
                                 asc::MemorySpace::kDevice);
  asc::DenseView<const float, 2> source = source_mutable;
  auto output = MakeView(Data<float>(*device_output), extents, strides,
                         asc::MemorySpace::kDevice);
  auto expression = asc::MakeAdd(source, 2.0F);
  ASC_DENSE_CUDA_CHECK(test, expression.ok());
  if (expression.ok()) {
    auto event = asc::CudaEvaluate(dense_context, *expression, output);
    ASC_DENSE_CUDA_CHECK(test, event.ok());
    if (event.ok()) {
      ASC_DENSE_CUDA_CHECK(test, event->Wait().ok());
    }
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(dense_context.execution_context(),
                                         *host_output, *device_output));
  for (std::size_t index = 0; index < kSpan; ++index) {
    bool logical = false;
    for (std::size_t offset : kOffsets) {
      logical = logical || offset == index;
    }
    const float expected =
        logical ? Data<float>(*host_source)[index] + 2.0F : -999.0F;
    ASC_DENSE_CUDA_EQ(test, Data<float>(*host_output)[index], expected);
  }

  auto aliased_negate = asc::MakeNegate(source);
  auto alias_failure =
      asc::CudaEvaluate(dense_context, aliased_negate, source_mutable);
  ASC_DENSE_CUDA_CHECK(test, !alias_failure.ok());
  if (!alias_failure.ok()) {
    ASC_DENSE_CUDA_EQ(test, alias_failure.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }

  auto nested = asc::MakeAdd(aliased_negate, source);
  ASC_DENSE_CUDA_CHECK(test, nested.ok());
  if (nested.ok()) {
    auto nested_failure = asc::CudaEvaluate(dense_context, *nested, output);
    ASC_DENSE_CUDA_CHECK(test, !nested_failure.ok());
    if (!nested_failure.ok()) {
      ASC_DENSE_CUDA_EQ(test, nested_failure.status().code(),
                        asc::ErrorCode::kUnsupported);
    }
  }

  m6_verifier::ExternalExpression external{{2}};
  auto external_failure = asc::CudaEvaluate(
      dense_context, external,
      MakeLeftView(Data<float>(*device_output), std::array<asc::extent_t, 1>{2},
                   asc::MemorySpace::kDevice));
  ASC_DENSE_CUDA_CHECK(test, !external_failure.ok());
  if (!external_failure.ok()) {
    ASC_DENSE_CUDA_EQ(test, external_failure.status().code(),
                      asc::ErrorCode::kUnsupported);
  }

  auto pinned_destination =
      MakeLeftView(Data<float>(*host_output), std::array<asc::extent_t, 1>{2},
                   asc::MemorySpace::kPinnedHost);
  auto placement_failure =
      asc::CudaEvaluate(dense_context, 1.0F, pinned_destination);
  ASC_DENSE_CUDA_CHECK(test, !placement_failure.ok());
}

void TestZeroExtentAndRankNine(asc_dense_cuda_test::TestContext& test,
                               asc::MemoryResource& device_resource,
                               asc::DenseCudaContext& dense_context) {
  const std::array<asc::extent_t, 2> empty_extents{0, 3};
  const std::array<asc::stride_t, 2> empty_strides{1, 0};
  auto empty_source =
      MakeView(static_cast<const float*>(nullptr), empty_extents, empty_strides,
               asc::MemorySpace::kDevice);
  auto empty_destination = MakeView(static_cast<float*>(nullptr), empty_extents,
                                    empty_strides, asc::MemorySpace::kDevice);
  auto empty_event =
      asc::CudaEvaluate(dense_context, empty_source, empty_destination);
  ASC_DENSE_CUDA_CHECK(test, empty_event.ok());
  if (empty_event.ok()) {
    ASC_DENSE_CUDA_CHECK(test, empty_event->Wait().ok());
  }

  auto device_value =
      asc::Buffer::Allocate(device_resource, sizeof(float), alignof(float));
  ASC_DENSE_CUDA_CHECK(test, device_value.ok());
  if (!device_value.ok()) {
    return;
  }
  std::array<asc::extent_t, 9> extents{};
  extents.fill(1);
  auto rank_nine_mutable = MakeLeftView(Data<float>(*device_value), extents,
                                        asc::MemorySpace::kDevice);
  asc::DenseView<const float, 9> rank_nine = rank_nine_mutable;
  auto unsupported =
      asc::CudaEvaluate(dense_context, rank_nine, rank_nine_mutable);
  ASC_DENSE_CUDA_CHECK(test, !unsupported.ok());
  if (!unsupported.ok()) {
    ASC_DENSE_CUDA_EQ(test, unsupported.status().code(),
                      asc::ErrorCode::kUnsupported);
  }
}

}  // namespace

int main() {
  if (asc_dense_cuda_test::ForceNoCudaDevice()) {
    return asc_dense_cuda_test::kSkipReturnCode;
  }
  asc_dense_cuda_test::TestContext test;
  auto count = asc::CudaDeviceCount();
  ASC_DENSE_CUDA_CHECK(test, count.ok());
  if (!count.ok()) {
    return test.Finish();
  }
  if (*count == 0) {
    return asc_dense_cuda_test::kSkipReturnCode;
  }

  auto pinned_upstream =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  auto device_upstream =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  auto execution = asc::CreateCudaExecutionContext(0);
  ASC_DENSE_CUDA_CHECK(test, pinned_upstream.ok());
  ASC_DENSE_CUDA_CHECK(test, device_upstream.ok());
  ASC_DENSE_CUDA_CHECK(test, execution.ok());
  if (!pinned_upstream.ok() || !device_upstream.ok() || !execution.ok()) {
    return test.Finish();
  }
  auto dense_context = asc::DenseCudaContext::Create(*execution);
  ASC_DENSE_CUDA_CHECK(test, dense_context.ok());
  if (!dense_context.ok()) {
    return test.Finish();
  }

  asc_dense_cuda_test::CountingResource pinned(**pinned_upstream);
  asc_dense_cuda_test::CountingResource device(**device_upstream);
  TestAllTerminalRanks(test, pinned, device, *dense_context);
  TestScalarAndOneLevelOperations(test, pinned, device, *dense_context);
  TestPaddedLayoutAndNegatives(test, pinned, device, *dense_context);
  TestZeroExtentAndRankNine(test, device, *dense_context);

  ASC_DENSE_CUDA_EQ(test, pinned.live_allocations(), std::size_t{0});
  ASC_DENSE_CUDA_EQ(test, device.live_allocations(), std::size_t{0});
  ASC_DENSE_CUDA_EQ(test, pinned.allocated_bytes(), pinned.deallocated_bytes());
  ASC_DENSE_CUDA_EQ(test, device.allocated_bytes(), device.deallocated_bytes());
  return test.Finish();
}
