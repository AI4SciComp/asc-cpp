#ifndef ASC_DENSE_ARRAY_H_
#define ASC_DENSE_ARRAY_H_

#include <concepts>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
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

namespace internal_dense_array {

template <typename T>
struct IsCoreExtents : std::false_type {};

template <extent_t... StaticExtents>
struct IsCoreExtents<Extents<StaticExtents...>> : std::true_type {};

}  // namespace internal_dense_array

template <typename T>
concept DenseExtents = internal_dense_array::IsCoreExtents<T>::value;

// A move-only dense owner. Create value-initializes host storage;
// CreateUninitialized explicitly permits any valid resource space without
// touching element storage. The MemoryResource must outlive the array and
// every view obtained from it.
template <DenseElement Element, DenseExtents ExtentsType>
class DenseArray {
  static_assert(!std::is_const_v<Element>,
                "DenseArray owns mutable, non-const elements");

 public:
  using element_type = Element;
  using extents_type = ExtentsType;
  static constexpr std::size_t kRank = ExtentsType::kRank;
  using Mapping = DenseLayoutMapping<kRank>;
  using ViewType = DenseView<Element, kRank>;
  using ConstViewType = DenseView<const Element, kRank>;

  static Result<DenseArray> Create(const ExtentsType& extents,
                                   MemoryResource& resource,
                                   LayoutLeft layout = {}) {
    return CreateWithLayout(extents, resource, layout);
  }

  static Result<DenseArray> Create(const ExtentsType& extents,
                                   MemoryResource& resource,
                                   LayoutRight layout) {
    return CreateWithLayout(extents, resource, layout);
  }

  static Result<DenseArray> CreateUninitialized(const ExtentsType& extents,
                                                MemoryResource& resource,
                                                LayoutLeft layout = {}) {
    return CreateUninitializedWithLayout(extents, resource, layout);
  }

  static Result<DenseArray> CreateUninitialized(const ExtentsType& extents,
                                                MemoryResource& resource,
                                                LayoutRight layout) {
    return CreateUninitializedWithLayout(extents, resource, layout);
  }

  DenseArray(const DenseArray&) = delete;
  DenseArray& operator=(const DenseArray&) = delete;

  DenseArray(DenseArray&& other) noexcept
      : extents_(std::move(other.extents_)),
        mapping_(other.mapping_),
        buffer_(std::move(other.buffer_)),
        resource_(std::exchange(other.resource_, nullptr)) {}

  DenseArray& operator=(DenseArray&& other) noexcept {
    if (this != &other) {
      extents_ = std::move(other.extents_);
      mapping_ = other.mapping_;
      buffer_ = std::move(other.buffer_);
      resource_ = std::exchange(other.resource_, nullptr);
    }
    return *this;
  }

  ~DenseArray() = default;

  [[nodiscard]] const ExtentsType& extents() const noexcept { return extents_; }
  [[nodiscard]] const Mapping& mapping() const noexcept { return mapping_; }
  [[nodiscard]] extent_t size() const noexcept {
    return mapping_.logical_size();
  }
  [[nodiscard]] MemoryResource* resource() const noexcept { return resource_; }

  [[nodiscard]] Result<ViewType> view() {
    if (resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from DenseArray cannot produce a view");
    }
    return ViewType::Create(static_cast<Element*>(buffer_.data()), mapping_,
                            resource_->space());
  }

  [[nodiscard]] Result<ConstViewType> view() const {
    if (resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from DenseArray cannot produce a view");
    }
    return ConstViewType::Create(static_cast<const Element*>(buffer_.data()),
                                 mapping_, resource_->space());
  }

  [[nodiscard]] Result<DenseArray> Clone(
      MemoryResource& destination_resource,
      const ExecutionContext& context) const {
    if (resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from DenseArray cannot be cloned");
    }
    if (!context.CanAccess(resource_->space()) ||
        !context.CanAccess(destination_resource.space())) {
      return Status(
          ErrorCode::kMemoryAccess,
          "The clone context cannot access a source or destination space");
    }

    auto clone = CreateUninitializedForKind(extents_, destination_resource,
                                            mapping_.kind());
    if (!clone.ok()) {
      return clone.status();
    }
    if (buffer_.size() != 0) {
      auto source_memory = buffer_.const_view();
      if (!source_memory.ok()) {
        return source_memory.status();
      }
      auto destination_memory = clone->buffer_.mutable_view();
      if (!destination_memory.ok()) {
        return destination_memory.status();
      }
      auto copied = CopyBytes(context, *destination_memory, *source_memory,
                              buffer_.size());
      if (!copied.ok()) {
        return copied.status();
      }
      const Status wait_status = copied->Wait();
      if (!wait_status.ok()) {
        return wait_status;
      }
    }
    return std::move(*clone);
  }

