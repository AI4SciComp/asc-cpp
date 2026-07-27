#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/layout.h"
#include "asc/dense/providers/cuda.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"
#include "test_support.h"

namespace {

using Shape2 = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

asc::Device CudaDevice() { return asc::Device{asc::Backend::kCuda, 0}; }

bool Wait(asc::Result<asc::CompletionEvent>& event,
          asc_dense_cuda_test::TestContext& test) {
  ASC_DENSE_CUDA_CHECK(test, event.ok());
  if (!event.ok()) {
    return false;
  }
  const asc::Status status = event->Wait();
  ASC_DENSE_CUDA_CHECK(test, status.ok());
  if (!status.ok()) {
    return false;
  }
  const auto query = event->Query();
  ASC_DENSE_CUDA_CHECK(test, query.ok());
  if (query.ok()) {
    ASC_DENSE_CUDA_CHECK(test, *query);
  }
  return status.ok();
}

template <typename Scalar>
void SetHost2D(asc::DenseArray<Scalar, Shape2>& array,
               std::span<const Scalar> values,
               asc_dense_cuda_test::TestContext& test) {
  auto view = array.view();
  ASC_DENSE_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  const std::size_t rows = static_cast<std::size_t>(view->shape()[0]);
  const std::size_t columns = static_cast<std::size_t>(view->shape()[1]);
  ASC_DENSE_CUDA_EQ(test, values.size(), rows * columns);
  for (std::size_t column = 0; column < columns; ++column) {
    for (std::size_t row = 0; row < rows; ++row) {
      const std::array<asc::index_t, 2> coordinate = {
          static_cast<asc::index_t>(row), static_cast<asc::index_t>(column)};
      auto element = view->At(coordinate);
      ASC_DENSE_CUDA_CHECK(test, element.ok());
      if (element.ok()) {
        **element = values[column * rows + row];
      }
    }
  }
}

template <typename Scalar>
std::vector<Scalar> Download2D(
    const asc::DenseArray<Scalar, Shape2>& device_array,
    const asc::ExecutionContext& execution,
    asc_dense_cuda_test::TestContext& test) {
  asc::HostMemoryResource host;
  auto clone = device_array.Clone(host, execution);
  ASC_DENSE_CUDA_CHECK(test, clone.ok());
  if (!clone.ok()) {
    return {};
  }
  auto view = clone->view();
  ASC_DENSE_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return {};
  }
  const std::size_t rows = static_cast<std::size_t>(view->shape()[0]);
  const std::size_t columns = static_cast<std::size_t>(view->shape()[1]);
  std::vector<Scalar> values(rows * columns);
  for (std::size_t column = 0; column < columns; ++column) {
    for (std::size_t row = 0; row < rows; ++row) {
      const std::array<asc::index_t, 2> coordinate = {
          static_cast<asc::index_t>(row), static_cast<asc::index_t>(column)};
      auto element = view->At(coordinate);
      ASC_DENSE_CUDA_CHECK(test, element.ok());
      if (element.ok()) {
        values[column * rows + row] = **element;
      }
    }
  }
  return values;
}

template <typename Scalar, typename Layout>
asc::Result<asc::DenseArray<Scalar, Shape2>> Upload2D(
    const Shape2& shape, std::span<const Scalar> values, Layout layout,
    asc::MemoryResource& device_resource,
    const asc::ExecutionContext& execution,
    asc_dense_cuda_test::TestContext& test) {
  asc::HostMemoryResource host;
  auto source = asc::DenseArray<Scalar, Shape2>::Create(shape, host, layout);
  if (!source.ok()) {
    return source.status();
  }
  SetHost2D(*source, values, test);
  return source->Clone(device_resource, execution);
}

