#ifndef ASC_DENSE_ARRAY_H_
#define ASC_DENSE_ARRAY_H_

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"

namespace asc {

template <DenseElement Element, DenseExtents ExtentsType>
class DenseArray {
 public:
  using element_type = Element;
  using extents_type = ExtentsType;
  static constexpr std::size_t kRank = ExtentsType::kRank;

  static Result<DenseArray> Create(MemoryResource& resource,
                                   ExtentsType extents,
                                   LayoutLeft layout = {}) {
    auto mapping = DenseLayout<kRank>::Create(extents.values(), layout);
    if (!mapping.ok()) {
      return mapping.status();
    }
    return CreateWithMapping(resource, std::move(extents), *mapping);
  }

  static Result<DenseArray> Create(MemoryResource& resource,
                                   ExtentsType extents, LayoutRight layout) {
    auto mapping = DenseLayout<kRank>::Create(extents.values(), layout);
    if (!mapping.ok()) {
      return mapping.status();
    }
    return CreateWithMapping(resource, std::move(extents), *mapping);
  }

  static Result<DenseArray> CreateUninitialized(const ExtentsType& extents,
                                                MemoryResource& resource,
                                                LayoutLeft layout = {}) {
    auto mapping = DenseLayout<kRank>::Create(extents.values(), layout);
    if (!mapping.ok()) {
      return mapping.status();
    }
    return CreateUninitializedWithMapping(resource, extents, *mapping);
  }

  static Result<DenseArray> CreateUninitialized(const ExtentsType& extents,
                                                MemoryResource& resource,
                                                LayoutRight layout) {
    auto mapping = DenseLayout<kRank>::Create(extents.values(), layout);
    if (!mapping.ok()) {
      return mapping.status();
    }
    return CreateUninitializedWithMapping(resource, extents, *mapping);
  }

  DenseArray(const DenseArray&) = delete;
  DenseArray& operator=(const DenseArray&) = delete;
  DenseArray(DenseArray&&) noexcept = default;
  DenseArray& operator=(DenseArray&&) noexcept = default;
  ~DenseArray() = default;

  [[nodiscard]] bool valid() const noexcept { return buffer_.valid(); }

  [[nodiscard]] const ExtentsType& extents() const noexcept { return extents_; }

  [[nodiscard]] const DenseLayout<kRank>& mapping() const noexcept {
    return mapping_;
  }

  [[nodiscard]] extent_t logical_size() const noexcept {
    return mapping_.logical_size();
  }

  [[nodiscard]] Result<DenseView<Element, kRank>> view() {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from DenseArray has no view");
    }
    auto memory_space = buffer_.space();
    if (!memory_space.ok()) {
      return memory_space.status();
    }
    return DenseView<Element, kRank>(static_cast<Element*>(buffer_.data()),
                                     mapping_, *memory_space);
  }

