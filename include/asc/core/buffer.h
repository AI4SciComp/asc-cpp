// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_CORE_BUFFER_H_
#define ASC_CORE_BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "asc/core/memory_resource.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {

/// @brief Move-only RAII ownership of a typed allocation in one memory space.
/// @tparam T Element type. Host elements must be default constructible.
///
/// Buffer owns exactly one allocation and retains its MemoryResource handle.
/// It never aliases, mirrors, resizes, synchronizes, or selects execution.
template <typename T>
class Buffer {
  static_assert(!std::is_reference_v<T>, "Buffer cannot store references");
  static_assert(!std::is_void_v<T>, "Buffer cannot store void elements");

 public:
  /// @brief Construct an empty host buffer.
  Buffer() : resource_(GetHostMemoryResource()) {}

  /// @brief Destroy constructed host elements and release the allocation.
  ~Buffer() noexcept { Release(); }

  /// @brief Copy construction is disabled; use a future explicit copy API.
  Buffer(const Buffer&) = delete;

  /// @brief Copy assignment is disabled; use a future explicit copy API.
  Buffer& operator=(const Buffer&) = delete;

  /// @brief Transfer ownership, leaving @p other valid and empty.
  Buffer(Buffer&& other) noexcept
      : data_(std::exchange(other.data_, nullptr)),
        size_(std::exchange(other.size_, 0)),
        byte_size_(std::exchange(other.byte_size_, 0)),
        resource_(other.resource_) {}

  /// @brief Release current ownership and transfer ownership from @p other.
  Buffer& operator=(Buffer&& other) noexcept {
    if (this != &other) {
      Release();
      data_ = std::exchange(other.data_, nullptr);
      size_ = std::exchange(other.size_, 0);
      byte_size_ = std::exchange(other.byte_size_, 0);
      resource_ = other.resource_;
    }
    return *this;
  }

  /// @brief Allocate storage for a number of elements.
  /// @param count Non-negative element count.
  /// @param resource Resource that owns allocation and deallocation.
  /// @return A buffer, or validation/allocation/construction failure status.
  static Result<Buffer> Allocate(
      extent_t count,
      MemoryResourcePtr resource = GetHostMemoryResource()) {
    if (!resource) {
      return Status(StatusCode::kInvalidArgument,
                    "Buffer allocation requires a memory resource");
    }
    if (count < 0) {
      return Status(StatusCode::kInvalidArgument,
                    "Buffer element count must be non-negative");
    }

    const auto unsigned_count = static_cast<std::uint64_t>(count);
    if (unsigned_count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
      return Status(StatusCode::kOverflow,
                    "Buffer element count overflows the byte-count type");
    }
    const std::size_t byte_size =
        static_cast<std::size_t>(unsigned_count) * sizeof(T);

    Buffer result(std::move(resource));
    result.size_ = count;
    result.byte_size_ = byte_size;
    if (count == 0) {
      return result;
    }

    const MemorySpace space = result.resource_->GetMemorySpace();
    if (!IsHostAccessible(space) &&
        (!std::is_trivially_copyable_v<T> ||
         !std::is_trivially_destructible_v<T>)) {
      return Status(StatusCode::kUnsupported,
                    "Non-host Buffer elements must be trivially transferable");
    }
    if constexpr (!std::is_default_constructible_v<T>) {
      if (IsHostAccessible(space)) {
        return Status(StatusCode::kUnsupported,
                      "Host Buffer elements must be default constructible");
      }
    }

    Result<void*> allocation =
        result.resource_->Allocate(byte_size, alignof(T));
    if (!allocation.ok()) {
      return allocation.status();
    }
    result.data_ = static_cast<T*>(allocation.value());
    if (result.data_ == nullptr) {
      result.size_ = 0;
      result.byte_size_ = 0;
      return Status(StatusCode::kAllocationFailed,
                    "Memory resource returned null for a non-zero allocation");
    }

    if constexpr (std::is_default_constructible_v<T>) {
      if (IsHostAccessible(space)) {
        try {
          std::uninitialized_default_construct_n(
              result.data_, static_cast<std::size_t>(count));
        } catch (const std::exception& error) {
          result.resource_->Deallocate(result.data_, byte_size, alignof(T));
          result.data_ = nullptr;
          result.size_ = 0;
          result.byte_size_ = 0;
          return Status(StatusCode::kAllocationFailed, error.what());
        } catch (...) {
          result.resource_->Deallocate(result.data_, byte_size, alignof(T));
          result.data_ = nullptr;
          result.size_ = 0;
          result.byte_size_ = 0;
          return Status(StatusCode::kAllocationFailed,
                        "Buffer element construction failed");
        }
      }
    }
    return result;
  }

  /// @brief Return the number of owned elements.
  extent_t GetSize() const noexcept { return size_; }

  /// @brief Return the checked allocation size in bytes.
  std::size_t GetByteSize() const noexcept { return byte_size_; }

  /// @brief Return true when the buffer owns no elements.
  bool IsEmpty() const noexcept { return size_ == 0; }

  /// @brief Return the allocation's memory space.
  MemorySpace GetMemorySpace() const noexcept {
    return resource_->GetMemorySpace();
  }

  /// @brief Return the retained allocation-resource handle.
  const MemoryResourcePtr& GetMemoryResource() const noexcept {
    return resource_;
  }

  /// @brief Return mutable storage when the allocation is host-accessible.
  Result<T*> HostData() {
    if (!IsHostAccessible(GetMemorySpace())) {
      return Status(StatusCode::kFailedPrecondition,
                    "Buffer storage is not host-accessible");
    }
    return data_;
  }

  /// @brief Return immutable storage when the allocation is host-accessible.
  Result<const T*> HostData() const {
    if (!IsHostAccessible(GetMemorySpace())) {
      return Status(StatusCode::kFailedPrecondition,
                    "Buffer storage is not host-accessible");
    }
    return data_;
  }

 private:
  explicit Buffer(MemoryResourcePtr resource) : resource_(std::move(resource)) {}

  void Release() noexcept {
    if (data_ == nullptr) {
      size_ = 0;
      byte_size_ = 0;
      return;
    }
    if (IsHostAccessible(resource_->GetMemorySpace())) {
      std::destroy_n(data_, static_cast<std::size_t>(size_));
    }
    resource_->Deallocate(data_, byte_size_, alignof(T));
    data_ = nullptr;
    size_ = 0;
    byte_size_ = 0;
  }

  T* data_ = nullptr;
  extent_t size_ = 0;
  std::size_t byte_size_ = 0;
  MemoryResourcePtr resource_;
};

}  // namespace asc

#endif  // ASC_CORE_BUFFER_H_
