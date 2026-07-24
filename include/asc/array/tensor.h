// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_ARRAY_TENSOR_H_
#define ASC_ARRAY_TENSOR_H_

#include <concepts>
#include <cstddef>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

#include "asc/array/layout.h"
#include "asc/array/tensor_view.h"
#include "asc/core/buffer.h"
#include "asc/core/contracts.h"
#include "asc/core/execution_context.h"
#include "asc/core/memory_resource.h"
#include "asc/core/memory_space.h"
#include "asc/core/status.h"

namespace asc {

/// @brief Move-only canonical owner of a contiguous host tensor allocation.
template <typename T, typename ExtentsT,
          typename Mapping = LayoutLeftMapping<ExtentsT>>
class Tensor {
  static_assert(!std::is_const_v<T>, "A tensor owner element cannot be const");
  static_assert(!std::is_reference_v<T>,
                "A tensor owner element cannot be a reference");
  static_assert(!std::is_void_v<T>, "A tensor owner element cannot be void");
  static_assert(std::same_as<typename Mapping::ExtentsType, ExtentsT>,
                "A tensor mapping must use its declared extents type");
  static_assert(Mapping::IsAlwaysUnique(),
                "Array M1 owners require an always-unique mapping");
  static_assert(Mapping::IsAlwaysExhaustive(),
                "Array M1 owners require an exhaustive mapping");
  static_assert(Mapping::IsAlwaysContiguous(),
                "Array M1 owners require a contiguous mapping");
  static_assert(std::is_nothrow_move_constructible_v<Mapping> &&
                    std::is_nothrow_move_assignable_v<Mapping>,
                "Canonical owner mappings must be no-throw movable");

 public:
  using ElementType = T;
  using ValueType = T;
  using ExtentsType = ExtentsT;
  using MappingType = Mapping;
  using MutableView = TensorView<T, ExtentsType, Mapping>;
  using ConstView = TensorView<const T, ExtentsType, Mapping>;

  static constexpr std::size_t Rank() noexcept { return ExtentsType::Rank(); }

  /// @brief Construct a valid owner with no descriptor and no allocation.
  Tensor() = default;

  /// @brief Canonical owners are not implicitly copied.
  Tensor(const Tensor&) = delete;
  Tensor& operator=(const Tensor&) = delete;

  /// @brief Transfer the allocation and descriptor, resetting the source.
  Tensor(Tensor&& other) noexcept
      : buffer_(std::move(other.buffer_)),
        mapping_(std::exchange(other.mapping_, std::nullopt)) {}

  /// @brief Replace ownership and reset the moved-from source.
  Tensor& operator=(Tensor&& other) noexcept {
    if (this != &other) {
      buffer_ = std::move(other.buffer_);
      mapping_ = std::exchange(other.mapping_, std::nullopt);
    }
    return *this;
  }

  ~Tensor() = default;

  /// @brief Allocate a canonical owner from extents and a resource.
  static Result<Tensor> Create(
      const ExtentsType& extents,
      MemoryResourcePtr resource = GetHostMemoryResource()) {
    auto mapping = Mapping::Create(extents);
    if (!mapping.ok()) {
      return mapping.status();
    }
    if (!resource) {
      return Status(StatusCode::kInvalidArgument,
                    "Tensor allocation requires a memory resource");
    }
    if (!IsHostAccessible(resource->GetMemorySpace())) {
      return Status(StatusCode::kUnsupported,
                    "Array M1 owners require host-accessible storage");
    }

    auto buffer = Buffer<T>::Allocate(mapping.value().GetRequiredSpan(),
                                      std::move(resource));
    if (!buffer.ok()) {
      return buffer.status();
    }
    return Tensor(std::move(buffer).value(), std::move(mapping).value());
  }

  /// @brief Allocate through a supported context resource.
  static Result<Tensor> Create(const ExtentsType& extents,
                               const ExecutionContext& context,
                               MemorySpace space) {
    Status context_status = ValidateContext(context);
    if (!context_status.ok()) {
      return context_status;
    }
    auto resource = context.GetMemoryResource(space);
    if (!resource.ok()) {
      return resource.status();
    }
    return Create(extents, resource.value());
  }

  /// @brief Whether this owner contains a descriptor, including a zero size.
  bool HasDescriptor() const noexcept { return mapping_.has_value(); }

  /// @brief Whether this owner has no logical elements.
  bool IsEmpty() const noexcept { return GetSize() == 0; }

