#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

struct CudaFixture {
  asc::ExecutionContext execution;
  asc::SparseCudaContext provider;
  std::unique_ptr<asc::CudaMemoryResource> resource;
};

asc::Result<CudaFixture> MakeFixture() {
  auto execution =
      asc::CreateCudaExecutionContext(asc_random_cuda_test::CudaDevice());
  if (!execution.ok()) {
    return execution.status();
  }
  auto provider = asc::SparseCudaContext::Create(*execution);
  if (!provider.ok()) {
    return provider.status();
  }
  auto resource = asc::CudaMemoryResource::Create(
      asc_random_cuda_test::CudaDevice(), asc::MemorySpace::kDevice);
  if (!resource.ok()) {
    return resource.status();
  }
  return CudaFixture{*execution, std::move(*provider), std::move(*resource)};
}

template <typename Element>
asc::Result<asc::CsrArray<Element>> CloneFixtureMatrix(
    CudaFixture& fixture, std::span<const asc::extent_t, 2> shape,
    std::span<const asc::nnz_t> offsets, std::span<const asc::index_t> indices,
    std::span<const Element> values) {
  auto source = asc::CsrView<const Element>::Create(
      shape, offsets, indices, values, asc::MemorySpace::kHost);
  if (!source.ok()) {
    return source.status();
  }
  auto clone = asc::CudaCloneCsr(fixture.provider, *source, *fixture.resource);
  if (!clone.ok()) {
    return clone.status();
  }
  const asc::Status wait = clone->completion.Wait();
  if (!wait.ok()) {
    return wait;
  }
  return std::move(clone->array);
}

template <typename Element>
void CheckClone(CudaFixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape = {4, 5};
  constexpr std::array<asc::nnz_t, 5> kOffsets = {0, 2, 3, 6, 8};
  constexpr std::array<asc::index_t, 8> kIndices = {0, 3, 1, 0, 2, 4, 1, 4};
  constexpr std::array<Element, 8> kValues = {
      Element{2}, Element{-1}, Element{4},  Element{7},
      Element{3}, Element{5},  Element{-2}, Element{6}};
  auto matrix =
      CloneFixtureMatrix<Element>(fixture, kShape, kOffsets, kIndices, kValues);
  ASC_M7_CUDA_CHECK(test, matrix.ok());
  if (!matrix.ok()) {
    return;
  }
  ASC_M7_CUDA_EQ(test, matrix->shape(), kShape);
  ASC_M7_CUDA_EQ(test, matrix->nnz(), asc::nnz_t{8});
  ASC_M7_CUDA_EQ(test, matrix->resource(), fixture.resource.get());
  auto view = matrix->view();
  ASC_M7_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  auto offsets = asc_random_cuda_test::Download<asc::nnz_t>(
      view->outer_offset_data(), kOffsets.size(), asc::MemorySpace::kDevice,
      fixture.execution);
  auto indices = asc_random_cuda_test::Download<asc::index_t>(
      view->inner_index_data(), kIndices.size(), asc::MemorySpace::kDevice,
      fixture.execution);
  auto values = asc_random_cuda_test::Download<Element>(
      view->value_data(), kValues.size(), asc::MemorySpace::kDevice,
      fixture.execution);
  ASC_M7_CUDA_CHECK(test, offsets.ok());
  ASC_M7_CUDA_CHECK(test, indices.ok());
  ASC_M7_CUDA_CHECK(test, values.ok());
  if (offsets.ok()) {
    ASC_M7_CUDA_CHECK(test, *offsets == std::vector<asc::nnz_t>(
                                            kOffsets.begin(), kOffsets.end()));
  }
  if (indices.ok()) {
    ASC_M7_CUDA_CHECK(test, *indices == std::vector<asc::index_t>(
                                            kIndices.begin(), kIndices.end()));
  }
  if (values.ok()) {
    ASC_M7_CUDA_CHECK(
        test, *values == std::vector<Element>(kValues.begin(), kValues.end()));
  }

  constexpr std::array<asc::extent_t, 2> kEmptyShape = {0, 5};
  constexpr std::array<asc::nnz_t, 1> kEmptyOffsets = {0};
  const std::span<const asc::index_t> no_indices;
  const std::span<const Element> no_values;
  auto empty = CloneFixtureMatrix<Element>(fixture, kEmptyShape, kEmptyOffsets,
                                           no_indices, no_values);
  ASC_M7_CUDA_CHECK(test, empty.ok());
  if (empty.ok()) {
    auto empty_view = empty->view();
    ASC_M7_CUDA_CHECK(test, empty_view.ok());
    if (empty_view.ok()) {
      auto downloaded = asc_random_cuda_test::Download<asc::nnz_t>(
          empty_view->outer_offset_data(), 1, asc::MemorySpace::kDevice,
          fixture.execution);
      ASC_M7_CUDA_CHECK(test, downloaded.ok());
      if (downloaded.ok()) {
        ASC_M7_CUDA_EQ(test, downloaded->front(), asc::nnz_t{0});
      }
    }
  }
}

