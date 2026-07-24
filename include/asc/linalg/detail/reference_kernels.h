// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_LINALG_DETAIL_REFERENCE_KERNELS_H_
#define ASC_LINALG_DETAIL_REFERENCE_KERNELS_H_

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/core/config.h"
#include "asc/core/memory_space.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/linalg/capabilities.h"
#include "asc/linalg/concepts.h"
#include "asc/linalg/types.h"

namespace asc::detail {

template <typename Operand>
using LinalgValueType = typename std::remove_cvref_t<Operand>::ValueType;

template <typename First, typename... Rest>
concept SameLinalgValueType =
    (std::same_as<LinalgValueType<First>, LinalgValueType<Rest>> && ...);

inline bool CheckedLinalgMultiply(extent_t left, extent_t right,
                                  extent_t* product) noexcept {
  if (left < 0 || right < 0) {
    return false;
  }
  if (left != 0 &&
      right > std::numeric_limits<extent_t>::max() / left) {
    return false;
  }
  *product = left * right;
  return true;
}

inline bool CheckedLinalgAdd(extent_t left, extent_t right,
                             extent_t* sum) noexcept {
  if (left < 0 || right < 0 ||
      right > std::numeric_limits<extent_t>::max() - left) {
    return false;
  }
  *sum = left + right;
  return true;
}

template <ReadableLinalgVector Operand>
Status ValidateLinalgMetadata(const Operand& operand) {
  bool empty = false;
  for (std::size_t dimension = 0;
       dimension < std::remove_cvref_t<Operand>::Rank(); ++dimension) {
    const extent_t extent = operand.GetExtent(dimension);
    const stride_t stride = operand.GetStride(dimension);
    if (extent < 0 || stride < 0) {
      return Status(StatusCode::kOverflow,
                    "A Linalg vector has invalid extent or stride metadata");
    }
    if (extent == 0) {
      empty = true;
    }
  }
  extent_t expected_size = empty ? 0 : 1;
  extent_t expected_span = empty ? 0 : 1;
  if (!empty) {
    for (std::size_t dimension = 0;
         dimension < std::remove_cvref_t<Operand>::Rank(); ++dimension) {
      const extent_t extent = operand.GetExtent(dimension);
      const stride_t stride = operand.GetStride(dimension);
      extent_t span_term = 0;
      if (!CheckedLinalgMultiply(expected_size, extent, &expected_size)) {
        return Status(StatusCode::kOverflow,
                      "A Linalg vector logical size overflowed");
      }
      if (!CheckedLinalgMultiply(extent - 1, stride, &span_term) ||
          !CheckedLinalgAdd(expected_span, span_term, &expected_span)) {
        return Status(StatusCode::kOverflow,
                      "A Linalg vector backing span overflowed");
      }
    }
  }
  if (operand.GetSize() != expected_size ||
      operand.GetRequiredSpan() != expected_span ||
      operand.GetAvailableSpan() < expected_span ||
      operand.GetAvailableSpan() < 0) {
    return Status(StatusCode::kOverflow,
                  "A Linalg vector has inconsistent span metadata");
  }
  if (!IsHostAccessible(operand.GetMemorySpace())) {
    return Status(StatusCode::kUnsupported,
                  "Canonical Linalg requires host-accessible vectors");
  }
  return Status::Ok();
}

template <ReadableLinalgMatrix Operand>
Status ValidateLinalgMetadata(const Operand& operand) {
  bool empty = false;
  for (std::size_t dimension = 0;
       dimension < std::remove_cvref_t<Operand>::Rank(); ++dimension) {
    const extent_t extent = operand.GetExtent(dimension);
    const stride_t stride = operand.GetStride(dimension);
    if (extent < 0 || stride < 0) {
      return Status(StatusCode::kOverflow,
                    "A Linalg matrix has invalid extent or stride metadata");
    }
    if (extent == 0) {
      empty = true;
    }
  }
  extent_t expected_size = empty ? 0 : 1;
  extent_t expected_span = empty ? 0 : 1;
  if (!empty) {
    for (std::size_t dimension = 0;
         dimension < std::remove_cvref_t<Operand>::Rank(); ++dimension) {
      const extent_t extent = operand.GetExtent(dimension);
      const stride_t stride = operand.GetStride(dimension);
      extent_t span_term = 0;
      if (!CheckedLinalgMultiply(expected_size, extent, &expected_size)) {
        return Status(StatusCode::kOverflow,
                      "A Linalg matrix logical size overflowed");
      }
      if (!CheckedLinalgMultiply(extent - 1, stride, &span_term) ||
          !CheckedLinalgAdd(expected_span, span_term, &expected_span)) {
        return Status(StatusCode::kOverflow,
                      "A Linalg matrix backing span overflowed");
      }
    }
  }
  if (operand.GetSize() != expected_size ||
      operand.GetRequiredSpan() != expected_span ||
      operand.GetAvailableSpan() < expected_span ||
      operand.GetAvailableSpan() < 0) {
    return Status(StatusCode::kOverflow,
                  "A Linalg matrix has inconsistent span metadata");
  }
  if (!IsHostAccessible(operand.GetMemorySpace())) {
    return Status(StatusCode::kUnsupported,
                  "Canonical Linalg requires host-accessible matrices");
  }
  return Status::Ok();
}

template <WritableLinalgVector Operand>
Status ValidateWritableLinalgMetadata(const Operand& operand) {
  Status status = ValidateLinalgMetadata(operand);
  if (!status.ok()) {
    return status;
  }
  if (!operand.GetMapping().IsUnique()) {
    return Status(StatusCode::kFailedPrecondition,
                  "A writable Linalg vector requires a unique mapping");
  }
  return Status::Ok();
}

template <WritableLinalgMatrix Operand>
Status ValidateWritableLinalgMetadata(const Operand& operand) {
  Status status = ValidateLinalgMetadata(operand);
  if (!status.ok()) {
    return status;
  }
  if (!operand.GetMapping().IsUnique()) {
    return Status(StatusCode::kFailedPrecondition,
                  "A writable Linalg matrix requires a unique mapping");
  }
  return Status::Ok();
}

template <ReadableLinalgVector Operand>
Status ValidateLinalgData(const Operand& operand) {
  if (operand.GetRequiredSpan() != 0 && operand.Data() == nullptr) {
    return Status(StatusCode::kFailedPrecondition,
                  "A nonempty Linalg vector requires a data handle");
  }
  return Status::Ok();
}

template <ReadableLinalgMatrix Operand>
Status ValidateLinalgData(const Operand& operand) {
  if (operand.GetRequiredSpan() != 0 && operand.Data() == nullptr) {
    return Status(StatusCode::kFailedPrecondition,
                  "A nonempty Linalg matrix requires a data handle");
  }
  return Status::Ok();
}

inline Status ValidateLinalgContext(const ExecutionContext& context,
                                    LinalgOperation operation) {
  auto capabilities = GetLinalgCapabilities(context);
  if (!capabilities.ok()) {
    return capabilities.status();
  }
  if (!capabilities.value().Supports(operation)) {
    return Status(StatusCode::kUnsupported,
                  "The selected Linalg provider does not support the "
                  "operation");
  }
  return Status::Ok();
}

inline bool IsValidTransposeMode(TransposeMode transpose) noexcept {
  return transpose == TransposeMode::kNoTranspose ||
         transpose == TransposeMode::kTranspose;
}

template <typename Left, typename Right>
bool IsExactLinalgDescriptor(const Left& left, const Right& right) {
  static_assert(std::remove_cvref_t<Left>::Rank() ==
                std::remove_cvref_t<Right>::Rank());
  if (static_cast<const void*>(left.Data()) !=
      static_cast<const void*>(right.Data())) {
    return false;
  }
  for (std::size_t dimension = 0;
       dimension < std::remove_cvref_t<Left>::Rank(); ++dimension) {
    if (left.GetExtent(dimension) != right.GetExtent(dimension) ||
        left.GetStride(dimension) != right.GetStride(dimension)) {
      return false;
    }
  }
  return true;
}

struct LinalgByteInterval {
  std::uintptr_t begin = 0;
  std::uintptr_t end = 0;
};

template <typename Operand>
Result<LinalgByteInterval> GetLinalgByteInterval(const Operand& operand) {
  const extent_t required_span = operand.GetRequiredSpan();
  if (required_span == 0) {
    return LinalgByteInterval{};
  }
  constexpr std::uintptr_t kElementSize =
      sizeof(typename std::remove_cvref_t<Operand>::ValueType);
  const auto unsigned_span = static_cast<std::uintmax_t>(required_span);
  if (unsigned_span >
      std::numeric_limits<std::uintptr_t>::max() / kElementSize) {
    return Status(StatusCode::kOverflow,
                  "A Linalg alias byte span overflowed");
  }
  const auto bytes =
      static_cast<std::uintptr_t>(unsigned_span) * kElementSize;
  const auto begin = reinterpret_cast<std::uintptr_t>(operand.Data());
  if (bytes > std::numeric_limits<std::uintptr_t>::max() - begin) {
    return Status(StatusCode::kOverflow,
                  "A Linalg alias byte range overflowed");
  }
  return LinalgByteInterval{begin, begin + bytes};
}

template <typename Left, typename Right>
Result<bool> LinalgStorageOverlaps(const Left& left, const Right& right) {
  auto left_interval = GetLinalgByteInterval(left);
  if (!left_interval.ok()) {
    return left_interval.status();
  }
  auto right_interval = GetLinalgByteInterval(right);
  if (!right_interval.ok()) {
    return right_interval.status();
  }
  if (left_interval.value().begin == left_interval.value().end ||
      right_interval.value().begin == right_interval.value().end) {
    return false;
  }
  return left_interval.value().begin < right_interval.value().end &&
         right_interval.value().begin < left_interval.value().end;
}

template <typename Operand>
auto LinalgDataOrNull(const Operand& operand) noexcept {
  using DataHandle = typename std::remove_cvref_t<Operand>::DataHandle;
  return operand.GetRequiredSpan() == 0 ? DataHandle{} : operand.Data();
}

ASC_EXPORT void ReferenceCopy(const float* x, stride_t x_stride, float* y,
                              stride_t y_stride, extent_t size) noexcept;
ASC_EXPORT void ReferenceCopy(const double* x, stride_t x_stride, double* y,
                              stride_t y_stride, extent_t size) noexcept;

ASC_EXPORT void ReferenceScal(float alpha, float* x, stride_t x_stride,
                              extent_t size) noexcept;
ASC_EXPORT void ReferenceScal(double alpha, double* x, stride_t x_stride,
                              extent_t size) noexcept;

ASC_EXPORT void ReferenceAxpy(float alpha, const float* x, stride_t x_stride,
                              float* y, stride_t y_stride,
                              extent_t size) noexcept;
ASC_EXPORT void ReferenceAxpy(double alpha, const double* x,
                              stride_t x_stride, double* y,
                              stride_t y_stride, extent_t size) noexcept;

ASC_EXPORT float ReferenceDot(const float* x, stride_t x_stride,
                              const float* y, stride_t y_stride,
                              extent_t size) noexcept;
ASC_EXPORT double ReferenceDot(const double* x, stride_t x_stride,
                               const double* y, stride_t y_stride,
                               extent_t size) noexcept;

ASC_EXPORT float ReferenceNrm2(const float* x, stride_t x_stride,
                               extent_t size) noexcept;
ASC_EXPORT double ReferenceNrm2(const double* x, stride_t x_stride,
                                extent_t size) noexcept;

ASC_EXPORT void ReferenceGemv(TransposeMode transpose, float alpha,
                              const float* matrix, stride_t row_stride,
                              stride_t column_stride, extent_t rows,
                              extent_t columns, const float* x,
                              stride_t x_stride, float beta, float* y,
                              stride_t y_stride) noexcept;
ASC_EXPORT void ReferenceGemv(TransposeMode transpose, double alpha,
                              const double* matrix, stride_t row_stride,
                              stride_t column_stride, extent_t rows,
                              extent_t columns, const double* x,
                              stride_t x_stride, double beta, double* y,
                              stride_t y_stride) noexcept;

ASC_EXPORT void ReferenceGemm(
    TransposeMode transpose_a, TransposeMode transpose_b, float alpha,
    const float* a, stride_t a_row_stride, stride_t a_column_stride,
    extent_t a_rows, extent_t a_columns, const float* b,
    stride_t b_row_stride, stride_t b_column_stride, extent_t b_rows,
    extent_t b_columns, float beta, float* c, stride_t c_row_stride,
    stride_t c_column_stride) noexcept;
ASC_EXPORT void ReferenceGemm(
    TransposeMode transpose_a, TransposeMode transpose_b, double alpha,
    const double* a, stride_t a_row_stride, stride_t a_column_stride,
    extent_t a_rows, extent_t a_columns, const double* b,
    stride_t b_row_stride, stride_t b_column_stride, extent_t b_rows,
    extent_t b_columns, double beta, double* c, stride_t c_row_stride,
    stride_t c_column_stride) noexcept;

}  // namespace asc::detail

#endif  // ASC_LINALG_DETAIL_REFERENCE_KERNELS_H_
