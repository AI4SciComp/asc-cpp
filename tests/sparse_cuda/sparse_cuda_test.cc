#include "asc/random/providers/sparse_cuda.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "../random_cuda/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/status.h"
#include "asc/expression/expression.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/providers/cuda.h"

namespace {

using asc_random_cuda_test::TestContext;

static_assert(!std::is_copy_constructible_v<asc::SparseCudaContext>);
static_assert(!std::is_copy_assignable_v<asc::SparseCudaContext>);
static_assert(std::is_nothrow_move_constructible_v<asc::SparseCudaContext>);
static_assert(std::is_nothrow_move_assignable_v<asc::SparseCudaContext>);
static_assert(
    std::is_trivially_copyable_v<asc::CudaStridedVectorView<const float>>);
static_assert(std::is_trivially_copyable_v<asc::CudaStridedVectorView<double>>);

struct Fixture {
  asc::ExecutionContext execution;
  asc::SparseCudaContext provider;
  std::unique_ptr<asc::CudaMemoryResource> resource;
};

class TrackingDeviceResource final : public asc::MemoryResource {
 public:
  explicit TrackingDeviceResource(asc::MemoryResource& backing)
      : backing_(backing) {}

  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return backing_.space();
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    const std::size_t call = attempts_++;
    if (call == failure_call_) {
      return asc::Status(asc::ErrorCode::kAllocation,
                         "Injected sparse CUDA clone allocation failure");
    }
    auto result = backing_.Allocate(bytes, alignment);
    if (result.ok() && *result != nullptr) {
      ++live_;
    }
    return result;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    if (pointer != nullptr) {
      --live_;
      ++deallocations_;
    }
    backing_.Deallocate(pointer, bytes, alignment);
  }

  void FailOnCall(std::size_t call) noexcept { failure_call_ = call; }
  [[nodiscard]] std::size_t attempts() const noexcept { return attempts_; }
  [[nodiscard]] std::size_t live() const noexcept { return live_; }
  [[nodiscard]] std::size_t deallocations() const noexcept {
    return deallocations_;
  }

 private:
  asc::MemoryResource& backing_;
  std::size_t attempts_ = 0;
  std::size_t live_ = 0;
  std::size_t deallocations_ = 0;
  std::size_t failure_call_ = std::numeric_limits<std::size_t>::max();
};

asc::Result<Fixture> MakeFixture() {
  auto execution =
      asc::CreateCudaExecutionContext(asc_random_cuda_test::kCudaDevice);
  if (!execution.ok()) {
    return execution.status();
  }
  auto provider = asc::SparseCudaContext::Create(*execution);
  if (!provider.ok()) {
    return provider.status();
  }
  auto resource = asc::CudaMemoryResource::Create(
      asc_random_cuda_test::kCudaDevice, asc::MemorySpace::kDevice);
  if (!resource.ok()) {
    return resource.status();
  }
  return Fixture{*execution, std::move(*provider), std::move(*resource)};
}

template <typename Element>
asc::Result<asc::CudaCsrArray<Element>> CloneMatrix(
    Fixture& fixture, std::span<const asc::extent_t, 2> shape,
    std::span<const asc::nnz_t> offsets, std::span<const asc::index_t> indices,
    std::span<const Element> values) {
  auto source = asc::CsrView<const Element>::Create(
      offsets, indices, values, shape, asc::MemorySpace::kHost);
  if (!source.ok()) {
    return source.status();
  }
  auto clone = asc::CudaCloneCsr(fixture.provider, *source, *fixture.resource);
  if (!clone.ok()) {
    return clone.status();
  }
  const asc::Status waited = clone->completion.Wait();
  if (!waited.ok()) {
    return waited;
  }
  return std::move(clone->array);
}

template <typename Element>
void CheckClone(Fixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape = {4, 5};
  constexpr std::array<asc::nnz_t, 5> kOffsets = {0, 2, 3, 6, 8};
  constexpr std::array<asc::index_t, 8> kIndices = {0, 3, 1, 0, 2, 4, 1, 4};
  constexpr std::array<Element, 8> kValues = {
      Element{2}, Element{-1}, Element{4},  Element{7},
      Element{3}, Element{5},  Element{-2}, Element{6}};
  auto matrix =
      CloneMatrix<Element>(fixture, kShape, kOffsets, kIndices, kValues);
  ASC_M7_CUDA_CHECK(test, matrix.ok());
  if (!matrix.ok()) {
    return;
  }
  ASC_M7_CUDA_EQ(test, matrix->rows(), asc::extent_t{4});
  ASC_M7_CUDA_EQ(test, matrix->columns(), asc::extent_t{5});
  ASC_M7_CUDA_EQ(test, matrix->nnz(), asc::nnz_t{8});
  auto view = matrix->view();
  ASC_M7_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  ASC_M7_CUDA_CHECK(test, view->canonical_structure_trusted());
  auto offsets = asc_random_cuda_test::Download<asc::nnz_t>(
      view->outer_offsets(), kOffsets.size(), asc::MemorySpace::kDevice,
      fixture.execution);
  auto indices = asc_random_cuda_test::Download<asc::index_t>(
      view->inner_indices(), kIndices.size(), asc::MemorySpace::kDevice,
      fixture.execution);
  auto values = asc_random_cuda_test::Download<Element>(
      view->values(), kValues.size(), asc::MemorySpace::kDevice,
      fixture.execution);
  ASC_M7_CUDA_CHECK(test, offsets.ok());
  ASC_M7_CUDA_CHECK(test, indices.ok());
  ASC_M7_CUDA_CHECK(test, values.ok());
  if (offsets.ok()) {
    ASC_M7_CUDA_EQ(test, *offsets,
                   std::vector<asc::nnz_t>(kOffsets.begin(), kOffsets.end()));
  }
  if (indices.ok()) {
    ASC_M7_CUDA_EQ(test, *indices,
                   std::vector<asc::index_t>(kIndices.begin(), kIndices.end()));
  }
  if (values.ok()) {
    ASC_M7_CUDA_EQ(test, *values,
                   std::vector<Element>(kValues.begin(), kValues.end()));
  }

  constexpr std::array<asc::extent_t, 2> kEmptyShape = {0, 5};
  constexpr std::array<asc::nnz_t, 1> kEmptyOffsets = {0};
  auto empty = CloneMatrix<Element>(fixture, kEmptyShape, kEmptyOffsets,
                                    std::span<const asc::index_t>(),
                                    std::span<const Element>());
  ASC_M7_CUDA_CHECK(test, empty.ok());
  if (empty.ok()) {
    auto empty_view = empty->view();
    ASC_M7_CUDA_CHECK(test, empty_view.ok());
    if (empty_view.ok()) {
      auto empty_offsets = asc_random_cuda_test::Download<asc::nnz_t>(
          empty_view->outer_offsets(), 1, asc::MemorySpace::kDevice,
          fixture.execution);
      ASC_M7_CUDA_CHECK(test, empty_offsets.ok());
      if (empty_offsets.ok()) {
        ASC_M7_CUDA_EQ(test, empty_offsets->front(), asc::nnz_t{0});
      }
    }
  }
}