template <typename Element>
void CheckSpmv(CudaFixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape = {4, 5};
  constexpr std::array<asc::nnz_t, 5> kOffsets = {0, 2, 3, 6, 8};
  constexpr std::array<asc::index_t, 8> kIndices = {0, 3, 1, 0, 2, 4, 1, 4};
  constexpr std::array<Element, 8> kValues = {
      Element{2}, Element{-1}, Element{4},  Element{7},
      Element{3}, Element{5},  Element{-2}, Element{6}};
  auto matrix =
      CloneFixtureMatrix<Element>(fixture, kShape, kOffsets, kIndices, kValues);
  ASC_M7_CUDA_CHECK(test, matrix.ok());
  if (!matrix.ok()) {
    return;
  }
  auto matrix_view = matrix->view();
  ASC_M7_CUDA_CHECK(test, matrix_view.ok());
  if (!matrix_view.ok()) {
    return;
  }
  const asc::CsrView<const Element> const_matrix(*matrix_view);

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
  auto workspace = asc::Buffer::Allocate(*fixture.resource, 1U << 20U, 256);
  ASC_M7_CUDA_CHECK(test, input_device.ok());
  ASC_M7_CUDA_CHECK(test, output_device.ok());
  ASC_M7_CUDA_CHECK(test, workspace.ok());
  if (!input_device.ok() || !output_device.ok() || !workspace.ok()) {
    return;
  }
  auto input_view = asc::CudaStridedVectorView<const Element>::Create(
      static_cast<const Element*>(input_device->data()), 5, kInputStride,
      asc::MemorySpace::kDevice);
  auto output_view = asc::CudaStridedVectorView<Element>::Create(
      static_cast<Element*>(output_device->data()), 4, kOutputStride,
      asc::MemorySpace::kDevice);
  auto workspace_view = workspace->mutable_view();
  ASC_M7_CUDA_CHECK(test, input_view.ok());
  ASC_M7_CUDA_CHECK(test, output_view.ok());
  ASC_M7_CUDA_CHECK(test, workspace_view.ok());
  if (!input_view.ok() || !output_view.ok() || !workspace_view.ok()) {
    return;
  }

  auto required_workspace =
      asc::CudaCsrSpmvWorkspaceSize(fixture.provider, Element{2}, const_matrix,
                                    *input_view, Element{-0.5}, *output_view);
  ASC_M7_CUDA_CHECK(test, required_workspace.ok());
  if (!required_workspace.ok()) {
    return;
  }
  const asc::MutableMemoryView active_workspace =
      *required_workspace == 0
          ? asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice)
          : *workspace_view;
  auto event =
      asc::CudaCsrSpmv(fixture.provider, Element{2}, const_matrix, *input_view,
                       Element{-0.5}, *output_view, active_workspace);
  ASC_M7_CUDA_CHECK(test, event.ok());
  if (!event.ok()) {
    return;
  }
  ASC_M7_CUDA_CHECK(test, *required_workspace <= workspace->size());
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
    for (const std::size_t padding :
         {std::size_t{1}, std::size_t{2}, std::size_t{4}, std::size_t{5},
          std::size_t{7}, std::size_t{8}}) {
      ASC_M7_CUDA_EQ(test, (*actual)[padding], kPadding);
    }
  }

  if (required_workspace.ok() && *required_workspace > 0) {
    auto undersized =
        asc::Buffer::Allocate(*fixture.resource, *required_workspace - 1U, 256);
    ASC_M7_CUDA_CHECK(test, undersized.ok());
    if (undersized.ok()) {
      auto undersized_view = undersized->mutable_view();
      ASC_M7_CUDA_CHECK(test, undersized_view.ok());
      if (undersized_view.ok()) {
        const auto rejected = asc::CudaCsrSpmv(
            fixture.provider, Element{2}, const_matrix, *input_view,
            Element{-0.5}, *output_view, *undersized_view);
        ASC_M7_CUDA_CHECK(test, !rejected.ok());
      }
    }

    std::vector<std::byte> host_workspace(*required_workspace);
    const auto wrong_space = asc::CudaCsrSpmv(
        fixture.provider, Element{2}, const_matrix, *input_view, Element{-0.5},
        *output_view,
        asc::MutableMemoryView(host_workspace.data(), host_workspace.size(),
                               asc::MemorySpace::kHost));
    ASC_M7_CUDA_CHECK(test, !wrong_space.ok());

    const std::size_t output_bytes = initial_output.size() * sizeof(Element);
    auto overlapping = asc::Buffer::Allocate(
        *fixture.resource, std::max(*required_workspace, output_bytes), 256);
    ASC_M7_CUDA_CHECK(test, overlapping.ok());
    if (overlapping.ok()) {
      auto overlapping_memory = overlapping->mutable_view();
      ASC_M7_CUDA_CHECK(test, overlapping_memory.ok());
      if (overlapping_memory.ok()) {
        const asc::Status initialized = asc_random_cuda_test::CopyAndWait(
            fixture.execution,
            asc::MutableMemoryView(overlapping->data(), output_bytes,
                                   asc::MemorySpace::kDevice),
            asc::ConstMemoryView(initial_output.data(), output_bytes,
                                 asc::MemorySpace::kHost));
        ASC_M7_CUDA_CHECK(test, initialized.ok());
        auto overlapping_output = asc::CudaStridedVectorView<Element>::Create(
            static_cast<Element*>(overlapping->data()), 4, kOutputStride,
            asc::MemorySpace::kDevice);
        ASC_M7_CUDA_CHECK(test, overlapping_output.ok());
        if (overlapping_output.ok()) {
          const auto rejected = asc::CudaCsrSpmv(
              fixture.provider, Element{2}, const_matrix, *input_view,
              Element{-0.5}, *overlapping_output, *overlapping_memory);
          ASC_M7_CUDA_CHECK(test, !rejected.ok());
          auto unchanged = asc_random_cuda_test::Download<Element>(
              overlapping->data(), initial_output.size(),
              asc::MemorySpace::kDevice, fixture.execution);
          ASC_M7_CUDA_CHECK(test, unchanged.ok());
          if (unchanged.ok()) {
            ASC_M7_CUDA_CHECK(
                test, *unchanged == std::vector<Element>(initial_output.begin(),
                                                         initial_output.end()));
          }
        }
      }
    }
  }

  if (actual.ok()) {
    auto after_failures = asc_random_cuda_test::Download<Element>(
        *output_device, fixture.execution);
    ASC_M7_CUDA_CHECK(test, after_failures.ok());
    if (after_failures.ok()) {
      ASC_M7_CUDA_CHECK(test, *after_failures == *actual);
    }
  }

  std::array<Element, 10> nan_output;
  nan_output.fill(std::numeric_limits<Element>::quiet_NaN());
  auto beta_zero_device = asc_random_cuda_test::Upload<Element>(
      nan_output, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, beta_zero_device.ok());
  if (!beta_zero_device.ok()) {
    return;
  }
  auto beta_zero_view = asc::CudaStridedVectorView<Element>::Create(
      static_cast<Element*>(beta_zero_device->data()), 4, kOutputStride,
      asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, beta_zero_view.ok());
  if (!beta_zero_view.ok()) {
    return;
  }
  auto beta_zero =
      asc::CudaCsrSpmv(fixture.provider, Element{1}, const_matrix, *input_view,
                       Element{0}, *beta_zero_view, active_workspace);
  ASC_M7_CUDA_CHECK(test, beta_zero.ok());
  if (beta_zero.ok()) {
    ASC_M7_CUDA_CHECK(test, beta_zero->Wait().ok());
    auto result = asc_random_cuda_test::Download<Element>(*beta_zero_device,
                                                          fixture.execution);
    ASC_M7_CUDA_CHECK(test, result.ok());
    if (result.ok()) {
      const std::array<Element, 4> expected = {Element{-2}, Element{-8},
                                               Element{11}, Element{-2}};
      for (std::size_t row = 0; row < expected.size(); ++row) {
        ASC_M7_CUDA_EQ(test,
                       (*result)[row * static_cast<std::size_t>(kOutputStride)],
                       expected[row]);
      }
    }
  }

  constexpr std::array<asc::extent_t, 2> kEmptyShape = {0, 5};
  constexpr std::array<asc::nnz_t, 1> kEmptyOffsets = {0};
  const std::span<const asc::index_t> no_indices;
  const std::span<const Element> no_values;
  auto empty = CloneFixtureMatrix<Element>(fixture, kEmptyShape, kEmptyOffsets,
                                           no_indices, no_values);
  ASC_M7_CUDA_CHECK(test, empty.ok());
  if (empty.ok()) {
    auto empty_matrix = empty->view();
    auto empty_output = asc::CudaStridedVectorView<Element>::Create(
        nullptr, 0, 1, asc::MemorySpace::kDevice);
    ASC_M7_CUDA_CHECK(test, empty_matrix.ok());
    ASC_M7_CUDA_CHECK(test, empty_output.ok());
    if (empty_matrix.ok() && empty_output.ok()) {
      const asc::CsrView<const Element> const_empty(*empty_matrix);
      auto empty_workspace = asc::CudaCsrSpmvWorkspaceSize(
          fixture.provider, Element{1}, const_empty, *input_view, Element{0},
          *empty_output);
      ASC_M7_CUDA_CHECK(test, empty_workspace.ok());
      if (empty_workspace.ok()) {
        ASC_M7_CUDA_EQ(test, *empty_workspace, std::size_t{0});
        auto empty_event = asc::CudaCsrSpmv(
            fixture.provider, Element{1}, const_empty, *input_view, Element{0},
            *empty_output,
            asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
        ASC_M7_CUDA_CHECK(test, empty_event.ok());
        if (empty_event.ok()) {
          ASC_M7_CUDA_CHECK(test, empty_event->Wait().ok());
        }
      }
    }
  }
}

