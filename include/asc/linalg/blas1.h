// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_LINALG_BLAS1_H_
#define ASC_LINALG_BLAS1_H_

#include <concepts>
#include <type_traits>

#include "asc/core/execution_context.h"
#include "asc/core/status.h"
#include "asc/linalg/concepts.h"
#include "asc/linalg/detail/reference_kernels.h"

namespace asc {

/// @brief Copy one canonical vector into a disjoint or identical vector.
template <ReadableLinalgVector VectorX, WritableLinalgVector VectorY>
  requires detail::SameLinalgValueType<VectorX, VectorY>
Status Copy(const ExecutionContext& context, const VectorX& x,
            const VectorY& y) {
  Status status =
      detail::ValidateLinalgContext(context, LinalgOperation::kCopy);
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
  if (x.GetExtent(0) != y.GetExtent(0)) {
    return Status(StatusCode::kInvalidArgument,
                  "Copy requires equal vector extents");
  }
  if (x.GetSize() == 0) {
    return Status::Ok();
  }
  status = detail::ValidateLinalgData(x);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgData(y);
  if (!status.ok()) {
    return status;
  }
  if (detail::IsExactLinalgDescriptor(x, y)) {
    return Status::Ok();
  }
  auto overlaps = detail::LinalgStorageOverlaps(x, y);
  if (!overlaps.ok()) {
    return overlaps.status();
  }
  if (overlaps.value()) {
    return Status(StatusCode::kFailedPrecondition,
                  "Copy does not permit partially overlapping vectors");
  }
  detail::ReferenceCopy(detail::LinalgDataOrNull(x), x.GetStride(0),
                        detail::LinalgDataOrNull(y), y.GetStride(0),
                        x.GetSize());
  return Status::Ok();
}

/// @brief Scale a canonical vector in place.
template <WritableLinalgVector Vector, typename Scalar>
  requires std::same_as<std::remove_cvref_t<Scalar>,
                        detail::LinalgValueType<Vector>>
Status Scal(const ExecutionContext& context, Scalar alpha, const Vector& x) {
  Status status =
      detail::ValidateLinalgContext(context, LinalgOperation::kScal);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateWritableLinalgMetadata(x);
  if (!status.ok()) {
    return status;
  }
  if (x.GetSize() == 0) {
    return Status::Ok();
  }
  status = detail::ValidateLinalgData(x);
  if (!status.ok()) {
    return status;
  }
  detail::ReferenceScal(alpha, detail::LinalgDataOrNull(x), x.GetStride(0),
                        x.GetSize());
  return Status::Ok();
}

/// @brief Add a scaled vector to a disjoint or identical vector.
template <ReadableLinalgVector VectorX, WritableLinalgVector VectorY,
          typename Scalar>
  requires detail::SameLinalgValueType<VectorX, VectorY> &&
           std::same_as<std::remove_cvref_t<Scalar>,
                        detail::LinalgValueType<VectorY>>
Status Axpy(const ExecutionContext& context, Scalar alpha, const VectorX& x,
            const VectorY& y) {
  Status status =
      detail::ValidateLinalgContext(context, LinalgOperation::kAxpy);
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
  if (x.GetExtent(0) != y.GetExtent(0)) {
    return Status(StatusCode::kInvalidArgument,
                  "Axpy requires equal vector extents");
  }
  if (x.GetSize() == 0) {
    return Status::Ok();
  }
  status = detail::ValidateLinalgData(x);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgData(y);
  if (!status.ok()) {
    return status;
  }
  if (!detail::IsExactLinalgDescriptor(x, y)) {
    auto overlaps = detail::LinalgStorageOverlaps(x, y);
    if (!overlaps.ok()) {
      return overlaps.status();
    }
    if (overlaps.value()) {
      return Status(StatusCode::kFailedPrecondition,
                    "Axpy does not permit partially overlapping vectors");
    }
  }
  detail::ReferenceAxpy(alpha, detail::LinalgDataOrNull(x), x.GetStride(0),
                        detail::LinalgDataOrNull(y), y.GetStride(0),
                        x.GetSize());
  return Status::Ok();
}

/// @brief Compute the real, unconjugated inner product of two vectors.
template <ReadableLinalgVector VectorX, ReadableLinalgVector VectorY>
  requires detail::SameLinalgValueType<VectorX, VectorY>
Result<detail::LinalgValueType<VectorX>> Dot(const ExecutionContext& context,
                                             const VectorX& x,
                                             const VectorY& y) {
  Status status =
      detail::ValidateLinalgContext(context, LinalgOperation::kDot);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgMetadata(x);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgMetadata(y);
  if (!status.ok()) {
    return status;
  }
  if (x.GetExtent(0) != y.GetExtent(0)) {
    return Status(StatusCode::kInvalidArgument,
                  "Dot requires equal vector extents");
  }
  if (x.GetSize() == 0) {
    return detail::LinalgValueType<VectorX>{0};
  }
  status = detail::ValidateLinalgData(x);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgData(y);
  if (!status.ok()) {
    return status;
  }
  return detail::ReferenceDot(detail::LinalgDataOrNull(x), x.GetStride(0),
                              detail::LinalgDataOrNull(y), y.GetStride(0),
                              x.GetSize());
}

/// @brief Compute a scaled-sum-of-squares Euclidean vector norm.
template <ReadableLinalgVector Vector>
Result<detail::LinalgValueType<Vector>> Nrm2(
    const ExecutionContext& context, const Vector& x) {
  Status status =
      detail::ValidateLinalgContext(context, LinalgOperation::kNrm2);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateLinalgMetadata(x);
  if (!status.ok()) {
    return status;
  }
  if (x.GetSize() == 0) {
    return detail::LinalgValueType<Vector>{0};
  }
  status = detail::ValidateLinalgData(x);
  if (!status.ok()) {
    return status;
  }
  return detail::ReferenceNrm2(detail::LinalgDataOrNull(x), x.GetStride(0),
                               x.GetSize());
}

}  // namespace asc

#endif  // ASC_LINALG_BLAS1_H_