void CheckCloneAllocations(Fixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape = {2, 3};
  constexpr std::array<asc::nnz_t, 3> kOffsets = {0, 2, 3};
  constexpr std::array<asc::index_t, 3> kIndices = {0, 2, 1};
  constexpr std::array<float, 3> kValues = {2.0F, -3.0F, 4.0F};
  auto source = asc::CsrView<const float>::Create(
      kOffsets, kIndices, kValues, kShape, asc::MemorySpace::kHost);
  ASC_M7_CUDA_CHECK(test, source.ok());
  if (!source.ok()) {
    return;
  }

  TrackingDeviceResource successful(*fixture.resource);
  {
    auto clone = asc::CudaCloneCsr(fixture.provider, *source, successful);
    ASC_M7_CUDA_CHECK(test, clone.ok());
    if (clone.ok()) {
      ASC_M7_CUDA_CHECK(test, clone->completion.Wait().ok());
      ASC_M7_CUDA_EQ(test, successful.attempts(), std::size_t{3});
      ASC_M7_CUDA_EQ(test, successful.live(), std::size_t{3});
    }
  }
  ASC_M7_CUDA_EQ(test, successful.live(), std::size_t{0});
  ASC_M7_CUDA_EQ(test, successful.deallocations(), std::size_t{3});

  for (std::size_t failure_call = 0; failure_call < 3; ++failure_call) {
    TrackingDeviceResource failing(*fixture.resource);
    failing.FailOnCall(failure_call);
    auto clone = asc::CudaCloneCsr(fixture.provider, *source, failing);
    ASC_M7_CUDA_CHECK(test, !clone.ok());
    ASC_M7_CUDA_EQ(test, failing.attempts(), failure_call + 1U);
    ASC_M7_CUDA_EQ(test, failing.live(), std::size_t{0});
    ASC_M7_CUDA_EQ(test, failing.deallocations(), failure_call);
  }

  auto offset_values = source->template RebindValues<const float>(
      reinterpret_cast<const float*>(source->outer_offsets()));
  auto index_values = source->template RebindValues<const float>(
      reinterpret_cast<const float*>(source->inner_indices()));
  ASC_M7_CUDA_CHECK(test, !offset_values.ok());
  ASC_M7_CUDA_CHECK(test, !index_values.ok());
}