template <typename Element>
void CheckUnitStrideSpmv(CudaFixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape = {2, 2};
  constexpr std::array<asc::nnz_t, 3> kOffsets = {0, 1, 2};
  constexpr std::array<asc::index_t, 2> kIndices = {0, 1};
  constexpr std::array<Element, 2> kValues = {Element{2}, Element{3}};
  constexpr std::array<Element, 2> kInput = {Element{4}, Element{5}};
  constexpr std::array<Element, 2> kInitialOutput = {Element{1}, Element{1}};

  auto matrix =
      CloneFixtureMatrix<Element>(fixture, kShape, kOffsets, kIndices, kValues);
  auto input = asc_random_cuda_test::Upload<Element>(kInput, *fixture.resource,
                                                     fixture.execution);
  auto output = asc_random_cuda_test::Upload<Element>(
      kInitialOutput, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, matrix.ok());
  ASC_M7_CUDA_CHECK(test, input.ok());
  ASC_M7_CUDA_CHECK(test, output.ok());
  if (!matrix.ok() || !input.ok() || !output.ok()) {
    return;
  }
  auto matrix_view = matrix->view();
  auto input_view = asc::CudaStridedVectorView<const Element>::Create(
      static_cast<const Element*>(input->data()), 2, 1,
      asc::MemorySpace::kDevice);
  auto output_view = asc::CudaStridedVectorView<Element>::Create(
      static_cast<Element*>(output->data()), 2, 1, asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, matrix_view.ok());
  ASC_M7_CUDA_CHECK(test, input_view.ok());
  ASC_M7_CUDA_CHECK(test, output_view.ok());
  if (!matrix_view.ok() || !input_view.ok() || !output_view.ok()) {
    return;
  }
  const asc::CsrView<const Element> const_matrix(*matrix_view);
  auto required =
      asc::CudaCsrSpmvWorkspaceSize(fixture.provider, Element{2}, const_matrix,
                                    *input_view, Element{-1}, *output_view);
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
  auto workspace_view = workspace->mutable_view();
  ASC_M7_CUDA_CHECK(test, workspace_view.ok());
  if (!workspace_view.ok()) {
    return;
  }
  const asc::MutableMemoryView active_workspace =
      *required == 0
          ? asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice)
          : asc::MutableMemoryView(workspace->data(), *required,
                                   asc::MemorySpace::kDevice);
  auto event =
      asc::CudaCsrSpmv(fixture.provider, Element{2}, const_matrix, *input_view,
                       Element{-1}, *output_view, active_workspace);
  ASC_M7_CUDA_CHECK(test, event.ok());
  if (event.ok()) {
    ASC_M7_CUDA_CHECK(test, event->Wait().ok());
    auto actual =
        asc_random_cuda_test::Download<Element>(*output, fixture.execution);
    ASC_M7_CUDA_CHECK(test, actual.ok());
    if (actual.ok()) {
      const std::array<Element, 2> expected = {Element{15}, Element{29}};
      ASC_M7_CUDA_CHECK(test, *actual == std::vector<Element>(expected.begin(),
                                                              expected.end()));
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
    const auto wrong_space = asc::CudaCsrSpmv(
        fixture.provider, Element{2}, const_matrix, *input_view, Element{-1},
        *output_view,
        asc::MutableMemoryView(host_workspace.data(), host_workspace.size(),
                               asc::MemorySpace::kHost));
    ASC_M7_CUDA_CHECK(test, !wrong_space.ok());

    const std::size_t output_bytes = kInitialOutput.size() * sizeof(Element);
    auto overlapping = asc::Buffer::Allocate(
        *fixture.resource, std::max(*required, output_bytes), 256);
    ASC_M7_CUDA_CHECK(test, overlapping.ok());
    if (overlapping.ok()) {
      auto overlapping_output = asc::CudaStridedVectorView<Element>::Create(
          static_cast<Element*>(overlapping->data()), 2, 1,
          asc::MemorySpace::kDevice);
      ASC_M7_CUDA_CHECK(test, overlapping_output.ok());
      if (overlapping_output.ok()) {
        const auto overlap = asc::CudaCsrSpmv(
            fixture.provider, Element{2}, const_matrix, *input_view,
            Element{-1}, *overlapping_output,
            asc::MutableMemoryView(overlapping->data(), *required,
                                   asc::MemorySpace::kDevice));
        ASC_M7_CUDA_CHECK(test, !overlap.ok());
      }
    }
  }
}

