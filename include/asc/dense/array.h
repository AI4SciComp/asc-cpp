#ifndef ASC_DENSE_ARRAY_H_
#define ASC_DENSE_ARRAY_H_

/**
 * @file
 * @brief Public Dense declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_dense
 */

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <new>
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

/**
 * @brief Owns contiguous dense storage with validated extents and layout.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Dense module contract.
 * Host complex elements have their C++20 lifetimes started by typed
 * construction. They are trivially destructible; releasing the buffer ends
 * their lifetimes without a separate destruction pass. Views remain borrowed
 * and are invalidated by owner destruction, assignment, or successful resize.
 * @ingroup asc_dense
 */
template <DenseElement Element, DenseExtents ExtentsType>
class DenseArray {
 public:
  /**
   * @brief Defines the public element_type type used by this Dense contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @ingroup asc_dense
   */
  using element_type = Element;
  /**
   * @brief Defines the public extents_type type used by this Dense contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @ingroup asc_dense
   */
  using extents_type = ExtentsType;
  /**
   * @brief Stores the Rank value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @ingroup asc_dense
   */
  static constexpr std::size_t kRank = ExtentsType::kRank;

  /**
   * @brief Validates inputs and creates the requested Dense object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @param[in] resource Allocator that must outlive storage allocated from it.
   * @param[in] extents Logical extents; every extent must satisfy the
   * documented bounds.
   * @param[in] layout The layout value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_dense
   */
  static Result<DenseArray> Create(MemoryResource& resource,
                                   ExtentsType extents,
                                   LayoutLeft layout = {}) {
    auto mapping = DenseLayout<kRank>::Create(extents.values(), layout);
    if (!mapping.ok()) {
      return mapping.status();
    }
    return CreateWithMapping(resource, std::move(extents), *mapping);
  }

  /**
   * @brief Validates inputs and creates the requested Dense object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @param[in] resource Allocator that must outlive storage allocated from it.
   * @param[in] extents Logical extents; every extent must satisfy the
   * documented bounds.
   * @param[in] layout The layout value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_dense
   */
  static Result<DenseArray> Create(MemoryResource& resource,
                                   ExtentsType extents, LayoutRight layout) {
    auto mapping = DenseLayout<kRank>::Create(extents.values(), layout);
    if (!mapping.ok()) {
      return mapping.status();
    }
    return CreateWithMapping(resource, std::move(extents), *mapping);
  }

  /**
   * @brief Validates inputs and creates the requested Dense object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @param[in] extents Logical extents; every extent must satisfy the
   * documented bounds.
   * @param[in] resource Allocator that must outlive storage allocated from it.
   * @param[in] layout The layout value required by this contract.
   * @note Arithmetic elements are not initialized. Float/double complex
   * elements are default-constructed to zero to start their C++20 lifetimes,
   * even in this factory. Complex storage requires host or pinned-host
   * resources; device/managed requests fail before allocation. Construction
   * performs one explicit buffer allocation and O(n) complex initializations.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_dense
   */
  static Result<DenseArray> CreateUninitialized(const ExtentsType& extents,
                                                MemoryResource& resource,
                                                LayoutLeft layout = {}) {
    auto mapping = DenseLayout<kRank>::Create(extents.values(), layout);
    if (!mapping.ok()) {
      return mapping.status();
    }
    return CreateUninitializedWithMapping(resource, extents, *mapping);
  }

  /**
   * @brief Validates inputs and creates the requested Dense object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @param[in] extents Logical extents; every extent must satisfy the
   * documented bounds.
   * @param[in] resource Allocator that must outlive storage allocated from it.
   * @param[in] layout The layout value required by this contract.
   * @note Arithmetic elements are not initialized. Float/double complex
   * elements are default-constructed to zero to start their C++20 lifetimes,
   * even in this factory. Complex storage requires host or pinned-host
   * resources; device/managed requests fail before allocation. Construction
   * performs one explicit buffer allocation and O(n) complex initializations.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_dense
   */
  static Result<DenseArray> CreateUninitialized(const ExtentsType& extents,
                                                MemoryResource& resource,
                                                LayoutRight layout) {
    auto mapping = DenseLayout<kRank>::Create(extents.values(), layout);
    if (!mapping.ok()) {
      return mapping.status();
    }
    return CreateUninitializedWithMapping(resource, extents, *mapping);
  }