template <typename Element>
void CheckStridedSpmv(Fixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape = {4, 5};
  constexpr std::array<asc::nnz_t, 5> kOffsets = {0, 2, 3, 6, 8};
  constexpr std::array<asc::index_t, 8> kIndices = {0, 3, 1, 0, 2, 4, 1, 4};
  constexpr std::array<Element, 8> kValues = {
      Element{2}, Element{-1}, Element{4},  Element{7},
      Element{3}, Element{5},  Element{-2}, Element{6}};
  auto matrix =
      CloneMatrix<Element>(fixture, kShape, kOffsets, kIndices, kValues);
  ASC_M7_CUDA_CHECK(test, matrix.ok());
  if (!matrix.ok()) {
    return;
  }
  auto mutable_matrix = matrix->view();
  ASC_M7_CUDA_CHECK(test, mutable_matrix.ok());
  if (!mutable_matrix.ok()) {
    return;
  }
  const asc::CsrView<const Element> const_matrix(*mutable_matrix);

  constexpr asc::stride_t kInputStride = 2;
  constexpr asc::stride_t kOutputStride = 3;
  constexpr Element kPadding = Element{91};
  const std::array<Element, 9> input = {Element{1}, kPadding,   Element{-2},
                                        kPadding,   Element{3}, kPadding,
                                        Element{4}, kPadding,   Element{-1}};
  const std::array<Element, 10> initial_output = {
      Element{10}, kPadding,    kPadding, Element{20}, kPadding,
      kPadding,    Element{30}, kPadding, kPadding,    Element{40}};
  auto input_device = asc_random_cuda_test::Upload<Element>(
      input, *fixture.resource, fixture.execution);
  auto output_device = asc_random_cuda_test::Upload<Element>(
      initial_output, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, input_device.ok());
  ASC_M7_CUDA_CHECK(test, output_device.ok());
  if (!input_device.ok() || !output_device.ok()) {
    return;
  }
  auto input_view = asc::CudaStridedVectorView<const Element>::Create(
      static_cast<const Element*>(input_device->data()), 5, kInputStride);
  auto output_view = asc::CudaStridedVectorView<Element>::Create(
      static_cast<Element*>(output_device->data()), 4, kOutputStride);
  ASC_M7_CUDA_CHECK(test, input_view.ok());
  ASC_M7_CUDA_CHECK(test, output_view.ok());
  if (!input_view.ok() || !output_view.ok()) {
    return;
  }
  auto workspace = asc::CudaCsrSpmvWorkspaceSize(fixture.provider, const_matrix,
                                                 *input_view, *output_view);
  ASC_M7_CUDA_CHECK(test, workspace.ok());
  if (!workspace.ok()) {
    return;
  }
  ASC_M7_CUDA_EQ(test, *workspace, std::size_t{0});
  auto event = asc::CudaCsrSpmv(
      fixture.provider, Element{2}, const_matrix, *input_view, Element{-0.5},
      *output_view,
      asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
  ASC_M7_CUDA_CHECK(test, event.ok());
  if (!event.ok()) {
    return;
  }
  ASC_M7_CUDA_CHECK(test, event->Wait().ok());
  auto actual = asc_random_cuda_test::Download<Element>(*output_device,
                                                        fixture.execution);
  ASC_M7_CUDA_CHECK(test, actual.ok());
  if (actual.ok()) {
    const std::array<Element, 4> expected = {Element{-9}, Element{-26},
                                             Element{7}, Element{-24}};
    for (std::size_t row = 0; row < expected.size(); ++row) {
      ASC_M7_CUDA_EQ(test,
                     (*actual)[row * static_cast<std::size_t>(kOutputStride)],
                     expected[row]);
    }
    for (std::size_t hole : {std::size_t{1}, std::size_t{2}, std::size_t{4},
                             std::size_t{5}, std::size_t{7}, std::size_t{8}}) {
      ASC_M7_CUDA_EQ(test, (*actual)[hole], kPadding);
    }
  }

  std::array<Element, 10> nan_output;
  nan_output.fill(std::numeric_limits<Element>::quiet_NaN());
  auto beta_zero_device = asc_random_cuda_test::Upload<Element>(
      nan_output, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, beta_zero_device.ok());
  if (beta_zero_device.ok()) {
    auto beta_zero_view = asc::CudaStridedVectorView<Element>::Create(
        static_cast<Element*>(beta_zero_device->data()), 4, kOutputStride);
    ASC_M7_CUDA_CHECK(test, beta_zero_view.ok());
    if (beta_zero_view.ok()) {
      auto beta_zero = asc::CudaCsrSpmv(
          fixture.provider, Element{1}, const_matrix, *input_view, Element{0},
          *beta_zero_view,
          asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
      ASC_M7_CUDA_CHECK(test, beta_zero.ok());
      if (beta_zero.ok()) {
        ASC_M7_CUDA_CHECK(test, beta_zero->Wait().ok());
        auto result = asc_random_cuda_test::Download<Element>(
            *beta_zero_device, fixture.execution);
        ASC_M7_CUDA_CHECK(test, result.ok());
        if (result.ok()) {
          const std::array<Element, 4> expected = {Element{-2}, Element{-8},
                                                   Element{11}, Element{-2}};
          for (std::size_t row = 0; row < expected.size(); ++row) {
            ASC_M7_CUDA_EQ(
                test, (*result)[row * static_cast<std::size_t>(kOutputStride)],
                expected[row]);
          }
        }
      }
    }
  }
}

template <typename Element>
void CheckUnitStrideWorkspace(Fixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape = {2, 2};
  constexpr std::array<asc::nnz_t, 3> kOffsets = {0, 1, 2};
  constexpr std::array<asc::index_t, 2> kIndices = {0, 1};
  constexpr std::array<Element, 2> kValues = {Element{2}, Element{3}};
  constexpr std::array<Element, 2> kInput = {Element{4}, Element{5}};
  constexpr std::array<Element, 2> kOutput = {Element{1}, Element{1}};
  auto matrix =
      CloneMatrix<Element>(fixture, kShape, kOffsets, kIndices, kValues);
  auto input = asc_random_cuda_test::Upload(kInput, *fixture.resource,
                                            fixture.execution);
  auto output = asc_random_cuda_test::Upload(kOutput, *fixture.resource,
                                             fixture.execution);
  ASC_M7_CUDA_CHECK(test, matrix.ok());
  ASC_M7_CUDA_CHECK(test, input.ok());
  ASC_M7_CUDA_CHECK(test, output.ok());
  if (!matrix.ok() || !input.ok() || !output.ok()) {
    return;
  }
  auto mutable_matrix = matrix->view();
  if (!mutable_matrix.ok()) {
    ASC_M7_CUDA_CHECK(test, false);
    return;
  }
  const asc::CsrView<const Element> const_matrix(*mutable_matrix);
  auto input_view = asc::CudaStridedVectorView<const Element>::Create(
      static_cast<const Element*>(input->data()), 2, 1);
  auto output_view = asc::CudaStridedVectorView<Element>::Create(
      static_cast<Element*>(output->data()), 2, 1);
  if (!input_view.ok() || !output_view.ok()) {
    ASC_M7_CUDA_CHECK(test, false);
    return;
  }
  auto required = asc::CudaCsrSpmvWorkspaceSize(fixture.provider, const_matrix,
                                                *input_view, *output_view);
  ASC_M7_CUDA_CHECK(test, required.ok());
  if (!required.ok()) {
    return;
  }
  auto workspace = asc::Buffer::Allocate(
      *fixture.resource, std::max(*required, std::size_t{1}), 256);
  ASC_M7_CUDA_CHECK(test, workspace.ok());
  if (!workspace.ok()) {
    return;
  }
  const asc::MutableMemoryView workspace_view =
      *required == 0
          ? asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice)
          : asc::MutableMemoryView(workspace->data(), *required,
                                   asc::MemorySpace::kDevice);
  auto event =
      asc::CudaCsrSpmv(fixture.provider, Element{2}, const_matrix, *input_view,
                       Element{-1}, *output_view, workspace_view);
  ASC_M7_CUDA_CHECK(test, event.ok());
  if (event.ok()) {
    ASC_M7_CUDA_CHECK(test, event->Wait().ok());
    auto actual =
        asc_random_cuda_test::Download<Element>(*output, fixture.execution);
    ASC_M7_CUDA_CHECK(test, actual.ok());
    if (actual.ok()) {
      const std::vector<Element> expected = {Element{15}, Element{29}};
      ASC_M7_CUDA_EQ(test, *actual, expected);
    }
  }
  if (*required > 0) {
    const auto undersized = asc::CudaCsrSpmv(
        fixture.provider, Element{2}, const_matrix, *input_view, Element{-1},
        *output_view,
        asc::MutableMemoryView(workspace->data(), *required - 1U,
                               asc::MemorySpace::kDevice));
    ASC_M7_CUDA_CHECK(test, !undersized.ok());
    std::vector<std::byte> host_workspace(*required);
    const auto host = asc::CudaCsrSpmv(
        fixture.provider, Element{2}, const_matrix, *input_view, Element{-1},
        *output_view,
        asc::MutableMemoryView(host_workspace.data(), host_workspace.size(),
                               asc::MemorySpace::kHost));
    ASC_M7_CUDA_CHECK(test, !host.ok());
  }
}

void CheckWorkspaceOverlap(Fixture& fixture, TestContext& test) {
  constexpr asc::extent_t kDimension = 4096;
  const std::array<asc::extent_t, 2> shape = {kDimension, kDimension};
  std::vector<asc::nnz_t> offsets(static_cast<std::size_t>(kDimension) + 1U);
  std::vector<asc::index_t> indices(static_cast<std::size_t>(kDimension));
  std::vector<float> values(static_cast<std::size_t>(kDimension), 2.0F);
  std::vector<float> input(static_cast<std::size_t>(kDimension), 3.0F);
  std::vector<float> output(static_cast<std::size_t>(kDimension), 41.0F);
  for (asc::extent_t index = 0; index < kDimension; ++index) {
    offsets[static_cast<std::size_t>(index)] = index;
    indices[static_cast<std::size_t>(index)] = index;
  }
  offsets.back() = kDimension;
  auto matrix = CloneMatrix<float>(fixture, shape, offsets, indices, values);
  auto input_device = asc_random_cuda_test::Upload(
      std::span<const float>(input), *fixture.resource, fixture.execution);
  auto output_device = asc_random_cuda_test::Upload(
      std::span<const float>(output), *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, matrix.ok());
  ASC_M7_CUDA_CHECK(test, input_device.ok());
  ASC_M7_CUDA_CHECK(test, output_device.ok());
  if (!matrix.ok() || !input_device.ok() || !output_device.ok()) {
    return;
  }
  auto matrix_view = matrix->view();
  auto input_view = asc::CudaStridedVectorView<const float>::Create(
      static_cast<const float*>(input_device->data()), kDimension, 1);
  auto output_view = asc::CudaStridedVectorView<float>::Create(
      static_cast<float*>(output_device->data()), kDimension, 1);
  ASC_M7_CUDA_CHECK(test, matrix_view.ok());
  ASC_M7_CUDA_CHECK(test, input_view.ok());
  ASC_M7_CUDA_CHECK(test, output_view.ok());
  if (!matrix_view.ok() || !input_view.ok() || !output_view.ok()) {
    return;
  }
  const asc::CsrView<const float> const_matrix(*matrix_view);
  auto required = asc::CudaCsrSpmvWorkspaceSize(fixture.provider, const_matrix,
                                                *input_view, *output_view);
  ASC_M7_CUDA_CHECK(test, required.ok());
  if (!required.ok()) {
    return;
  }
  const std::array<asc::MutableMemoryView, 5> overlapping_workspaces = {
      asc::MutableMemoryView(
          const_cast<asc::nnz_t*>(const_matrix.outer_offsets()),
          offsets.size() * sizeof(asc::nnz_t), asc::MemorySpace::kDevice),
      asc::MutableMemoryView(
          const_cast<asc::index_t*>(const_matrix.inner_indices()),
          indices.size() * sizeof(asc::index_t), asc::MemorySpace::kDevice),
      asc::MutableMemoryView(const_cast<float*>(const_matrix.values()),
                             values.size() * sizeof(float),
                             asc::MemorySpace::kDevice),
      asc::MutableMemoryView(const_cast<float*>(input_view->data()),
                             input.size() * sizeof(float),
                             asc::MemorySpace::kDevice),
      asc::MutableMemoryView(output_view->data(), output.size() * sizeof(float),
                             asc::MemorySpace::kDevice)};
  for (const auto overlapping_workspace : overlapping_workspaces) {
    ASC_M7_CUDA_CHECK(test, overlapping_workspace.size() >= *required);
    if (overlapping_workspace.size() < *required) {
      continue;
    }
    const auto overlap =
        asc::CudaCsrSpmv(fixture.provider, 1.0F, const_matrix, *input_view,
                         0.0F, *output_view, overlapping_workspace);
    ASC_M7_CUDA_CHECK(test, !overlap.ok());
  }
  auto unchanged =
      asc_random_cuda_test::Download<float>(*output_device, fixture.execution);
  ASC_M7_CUDA_CHECK(test, unchanged.ok());
  if (unchanged.ok()) {
    ASC_M7_CUDA_EQ(test, *unchanged, output);
  }
}

template <typename Element>
void CheckEvaluator(Fixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape = {2, 3};
  constexpr std::array<asc::nnz_t, 3> kOffsets = {0, 2, 3};
  constexpr std::array<asc::index_t, 3> kIndices = {0, 2, 1};
  constexpr std::array<Element, 3> kValues = {Element{2}, Element{-3},
                                              Element{4}};
  auto matrix =
      CloneMatrix<Element>(fixture, kShape, kOffsets, kIndices, kValues);
  ASC_M7_CUDA_CHECK(test, matrix.ok());
  if (!matrix.ok()) {
    return;
  }
  auto view = matrix->view();
  ASC_M7_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  const asc::CsrView<const Element> source(*view);

  auto exact_values = view->template RebindValues<Element>(view->values());
  ASC_M7_CUDA_CHECK(test, exact_values.ok());
  if (!exact_values.ok()) {
    return;
  }
  auto terminal = asc::CudaEvaluate(fixture.provider, source, *exact_values);
  ASC_M7_CUDA_CHECK(test, terminal.ok());
  if (terminal.ok()) {
    ASC_M7_CUDA_CHECK(test, terminal->Wait().ok());
  }

  constexpr std::array<Element, 3> kDistinctValues = {Element{11}, Element{13},
                                                      Element{17}};
  auto distinct_storage = asc_random_cuda_test::Upload<Element>(
      kDistinctValues, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, distinct_storage.ok());
  if (!distinct_storage.ok()) {
    return;
  }
  auto distinct_source = view->template RebindValues<const Element>(
      static_cast<const Element*>(distinct_storage->data()));
  ASC_M7_CUDA_CHECK(test, distinct_source.ok());
  if (distinct_source.ok()) {
    auto copied =
        asc::CudaEvaluate(fixture.provider, *distinct_source, *exact_values);
    ASC_M7_CUDA_CHECK(test, copied.ok());
    if (copied.ok()) {
      ASC_M7_CUDA_CHECK(test, copied->Wait().ok());
    }
  }

  constexpr std::array<Element, 4> kOverlapValues = {Element{19}, Element{23},
                                                     Element{29}, Element{31}};
  auto overlap_storage = asc_random_cuda_test::Upload<Element>(
      kOverlapValues, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, overlap_storage.ok());
  if (overlap_storage.ok()) {
    auto overlap_destination = view->template RebindValues<Element>(
        static_cast<Element*>(overlap_storage->data()));
    auto overlap_source = view->template RebindValues<const Element>(
        static_cast<const Element*>(overlap_storage->data()) + 1);
    ASC_M7_CUDA_CHECK(test, overlap_destination.ok());
    ASC_M7_CUDA_CHECK(test, overlap_source.ok());
    if (overlap_destination.ok() && overlap_source.ok()) {
      auto partial_rebind = overlap_destination->template RebindValues<Element>(
          static_cast<Element*>(overlap_storage->data()) + 1);
      ASC_M7_CUDA_CHECK(test, !partial_rebind.ok());
      const auto rejected = asc::CudaEvaluate(fixture.provider, *overlap_source,
                                              *overlap_destination);
      ASC_M7_CUDA_CHECK(test, !rejected.ok());
      auto unchanged = asc_random_cuda_test::Download<Element>(
          *overlap_storage, fixture.execution);
      ASC_M7_CUDA_CHECK(test, unchanged.ok());
      if (unchanged.ok()) {
        ASC_M7_CUDA_EQ(
            test, *unchanged,
            std::vector<Element>(kOverlapValues.begin(), kOverlapValues.end()));
      }
    }
  }

  auto offset_values =
      view->template RebindValues<Element>(reinterpret_cast<Element*>(
          const_cast<asc::nnz_t*>(view->outer_offsets())));
  auto index_values =
      view->template RebindValues<Element>(reinterpret_cast<Element*>(
          const_cast<asc::index_t*>(view->inner_indices())));
  ASC_M7_CUDA_CHECK(test, !offset_values.ok());
  ASC_M7_CUDA_CHECK(test, !index_values.ok());

  const auto negated_expression = asc::MakeNegate(source);
  auto negated = asc::CudaEvaluate(fixture.provider, negated_expression, *view);
  ASC_M7_CUDA_CHECK(test, negated.ok());
  if (negated.ok()) {
    ASC_M7_CUDA_CHECK(test, negated->Wait().ok());
  }

  auto nested_inner = asc::MakeMultiply(source, Element{2});
  ASC_M7_CUDA_CHECK(test, nested_inner.ok());
  if (nested_inner.ok()) {
    const auto nested = asc::MakeNegate(*nested_inner);
    ASC_M7_CUDA_CHECK(test,
                      !asc::CudaEvaluate(fixture.provider, nested, *view).ok());
  }

  auto scaled_expression = asc::MakeMultiply(source, Element{-2});
  ASC_M7_CUDA_CHECK(test, scaled_expression.ok());
  if (scaled_expression.ok()) {
    auto scaled =
        asc::CudaEvaluate(fixture.provider, *scaled_expression, *view);
    ASC_M7_CUDA_CHECK(test, scaled.ok());
    if (scaled.ok()) {
      ASC_M7_CUDA_CHECK(test, scaled->Wait().ok());
    }
  }

  auto add_zero_expression = asc::MakeAdd(source, Element{0});
  auto zero_add_expression = asc::MakeAdd(Element{0}, source);
  auto subtract_zero_expression = asc::MakeSubtract(source, Element{0});
  auto zero_subtract_expression = asc::MakeSubtract(Element{0}, source);
  ASC_M7_CUDA_CHECK(test, add_zero_expression.ok());
  ASC_M7_CUDA_CHECK(test, zero_add_expression.ok());
  ASC_M7_CUDA_CHECK(test, subtract_zero_expression.ok());
  ASC_M7_CUDA_CHECK(test, zero_subtract_expression.ok());
  if (!add_zero_expression.ok() || !zero_add_expression.ok() ||
      !subtract_zero_expression.ok() || !zero_subtract_expression.ok()) {
    return;
  }
  auto add_zero =
      asc::CudaEvaluate(fixture.provider, *add_zero_expression, *view);
  auto zero_add =
      asc::CudaEvaluate(fixture.provider, *zero_add_expression, *view);
  auto subtract_zero =
      asc::CudaEvaluate(fixture.provider, *subtract_zero_expression, *view);
  auto zero_subtract =
      asc::CudaEvaluate(fixture.provider, *zero_subtract_expression, *view);
  ASC_M7_CUDA_CHECK(test, add_zero.ok());
  ASC_M7_CUDA_CHECK(test, zero_add.ok());
  ASC_M7_CUDA_CHECK(test, subtract_zero.ok());
  ASC_M7_CUDA_CHECK(test, zero_subtract.ok());
  if (add_zero.ok()) {
    ASC_M7_CUDA_CHECK(test, add_zero->Wait().ok());
  }
  if (zero_add.ok()) {
    ASC_M7_CUDA_CHECK(test, zero_add->Wait().ok());
  }
  if (subtract_zero.ok()) {
    ASC_M7_CUDA_CHECK(test, subtract_zero->Wait().ok());
  }
  if (zero_subtract.ok()) {
    ASC_M7_CUDA_CHECK(test, zero_subtract->Wait().ok());
  }

  auto add_expression = asc::MakeAdd(source, source);
  ASC_M7_CUDA_CHECK(test, add_expression.ok());
  if (add_expression.ok()) {
    auto added = asc::CudaEvaluate(fixture.provider, *add_expression, *view);
    ASC_M7_CUDA_CHECK(test, added.ok());
    if (added.ok()) {
      ASC_M7_CUDA_CHECK(test, added->Wait().ok());
    }
  }
  auto multiply_expression = asc::MakeMultiply(source, source);
  ASC_M7_CUDA_CHECK(test, multiply_expression.ok());
  if (multiply_expression.ok()) {
    auto multiplied =
        asc::CudaEvaluate(fixture.provider, *multiply_expression, *view);
    ASC_M7_CUDA_CHECK(test, multiplied.ok());
    if (multiplied.ok()) {
      ASC_M7_CUDA_CHECK(test, multiplied->Wait().ok());
    }
  }
  auto subtract_expression = asc::MakeSubtract(source, source);
  ASC_M7_CUDA_CHECK(test, subtract_expression.ok());
  if (subtract_expression.ok()) {
    auto subtracted =
        asc::CudaEvaluate(fixture.provider, *subtract_expression, *view);
    ASC_M7_CUDA_CHECK(test, subtracted.ok());
    if (subtracted.ok()) {
      ASC_M7_CUDA_CHECK(test, subtracted->Wait().ok());
    }
  }

  auto add_nonzero_expression = asc::MakeAdd(source, Element{1});
  auto nonzero_add_expression = asc::MakeAdd(Element{1}, source);
  auto subtract_nonzero_expression = asc::MakeSubtract(source, Element{1});
  auto nonzero_subtract_expression = asc::MakeSubtract(Element{1}, source);
  ASC_M7_CUDA_CHECK(test, add_nonzero_expression.ok());
  ASC_M7_CUDA_CHECK(test, nonzero_add_expression.ok());
  ASC_M7_CUDA_CHECK(test, subtract_nonzero_expression.ok());
  ASC_M7_CUDA_CHECK(test, nonzero_subtract_expression.ok());
  if (add_nonzero_expression.ok()) {
    ASC_M7_CUDA_CHECK(test, !asc::CudaEvaluate(fixture.provider,
                                               *add_nonzero_expression, *view)
                                 .ok());
  }
  if (nonzero_add_expression.ok()) {
    ASC_M7_CUDA_CHECK(test, !asc::CudaEvaluate(fixture.provider,
                                               *nonzero_add_expression, *view)
                                 .ok());
  }
  if (subtract_nonzero_expression.ok()) {
    ASC_M7_CUDA_CHECK(
        test, !asc::CudaEvaluate(fixture.provider, *subtract_nonzero_expression,
                                 *view)
                   .ok());
  }
  if (nonzero_subtract_expression.ok()) {
    ASC_M7_CUDA_CHECK(
        test, !asc::CudaEvaluate(fixture.provider, *nonzero_subtract_expression,
                                 *view)
                   .ok());
  }

  auto final_values = asc_random_cuda_test::Download<Element>(
      view->values(), kValues.size(), asc::MemorySpace::kDevice,
      fixture.execution);
  ASC_M7_CUDA_CHECK(test, final_values.ok());
  if (final_values.ok()) {
    const std::vector<Element> expected(kValues.size(), Element{0});
    ASC_M7_CUDA_EQ(test, *final_values, expected);
  }
  auto offsets = asc_random_cuda_test::Download<asc::nnz_t>(
      view->outer_offsets(), kOffsets.size(), asc::MemorySpace::kDevice,
      fixture.execution);
  auto indices = asc_random_cuda_test::Download<asc::index_t>(
      view->inner_indices(), kIndices.size(), asc::MemorySpace::kDevice,
      fixture.execution);
  if (offsets.ok()) {
    ASC_M7_CUDA_EQ(test, *offsets,
                   std::vector<asc::nnz_t>(kOffsets.begin(), kOffsets.end()));
  }
  if (indices.ok()) {
    ASC_M7_CUDA_EQ(test, *indices,
                   std::vector<asc::index_t>(kIndices.begin(), kIndices.end()));
  }
}

template <typename Element>
void CheckCoordinateEvaluator(Fixture& fixture, TestContext& test) {
  using Shape = asc::Extents<2, 3>;
  auto extents = Shape::Create();
  ASC_M7_CUDA_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }
  auto generated = asc::CudaGenerateSparseUniform01<Element>(
      fixture.execution, *extents, 3, *fixture.resource, 101, 103, 107, 109,
      113, 127);
  ASC_M7_CUDA_CHECK(test, generated.ok());
  if (!generated.ok()) {
    return;
  }
  ASC_M7_CUDA_CHECK(test, generated->completion.Wait().ok());
  auto view = generated->array.view();
  ASC_M7_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  auto coordinates_before = asc_random_cuda_test::Download<asc::index_t>(
      view->coordinates(), static_cast<std::size_t>(view->nnz()) * Shape::kRank,
      asc::MemorySpace::kDevice, fixture.execution);
  auto values_before = asc_random_cuda_test::Download<Element>(
      view->values(), static_cast<std::size_t>(view->nnz()),
      asc::MemorySpace::kDevice, fixture.execution);
  ASC_M7_CUDA_CHECK(test, coordinates_before.ok());
  ASC_M7_CUDA_CHECK(test, values_before.ok());
  if (!coordinates_before.ok() || !values_before.ok()) {
    return;
  }

  auto exact_values = view->template RebindValues<Element>(view->values());
  ASC_M7_CUDA_CHECK(test, exact_values.ok());
  if (exact_values.ok()) {
    const asc::CoordinateView<const Element, Shape::kRank> source(*view);
    auto copied = asc::CudaEvaluate(fixture.provider, source, *exact_values);
    ASC_M7_CUDA_CHECK(test, copied.ok());
    if (copied.ok()) {
      ASC_M7_CUDA_CHECK(test, copied->Wait().ok());
    }
  }

  constexpr std::array<Element, 3> kDistinctValues = {Element{2}, Element{3},
                                                      Element{5}};
  auto distinct_storage = asc_random_cuda_test::Upload<Element>(
      kDistinctValues, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, distinct_storage.ok());
  if (distinct_storage.ok()) {
    auto distinct_source = view->template RebindValues<const Element>(
        static_cast<const Element*>(distinct_storage->data()));
    ASC_M7_CUDA_CHECK(test, distinct_source.ok());
    if (distinct_source.ok()) {
      auto copied =
          asc::CudaEvaluate(fixture.provider, *distinct_source, *view);
      ASC_M7_CUDA_CHECK(test, copied.ok());
      if (copied.ok()) {
        ASC_M7_CUDA_CHECK(test, copied->Wait().ok());
      }
    }
  }

  constexpr std::array<Element, 4> kOverlapValues = {Element{7}, Element{11},
                                                     Element{13}, Element{17}};
  auto overlap_storage = asc_random_cuda_test::Upload<Element>(
      kOverlapValues, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, overlap_storage.ok());
  if (overlap_storage.ok()) {
    auto overlap_destination = view->template RebindValues<Element>(
        static_cast<Element*>(overlap_storage->data()));
    auto overlap_source = view->template RebindValues<const Element>(
        static_cast<const Element*>(overlap_storage->data()) + 1);
    ASC_M7_CUDA_CHECK(test, overlap_destination.ok());
    ASC_M7_CUDA_CHECK(test, overlap_source.ok());
    if (overlap_destination.ok() && overlap_source.ok()) {
      auto partial_rebind = overlap_destination->template RebindValues<Element>(
          static_cast<Element*>(overlap_storage->data()) + 1);
      ASC_M7_CUDA_CHECK(test, !partial_rebind.ok());
      ASC_M7_CUDA_CHECK(test,
                        !asc::CudaEvaluate(fixture.provider, *overlap_source,
                                           *overlap_destination)
                             .ok());
    }
  }

  auto structure_values =
      view->template RebindValues<Element>(reinterpret_cast<Element*>(
          const_cast<asc::index_t*>(view->coordinates())));
  ASC_M7_CUDA_CHECK(test, !structure_values.ok());

  const auto negated_expression =
      asc::MakeNegate(asc::CoordinateView<const Element, Shape::kRank>(*view));
  auto negated = asc::CudaEvaluate(fixture.provider, negated_expression, *view);
  ASC_M7_CUDA_CHECK(test, negated.ok());
  if (negated.ok()) {
    ASC_M7_CUDA_CHECK(test, negated->Wait().ok());
  }

  auto coordinates_after = asc_random_cuda_test::Download<asc::index_t>(
      view->coordinates(), static_cast<std::size_t>(view->nnz()) * Shape::kRank,
      asc::MemorySpace::kDevice, fixture.execution);
  ASC_M7_CUDA_CHECK(test, coordinates_after.ok());
  if (coordinates_after.ok()) {
    ASC_M7_CUDA_EQ(test, *coordinates_after, *coordinates_before);
  }
}