template <typename Element>
void CheckEvaluator(CudaFixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape = {2, 3};
  constexpr std::array<asc::nnz_t, 3> kOffsets = {0, 2, 3};
  constexpr std::array<asc::index_t, 3> kIndices = {0, 2, 1};
  constexpr std::array<Element, 3> kSourceValues = {Element{2}, Element{-3},
                                                    Element{4}};
  auto source = CloneFixtureMatrix<Element>(fixture, kShape, kOffsets, kIndices,
                                            kSourceValues);
  ASC_M7_CUDA_CHECK(test, source.ok());
  if (!source.ok()) {
    return;
  }
  auto source_view = source->view();
  ASC_M7_CUDA_CHECK(test, source_view.ok());
  if (!source_view.ok()) {
    return;
  }
  const asc::CsrView<const Element> const_source(*source_view);
  const auto overlapping_rebind =
      source_view->RebindValues(std::span<const Element>(
          reinterpret_cast<const Element*>(source_view->inner_index_data()),
          kSourceValues.size()));
  ASC_M7_CUDA_CHECK(test, !overlapping_rebind.ok());

  constexpr std::array<Element, 3> kOtherValues = {Element{5}, Element{6},
                                                   Element{-7}};
  constexpr std::array<Element, 3> kOutputSentinel = {Element{41}, Element{43},
                                                      Element{47}};
  auto other_device = asc_random_cuda_test::Upload<Element>(
      kOtherValues, *fixture.resource, fixture.execution);
  auto destination_device = asc_random_cuda_test::Upload<Element>(
      kOutputSentinel, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, other_device.ok());
  ASC_M7_CUDA_CHECK(test, destination_device.ok());
  if (other_device.ok() && destination_device.ok()) {
    auto other = source_view->RebindValues(std::span<const Element>(
        static_cast<const Element*>(other_device->data()),
        kOtherValues.size()));
    auto destination = source_view->RebindValues(
        std::span<Element>(static_cast<Element*>(destination_device->data()),
                           kOutputSentinel.size()));
    ASC_M7_CUDA_CHECK(test, other.ok());
    ASC_M7_CUDA_CHECK(test, destination.ok());
    if (other.ok() && destination.ok()) {
      auto add_distinct = asc::MakeAdd(const_source, *other);
      ASC_M7_CUDA_CHECK(test, add_distinct.ok());
      if (add_distinct.ok()) {
        auto distinct_event =
            asc::CudaEvaluate(fixture.provider, *destination, *add_distinct);
        ASC_M7_CUDA_CHECK(test, distinct_event.ok());
        if (distinct_event.ok()) {
          ASC_M7_CUDA_CHECK(test, distinct_event->Wait().ok());
          auto values = asc_random_cuda_test::Download<Element>(
              *destination_device, fixture.execution);
          ASC_M7_CUDA_CHECK(test, values.ok());
          if (values.ok()) {
            const std::array<Element, 3> expected = {Element{7}, Element{3},
                                                     Element{-3}};
            ASC_M7_CUDA_CHECK(test,
                              *values == std::vector<Element>(expected.begin(),
                                                              expected.end()));
          }
        }
      }
    }
  }

  auto negate = asc::MakeNegate(const_source);
  ASC_M7_CUDA_CHECK(test, negate.ok());
  if (!negate.ok()) {
    return;
  }
  auto event = asc::CudaEvaluate(fixture.provider, *source_view, *negate);
  ASC_M7_CUDA_CHECK(test, event.ok());
  if (event.ok()) {
    ASC_M7_CUDA_CHECK(test, event->Wait().ok());
    auto values = asc_random_cuda_test::Download<Element>(
        source_view->value_data(), kSourceValues.size(),
        asc::MemorySpace::kDevice, fixture.execution);
    ASC_M7_CUDA_CHECK(test, values.ok());
    if (values.ok()) {
      const std::array<Element, 3> expected = {Element{-2}, Element{3},
                                               Element{-4}};
      ASC_M7_CUDA_CHECK(test, *values == std::vector<Element>(expected.begin(),
                                                              expected.end()));
    }
  }

  auto add = asc::MakeAdd(const_source, const_source);
  ASC_M7_CUDA_CHECK(test, add.ok());
  if (add.ok()) {
    auto added = asc::CudaEvaluate(fixture.provider, *source_view, *add);
    ASC_M7_CUDA_CHECK(test, added.ok());
    if (added.ok()) {
      ASC_M7_CUDA_CHECK(test, added->Wait().ok());
      auto values = asc_random_cuda_test::Download<Element>(
          source_view->value_data(), kSourceValues.size(),
          asc::MemorySpace::kDevice, fixture.execution);
      ASC_M7_CUDA_CHECK(test, values.ok());
      if (values.ok()) {
        const std::array<Element, 3> expected = {Element{-4}, Element{6},
                                                 Element{-8}};
        ASC_M7_CUDA_CHECK(
            test,
            *values == std::vector<Element>(expected.begin(), expected.end()));
      }
    }
  }

  auto scaled = asc::MakeMultiply(const_source, Element{-2});
  ASC_M7_CUDA_CHECK(test, scaled.ok());
  if (scaled.ok()) {
    auto scaled_event =
        asc::CudaEvaluate(fixture.provider, *source_view, *scaled);
    ASC_M7_CUDA_CHECK(test, scaled_event.ok());
    if (scaled_event.ok()) {
      ASC_M7_CUDA_CHECK(test, scaled_event->Wait().ok());
      auto values = asc_random_cuda_test::Download<Element>(
          source_view->value_data(), kSourceValues.size(),
          asc::MemorySpace::kDevice, fixture.execution);
      ASC_M7_CUDA_CHECK(test, values.ok());
      if (values.ok()) {
        const std::array<Element, 3> expected = {Element{8}, Element{-12},
                                                 Element{16}};
        ASC_M7_CUDA_CHECK(
            test,
            *values == std::vector<Element>(expected.begin(), expected.end()));
      }
    }
  }

  auto add_zero = asc::MakeAdd(const_source, Element{0});
  auto zero_add = asc::MakeAdd(Element{0}, const_source);
  auto subtract_zero = asc::MakeSubtract(const_source, Element{0});
  ASC_M7_CUDA_CHECK(test, add_zero.ok());
  ASC_M7_CUDA_CHECK(test, zero_add.ok());
  ASC_M7_CUDA_CHECK(test, subtract_zero.ok());
  if (add_zero.ok() && zero_add.ok() && subtract_zero.ok()) {
    auto add_zero_event =
        asc::CudaEvaluate(fixture.provider, *source_view, *add_zero);
    ASC_M7_CUDA_CHECK(test, add_zero_event.ok());
    if (add_zero_event.ok()) {
      ASC_M7_CUDA_CHECK(test, add_zero_event->Wait().ok());
    }
    auto zero_add_event =
        asc::CudaEvaluate(fixture.provider, *source_view, *zero_add);
    ASC_M7_CUDA_CHECK(test, zero_add_event.ok());
    if (zero_add_event.ok()) {
      ASC_M7_CUDA_CHECK(test, zero_add_event->Wait().ok());
    }
    auto subtract_zero_event =
        asc::CudaEvaluate(fixture.provider, *source_view, *subtract_zero);
    ASC_M7_CUDA_CHECK(test, subtract_zero_event.ok());
    if (subtract_zero_event.ok()) {
      ASC_M7_CUDA_CHECK(test, subtract_zero_event->Wait().ok());
    }
  }

  auto left_scaled = asc::MakeMultiply(Element{3}, const_source);
  ASC_M7_CUDA_CHECK(test, left_scaled.ok());
  if (left_scaled.ok()) {
    auto left_scaled_event =
        asc::CudaEvaluate(fixture.provider, *source_view, *left_scaled);
    ASC_M7_CUDA_CHECK(test, left_scaled_event.ok());
    if (left_scaled_event.ok()) {
      ASC_M7_CUDA_CHECK(test, left_scaled_event->Wait().ok());
    }
  }

  auto add_nonzero = asc::MakeAdd(const_source, Element{1});
  auto nonzero_add = asc::MakeAdd(Element{1}, const_source);
  auto subtract_nonzero = asc::MakeSubtract(const_source, Element{1});
  auto zero_subtract = asc::MakeSubtract(Element{0}, const_source);
  auto scalar_subtract_nonzero = asc::MakeSubtract(Element{1}, const_source);
  ASC_M7_CUDA_CHECK(test, add_nonzero.ok());
  ASC_M7_CUDA_CHECK(test, nonzero_add.ok());
  ASC_M7_CUDA_CHECK(test, subtract_nonzero.ok());
  ASC_M7_CUDA_CHECK(test, zero_subtract.ok());
  ASC_M7_CUDA_CHECK(test, scalar_subtract_nonzero.ok());
  if (zero_subtract.ok()) {
    auto zero_subtract_event =
        asc::CudaEvaluate(fixture.provider, *source_view, *zero_subtract);
    ASC_M7_CUDA_CHECK(test, zero_subtract_event.ok());
    if (zero_subtract_event.ok()) {
      ASC_M7_CUDA_CHECK(test, zero_subtract_event->Wait().ok());
    }
  }
  if (add_nonzero.ok()) {
    const auto rejected =
        asc::CudaEvaluate(fixture.provider, *source_view, *add_nonzero);
    ASC_M7_CUDA_CHECK(test, !rejected.ok());
  }
  if (nonzero_add.ok()) {
    const auto rejected =
        asc::CudaEvaluate(fixture.provider, *source_view, *nonzero_add);
    ASC_M7_CUDA_CHECK(test, !rejected.ok());
  }
  if (subtract_nonzero.ok()) {
    const auto rejected =
        asc::CudaEvaluate(fixture.provider, *source_view, *subtract_nonzero);
    ASC_M7_CUDA_CHECK(test, !rejected.ok());
  }
  if (scalar_subtract_nonzero.ok()) {
    const auto rejected = asc::CudaEvaluate(fixture.provider, *source_view,
                                            *scalar_subtract_nonzero);
    ASC_M7_CUDA_CHECK(test, !rejected.ok());
  }

  auto final_values = asc_random_cuda_test::Download<Element>(
      source_view->value_data(), kSourceValues.size(),
      asc::MemorySpace::kDevice, fixture.execution);
  ASC_M7_CUDA_CHECK(test, final_values.ok());
  if (final_values.ok()) {
    const std::array<Element, 3> expected = {Element{-24}, Element{36},
                                             Element{-48}};
    ASC_M7_CUDA_CHECK(
        test, *final_values ==
                  std::vector<Element>(expected.begin(), expected.end()));
  }
}