template <typename Scalar>
void CheckValues(std::span<const Scalar> actual,
                 std::span<const Scalar> expected,
                 asc_dense_cuda_test::TestContext& test) {
  ASC_DENSE_CUDA_EQ(test, actual.size(), expected.size());
  if (actual.size() != expected.size()) {
    return;
  }
  for (std::size_t index = 0; index < actual.size(); ++index) {
    ASC_DENSE_CUDA_EQ(test, actual[index], expected[index]);
  }
}

template <typename Scalar>
void CheckStorageAndClone(asc_dense_cuda_test::TestContext& test) {
  auto execution = asc::CreateCudaExecutionContext(CudaDevice());
  auto device_resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  auto pinned_resource = asc::CudaMemoryResource::Create(
      CudaDevice(), asc::MemorySpace::kPinnedHost);
  auto managed_resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kManaged);
  ASC_DENSE_CUDA_CHECK(test, execution.ok());
  ASC_DENSE_CUDA_CHECK(test, device_resource.ok());
  ASC_DENSE_CUDA_CHECK(test, pinned_resource.ok());
  ASC_DENSE_CUDA_CHECK(test, managed_resource.ok());
  if (!execution.ok() || !device_resource.ok() || !pinned_resource.ok() ||
      !managed_resource.ok()) {
    return;
  }

  const auto shape = Shape2::Create(2, 3);
  ASC_DENSE_CUDA_CHECK(test, shape.ok());
  if (!shape.ok()) {
    return;
  }
  const std::array<Scalar, 6> expected = {Scalar{1}, Scalar{2}, Scalar{-3},
                                          Scalar{4}, Scalar{5}, Scalar{-6}};
  auto device = Upload2D<Scalar>(*shape, expected, asc::LayoutRight{},
                                 **device_resource, *execution, test);
  ASC_DENSE_CUDA_CHECK(test, device.ok());
  if (!device.ok()) {
    return;
  }
  ASC_DENSE_CUDA_EQ(test, device->resource(), device_resource->get());
  ASC_DENSE_CUDA_EQ(test, device->mapping().kind(),
                    asc::DenseLayoutKind::kRight);
  auto device_view = device->view();
  ASC_DENSE_CUDA_CHECK(test, device_view.ok());
  if (device_view.ok()) {
    const std::array<asc::index_t, 2> coordinate = {0, 0};
    const auto inaccessible = device_view->At(coordinate);
    ASC_DENSE_CUDA_CHECK(test, !inaccessible.ok());
    if (!inaccessible.ok()) {
      ASC_DENSE_CUDA_EQ(test, inaccessible.status().code(),
                        asc::ErrorCode::kMemoryAccess);
    }
  }
  const auto round_trip = Download2D(*device, *execution, test);
  CheckValues<Scalar>(round_trip, expected, test);

  for (asc::MemoryResource* resource :
       {pinned_resource->get(), managed_resource->get()}) {
    auto owner = asc::DenseArray<Scalar, Shape2>::CreateUninitialized(
        *shape, *resource, asc::LayoutLeft{});
    ASC_DENSE_CUDA_CHECK(test, owner.ok());
    if (owner.ok()) {
      ASC_DENSE_CUDA_EQ(test, owner->size(), asc::extent_t{6});
      ASC_DENSE_CUDA_EQ(test, owner->resource(), resource);
      if (resource == managed_resource->get()) {
        auto view = owner->view();
        ASC_DENSE_CUDA_CHECK(test, view.ok());
        if (view.ok()) {
          const std::array<asc::index_t, 2> coordinate = {0, 0};
          const auto inaccessible = view->At(coordinate);
          ASC_DENSE_CUDA_CHECK(test, !inaccessible.ok());
          if (!inaccessible.ok()) {
            ASC_DENSE_CUDA_EQ(test, inaccessible.status().code(),
                              asc::ErrorCode::kMemoryAccess);
          }
        }
      }
    }
  }

  const auto zero_shape = Shape2::Create(0, 7);
  ASC_DENSE_CUDA_CHECK(test, zero_shape.ok());
  auto zero = asc::DenseArray<Scalar, Shape2>::CreateUninitialized(
      *zero_shape, **device_resource, asc::LayoutLeft{});
  ASC_DENSE_CUDA_CHECK(test, zero.ok());
  if (zero.ok()) {
    auto view = zero->view();
    ASC_DENSE_CUDA_CHECK(test, view.ok());
    if (view.ok()) {
      ASC_DENSE_CUDA_EQ(test, view->data(), nullptr);
      ASC_DENSE_CUDA_EQ(test, view->mapping().required_span_size(),
                        asc::extent_t{0});
    }
  }

  auto wrong_context =
      device->Clone(**device_resource, asc::ExecutionContext::Serial());
  ASC_DENSE_CUDA_CHECK(test, !wrong_context.ok());

  auto moved(std::move(*device));
  ASC_DENSE_CUDA_CHECK(test, !device->view().ok());
  ASC_DENSE_CUDA_CHECK(test, moved.view().ok());
}

