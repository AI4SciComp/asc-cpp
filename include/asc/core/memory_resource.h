// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_CORE_MEMORY_RESOURCE_H_
#define ASC_CORE_MEMORY_RESOURCE_H_

#include <cstddef>
#include <memory>
#include <string_view>

#include "asc/core/config.h"
#include "asc/core/memory_space.h"
#include "asc/core/status.h"

namespace asc {

/// @brief Abstract allocation boundary for one memory space.
///
/// Implementations must be safe for concurrent allocation and deallocation.
/// An allocation must be returned to an equal resource with the same byte
/// count and alignment.
class ASC_EXPORT MemoryResource {
 public:
  /// @brief Destroy the allocation resource.
  virtual ~MemoryResource() = default;

  /// @brief Return the address space served by this resource.
  virtual MemorySpace GetMemorySpace() const noexcept = 0;

  /// @brief Return a stable diagnostic name for this resource.
  virtual std::string_view GetName() const noexcept = 0;

  /// @brief Allocate bytes with an explicit power-of-two alignment.
  /// @param bytes Number of bytes to allocate. Zero returns a null pointer.
  /// @param alignment Required power-of-two alignment.
  /// @return The allocation, or a status describing validation/allocation
  /// failure.
  virtual Result<void*> Allocate(std::size_t bytes,
                                 std::size_t alignment) = 0;

  /// @brief Release an allocation previously returned by this resource.
  /// @param pointer Allocation pointer; null is accepted.
  /// @param bytes Original allocation byte count.
  /// @param alignment Original allocation alignment.
  virtual void Deallocate(void* pointer, std::size_t bytes,
                          std::size_t alignment) noexcept = 0;

  /// @brief Test whether allocations may be deallocated interchangeably.
  virtual bool IsEqual(const MemoryResource& other) const noexcept = 0;
};

/// @brief Shared handle that keeps an allocation resource alive.
using MemoryResourcePtr = std::shared_ptr<MemoryResource>;

/// @brief Return the process-wide, thread-safe host allocation resource.
ASC_EXPORT MemoryResourcePtr GetHostMemoryResource();

}  // namespace asc

#endif  // ASC_CORE_MEMORY_RESOURCE_H_