void CheckFailures(CudaFixture& fixture, TestContext& test) {
  const auto serial_context =
      asc::SparseCudaContext::Create(asc::ExecutionContext::Serial());
  ASC_M7_CUDA_CHECK(test, !serial_context.ok());

  const auto negative_stride = asc::CudaStridedVectorView<float>::Create(
      nullptr, 0, -1, asc::MemorySpace::kDevice);
  const auto zero_stride = asc::CudaStridedVectorView<float>::Create(
      nullptr, 0, 0, asc::MemorySpace::kDevice);
  std::array<float, 1> host = {7.0F};
  const auto host_vector = asc::CudaStridedVectorView<float>::Create(
      host.data(), 1, 1, asc::MemorySpace::kHost);
  ASC_M7_CUDA_CHECK(test, !negative_stride.ok());
  ASC_M7_CUDA_CHECK(test, !zero_stride.ok());
  ASC_M7_CUDA_CHECK(test, !host_vector.ok());

  constexpr std::array<asc::extent_t, 2> kShape = {2, 2};
  constexpr std::array<asc::nnz_t, 3> kOffsets = {0, 1, 2};
  constexpr std::array<asc::index_t, 2> kIndices = {0, 1};
  constexpr std::array<float, 2> kValues = {2.0F, 3.0F};
  auto matrix =
      CloneFixtureMatrix<float>(fixture, kShape, kOffsets, kIndices, kValues);
  ASC_M7_CUDA_CHECK(test, matrix.ok());
  if (!matrix.ok()) {
    return;
  }
  auto matrix_view = matrix->view();
  ASC_M7_CUDA_CHECK(test, matrix_view.ok());
  const std::array<float, 2> input = {1.0F, 2.0F};
  const std::array<float, 2> output = {41.0F, 43.0F};
  auto input_device = asc_random_cuda_test::Upload<float>(
      input, *fixture.resource, fixture.execution);
  auto output_device = asc_random_cuda_test::Upload<float>(
      output, *fixture.resource, fixture.execution);
  auto workspace = asc::Buffer::Allocate(*fixture.resource, 1U << 20U, 256);
  if (!matrix_view.ok() || !input_device.ok() || !output_device.ok() ||
      !workspace.ok()) {
    ASC_M7_CUDA_CHECK(test, false);
    return;
  }
  auto input_view = asc::CudaStridedVectorView<const float>::Create(
      static_cast<const float*>(input_device->data()), 2, 1,
      asc::MemorySpace::kDevice);
  auto short_input = asc::CudaStridedVectorView<const float>::Create(
      static_cast<const float*>(input_device->data()), 1, 1,
      asc::MemorySpace::kDevice);
  auto output_view = asc::CudaStridedVectorView<float>::Create(
      static_cast<float*>(output_device->data()), 2, 1,
      asc::MemorySpace::kDevice);
  auto workspace_view = workspace->mutable_view();
  if (!input_view.ok() || !short_input.ok() || !output_view.ok() ||
      !workspace_view.ok()) {
    ASC_M7_CUDA_CHECK(test, false);
    return;
  }
  const asc::CsrView<const float> const_matrix(*matrix_view);
  auto untrusted_matrix = asc::CsrView<const float>::Create(
      kShape,
      std::span<const asc::nnz_t>(matrix_view->outer_offset_data(),
                                  kOffsets.size()),
      std::span<const asc::index_t>(matrix_view->inner_index_data(),
                                    kIndices.size()),
      std::span<const float>(matrix_view->value_data(), kValues.size()),
      asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, untrusted_matrix.ok());
  if (untrusted_matrix.ok()) {
    const auto untrusted =
        asc::CudaCsrSpmvWorkspaceSize(fixture.provider, 1.0F, *untrusted_matrix,
                                      *input_view, 0.0F, *output_view);
    ASC_M7_CUDA_CHECK(test, !untrusted.ok());
  }
  const auto wrong_shape =
      asc::CudaCsrSpmv(fixture.provider, 1.0F, const_matrix, *short_input, 0.0F,
                       *output_view, *workspace_view);
  ASC_M7_CUDA_CHECK(test, !wrong_shape.ok());

  auto overlapping_output = asc::CudaStridedVectorView<float>::Create(
      static_cast<float*>(input_device->data()), 2, 1,
      asc::MemorySpace::kDevice);
  ASC_M7_CUDA_CHECK(test, overlapping_output.ok());
  if (overlapping_output.ok()) {
    const auto overlap =
        asc::CudaCsrSpmv(fixture.provider, 1.0F, const_matrix, *input_view,
                         0.0F, *overlapping_output, *workspace_view);
    ASC_M7_CUDA_CHECK(test, !overlap.ok());
  }

  auto unchanged =
      asc_random_cuda_test::Download<float>(*output_device, fixture.execution);
  ASC_M7_CUDA_CHECK(test, unchanged.ok());
  if (unchanged.ok()) {
    ASC_M7_CUDA_CHECK(
        test, *unchanged == std::vector<float>(output.begin(), output.end()));
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
  CheckSpmv<float>(*fixture, test);
  CheckSpmv<double>(*fixture, test);
  CheckUnitStrideSpmv<float>(*fixture, test);
  CheckUnitStrideSpmv<double>(*fixture, test);
  CheckEvaluator<float>(*fixture, test);
  CheckEvaluator<double>(*fixture, test);
  CheckFailures(*fixture, test);
  return test.Finish();
}
