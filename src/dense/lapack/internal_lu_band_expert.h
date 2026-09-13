#ifndef ASC_DENSE_LAPACK_INTERNAL_LU_BAND_EXPERT_H_
#define ASC_DENSE_LAPACK_INTERNAL_LU_BAND_EXPERT_H_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.
#include <span>

#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "internal_indefinite.h"
#include "internal_lu_band_expert_counts.h"

namespace asc::internal_lu_band_expert {
namespace common = internal_indefinite;
namespace counts = internal_lu_band_expert_counts;

template <typename T>
Status Compact(const ReferenceLapackProvider& provider,
               ReferenceGeneralBandView<T> matrix) {
  auto status =
      common::Accessible(provider, matrix.storage().reachable_storage());
  if (!status.ok()) {
    return status;
  }
  return counts::CompactStorage(
      matrix.rows(), matrix.columns(), matrix.lower_bandwidth(),
      matrix.upper_bandwidth(), matrix.storage().leading_dimension(),
      common::kIntegerLimit);
}

template <typename T>
Status Vector(const ReferenceLapackProvider& provider,
              DenseBlasVectorView<T> vector, extent_t count) {
  if (vector.size() != count || vector.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  return common::Accessible(provider, vector.reachable_storage());
}

template <typename T>
T* Nonnull(T* pointer, T& dummy) {
  return pointer == nullptr ? &dummy : pointer;
}

constexpr auto kReal = static_cast<std::size_t>(LapackWorkspaceKind::kReal);

template <typename Integer>
Status Pivots(std::span<const Integer> pivots, extent_t n, extent_t kl) {
  if (pivots.size() != static_cast<std::size_t>(n)) {
    return Status(ErrorCode::kShape);
  }
  for (extent_t j = 0; j < n; ++j) {
    const index_t value = pivots[static_cast<std::size_t>(j)];
    if (value < j + 1 || value > j + 1 + std::min(kl, n - j - 1)) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  return Status::Ok();
}

inline lapack_int* Integers(const LapackWorkspace& workspace, extent_t count,
                            lapack_int& dummy) {
  return count == 0 ? &dummy
                    : ::new (workspace.regions[common::kPivot].data())
                          lapack_int[static_cast<std::size_t>(count)];
}

template <typename T>
Status EstimateWorkspace(extent_t n, LapackWorkspacePlan& plan,
                         bool growth = false) {
  using Real = DenseBlasRealType<T>;
  const extent_t scalar =
      DenseBlasComplex<T> ? 2 * n : std::max<extent_t>(growth, 3 * n);
  const extent_t integer = DenseBlasComplex<T> ? n : 2 * n;
  plan.regions[common::kScalar] = {scalar, scalar, sizeof(T), alignof(T)};
  plan.regions[common::kPivot] = {integer, integer, sizeof(lapack_int),
                                  alignof(lapack_int)};
  if constexpr (DenseBlasComplex<T>) {
    const extent_t real = std::max<extent_t>(growth, n);
    plan.regions[kReal] = {real, real, sizeof(Real), alignof(Real)};
  }
  return common::Total(plan);
}

inline bool Operation(DenseBlasTranspose operation) {
  return operation == DenseBlasTranspose::kNone ||
         operation == DenseBlasTranspose::kTranspose ||
         operation == DenseBlasTranspose::kConjugateTranspose;
}
inline char Transpose(DenseBlasTranspose operation) {
  if (operation == DenseBlasTranspose::kNone) {
    return 'N';
  }
  return operation == DenseBlasTranspose::kTranspose ? 'T' : 'C';
}
template <typename T>
extent_t RhsLeading(DenseBlasMatrixView<T> matrix) {
  return matrix.columns() == 0 ? std::max<extent_t>(1, matrix.rows())
                               : common::Leading(matrix);
}
template <typename T>
Status ZeroDiagonal(LapackLuBandView<const T> factors, LapackReport& report) {
  for (extent_t j = 0; j < factors.rows(); ++j) {
    if (factors.storage().data()[j * factors.storage().leading_dimension() +
                                 factors.lower_bandwidth() +
                                 factors.upper_bandwidth()] == T{}) {
      report.outcome = LapackOutcome::kSingular;
      report.diagnostic_index = j;
      return Status(ErrorCode::kNumerical);
    }
  }
  return Status::Ok();
}
template <typename Real>
Status Errors(DenseBlasVectorView<Real> ferr, DenseBlasVectorView<Real> berr,
              LapackReport& report) {
  index_t nonfinite = -1;
  for (extent_t j = 0; j < ferr.size(); ++j) {
    if (ferr.data()[j] < Real{0} || berr.data()[j] < Real{0}) {
      report.outcome = LapackOutcome::kPartialResult;
      report.output_validity = LapackOutputValidity::kDocumentedPartial;
      report.diagnostic_index = j;
      return Status(ErrorCode::kProvider);
    }
    if (nonfinite < 0 &&
        (!std::isfinite(ferr.data()[j]) || !std::isfinite(berr.data()[j]))) {
      nonfinite = j;
    }
  }
  if (nonfinite >= 0) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = nonfinite;
    return Status(ErrorCode::kNumerical);
  }
  return common::Complete(report);
}

}  // namespace asc::internal_lu_band_expert

#endif  // ASC_DENSE_LAPACK_INTERNAL_LU_BAND_EXPERT_H_