template <typename Scalar>
void CheckEvaluation(asc_dense_cuda_test::TestContext& test) {
  auto execution = asc::CreateCudaExecutionContext(CudaDevice());
  auto provider = execution.ok()
                      ? asc::DenseCudaContext::Create(*execution)
                      : asc::Result<asc::DenseCudaContext>(execution.status());
  auto device_resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  ASC_DENSE_CUDA_CHECK(test, execution.ok());
  ASC_DENSE_CUDA_CHECK(test, provider.ok());
  ASC_DENSE_CUDA_CHECK(test, device_resource.ok());
  if (!execution.ok() || !provider.ok() || !device_resource.ok()) {
    return;
  }
  ASC_DENSE_CUDA_EQ(test, provider->execution_context().backend(),
                    asc::Backend::kCuda);
  ASC_DENSE_CUDA_EQ(test, provider->execution_context().device(), CudaDevice());

  const auto shape = Shape2::Create(2, 3);
  ASC_DENSE_CUDA_CHECK(test, shape.ok());
  if (!shape.ok()) {
    return;
  }
  const std::array<Scalar, 6> source_values = {
      Scalar{1}, Scalar{-2}, Scalar{3}, Scalar{-4}, Scalar{5}, Scalar{-6}};
  auto source = Upload2D<Scalar>(*shape, source_values, asc::LayoutLeft{},
                                 **device_resource, *execution, test);
  auto destination = asc::DenseArray<Scalar, Shape2>::CreateUninitialized(
      *shape, **device_resource, asc::LayoutRight{});
  ASC_DENSE_CUDA_CHECK(test, source.ok());
  ASC_DENSE_CUDA_CHECK(test, destination.ok());
  if (!source.ok() || !destination.ok()) {
    return;
  }
  auto source_view = source->view();
  auto destination_view = destination->view();
  ASC_DENSE_CUDA_CHECK(test, source_view.ok());
  ASC_DENSE_CUDA_CHECK(test, destination_view.ok());
  if (!source_view.ok() || !destination_view.ok()) {
    return;
  }

  const asc::DenseView<const Scalar, 2> const_source(*source_view);
  auto terminal = asc::CudaEvaluate(*provider, *destination_view, const_source);
  Wait(terminal, test);
  CheckValues<Scalar>(Download2D(*destination, *execution, test), source_values,
                      test);

  const asc::ScalarExpression<Scalar> scalar(Scalar{2});
  auto fill = asc::CudaEvaluate(*provider, *destination_view, scalar);
  Wait(fill, test);
  const std::array<Scalar, 6> twos = {Scalar{2}, Scalar{2}, Scalar{2},
                                      Scalar{2}, Scalar{2}, Scalar{2}};
  CheckValues<Scalar>(Download2D(*destination, *execution, test), twos, test);

  auto negate = asc::MakeNegate(const_source);
  ASC_DENSE_CUDA_CHECK(test, negate.ok());
  auto negate_event = asc::CudaEvaluate(*provider, *destination_view, *negate);
  Wait(negate_event, test);
  const std::array<Scalar, 6> negated = {Scalar{-1}, Scalar{2},  Scalar{-3},
                                         Scalar{4},  Scalar{-5}, Scalar{6}};
  CheckValues<Scalar>(Download2D(*destination, *execution, test), negated,
                      test);

  auto add = asc::MakeAdd(const_source, Scalar{3});
  ASC_DENSE_CUDA_CHECK(test, add.ok());
  auto add_event = asc::CudaEvaluate(*provider, *destination_view, *add);
  Wait(add_event, test);
  const std::array<Scalar, 6> added = {Scalar{4},  Scalar{1}, Scalar{6},
                                       Scalar{-1}, Scalar{8}, Scalar{-3}};
  CheckValues<Scalar>(Download2D(*destination, *execution, test), added, test);

  auto subtract = asc::MakeSubtract(Scalar{10}, const_source);
  ASC_DENSE_CUDA_CHECK(test, subtract.ok());
  auto subtract_event =
      asc::CudaEvaluate(*provider, *destination_view, *subtract);
  Wait(subtract_event, test);
  const std::array<Scalar, 6> subtracted = {Scalar{9},  Scalar{12}, Scalar{7},
                                            Scalar{14}, Scalar{5},  Scalar{16}};
  CheckValues<Scalar>(Download2D(*destination, *execution, test), subtracted,
                      test);

  auto multiply = asc::MakeMultiply(const_source, const_source);
  ASC_DENSE_CUDA_CHECK(test, multiply.ok());
  auto multiply_event =
      asc::CudaEvaluate(*provider, *destination_view, *multiply);
  Wait(multiply_event, test);
  const std::array<Scalar, 6> squared = {Scalar{1},  Scalar{4},  Scalar{9},
                                         Scalar{16}, Scalar{25}, Scalar{36}};
  CheckValues<Scalar>(Download2D(*destination, *execution, test), squared,
                      test);

  auto self = asc::CudaEvaluate(*provider, *source_view, *source_view);
  Wait(self, test);
  CheckValues<Scalar>(Download2D(*source, *execution, test), source_values,
                      test);

  auto nested_inner = asc::MakeNegate(const_source);
  auto nested = asc::MakeNegate(*nested_inner);
  ASC_DENSE_CUDA_CHECK(test, nested.ok());
  const auto nested_event =
      asc::CudaEvaluate(*provider, *destination_view, *nested);
  ASC_DENSE_CUDA_CHECK(test, !nested_event.ok());
  if (!nested_event.ok()) {
    ASC_DENSE_CUDA_EQ(test, nested_event.status().code(),
                      asc::ErrorCode::kUnsupported);
  }

  auto scalar_negate = asc::MakeNegate(Scalar{2});
  ASC_DENSE_CUDA_CHECK(test, scalar_negate.ok());
  const auto scalar_negate_event =
      asc::CudaEvaluate(*provider, *destination_view, *scalar_negate);
  ASC_DENSE_CUDA_CHECK(test, !scalar_negate_event.ok());
  if (!scalar_negate_event.ok()) {
    ASC_DENSE_CUDA_EQ(test, scalar_negate_event.status().code(),
                      asc::ErrorCode::kUnsupported);
  }

  asc::HostMemoryResource host;
  auto host_destination =
      asc::DenseArray<Scalar, Shape2>::Create(*shape, host, asc::LayoutLeft{});
  auto host_view = host_destination->view();
  const auto wrong_placement = asc::CudaEvaluate(*provider, *host_view, scalar);
  ASC_DENSE_CUDA_CHECK(test, !wrong_placement.ok());

  using ScalarShape = asc::Extents<>;
  auto scalar_shape = ScalarShape::Create();
  auto scalar_owner = asc::DenseArray<Scalar, ScalarShape>::CreateUninitialized(
      *scalar_shape, **device_resource, asc::LayoutLeft{});
  auto scalar_view = scalar_owner->view();
  const asc::ScalarExpression<Scalar> seven(Scalar{7});
  auto scalar_event = asc::CudaEvaluate(*provider, *scalar_view, seven);
  Wait(scalar_event, test);
  asc::HostMemoryResource scalar_host;
  auto scalar_result = scalar_owner->Clone(scalar_host, *execution);
  auto scalar_result_view = scalar_result->view();
  const std::array<asc::index_t, 0> scalar_coordinate{};
  const auto scalar_element = scalar_result_view->At(scalar_coordinate);
  ASC_DENSE_CUDA_CHECK(test, scalar_element.ok());
  if (scalar_element.ok()) {
    ASC_DENSE_CUDA_EQ(test, **scalar_element, Scalar{7});
  }

  const auto zero_shape = Shape2::Create(0, 3);
  auto zero_owner = asc::DenseArray<Scalar, Shape2>::CreateUninitialized(
      *zero_shape, **device_resource, asc::LayoutLeft{});
  auto zero_view = zero_owner->view();
  auto zero_event = asc::CudaEvaluate(*provider, *zero_view, scalar);
  Wait(zero_event, test);
}

