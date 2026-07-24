// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_RANDOM_DETAIL_FILL_VALIDATION_H_
#define ASC_RANDOM_DETAIL_FILL_VALIDATION_H_

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/array/tensor_concepts.h"
#include "asc/core/execution_context.h"
#include "asc/core/memory_space.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/random/distribution.h"
#include "asc/random/types.h"

namespace asc::detail {

template <typename T>
concept WritableRandomTensor =
    WritableTensor<T> &&
    SupportedRandomScalar<typename std::remove_cvref_t<T>::ValueType> &&
    !std::is_volatile_v<typename std::remove_cvref_t<T>::ElementType> &&
    std::same_as<
        typename std::remove_cvref_t<T>::ElementType,
        typename std::remove_cvref_t<T>::ValueType> &&
    std::is_pointer_v<typename std::remove_cvref_t<T>::DataHandle> &&
    std::same_as<
        std::remove_pointer_t<typename std::remove_cvref_t<T>::DataHandle>,
        typename std::remove_cvref_t<T>::ElementType> &&
    requires(const std::remove_cvref_t<T>& destination) {
      { destination.GetRequiredSpan() } -> std::same_as<extent_t>;
      { destination.GetAvailableSpan() } -> std::same_as<extent_t>;
      { destination.GetMemorySpace() } -> std::same_as<MemorySpace>;
      { destination.GetMapping().IsUnique() } -> std::same_as<bool>;
    };

struct RandomFillMetadata {
  extent_t logical_size = 0;
};

inline bool CheckedRandomMultiply(extent_t left, extent_t right,
                                  extent_t* product) noexcept {
  if (left < 0 || right < 0 ||
      (left != 0 &&
       right > std::numeric_limits<extent_t>::max() / left)) {
    return false;
  }
  *product = left * right;
  return true;
}

inline bool CheckedRandomAdd(extent_t left, extent_t right,
                             extent_t* sum) noexcept {
  if (left < 0 || right < 0 ||
      right > std::numeric_limits<extent_t>::max() - left) {
    return false;
  }
  *sum = left + right;
  return true;
}

inline Status ValidateRandomContext(const ExecutionContext& context) {
  if (context.GetBackend() != BackendKind::kSerial ||
      !context.Supports(ExecutionCapability::kSynchronous)) {
    return Status(StatusCode::kUnavailable,
                  "The selected Random execution provider is unavailable");
  }
  return Status::Ok();
}

template <WritableRandomTensor Destination>
Status ValidateRandomMetadata(const Destination& destination,
                              RandomFillMetadata* metadata) {
  bool empty = false;
  for (std::size_t dimension = 0;
       dimension < std::remove_cvref_t<Destination>::Rank(); ++dimension) {
    if (destination.GetExtent(dimension) < 0 ||
        destination.GetStride(dimension) < 0) {
      return Status(StatusCode::kInvalidArgument,
                    "A Random destination has negative extent or stride "
                    "metadata");
    }
    if (destination.GetExtent(dimension) == 0) {
      empty = true;
    }
  }

  extent_t expected_size = empty ? 0 : 1;
  extent_t expected_span = empty ? 0 : 1;
  if (!empty) {
    for (std::size_t dimension = 0;
         dimension < std::remove_cvref_t<Destination>::Rank(); ++dimension) {
      const extent_t extent = destination.GetExtent(dimension);
      const stride_t stride = destination.GetStride(dimension);
      extent_t span_term = 0;
      if (!CheckedRandomMultiply(expected_size, extent, &expected_size)) {
        return Status(StatusCode::kOverflow,
                      "A Random destination logical size overflowed");
      }
      if (!CheckedRandomMultiply(extent - 1, stride, &span_term) ||
          !CheckedRandomAdd(expected_span, span_term, &expected_span)) {
        return Status(StatusCode::kOverflow,
                      "A Random destination backing span overflowed");
      }
    }
  }

  if (destination.GetSize() != expected_size ||
      destination.GetRequiredSpan() != expected_span ||
      destination.GetAvailableSpan() < 0) {
    return Status(StatusCode::kInvalidArgument,
                  "A Random destination has inconsistent descriptor "
                  "metadata");
  }
  if (destination.GetAvailableSpan() < expected_span) {
    return Status(StatusCode::kOutOfRange,
                  "A Random destination backing span is insufficient");
  }
  constexpr extent_t kElementSize =
      sizeof(typename std::remove_cvref_t<Destination>::ValueType);
  extent_t byte_span = 0;
  if (!CheckedRandomMultiply(expected_span, kElementSize, &byte_span)) {
    return Status(StatusCode::kOverflow,
                  "A Random destination byte span overflowed");
  }
  if (!IsHostAccessible(destination.GetMemorySpace())) {
    return Status(StatusCode::kUnsupported,
                  "Canonical Random requires host-accessible storage");
  }
  if (!destination.GetMapping().IsUnique()) {
    return Status(StatusCode::kFailedPrecondition,
                  "A Random destination requires a unique mapping");
  }
  metadata->logical_size = expected_size;
  return Status::Ok();
}

inline Status ValidateRandomCounter(RandomCounter counter,
                                    extent_t logical_size) {
  if (logical_size <= 0) {
    return Status::Ok();
  }
  const auto final_delta = static_cast<std::uint64_t>(logical_size - 1);
  if (final_delta >
      std::numeric_limits<std::uint64_t>::max() - counter.offset) {
    return Status(StatusCode::kOverflow,
                  "A Random destination counter range overflowed");
  }
  return Status::Ok();
}

template <WritableRandomTensor Destination>
Status ValidateRandomData(const Destination& destination) {
  if (destination.Data() == nullptr) {
    return Status(StatusCode::kInvalidArgument,
                  "A nonempty Random destination requires a data handle");
  }
  return Status::Ok();
}

}  // namespace asc::detail

#endif  // ASC_RANDOM_DETAIL_FILL_VALIDATION_H_