  /**
   * @brief Constructs a DenseArray with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   * @ingroup asc_dense
   */
  DenseArray(const DenseArray&) = delete;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_dense
   */
  DenseArray& operator=(const DenseArray&) = delete;
  /**
   * @brief Constructs a DenseArray with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   * @ingroup asc_dense
   */
  DenseArray(DenseArray&&) noexcept = default;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_dense
   */
  DenseArray& operator=(DenseArray&&) noexcept = default;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   * @ingroup asc_dense
   */
  ~DenseArray() = default;

  /**
   * @brief Reports whether the documented valid condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_dense
   */
  [[nodiscard]] bool valid() const noexcept { return buffer_.valid(); }

  /**
   * @brief Returns the object's extents contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_dense
   */
  [[nodiscard]] const ExtentsType& extents() const noexcept { return extents_; }

  /**
   * @brief Performs the public mapping operation defined by the Dense contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_dense
   */
  [[nodiscard]] const DenseLayout<kRank>& mapping() const noexcept {
    return mapping_;
  }

  /**
   * @brief Performs the public logical_size operation defined by the Dense
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_dense
   */
  [[nodiscard]] extent_t logical_size() const noexcept {
    return mapping_.logical_size();
  }

  /**
   * @brief Performs the public view operation defined by the Dense contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_dense
   */
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

  /**
   * @brief Performs the public view operation defined by the Dense contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_dense
   */
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

  /**
   * @brief Performs the public Clone operation defined by the Dense contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @param[in] destination_resource Allocator for the returned owning
   * destination.
   * @param[in] context Execution backend and accessibility/order contract.
   * @note Complex cloning requires serial execution and host source and
   * destination storage. It allocates one destination buffer and copies n
   * live values in O(n) work, with no hidden workspace or provider dispatch.
   * The source is unchanged on allocation or validation failure.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_dense
   */
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
    if constexpr (!std::is_arithmetic_v<Element>) {
      if (context.backend() != Backend::kSerial) {
        return Status(ErrorCode::kUnsupported,
                      "Complex DenseArray cloning requires serial execution");
      }
    }
    auto clone = CreateUninitializedWithMapping(destination_resource, extents_,
                                                mapping_);
    if (!clone.ok()) {
      return clone.status();
    }
    if constexpr (!std::is_arithmetic_v<Element>) {
      // Both ranges contain live objects. No byte reinterpretation, transfer,
      // workspace allocation, or provider dispatch is needed for this copy.
      if (mapping_.required_span_size() != 0) {
        std::copy_n(static_cast<const Element*>(buffer_.data()),
                    mapping_.required_span_size(),
                    static_cast<Element*>(clone->buffer_.data()));
      }
      return std::move(*clone);
    } else {
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
  }

  /**
   * @brief Performs the public DiscardResize operation defined by the Dense
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @param[in] extents Logical extents; every extent must satisfy the
   * documented bounds.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_dense
   */
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

  /**
   * @brief Performs the public DiscardResize operation defined by the Dense
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @param[in] extents Logical extents; every extent must satisfy the
   * documented bounds.
   * @param[in] layout The layout value required by this contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_dense
   */
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

  /**
   * @brief Performs the public DiscardResize operation defined by the Dense
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Dense module contract.
   *
   * @param[in] extents Logical extents; every extent must satisfy the
   * documented bounds.
   * @param[in] layout The layout value required by this contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_dense
   */
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
                    "DenseArray supports only host resources");
    }
    auto array = CreateUninitializedWithMapping(resource, std::move(extents),
                                                std::move(mapping));
    if (!array.ok()) {
      return internal_core_result::StatusAccess::TakeFailure(std::move(array));
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
    if constexpr (!std::is_arithmetic_v<Element>) {
      if (resource.space() != MemorySpace::kHost &&
          resource.space() != MemorySpace::kPinnedHost) {
        return Status(
            ErrorCode::kUnsupported,
            "Complex DenseArray construction requires host or pinned storage");
      }
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
      return internal_core_result::StatusAccess::TakeFailure(std::move(buffer));
    }
    if constexpr (!std::is_arithmetic_v<Element>) {
      if (mapping.required_span_size() != 0) {
        // Nonallocating placement array new starts the array and element
        // lifetimes in C++20. std::complex default construction yields zero;
        // byte zeroing or C++23 implicit-lifetime guarantees are not used.
        ::new (buffer->data()) Element[mapping.required_span_size()];
      }
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