template <typename Scalar>
void CheckPaddedAndOverlap(asc_dense_cuda_test::TestContext& test) {
  auto execution = asc::CreateCudaExecutionContext(CudaDevice());
  auto provider = asc::DenseCudaContext::Create(*execution);
  auto resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  ASC_DENSE_CUDA_CHECK(test, execution.ok());
  ASC_DENSE_CUDA_CHECK(test, provider.ok());
  ASC_DENSE_CUDA_CHECK(test, resource.ok());
  if (!execution.ok() || !provider.ok() || !resource.ok()) {
    return;
  }

  constexpr std::array<asc::extent_t, 2> kShape = {2, 2};
  constexpr std::array<asc::stride_t, 2> kPaddedStride = {1, 3};
  const auto mapping = asc::DenseLayoutMapping<2>::Create(
      asc::LayoutStride{}, kShape, kPaddedStride);
  ASC_DENSE_CUDA_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  const std::size_t element_count =
      static_cast<std::size_t>(mapping->required_span_size());
  std::vector<Scalar> host_source(element_count, Scalar{-91});
  std::vector<Scalar> host_result(element_count, Scalar{-77});
  host_source[0] = Scalar{1};
  host_source[1] = Scalar{2};
  host_source[3] = Scalar{3};
  host_source[4] = Scalar{4};
  auto source_buffer = asc::Buffer::Allocate(
      **resource, element_count * sizeof(Scalar), alignof(Scalar));
  auto destination_buffer = asc::Buffer::Allocate(
      **resource, element_count * sizeof(Scalar), alignof(Scalar));
  ASC_DENSE_CUDA_CHECK(test, source_buffer.ok());
  ASC_DENSE_CUDA_CHECK(test, destination_buffer.ok());
  if (!source_buffer.ok() || !destination_buffer.ok()) {
    return;
  }
  std::vector<Scalar> destination_canary(element_count, Scalar{-77});
  auto upload = asc::CopyBytes(
      *execution, *source_buffer->mutable_view(),
      asc::ConstMemoryView(host_source.data(), element_count * sizeof(Scalar),
                           asc::MemorySpace::kHost));
  Wait(upload, test);
  auto upload_destination =
      asc::CopyBytes(*execution, *destination_buffer->mutable_view(),
                     asc::ConstMemoryView(destination_canary.data(),
                                          element_count * sizeof(Scalar),
                                          asc::MemorySpace::kHost));
  Wait(upload_destination, test);
  auto source = asc::DenseView<const Scalar, 2>::Create(
      static_cast<const Scalar*>(source_buffer->data()), *mapping,
      asc::MemorySpace::kDevice);
  auto destination = asc::DenseView<Scalar, 2>::Create(
      static_cast<Scalar*>(destination_buffer->data()), *mapping,
      asc::MemorySpace::kDevice);
  auto negate = asc::MakeNegate(*source);
  auto event = asc::CudaEvaluate(*provider, *destination, *negate);
  Wait(event, test);
  auto download = asc::CopyBytes(
      *execution,
      asc::MutableMemoryView(host_result.data(), element_count * sizeof(Scalar),
                             asc::MemorySpace::kHost),
      *destination_buffer->const_view());
  Wait(download, test);
  ASC_DENSE_CUDA_EQ(test, host_result[0], Scalar{-1});
  ASC_DENSE_CUDA_EQ(test, host_result[1], Scalar{-2});
  ASC_DENSE_CUDA_EQ(test, host_result[2], Scalar{-77});
  ASC_DENSE_CUDA_EQ(test, host_result[3], Scalar{-3});
  ASC_DENSE_CUDA_EQ(test, host_result[4], Scalar{-4});

  constexpr std::array<asc::extent_t, 1> kOverlapShape = {4};
  const auto overlap_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kOverlapShape);
  auto overlap_buffer =
      asc::Buffer::Allocate(**resource, 5 * sizeof(Scalar), alignof(Scalar));
  const std::array<Scalar, 5> overlap_initial = {
      Scalar{11}, Scalar{12}, Scalar{13}, Scalar{14}, Scalar{15}};
  auto overlap_upload = asc::CopyBytes(
      *execution, *overlap_buffer->mutable_view(),
      asc::ConstMemoryView(overlap_initial.data(), sizeof(overlap_initial),
                           asc::MemorySpace::kHost));
  Wait(overlap_upload, test);
  auto overlap_source = asc::DenseView<const Scalar, 1>::Create(
      static_cast<const Scalar*>(overlap_buffer->data()), *overlap_mapping,
      asc::MemorySpace::kDevice);
  auto overlap_destination = asc::DenseView<Scalar, 1>::Create(
      static_cast<Scalar*>(overlap_buffer->data()) + 1, *overlap_mapping,
      asc::MemorySpace::kDevice);
  const auto overlap =
      asc::CudaEvaluate(*provider, *overlap_destination, *overlap_source);
  ASC_DENSE_CUDA_CHECK(test, !overlap.ok());
  std::array<Scalar, 5> overlap_result{};
  auto overlap_download = asc::CopyBytes(
      *execution,
      asc::MutableMemoryView(overlap_result.data(), sizeof(overlap_result),
                             asc::MemorySpace::kHost),
      *overlap_buffer->const_view());
  Wait(overlap_download, test);
  CheckValues<Scalar>(overlap_result, overlap_initial, test);

  auto reset_destination =
      asc::CopyBytes(*execution, *destination_buffer->mutable_view(),
                     asc::ConstMemoryView(destination_canary.data(),
                                          element_count * sizeof(Scalar),
                                          asc::MemorySpace::kHost));
  Wait(reset_destination, test);
  auto host_destination = asc::DenseView<Scalar, 2>::Create(
      static_cast<Scalar*>(destination_buffer->data()), *mapping,
      asc::MemorySpace::kHost);
  auto managed_destination = asc::DenseView<Scalar, 2>::Create(
      static_cast<Scalar*>(destination_buffer->data()), *mapping,
      asc::MemorySpace::kManaged);
  auto host_declared_source = asc::DenseView<const Scalar, 2>::Create(
      static_cast<const Scalar*>(source_buffer->data()), *mapping,
      asc::MemorySpace::kHost);
  auto managed_source = asc::DenseView<const Scalar, 2>::Create(
      static_cast<const Scalar*>(source_buffer->data()), *mapping,
      asc::MemorySpace::kManaged);
  const asc::ScalarExpression<Scalar> placement_value(Scalar{7});
  const auto host_destination_result =
      asc::CudaEvaluate(*provider, *host_destination, placement_value);
  const auto managed_destination_result =
      asc::CudaEvaluate(*provider, *managed_destination, placement_value);
  const auto host_source_result =
      asc::CudaEvaluate(*provider, *destination, *host_declared_source);
  const auto managed_source_result =
      asc::CudaEvaluate(*provider, *destination, *managed_source);
  ASC_DENSE_CUDA_CHECK(test, !host_destination_result.ok());
  ASC_DENSE_CUDA_CHECK(test, !managed_destination_result.ok());
  ASC_DENSE_CUDA_CHECK(test, !host_source_result.ok());
  ASC_DENSE_CUDA_CHECK(test, !managed_source_result.ok());
  std::vector<Scalar> placement_result(element_count);
  auto placement_download =
      asc::CopyBytes(*execution,
                     asc::MutableMemoryView(placement_result.data(),
                                            element_count * sizeof(Scalar),
                                            asc::MemorySpace::kHost),
                     *destination_buffer->const_view());
  Wait(placement_download, test);
  CheckValues<Scalar>(placement_result, destination_canary, test);
}

