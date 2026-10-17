#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_EXPERT_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_EXPERT_H_

#include <cmath>
#include <complex>
#include <cstddef>
#include <new>  // IWYU pragma: keep; caller-owned provider integer lifetimes.
#include <span>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "internal_indefinite.h"
#include "internal_indefinite_expert_counts.h"

namespace asc::internal_indefinite_expert {

constexpr std::size_t kReal =
    static_cast<std::size_t>(LapackWorkspaceKind::kReal);

template <typename Real>
Status ErrorVector(const ReferenceLapackProvider& provider,
                   DenseBlasVectorView<Real> values, extent_t size) {
  if (values.size() != size) {
    return Status(ErrorCode::kShape);
  }
  if (values.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return internal_indefinite::Accessible(provider, values.reachable_storage());
}

// Unlike the frozen mutable-RHS helper, this also accepts immutable B and
// output-only X. It advances only caller-owned live scalar storage and never
// reads old output entries. Empty foreign-call dummies are selected by callers.
template <typename T>
T* PackFull(DenseBlasMatrixView<T> matrix, std::remove_const_t<T>*& cursor,
            bool read_input) {
  namespace bk = internal_indefinite;
  if (!bk::Packed(matrix) || matrix.rows() == 0 || matrix.columns() == 0) {
    return matrix.data();
  }
  auto* packed = cursor;
  cursor += matrix.rows() * matrix.columns();
  if (read_input) {
    for (extent_t j = 0; j < matrix.columns(); ++j) {
      for (extent_t i = 0; i < matrix.rows(); ++i) {
        packed[j * matrix.rows() + i] = bk::Entry(matrix, i, j);
      }
    }
  }
  return packed;
}

inline Status PivotMetadata(const ReferenceLapackProvider& provider,
                            RawLapackPivotView pivots, extent_t order) {
  if (pivots.family() != LapackFactorFamily::kBunchKaufman) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (pivots.values().size() != static_cast<std::size_t>(order)) {
    return Status(ErrorCode::kShape);
  }
  return internal_indefinite::Accessible(provider, pivots.reachable_storage());
}

template <typename T>
Status EstimatorWork(extent_t order, bool refinement,
                     LapackWorkspacePlan& plan) {
  namespace bk = internal_indefinite;
  Status counts =
      internal_indefinite_expert_counts::Estimator(order, bk::kIntegerLimit);
  if (!counts.ok()) {
    return counts;
  }
  const extent_t scalar = (refinement && !DenseBlasComplex<T> ? 3 : 2) * order;
  const extent_t integers = (DenseBlasComplex<T> ? 1 : 2) * order;
  plan.regions[bk::kScalar] = {scalar, scalar, sizeof(T), alignof(T)};
  // IPIV occupies [0,n). Real IWORK occupies a simultaneous, disjoint
  // [n,2*n) provider-width region. Neither is an ASC index_t reinterpretation.
  plan.regions[bk::kPivot] = {integers, integers, sizeof(lapack_int),
                              alignof(lapack_int)};
  if constexpr (DenseBlasComplex<T>) {
    if (refinement) {
      plan.regions[kReal] = {order, order, sizeof(DenseBlasRealType<T>),
                             alignof(DenseBlasRealType<T>)};
    }
  }
  return bk::Total(plan);
}

inline lapack_int* PreparePivots(RawLapackPivotView pivots,
                                 const LapackWorkspacePlan& plan,
                                 const LapackWorkspace& workspace) {
  namespace bk = internal_indefinite;
  auto* native = ::new (workspace.regions[bk::kPivot].data())
      lapack_int[static_cast<std::size_t>(
          plan.regions[bk::kPivot].minimum_entries)];
  for (std::size_t i = 0; i < pivots.values().size(); ++i) {
    native[i] = static_cast<lapack_int>(pivots.values()[i]);
  }
  return native;
}

template <typename T>
T Conjugate(T value) {
  if constexpr (DenseBlasComplex<T>) {
    return std::conj(value);
  } else {
    return value;
  }
}

// Preserve CON's source early return on any exactly zero 1-by-1 D entry.
// Otherwise reject precisely the divisors that the following TRS would
// evaluate as zero. This is not a broad finiteness/conditioning scan.
template <typename T>
Status BlockDivisors(DenseBlasMatrixView<const T> factors,
                     RawLapackPivotView pivots, DenseBlasTriangle triangle,
                     bool hermitian, bool condition, LapackReport& report) {
  namespace bk = internal_indefinite;
  if (condition) {
    for (extent_t i = 0; i < factors.rows(); ++i) {
      if (pivots.values()[static_cast<std::size_t>(i)] > 0 &&
          bk::Entry(factors, i, i) == T{}) {
        return Status::Ok();
      }
    }
  }
  for (extent_t i = 0; i < factors.rows();) {
    const extent_t first_index = i;
    bool zero = false;
    if (pivots.values()[static_cast<std::size_t>(i)] > 0) {
      zero = hermitian ? bk::Real(bk::Entry(factors, i, i)) == 0
                       : bk::Entry(factors, i, i) == T{};
      ++i;
    } else {
      const T off = triangle == DenseBlasTriangle::kUpper
                        ? bk::Entry(factors, i, i + 1)
                        : bk::Entry(factors, i + 1, i);
      zero = off == T{};
      if (!zero) {
        const T first = hermitian && triangle == DenseBlasTriangle::kLower
                            ? Conjugate(off)
                            : off;
        const T second = hermitian && triangle == DenseBlasTriangle::kUpper
                             ? Conjugate(off)
                             : off;
        zero = (bk::Entry(factors, i, i) / first) *
                       (bk::Entry(factors, i + 1, i + 1) / second) -
                   T{1} ==
               T{};
      }
      i += 2;
    }
    if (zero) {
      report.outcome = LapackOutcome::kSingular;
      report.diagnostic_index = first_index;
      return Status(ErrorCode::kNumerical);
    }
  }
  return Status::Ok();
}

template <typename Real>
Status Estimate(Real value, LapackReport& report) {
  if (value < 0) {
    return internal_indefinite::Defect(0, report);
  }
  if (!std::isfinite(value)) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return internal_indefinite::Complete(report);
}

}  // namespace asc::internal_indefinite_expert

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_EXPERT_H_