void CheckCoordinateRankLimit(Fixture& fixture, TestContext& test) {
  using Rank9 = asc::Extents<1, 1, 1, 1, 1, 1, 1, 1, 1>;
  auto extents = Rank9::Create();
  ASC_M7_CUDA_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }
  auto generated = asc::CudaGenerateSparseUniform01<float>(
      fixture.execution, *extents, 1, *fixture.resource, 131, 137, 139, 149,
      151, 157);
  ASC_M7_CUDA_CHECK(test, generated.ok());
  if (!generated.ok()) {
    return;
  }
  ASC_M7_CUDA_CHECK(test, generated->completion.Wait().ok());
  auto view = generated->array.view();
  ASC_M7_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  const asc::CoordinateView<const float, Rank9::kRank> source(*view);
  auto coordinates_before = asc_random_cuda_test::Download<asc::index_t>(
      view->coordinates(), Rank9::kRank, asc::MemorySpace::kDevice,
      fixture.execution);
  auto values_before = asc_random_cuda_test::Download<float>(
      view->values(), 1, asc::MemorySpace::kDevice, fixture.execution);
  ASC_M7_CUDA_CHECK(test, coordinates_before.ok());
  ASC_M7_CUDA_CHECK(test, values_before.ok());
  auto evaluated = asc::CudaEvaluate(fixture.provider, source, *view);
  ASC_M7_CUDA_CHECK(test, evaluated.ok());
  if (evaluated.ok()) {
    ASC_M7_CUDA_CHECK(test, evaluated->Wait().ok());
  }
  auto coordinates_after = asc_random_cuda_test::Download<asc::index_t>(
      view->coordinates(), Rank9::kRank, asc::MemorySpace::kDevice,
      fixture.execution);
  auto values_after = asc_random_cuda_test::Download<float>(
      view->values(), 1, asc::MemorySpace::kDevice, fixture.execution);
  ASC_M7_CUDA_CHECK(test, coordinates_after.ok());
  ASC_M7_CUDA_CHECK(test, values_after.ok());
  if (coordinates_before.ok() && coordinates_after.ok()) {
    ASC_M7_CUDA_EQ(test, *coordinates_after, *coordinates_before);
  }
  if (values_before.ok() && values_after.ok()) {
    ASC_M7_CUDA_BIT_EQ(test, values_after->front(), values_before->front());
  }
}

