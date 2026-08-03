#ifndef ASC_CORE_TYPES_H_
#define ASC_CORE_TYPES_H_

/**
 * @file
 * @brief Public Core declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_core
 */

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

/**
 * @brief Defines the public index_t type used by this Core contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
using index_t = std::int64_t;
/**
 * @brief Defines the public extent_t type used by this Core contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
using extent_t = std::int64_t;
/**
 * @brief Defines the public stride_t type used by this Core contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
using stride_t = std::int64_t;
/**
 * @brief Defines the public nnz_t type used by this Core contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
using nnz_t = std::int64_t;
/**
 * @brief Defines the public rank_t type used by this Core contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
using rank_t = std::uint32_t;

/**
 * @brief Stores the DynamicExtent value for this contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
inline constexpr extent_t kDynamicExtent = -1;

/**
 * @brief Defines the public CheckedInteger concept contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
template <typename T>
concept CheckedInteger =
    std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>;

/**
 * @brief Performs the public CheckedCast operation defined by the Core
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @tparam To Type or non-type argument satisfying the declaration's
 * constraints.
 * @tparam From Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] value Value read or written by the operation.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_core
 */
template <CheckedInteger To, CheckedInteger From>
Result<To> CheckedCast(From value) {
  if (!std::in_range<To>(value)) {
    return Status(ErrorCode::kOverflow,
                  "Integral conversion is outside the destination range");
  }
  return static_cast<To>(value);
}

/**
 * @brief Performs the public CheckedAdd operation defined by the Core contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @param[in] left The left value required by this contract.
 * @param[in] right The right value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_core
 */
template <CheckedInteger T>
Result<T> CheckedAdd(T left, T right) {
  constexpr T kMinimum = std::numeric_limits<T>::min();
  constexpr T kMaximum = std::numeric_limits<T>::max();

  if constexpr (std::is_unsigned_v<T>) {
    if (left > kMaximum - right) {
      return Status(ErrorCode::kOverflow, "Integral addition overflow");
    }
  } else {
    if ((right > 0 && left > kMaximum - right) ||
        (right < 0 && left < kMinimum - right)) {
      return Status(ErrorCode::kOverflow, "Integral addition overflow");
    }
  }
  return static_cast<T>(left + right);
}

/**
 * @brief Performs the public CheckedMultiply operation defined by the Core
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @param[in] left The left value required by this contract.
 * @param[in] right The right value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_core
 */
template <CheckedInteger T>
Result<T> CheckedMultiply(T left, T right) {
  constexpr T kMinimum = std::numeric_limits<T>::min();
  constexpr T kMaximum = std::numeric_limits<T>::max();

  if (left == 0 || right == 0) {
    return static_cast<T>(0);
  }

  if constexpr (std::is_unsigned_v<T>) {
    if (left > kMaximum / right) {
      return Status(ErrorCode::kOverflow, "Integral multiplication overflow");
    }
  } else {
    if ((left == -1 && right == kMinimum) ||
        (right == -1 && left == kMinimum)) {
      return Status(ErrorCode::kOverflow, "Integral multiplication overflow");
    }
    if (left > 0) {
      if ((right > 0 && left > kMaximum / right) ||
          (right < 0 && right < kMinimum / left)) {
        return Status(ErrorCode::kOverflow, "Integral multiplication overflow");
      }
    } else {
      if ((right > 0 && left < kMinimum / right) ||
          (right < 0 && left < kMaximum / right)) {
        return Status(ErrorCode::kOverflow, "Integral multiplication overflow");
      }
    }
  }
  return static_cast<T>(left * right);
}

/**
 * @brief Performs the public CheckedByteCount operation defined by the Core
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] element_count The element count value required by this contract.
 * @param[in] element_size The element size value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_core
 */
inline Result<std::size_t> CheckedByteCount(extent_t element_count,
                                            std::size_t element_size) {
  if (element_count < 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "A byte count cannot use a negative element count");
  }
  auto checked_count = CheckedCast<std::size_t>(element_count);
  if (!checked_count.ok()) {
    return checked_count.status();
  }
  return CheckedMultiply(*checked_count, element_size);
}

}  // namespace asc

#endif  // ASC_CORE_TYPES_H_
