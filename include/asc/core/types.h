// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_CORE_TYPES_H_
#define ASC_CORE_TYPES_H_

#include <cstdint>

#include "asc/core/config.h"

namespace asc {

/// @brief Signed public type for logical indices and offsets.
using index_t = std::int64_t;

/// @brief Signed public type for dimension extents and element counts.
using extent_t = std::int64_t;

/// @brief Signed public type for physical storage strides.
using stride_t = std::int64_t;

/// @brief Signed public type for sparse nonzero counts.
using nnz_t = std::int64_t;

/// @brief Sentinel denoting an extent supplied at runtime.
inline constexpr extent_t dynamic_extent = -1;

}  // namespace asc

#endif  // ASC_CORE_TYPES_H_
