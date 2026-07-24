// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_CORE_MEMORY_SPACE_H_
#define ASC_CORE_MEMORY_SPACE_H_

#include <cstdint>

#include "asc/core/types.h"

namespace asc {

/// @brief Physical address space occupied by one canonical allocation.
enum class MemorySpace : std::uint8_t {
  kHost = 0,    ///< Ordinary host memory.
  kPinnedHost,  ///< Page-locked host memory supplied by a future provider.
  kDevice,      ///< Device-only memory supplied by a future provider.
  kManaged,     ///< Explicit managed/unified memory.
};

/// @brief Return whether ordinary host code may access the memory space.
constexpr bool IsHostAccessible(MemorySpace space) noexcept {
  return space == MemorySpace::kHost || space == MemorySpace::kPinnedHost ||
         space == MemorySpace::kManaged;
}

/// @brief Return whether a device provider may access the memory space.
constexpr bool IsDeviceAccessible(MemorySpace space) noexcept {
  return space == MemorySpace::kDevice || space == MemorySpace::kManaged;
}

}  // namespace asc

#endif  // ASC_CORE_MEMORY_SPACE_H_
