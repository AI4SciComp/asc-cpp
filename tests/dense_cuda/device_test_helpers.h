#ifndef ASC_TESTS_DENSE_CUDA_DEVICE_TEST_HELPERS_H_
#define ASC_TESTS_DENSE_CUDA_DEVICE_TEST_HELPERS_H_

#include <array>
#include <cstddef>
#include <cstdlib>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"

namespace asc_dense_cuda_test {

template <typename Element, std::size_t Rank>
asc::DenseView<Element, Rank> MakeView(
    Element* data, const std::array<asc::extent_t, Rank>& extents,
    const std::array<asc::stride_t, Rank>& strides, asc::MemorySpace space) {
  auto mapping = asc::DenseLayout<Rank>::Create(
      std::span<const asc::extent_t, Rank>(extents),
      asc::LayoutStride<Rank>{.strides = strides});
  if (!mapping.ok()) {
    std::abort();
  }
  auto view = asc::DenseView<Element, Rank>::Create(data, *mapping, space);
  if (!view.ok()) {
    std::abort();
  }
  return *view;
}

template <typename Element, std::size_t Rank>
asc::DenseView<Element, Rank> MakeLeftView(
    Element* data, const std::array<asc::extent_t, Rank>& extents,
    asc::MemorySpace space) {
  auto mapping = asc::DenseLayout<Rank>::Create(
      std::span<const asc::extent_t, Rank>(extents), asc::LayoutLeft{});
  if (!mapping.ok()) {
    std::abort();
  }
  auto view = asc::DenseView<Element, Rank>::Create(data, *mapping, space);
  if (!view.ok()) {
    std::abort();
  }
  return *view;
}

template <typename Element, std::size_t Rank>
asc::DenseView<Element, Rank> MakeRightView(
    Element* data, const std::array<asc::extent_t, Rank>& extents,
    asc::MemorySpace space) {
  auto mapping = asc::DenseLayout<Rank>::Create(
      std::span<const asc::extent_t, Rank>(extents), asc::LayoutRight{});
  if (!mapping.ok()) {
    std::abort();
  }
  auto view = asc::DenseView<Element, Rank>::Create(data, *mapping, space);
  if (!view.ok()) {
    std::abort();
  }
  return *view;
}

inline bool CopyAndWait(const asc::ExecutionContext& context,
                        asc::MutableMemoryView destination,
                        asc::ConstMemoryView source) {
  auto event = asc::CopyBytes(context, destination, source);
  return event.ok() && event->Wait().ok();
}

inline bool CopyAndWait(const asc::ExecutionContext& context,
                        asc::Buffer& destination, const asc::Buffer& source) {
  auto destination_view = destination.mutable_view();
  auto source_view = source.const_view();
  return destination_view.ok() && source_view.ok() &&
         CopyAndWait(context, *destination_view, *source_view);
}

template <typename T>
T* Data(asc::Buffer& buffer) {
  return static_cast<T*>(buffer.data());
}

template <typename T>
const T* Data(const asc::Buffer& buffer) {
  return static_cast<const T*>(buffer.data());
}

}  // namespace asc_dense_cuda_test

#endif  // ASC_TESTS_DENSE_CUDA_DEVICE_TEST_HELPERS_H_
