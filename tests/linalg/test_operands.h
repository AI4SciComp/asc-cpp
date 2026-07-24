#ifndef ASC_TESTS_LINALG_TEST_OPERANDS_H_
#define ASC_TESTS_LINALG_TEST_OPERANDS_H_

#include <asc/array/tensor_concepts.h>
#include <asc/core/memory_space.h>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace asc::test {

/// Structural operand used to verify that Linalg revalidates foreign views.
template <typename Element, typename Mapping>
class LinalgTestOperand {
 public:
  using ElementType = Element;
  using ValueType = std::remove_cv_t<Element>;
  using ExtentsType = typename Mapping::ExtentsType;
  using MappingType = Mapping;
  using DataHandle = Element*;

  static constexpr std::size_t Rank() noexcept { return Mapping::Rank(); }

  LinalgTestOperand(DataHandle data, Mapping mapping, extent_t available_span,
                    MemorySpace memory_space = MemorySpace::kHost)
      : data_(data),
        mapping_(std::move(mapping)),
        size_(mapping_.GetSize()),
        required_span_(mapping_.GetRequiredSpan()),
        available_span_(available_span),
        memory_space_(memory_space) {
    for (std::size_t dimension = 0; dimension < Rank(); ++dimension) {
      extents_[dimension] = mapping_.GetExtent(dimension);
      strides_[dimension] = mapping_.GetStride(dimension);
    }
  }

  const ExtentsType& GetExtents() const noexcept {
    return mapping_.GetExtents();
  }
  const Mapping& GetMapping() const noexcept { return mapping_; }
  extent_t GetExtent(std::size_t dimension) const {
    return extents_[dimension];
  }
  stride_t GetStride(std::size_t dimension) const {
    return strides_[dimension];
  }
  extent_t GetSize() const noexcept { return size_; }
  extent_t GetRequiredSpan() const noexcept { return required_span_; }
  extent_t GetAvailableSpan() const noexcept { return available_span_; }
  MemorySpace GetMemorySpace() const noexcept { return memory_space_; }
  bool IsContiguous() const noexcept { return mapping_.IsContiguous(); }
  DataHandle Data() const noexcept { return data_; }

  void SetExtent(std::size_t dimension, extent_t extent) {
    extents_[dimension] = extent;
  }
  void SetStride(std::size_t dimension, stride_t stride) {
    strides_[dimension] = stride;
  }
  void SetSize(extent_t size) noexcept { size_ = size; }
  void SetRequiredSpan(extent_t span) noexcept { required_span_ = span; }
  void SetAvailableSpan(extent_t span) noexcept { available_span_ = span; }

 private:
  DataHandle data_ = nullptr;
  Mapping mapping_;
  std::array<extent_t, Rank()> extents_{};
  std::array<stride_t, Rank()> strides_{};
  extent_t size_ = 0;
  extent_t required_span_ = 0;
  extent_t available_span_ = 0;
  MemorySpace memory_space_ = MemorySpace::kHost;
};

}  // namespace asc::test

#endif  // ASC_TESTS_LINALG_TEST_OPERANDS_H_