template <typename Scalar, std::size_t Rank>
void CheckSupportedRank(asc_dense_cuda_test::TestContext& test) {
  static_assert(Rank >= 1 && Rank <= 8);
  auto execution = asc::CreateCudaExecutionContext(CudaDevice());
  auto provider = asc::DenseCudaContext::Create(*execution);
  auto resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  if (!execution.ok() || !provider.ok() || !resource.ok()) {
    ASC_DENSE_CUDA_CHECK(test, false);
    return;
  }
  std::array<asc::extent_t, Rank> shape{};
  shape.fill(1);
  shape[0] = 2;
  const auto mapping =
      asc::DenseLayoutMapping<Rank>::Create(asc::LayoutRight{}, shape);
  auto buffer =
      asc::Buffer::Allocate(**resource, 2 * sizeof(Scalar), alignof(Scalar));
  auto view =
      asc::DenseView<Scalar, Rank>::Create(static_cast<Scalar*>(buffer->data()),
                                           *mapping, asc::MemorySpace::kDevice);
  const asc::ScalarExpression<Scalar> value(Scalar{13});
  auto event = asc::CudaEvaluate(*provider, *view, value);
  Wait(event, test);
  std::array<Scalar, 2> result{};
  auto copied =
      asc::CopyBytes(*execution,
                     asc::MutableMemoryView(result.data(), sizeof(result),
                                            asc::MemorySpace::kHost),
                     *buffer->const_view());
  Wait(copied, test);
  ASC_DENSE_CUDA_EQ(test, result[0], Scalar{13});
  ASC_DENSE_CUDA_EQ(test, result[1], Scalar{13});
}