  /// @brief Logical element count, or zero for the descriptor-less owner.
  extent_t GetSize() const noexcept {
    return mapping_ ? mapping_->GetSize() : extent_t{0};
  }

  /// @brief Backing span, or zero for the descriptor-less owner.
  extent_t GetRequiredSpan() const noexcept {
    return mapping_ ? mapping_->GetRequiredSpan() : extent_t{0};
  }

  /// @brief Mapping for a descriptor-bearing owner.
  const Mapping& GetMapping() const {
    ASC_REQUIRE(mapping_.has_value(), "A default Tensor has no mapping");
    return *mapping_;
  }

  /// @brief Extents for a descriptor-bearing owner.
  const ExtentsType& GetExtents() const { return GetMapping().GetExtents(); }

  extent_t GetExtent(std::size_t dimension) const {
    return GetMapping().GetExtent(dimension);
  }

  stride_t GetStride(std::size_t dimension) const {
    return GetMapping().GetStride(dimension);
  }

  MemorySpace GetMemorySpace() const noexcept {
    return buffer_.GetMemorySpace();
  }

  const MemoryResourcePtr& GetMemoryResource() const noexcept {
    return buffer_.GetMemoryResource();
  }

  /// @brief Create mutable-element authority over this owner's allocation.
  Result<MutableView> View() {
    if (!mapping_) {
      return Status(StatusCode::kFailedPrecondition,
                    "A default Tensor has no descriptor to view");
    }
    auto data = buffer_.HostData();
    if (!data.ok()) {
      return data.status();
    }
    return MutableView::Create(data.value(), *mapping_, buffer_.GetSize(),
                               buffer_.GetMemorySpace());
  }

  /// @brief Create const-element authority over this owner's allocation.
  Result<ConstView> View() const {
    if (!mapping_) {
      return Status(StatusCode::kFailedPrecondition,
                    "A default Tensor has no descriptor to view");
    }
    auto data = buffer_.HostData();
    if (!data.ok()) {
      return data.status();
    }
    return ConstView::Create(data.value(), *mapping_, buffer_.GetSize(),
                             buffer_.GetMemorySpace());
  }

  /// @brief Deep-copy through an explicit serial context and resource.
  Result<Tensor> Clone(const ExecutionContext& context,
                       MemoryResourcePtr destination_resource) const {
    Status context_status = ValidateContext(context);
    if (!context_status.ok()) {
      return context_status;
    }
    if (!mapping_) {
      return Tensor();
    }
    if constexpr (!std::is_copy_assignable_v<T>) {
      return Status(StatusCode::kUnsupported,
                    "Tensor Clone requires copy-assignable elements");
    } else {
      auto destination = Create(GetExtents(), std::move(destination_resource));
      if (!destination.ok()) {
        return destination.status();
      }
      auto source_view = View();
      if (!source_view.ok()) {
        return source_view.status();
      }
      auto destination_view = destination.value().View();
      if (!destination_view.ok()) {
        return destination_view.status();
      }

      T* destination_data = destination_view.value().Data();
      const T* source_data = source_view.value().Data();
      try {
        for (extent_t index = 0; index < GetRequiredSpan(); ++index) {
          destination_data[index] = source_data[index];
        }
      } catch (const std::exception& error) {
        return Status(StatusCode::kInternal, error.what());
      } catch (...) {
        return Status(StatusCode::kInternal,
                      "Tensor element copy failed during Clone");
      }
      return std::move(destination).value();
    }
  }

  /// @brief Deep-copy through a context-selected destination space.
  Result<Tensor> Clone(const ExecutionContext& context,
                       MemorySpace destination_space) const {
    Status context_status = ValidateContext(context);
    if (!context_status.ok()) {
      return context_status;
    }
    auto resource = context.GetMemoryResource(destination_space);
    if (!resource.ok()) {
      return resource.status();
    }
    return Clone(context, resource.value());
  }

 private:
  Tensor(Buffer<T>&& buffer, Mapping&& mapping) noexcept
      : buffer_(std::move(buffer)), mapping_(std::move(mapping)) {}

  static Status ValidateContext(const ExecutionContext& context) {
    if (context.GetBackend() != BackendKind::kSerial ||
        !context.Supports(ExecutionCapability::kSynchronous)) {
      return Status(StatusCode::kUnsupported,
                    "Array M1 requires a synchronous serial context");
    }
    return Status::Ok();
  }

  Buffer<T> buffer_;
  std::optional<Mapping> mapping_;
};

}  // namespace asc

#endif  // ASC_ARRAY_TENSOR_H_