  [[nodiscard]] Result<DenseView<const Element, kRank>> view() const {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from DenseArray has no view");
    }
    auto memory_space = buffer_.space();
    if (!memory_space.ok()) {
      return memory_space.status();
    }
    return DenseView<const Element, kRank>(
        static_cast<const Element*>(buffer_.data()), mapping_, *memory_space);
  }

  [[nodiscard]] Result<DenseArray> Clone(
      MemoryResource& destination_resource,
      const ExecutionContext& context) const {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from DenseArray cannot be cloned");
    }

    auto source_memory = buffer_.const_view();
    if (!source_memory.ok()) {
      return source_memory.status();
    }
    if (!context.CanAccess(source_memory->space()) ||
        !context.CanAccess(destination_resource.space())) {
      return Status(ErrorCode::kMemoryAccess,
                    "DenseArray clone storage is inaccessible to the context");
    }
    auto clone = CreateUninitializedWithMapping(destination_resource, extents_,
                                                mapping_);
    if (!clone.ok()) {
      return clone.status();
    }
    auto destination_memory = clone->buffer_.mutable_view();
    if (!destination_memory.ok()) {
      return destination_memory.status();
    }
    auto copied = CopyBytes(context, *destination_memory, *source_memory);
    if (!copied.ok()) {
      return copied.status();
    }
    Status wait_status = copied->Wait();
    if (!wait_status.ok()) {
      return wait_status;
    }
    return std::move(*clone);
  }

  Status DiscardResize(ExtentsType extents) {
    if (!valid() || resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from DenseArray cannot be resized");
    }
    if (mapping_.kind() == DenseLayoutKind::kRight) {
      return DiscardResize(std::move(extents), LayoutRight{});
    }
    return DiscardResize(std::move(extents), LayoutLeft{});
  }

  Status DiscardResize(ExtentsType extents, LayoutLeft layout) {
    if (!valid() || resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from DenseArray cannot be resized");
    }
    auto replacement = Create(*resource_, std::move(extents), layout);
    if (!replacement.ok()) {
      return replacement.status();
    }
    *this = std::move(*replacement);
    return Status::Ok();
  }

  Status DiscardResize(ExtentsType extents, LayoutRight layout) {
    if (!valid() || resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from DenseArray cannot be resized");
    }
    auto replacement = Create(*resource_, std::move(extents), layout);
    if (!replacement.ok()) {
      return replacement.status();
    }
    *this = std::move(*replacement);
    return Status::Ok();
  }

 private:
  static Result<DenseArray> CreateWithMapping(MemoryResource& resource,
                                              ExtentsType extents,
                                              DenseLayout<kRank> mapping) {
    if (resource.space() != MemorySpace::kHost) {
      return Status(ErrorCode::kUnsupported,
                    "Milestone 3 DenseArray supports only host resources");
    }
    auto array = CreateUninitializedWithMapping(resource, std::move(extents),
                                                std::move(mapping));
    if (!array.ok()) {
      return array.status();
    }
    if (array->mapping_.logical_size() != 0) {
      std::fill_n(static_cast<Element*>(array->buffer_.data()),
                  array->mapping_.required_span_size(), Element{});
    }
    return std::move(*array);
  }

  static Result<DenseArray> CreateUninitializedWithMapping(
      MemoryResource& resource, ExtentsType extents,
      DenseLayout<kRank> mapping) {
    switch (resource.space()) {
      case MemorySpace::kHost:
      case MemorySpace::kPinnedHost:
      case MemorySpace::kDevice:
      case MemorySpace::kManaged:
        break;
      default:
        return Status(ErrorCode::kInvalidArgument,
                      "DenseArray requires a recognized memory space");
    }
    if (!mapping.is_unique() || !mapping.is_exhaustive()) {
      return Status(
          ErrorCode::kInvalidArgument,
          "DenseArray ownership requires a unique exhaustive mapping");
    }
    if (mapping.logical_size() != extents.logical_size()) {
      return Status(ErrorCode::kShape,
                    "DenseArray extents and mapping logical sizes differ");
    }

    auto bytes = CheckedMultiply(mapping.required_span_size(), sizeof(Element));
    if (!bytes.ok()) {
      return bytes.status();
    }
    auto buffer = Buffer::Allocate(resource, *bytes, alignof(Element));
    if (!buffer.ok()) {
      return buffer.status();
    }
    return DenseArray(&resource, std::move(*buffer), std::move(extents),
                      mapping);
  }

  DenseArray(MemoryResource* resource, Buffer buffer, ExtentsType extents,
             DenseLayout<kRank> mapping) noexcept
      : resource_(resource),
        buffer_(std::move(buffer)),
        extents_(std::move(extents)),
        mapping_(mapping) {}

  MemoryResource* resource_;
  Buffer buffer_;
  ExtentsType extents_;
  DenseLayout<kRank> mapping_;
};

}  // namespace asc

#endif  // ASC_DENSE_ARRAY_H_