template <typename Scalar>
void CheckAllSupportedRanks(asc_dense_cuda_test::TestContext& test) {
  CheckSupportedRank<Scalar, 1>(test);
  CheckSupportedRank<Scalar, 3>(test);
  CheckSupportedRank<Scalar, 4>(test);
  CheckSupportedRank<Scalar, 5>(test);
  CheckSupportedRank<Scalar, 6>(test);
  CheckSupportedRank<Scalar, 7>(test);
  CheckSupportedRank<Scalar, 8>(test);
}

template <typename Scalar>
void CheckUnsupportedRankNine(asc_dense_cuda_test::TestContext& test) {
  auto execution = asc::CreateCudaExecutionContext(CudaDevice());
  auto provider = asc::DenseCudaContext::Create(*execution);
  std::array<asc::extent_t, 9> shape{};
  shape.fill(0);
  const auto mapping =
      asc::DenseLayoutMapping<9>::Create(asc::LayoutLeft{}, shape);
  auto view = asc::DenseView<Scalar, 9>::Create(nullptr, *mapping,
                                                asc::MemorySpace::kDevice);
  const asc::ScalarExpression<Scalar> value(Scalar{1});
  const auto event = asc::CudaEvaluate(*provider, *view, value);
  ASC_DENSE_CUDA_CHECK(test, !event.ok());
  if (!event.ok()) {
    ASC_DENSE_CUDA_EQ(test, event.status().code(),
                      asc::ErrorCode::kUnsupported);
  }
}

