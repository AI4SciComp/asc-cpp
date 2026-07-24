// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_ARRAY_TENSOR_VIEW_H_
#define ASC_ARRAY_TENSOR_VIEW_H_

#include <concepts>
#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "asc/array/accessor.h"
#include "asc/array/layout.h"
#include "asc/core/contracts.h"
#include "asc/core/memory_space.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {

/// @brief Lightweight non-owning view of host-accessible tensor elements.
///
/// Element constness is part of `Element`; constness of the view handle does
/// not alter mutation authority. The complete backing allocation must outlive
/// every use of the view.
template <typename Element, typename ExtentsT,
          typename Mapping = LayoutLeftMapping<ExtentsT>,
          typename Accessor = DefaultAccessor<Element>>
class TensorView {
  static_assert(!std::is_reference_v<Element>,
                "A tensor view element cannot be a reference");
  static_assert(!std::is_void_v<Element>,
                "A tensor view element cannot be void");
  static_assert(std::same_as<typename Mapping::ExtentsType, ExtentsT>,
                "A tensor view mapping must use its declared extents type");
  static_assert(std::same_as<typename Accessor::ElementType, Element>,
                "A tensor view accessor must use its declared element type");

 public:
  using ElementType = Element;
  using ValueType = std::remove_cv_t<Element>;
  using ExtentsType = ExtentsT;
  using MappingType = Mapping;
  using AccessorType = Accessor;
  using DataHandle = typename Accessor::DataHandle;
  using Reference = typename Accessor::Reference;

  static constexpr std::size_t Rank() noexcept { return ExtentsType::Rank(); }

  /// @brief Validate external storage and create a non-owning view.
  static Result<TensorView> Create(
      DataHandle data, const Mapping& mapping, extent_t available_span,
      MemorySpace space = MemorySpace::kHost,
      const Accessor& accessor = Accessor()) {
    if (available_span < 0) {
      return Status(StatusCode::kInvalidArgument,
                    "A tensor view span must be non-negative");
    }
    if (available_span < mapping.GetRequiredSpan()) {
      return Status(StatusCode::kOutOfRange,
                    "A tensor view span is smaller than its mapping requires");
    }
    if (mapping.GetRequiredSpan() != 0 && data == nullptr) {
      return Status(StatusCode::kInvalidArgument,
                    "A nonempty tensor view requires a data handle");
    }
    if (!IsHostAccessible(space)) {
      return Status(StatusCode::kUnsupported,
                    "Array M1 views require host-accessible storage");
    }
    if constexpr (!std::is_const_v<Element>) {
      if (!mapping.IsUnique()) {
        return Status(
            StatusCode::kFailedPrecondition,
            "A mutable tensor view requires a proven-unique mapping");
      }
    }
    return TensorView(data, mapping, accessor, available_span, space);
  }

  /// @brief Convert mutable-element authority to const-element authority.
  template <typename OtherElement, typename OtherAccessor>
    requires(std::is_const_v<Element> &&
             std::same_as<std::remove_const_t<Element>, OtherElement> &&
             std::constructible_from<Accessor, const OtherAccessor&>)
  constexpr TensorView(
      const TensorView<OtherElement, ExtentsType, Mapping,
                       OtherAccessor>& other) noexcept
      : data_(other.Data()),
        mapping_(other.GetMapping()),
        accessor_(other.GetAccessor()),
        available_span_(other.GetAvailableSpan()),
        memory_space_(other.GetMemorySpace()) {}

  DataHandle Data() const noexcept { return data_; }
  const Mapping& GetMapping() const noexcept { return mapping_; }
  const Accessor& GetAccessor() const noexcept { return accessor_; }
  const ExtentsType& GetExtents() const noexcept {
    return mapping_.GetExtents();
  }
  extent_t GetExtent(std::size_t dimension) const {
    return mapping_.GetExtent(dimension);
  }
  stride_t GetStride(std::size_t dimension) const {
    return mapping_.GetStride(dimension);
  }
  extent_t GetSize() const noexcept { return mapping_.GetSize(); }
  extent_t GetRequiredSpan() const noexcept {
    return mapping_.GetRequiredSpan();
  }
  extent_t GetAvailableSpan() const noexcept { return available_span_; }
  MemorySpace GetMemorySpace() const noexcept { return memory_space_; }
  bool IsContiguous() const noexcept { return mapping_.IsContiguous(); }

  /// @brief Return a checked pointer for untrusted coordinates.
  Result<DataHandle> TryAt(
      const std::array<index_t, Rank()>& coordinates) const {
    auto offset = mapping_.TryOffset(coordinates);
    if (!offset.ok()) {
      return offset.status();
    }
    if (offset.value() < 0 || offset.value() >= available_span_) {
      return Status(StatusCode::kOutOfRange,
                    "A tensor coordinate exceeds the available backing span");
    }
    return accessor_.Offset(data_, offset.value());
  }

  template <detail::ArrayIndexValue... Indices>
    requires(sizeof...(Indices) == Rank())
  Result<DataHandle> TryAt(Indices... indices) const {
    auto offset = mapping_.TryOffset(indices...);
    if (!offset.ok()) {
      return offset.status();
    }
    if (offset.value() < 0 || offset.value() >= available_span_) {
      return Status(StatusCode::kOutOfRange,
                    "A tensor coordinate exceeds the available backing span");
    }
    return accessor_.Offset(data_, offset.value());
  }

  /// @brief Contract-checked element access.
  template <detail::ArrayIndexValue... Indices>
    requires(sizeof...(Indices) == Rank())
  Reference operator()(Indices... indices) const {
    auto pointer = TryAt(indices...);
    ASC_REQUIRE(pointer.ok(), pointer.status().message());
    return *pointer.value();
  }

  /// @brief Element access after the caller has established valid bounds.
  template <detail::ArrayIndexValue... Indices>
    requires(sizeof...(Indices) == Rank())
  Reference UncheckedAt(Indices... indices) const noexcept {
    return accessor_.Access(data_, mapping_.UncheckedOffset(indices...));
  }

 private:
  constexpr TensorView(DataHandle data, const Mapping& mapping,
                       const Accessor& accessor, extent_t available_span,
                       MemorySpace memory_space) noexcept
      : data_(data),
        mapping_(mapping),
        accessor_(accessor),
        available_span_(available_span),
        memory_space_(memory_space) {}

  DataHandle data_ = nullptr;
  Mapping mapping_;
  [[no_unique_address]] Accessor accessor_;
  extent_t available_span_ = 0;
  MemorySpace memory_space_ = MemorySpace::kHost;
};

}  // namespace asc

#endif  // ASC_ARRAY_TENSOR_VIEW_H_
