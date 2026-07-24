// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_LINALG_BLAS2_H_
#define ASC_LINALG_BLAS2_H_

#include <concepts>
#include <type_traits>

#include "asc/core/execution_context.h"
#include "asc/core/status.h"
#include "asc/linalg/concepts.h"
#include "asc/linalg/detail/reference_kernels.h"
#include "asc/linalg/types.h"

namespace asc {

/// @brief Compute y = alpha * op(A) * x + beta * y.
template <ReadableLinalgMatrix MatrixA, ReadableLinalgVector VectorX,
          WritableLinalgVector VectorY, typename Alpha, typename Beta>
  requires detail::SameLinalgValueType<MatrixA, VectorX, VectorY> &&
           std::same_as<std::remove_cvref_t<Alpha>,
                        detail::LinalgValueType<MatrixA>> &&
           std::same_as<std::remove_cvref_t<Beta>,
                        detail::LinalgValueType<MatrixA>>
Status Gemv(const ExecutionContext& context, TransposeMode transpose,
            Alpha alpha, const MatrixA& a, const VectorX& x, Beta beta,
            const VectorY& y) {
  Status status =
      detail::ValidateLinalgContext(context, LinalgOperation::kGemv);
  if (!status.ok()) {
    return status;
  }
  if (!detail::IsValidTransposeMode(transpose)) {
    return Status(StatusCode::kInvalidArgument,
                  "Gemv received an invalid transpose mode");
  }
  status = detail::ValidateLinalgMetadata(a);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgMetadata(x);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateWritableLinalgMetadata(y);
  if (!status.ok()) {
    return status;
  }

  const extent_t output_size =
      transpose == TransposeMode::kNoTranspose ? a.GetExtent(0)
                                               : a.GetExtent(1);
  const extent_t inner_size =
      transpose == TransposeMode::kNoTranspose ? a.GetExtent(1)
                                               : a.GetExtent(0);
  if (x.GetExtent(0) != inner_size || y.GetExtent(0) != output_size) {
    return Status(StatusCode::kInvalidArgument,
                  "Gemv operand extents do not satisfy op(A) * x -> y");
  }
  status = detail::ValidateLinalgData(a);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgData(x);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgData(y);
  if (!status.ok()) {
    return status;
  }
  if (output_size == 0) {
    return Status::Ok();
  }

  auto overlaps_a = detail::LinalgStorageOverlaps(a, y);
  if (!overlaps_a.ok()) {
    return overlaps_a.status();
  }
  auto overlaps_x = detail::LinalgStorageOverlaps(x, y);
  if (!overlaps_x.ok()) {
    return overlaps_x.status();
  }
  if (overlaps_a.value() || overlaps_x.value()) {
    return Status(StatusCode::kFailedPrecondition,
                  "Gemv output must not overlap an input");
  }

  detail::ReferenceGemv(
      transpose, alpha, detail::LinalgDataOrNull(a), a.GetStride(0),
      a.GetStride(1), a.GetExtent(0), a.GetExtent(1),
      detail::LinalgDataOrNull(x), x.GetStride(0), beta,
      detail::LinalgDataOrNull(y), y.GetStride(0));
  return Status::Ok();
}

}  // namespace asc

#endif  // ASC_LINALG_BLAS2_H_