template <typename Scalar>
void CheckExtremeLaunchMetadataWithoutAllocation(
    asc_dense_cuda_test::TestContext& test) {
  auto execution = asc::CreateCudaExecutionContext(CudaDevice());
  ASC_DENSE_CUDA_CHECK(test, execution.ok());
  if (!execution.ok()) {
    return;
  }
  auto provider = asc::DenseCudaContext::Create(*execution);
  ASC_DENSE_CUDA_CHECK(test, provider.ok());
  if (!provider.ok()) {
    return;
  }
  auto resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  ASC_DENSE_CUDA_CHECK(test, resource.ok());
  if (!resource.ok()) {
    return;
  }

  // This extent would have overflowed the former
  // `(logical_size + block_size - 1)` launch arithmetic. The public view
  // contract must reject its byte span before provider inspection, so the
  // extreme metadata check requires no allocation.
  const std::array<asc::extent_t, 1> extreme_shape = {
      std::numeric_limits<asc::extent_t>::max()};
  const auto mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, extreme_shape);
  ASC_DENSE_CUDA_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return;
  }
  auto* synthetic_pointer =
      reinterpret_cast<Scalar*>(static_cast<std::uintptr_t>(4096));
  auto destination = asc::DenseView<Scalar, 1>::Create(
      synthetic_pointer, *mapping, asc::MemorySpace::kDevice);
  ASC_DENSE_CUDA_CHECK(test, !destination.ok());
  if (!destination.ok()) {
    ASC_DENSE_CUDA_EQ(test, destination.status().code(),
                      asc::ErrorCode::kOverflow);
  }

  // Cross the former one-dimensional 65,535-block launch capacity without a
  // size-proportional allocation. Exact-self copy is a specified no-op, but
  // still exercises the complete public/provider metadata validation path.
  constexpr asc::extent_t kFormerLaunchCapacity =
      static_cast<asc::extent_t>(65535) * 256;
  const std::array<asc::extent_t, 1> above_former_capacity_shape = {
      kFormerLaunchCapacity + 1};
  const auto above_former_capacity_mapping = asc::DenseLayoutMapping<1>::Create(
      asc::LayoutLeft{}, above_former_capacity_shape);
  ASC_DENSE_CUDA_CHECK(test, above_former_capacity_mapping.ok());
  auto storage =
      asc::Buffer::Allocate(**resource, sizeof(Scalar), alignof(Scalar));
  ASC_DENSE_CUDA_CHECK(test, storage.ok());
  if (!above_former_capacity_mapping.ok() || !storage.ok()) {
    return;
  }
  auto above_former_capacity = asc::DenseView<Scalar, 1>::Create(
      static_cast<Scalar*>(storage->data()), *above_former_capacity_mapping,
      asc::MemorySpace::kDevice);
  ASC_DENSE_CUDA_CHECK(test, above_former_capacity.ok());
  if (!above_former_capacity.ok()) {
    return;
  }
  auto no_op =
      asc::CudaCopy(*provider, *above_former_capacity, *above_former_capacity);
  Wait(no_op, test);
}

}  // namespace

int main() {
  asc_dense_cuda_test::TestContext test;
  CheckStorageAndClone<float>(test);
  CheckStorageAndClone<double>(test);
  CheckEvaluation<float>(test);
  CheckEvaluation<double>(test);
  CheckPaddedAndOverlap<float>(test);
  CheckPaddedAndOverlap<double>(test);
  CheckAllSupportedRanks<float>(test);
  CheckAllSupportedRanks<double>(test);
  CheckUnsupportedRankNine<float>(test);
  CheckUnsupportedRankNine<double>(test);
  CheckExtremeLaunchMetadataWithoutAllocation<float>(test);
  CheckExtremeLaunchMetadataWithoutAllocation<double>(test);
  return test.Finish();
}
