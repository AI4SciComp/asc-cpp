#ifndef ASC_CORE_TYPES_H_
#define ASC_CORE_TYPES_H_

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

using index_t = std::int64_t;
using extent_t = std::int64_t;
using stride_t = std::int64_t;
using nnz_t = std::int64_t;
using rank_t = std::uint32_t;

inline constexpr extent_t kDynamicExtent = -1;

template <typename T>
concept CheckedInteger =
    std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>;

template <CheckedInteger To, CheckedInteger From>
Result<To> CheckedCast(From value) {
  if (!std::in_range<To>(value)) {
    return Status(ErrorCode::kOverflow,
                  "Integral conversion is outside the destination range");
  }
  return static_cast<To>(value);
}

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