void CheckIndependentContexts(Fixture& fixture, TestContext& test) {
  auto second = MakeFixture();
  ASC_M7_CUDA_CHECK(test, second.ok());
  if (!second.ok()) {
    return;
  }
  constexpr std::array<asc::extent_t, 2> kShape = {2, 2};
  constexpr std::array<asc::nnz_t, 3> kOffsets = {0, 1, 2};
  constexpr std::array<asc::index_t, 2> kIndices = {0, 1};
  constexpr std::array<float, 2> kValues = {2.0F, 3.0F};
  auto source = asc::CsrView<const float>::Create(
      kOffsets, kIndices, kValues, kShape, asc::MemorySpace::kHost);
  ASC_M7_CUDA_CHECK(test, source.ok());
  if (!source.ok()) {
    return;
  }
  auto first_clone =
      asc::CudaCloneCsr(fixture.provider, *source, *fixture.resource);
  auto second_clone =
      asc::CudaCloneCsr(second->provider, *source, *second->resource);
  ASC_M7_CUDA_CHECK(test, first_clone.ok());
  ASC_M7_CUDA_CHECK(test, second_clone.ok());
  if (!first_clone.ok() || !second_clone.ok()) {
    return;
  }
  ASC_M7_CUDA_CHECK(test, second_clone->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, first_clone->completion.Wait().ok());
  auto first_matrix = first_clone->array.view();
  auto second_matrix = second_clone->array.view();
  ASC_M7_CUDA_CHECK(test, first_matrix.ok());
  ASC_M7_CUDA_CHECK(test, second_matrix.ok());
  if (!first_matrix.ok() || !second_matrix.ok()) {
    return;
  }

  const asc::CsrView<const float> first_source(*first_matrix);
  const asc::CsrView<const float> second_source(*second_matrix);
  const auto first_expression = asc::MakeNegate(first_source);
  const auto second_expression = asc::MakeNegate(second_source);
  auto first_evaluation =
      asc::CudaEvaluate(fixture.provider, first_expression, *first_matrix);
  auto second_evaluation =
      asc::CudaEvaluate(second->provider, second_expression, *second_matrix);
  ASC_M7_CUDA_CHECK(test, first_evaluation.ok());
  ASC_M7_CUDA_CHECK(test, second_evaluation.ok());
  if (!first_evaluation.ok() || !second_evaluation.ok()) {
    return;
  }
  ASC_M7_CUDA_CHECK(test, second_evaluation->Wait().ok());
  ASC_M7_CUDA_CHECK(test, first_evaluation->Wait().ok());

  constexpr std::array<float, 3> kInput = {4.0F, 91.0F, 5.0F};
  constexpr std::array<float, 3> kOutput = {0.0F, 91.0F, 0.0F};
  auto first_input = asc_random_cuda_test::Upload(kInput, *fixture.resource,
                                                  fixture.execution);
  auto first_output = asc_random_cuda_test::Upload(kOutput, *fixture.resource,
                                                   fixture.execution);
  auto second_input = asc_random_cuda_test::Upload(kInput, *second->resource,
                                                   second->execution);
  auto second_output = asc_random_cuda_test::Upload(kOutput, *second->resource,
                                                    second->execution);
  if (!first_input.ok() || !first_output.ok() || !second_input.ok() ||
      !second_output.ok()) {
    ASC_M7_CUDA_CHECK(test, false);
    return;
  }
  auto first_input_view = asc::CudaStridedVectorView<const float>::Create(
      static_cast<const float*>(first_input->data()), 2, 2);
  auto first_output_view = asc::CudaStridedVectorView<float>::Create(
      static_cast<float*>(first_output->data()), 2, 2);
  auto second_input_view = asc::CudaStridedVectorView<const float>::Create(
      static_cast<const float*>(second_input->data()), 2, 2);
  auto second_output_view = asc::CudaStridedVectorView<float>::Create(
      static_cast<float*>(second_output->data()), 2, 2);
  if (!first_input_view.ok() || !first_output_view.ok() ||
      !second_input_view.ok() || !second_output_view.ok()) {
    ASC_M7_CUDA_CHECK(test, false);
    return;
  }
  auto first_spmv = asc::CudaCsrSpmv(
      fixture.provider, 1.0F, asc::CsrView<const float>(*first_matrix),
      *first_input_view, 0.0F, *first_output_view,
      asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
  auto second_spmv = asc::CudaCsrSpmv(
      second->provider, 1.0F, asc::CsrView<const float>(*second_matrix),
      *second_input_view, 0.0F, *second_output_view,
      asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
  ASC_M7_CUDA_CHECK(test, first_spmv.ok());
  ASC_M7_CUDA_CHECK(test, second_spmv.ok());
  if (first_spmv.ok() && second_spmv.ok()) {
    ASC_M7_CUDA_CHECK(test, second_spmv->Wait().ok());
    ASC_M7_CUDA_CHECK(test, first_spmv->Wait().ok());
  }
  auto first_result =
      asc_random_cuda_test::Download<float>(*first_output, fixture.execution);
  auto second_result =
      asc_random_cuda_test::Download<float>(*second_output, second->execution);
  ASC_M7_CUDA_CHECK(test, first_result.ok());
  ASC_M7_CUDA_CHECK(test, second_result.ok());
  if (first_result.ok() && second_result.ok()) {
    ASC_M7_CUDA_EQ(test, *first_result, *second_result);
    const std::vector<float> expected = {-8.0F, 91.0F, -15.0F};
    ASC_M7_CUDA_EQ(test, *first_result, expected);
  }
}

void CheckFailures(Fixture& fixture, TestContext& test) {
  const auto serial =
      asc::SparseCudaContext::Create(asc::ExecutionContext::Serial());
  ASC_M7_CUDA_CHECK(test, !serial.ok());
  ASC_M7_CUDA_CHECK(
      test, !asc::CudaStridedVectorView<float>::Create(nullptr, 0, 0).ok());
  ASC_M7_CUDA_CHECK(
      test, !asc::CudaStridedVectorView<float>::Create(nullptr, 0, -1).ok());

  constexpr std::array<asc::extent_t, 2> kShape = {2, 2};
  constexpr std::array<asc::nnz_t, 3> kOffsets = {0, 1, 2};
  constexpr std::array<asc::index_t, 2> kIndices = {0, 1};
  constexpr std::array<float, 2> kValues = {2.0F, 3.0F};
  auto matrix =
      CloneMatrix<float>(fixture, kShape, kOffsets, kIndices, kValues);
  ASC_M7_CUDA_CHECK(test, matrix.ok());
  if (!matrix.ok()) {
    return;
  }
  auto matrix_view = matrix->view();
  const std::array<float, 2> input = {1.0F, 2.0F};
  const std::array<float, 2> output = {41.0F, 43.0F};
  auto input_device =
      asc_random_cuda_test::Upload(input, *fixture.resource, fixture.execution);
  auto output_device = asc_random_cuda_test::Upload(output, *fixture.resource,
                                                    fixture.execution);
  if (!matrix_view.ok() || !input_device.ok() || !output_device.ok()) {
    ASC_M7_CUDA_CHECK(test, false);
    return;
  }
  auto input_view = asc::CudaStridedVectorView<const float>::Create(
      static_cast<const float*>(input_device->data()), 2, 1);
  auto short_input = asc::CudaStridedVectorView<const float>::Create(
      static_cast<const float*>(input_device->data()), 1, 1);
  auto output_view = asc::CudaStridedVectorView<float>::Create(
      static_cast<float*>(output_device->data()), 2, 1);
  if (!input_view.ok() || !short_input.ok() || !output_view.ok()) {
    ASC_M7_CUDA_CHECK(test, false);
    return;
  }
  const asc::CsrView<const float> const_matrix(*matrix_view);
  auto offset_values = matrix_view->template RebindValues<const float>(
      reinterpret_cast<const float*>(matrix_view->outer_offsets()));
  auto index_values = matrix_view->template RebindValues<const float>(
      reinterpret_cast<const float*>(matrix_view->inner_indices()));
  ASC_M7_CUDA_CHECK(test, !offset_values.ok());
  ASC_M7_CUDA_CHECK(test, !index_values.ok());
  const auto wrong_shape = asc::CudaCsrSpmv(
      fixture.provider, 1.0F, const_matrix, *short_input, 0.0F, *output_view,
      asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
  ASC_M7_CUDA_CHECK(test, !wrong_shape.ok());

  auto overlapping_output = asc::CudaStridedVectorView<float>::Create(
      static_cast<float*>(input_device->data()), 2, 1);
  ASC_M7_CUDA_CHECK(test, overlapping_output.ok());
  if (overlapping_output.ok()) {
    const auto overlap = asc::CudaCsrSpmv(
        fixture.provider, 1.0F, const_matrix, *input_view, 0.0F,
        *overlapping_output,
        asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
    ASC_M7_CUDA_CHECK(test, !overlap.ok());
  }

  auto untrusted = asc::CsrView<const float>::Create(
      matrix_view->outer_offsets(), matrix_view->inner_indices(),
      matrix_view->values(), kShape, matrix_view->nnz(),
      asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, untrusted.ok());
  if (untrusted.ok()) {
    const auto rejected = asc::CudaCsrSpmvWorkspaceSize(
        fixture.provider, *untrusted, *input_view, *output_view);
    ASC_M7_CUDA_CHECK(test, !rejected.ok());
  }

  auto unchanged =
      asc_random_cuda_test::Download<float>(*output_device, fixture.execution);
  ASC_M7_CUDA_CHECK(test, unchanged.ok());
  if (unchanged.ok()) {
    ASC_M7_CUDA_EQ(test, *unchanged,
                   std::vector<float>(output.begin(), output.end()));
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
  CheckClone<float>(*fixture, test);
  CheckClone<double>(*fixture, test);
  CheckCloneAllocations(*fixture, test);
  CheckStridedSpmv<float>(*fixture, test);
  CheckStridedSpmv<double>(*fixture, test);
  CheckUnitStrideWorkspace<float>(*fixture, test);
  CheckUnitStrideWorkspace<double>(*fixture, test);
  CheckWorkspaceOverlap(*fixture, test);
  CheckEvaluator<float>(*fixture, test);
  CheckEvaluator<double>(*fixture, test);
  CheckCoordinateEvaluator<float>(*fixture, test);
  CheckCoordinateEvaluator<double>(*fixture, test);
  CheckCoordinateRankLimit(*fixture, test);
  CheckIndependentContexts(*fixture, test);
  CheckFailures(*fixture, test);
  return test.Finish();
}