  // Allocates and value-initializes replacement storage before changing this
  // owner. Success invalidates every prior view; failure leaves it unchanged.
  Status ResizeDiscard(const ExtentsType& extents) {
    if (resource_ == nullptr) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from DenseArray cannot be resized");
    }
    auto replacement = CreateForKind(extents, *resource_, mapping_.kind());
    if (!replacement.ok()) {
      return replacement.status();
    }
    *this = std::move(*replacement);
    return Status::Ok();
  }

 private:
  template <typename Layout>
  static Result<DenseArray> CreateWithLayout(const ExtentsType& extents,
                                             MemoryResource& resource,
                                             Layout layout) {
    if (resource.space() != MemorySpace::kHost) {
      return Status(ErrorCode::kUnsupported,
                    "Milestone 3 DenseArray supports host memory only");
    }
    auto array = CreateUninitializedWithLayout(extents, resource, layout);
    if (!array.ok()) {
      return array.status();
    }
    auto element_count =
        CheckedCast<std::size_t>(array->mapping_.required_span_size());
    if (!element_count.ok()) {
      return element_count.status();
    }
    Element* data = static_cast<Element*>(array->buffer_.data());
    for (std::size_t index = 0; index < *element_count; ++index) {
      std::construct_at(data + index, Element{});
    }
    return std::move(*array);
  }

  template <typename Layout>
  static Result<DenseArray> CreateUninitializedWithLayout(
      const ExtentsType& extents, MemoryResource& resource, Layout layout) {
    switch (resource.space()) {
      case MemorySpace::kHost:
      case MemorySpace::kPinnedHost:
      case MemorySpace::kDevice:
      case MemorySpace::kManaged:
        break;
      default:
        return Status(ErrorCode::kInvalidArgument,
                      "A DenseArray resource has an invalid memory space");
    }
    auto mapping = Mapping::Create(layout, extents.values());
    if (!mapping.ok()) {
      return mapping.status();
    }
    if (!mapping->is_unique() || !mapping->is_exhaustive()) {
      return Status(
          ErrorCode::kInvalidArgument,
          "A DenseArray requires a unique and exhaustive layout mapping");
    }
    auto bytes =
        CheckedByteCount(mapping->required_span_size(), sizeof(Element));
    if (!bytes.ok()) {
      return bytes.status();
    }
    auto buffer = Buffer::Allocate(resource, *bytes, alignof(Element));
    if (!buffer.ok()) {
      return buffer.status();
    }
    return DenseArray(extents, *mapping, std::move(*buffer), &resource);
  }

  static Result<DenseArray> CreateForKind(const ExtentsType& extents,
                                          MemoryResource& resource,
                                          DenseLayoutKind kind) {
    switch (kind) {
      case DenseLayoutKind::kLeft:
        return Create(extents, resource, LayoutLeft{});
      case DenseLayoutKind::kRight:
        return Create(extents, resource, LayoutRight{});
      case DenseLayoutKind::kStride:
        return Status(ErrorCode::kInternal,
                      "A DenseArray cannot own a LayoutStride mapping");
    }
    return Status(ErrorCode::kInvalidState,
                  "A DenseArray has an invalid layout kind");
  }

  static Result<DenseArray> CreateUninitializedForKind(
      const ExtentsType& extents, MemoryResource& resource,
      DenseLayoutKind kind) {
    switch (kind) {
      case DenseLayoutKind::kLeft:
        return CreateUninitialized(extents, resource, LayoutLeft{});
      case DenseLayoutKind::kRight:
        return CreateUninitialized(extents, resource, LayoutRight{});
      case DenseLayoutKind::kStride:
        return Status(ErrorCode::kInternal,
                      "A DenseArray cannot own a LayoutStride mapping");
    }
    return Status(ErrorCode::kInvalidState,
                  "A DenseArray has an invalid layout kind");
  }

  DenseArray(ExtentsType extents, Mapping mapping, Buffer buffer,
             MemoryResource* resource) noexcept
      : extents_(std::move(extents)),
        mapping_(mapping),
        buffer_(std::move(buffer)),
        resource_(resource) {}

  ExtentsType extents_;
  Mapping mapping_;
  Buffer buffer_;
  MemoryResource* resource_;
};

}  // namespace asc

#endif  // ASC_DENSE_ARRAY_H_
