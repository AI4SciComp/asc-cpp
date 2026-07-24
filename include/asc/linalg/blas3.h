// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_LINALG_BLAS3_H_
#define ASC_LINALG_BLAS3_H_

#include <concepts>
#include <type_traits>

#include "asc/core/execution_context.h"
#include "asc/core/status.h"
#include "asc/linalg/concepts.h"
#include "asc/linalg/detail/reference_kernels.h"
#include "asc/linalg/types.h"

namespace asc {

/// @brief Compute C = alpha * op(A) * op(B) + beta * C.
template <ReadableLinalgMatrix MatrixA, ReadableLinalgMatrix MatrixB,
          WritableLinalgMatrix MatrixC, typename Alpha, typename Beta>
  requires detail::SameLinalgValueType<MatrixA, MatrixB, MatrixC> &&
           std::same_as<std::remove_cvref_t<Alpha>,
                        detail::LinalgValueType<MatrixA>> &&
           std::same_as<std::remove_cvref_t<Beta>,
                        detail::LinalgValueType<MatrixA>>
Status Gemm(const ExecutionContext& context, TransposeMode transpose_a,
            TransposeMode transpose_b, Alpha alpha, const MatrixA& a,
            const MatrixB& b, Beta beta, const MatrixC& c) {
  Status status =
      detail::ValidateLinalgContext(context, LinalgOperation::kGemm);
  if (!status.ok()) {
    return status;
  }
  if (!detail::IsValidTransposeMode(transpose_a) ||
      !detail::IsValidTransposeMode(transpose_b)) {
    return Status(StatusCode::kInvalidArgument,
                  "Gemm received an invalid transpose mode");
  }
  status = detail::ValidateLinalgMetadata(a);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgMetadata(b);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateWritableLinalgMetadata(c);
  if (!status.ok()) {
    return status;
  }

  const extent_t output_rows =
      transpose_a == TransposeMode::kNoTranspose ? a.GetExtent(0)
                                                 : a.GetExtent(1);
  const extent_t inner_a =
      transpose_a == TransposeMode::kNoTranspose ? a.GetExtent(1)
                                                 : a.GetExtent(0);
  const extent_t inner_b =
      transpose_b == TransposeMode::kNoTranspose ? b.GetExtent(0)
                                                 : b.GetExtent(1);
  const extent_t output_columns =
      transpose_b == TransposeMode::kNoTranspose ? b.GetExtent(1)
                                                 : b.GetExtent(0);
  if (inner_a != inner_b || c.GetExtent(0) != output_rows ||
      c.GetExtent(1) != output_columns) {
    return Status(StatusCode::kInvalidArgument,
                  "Gemm operand extents do not satisfy op(A) * op(B) -> C");
  }
  status = detail::ValidateLinalgData(a);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgData(b);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgData(c);
  if (!status.ok()) {
    return status;
  }
  if (output_rows == 0 || output_columns == 0) {
    return Status::Ok();
  }

  auto overlaps_a = detail::LinalgStorageOverlaps(a, c);
  if (!overlaps_a.ok()) {
    return overlaps_a.status();
  }
  auto overlaps_b = detail::LinalgStorageOverlaps(b, c);
  if (!overlaps_b.ok()) {
    return overlaps_b.status();
  }
  if (overlaps_a.value() || overlaps_b.value()) {
    return Status(StatusCode::kFailedPrecondition,
                  "Gemm output must not overlap an input");
  }

  detail::ReferenceGemm(
      transpose_a, transpose_b, alpha, detail::LinalgDataOrNull(a),
      a.GetStride(0), a.GetStride(1), a.GetExtent(0), a.GetExtent(1),
      detail::LinalgDataOrNull(b), b.GetStride(0), b.GetStride(1),
      b.GetExtent(0), b.GetExtent(1), beta, detail::LinalgDataOrNull(c),
      c.GetStride(0), c.GetStride(1));
  return Status::Ok();
}

}  // namespace asc

#endif  // ASC_LINALG_BLAS3_H_
