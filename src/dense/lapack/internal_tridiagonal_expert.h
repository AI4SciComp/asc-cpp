#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIDIAGONAL_EXPERT_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIDIAGONAL_EXPERT_H_

#include <array>
#include <cstdint>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_tridiagonal.h"
#include "internal_layout.h"
#include "internal_tridiagonal.h"
#include "internal_tridiagonal_counts.h"

namespace asc::internal_tridiagonal {

inline Status PivotMetadata(const ReferenceLapackProvider& provider,
                            DenseBlasVectorView<index_t> pivots,
                            extent_t order) {
  return Vector(provider, pivots, order);
}

template <typename T, typename FactorElement, typename Pivot>
auto ExpertOperands(LapackTridiagonalView<const T> original,
                    LapackTridiagonalLuStorage<FactorElement> factors,
                    Pivot pivots, DenseBlasMatrixView<const T> rhs,
                    DenseBlasMatrixView<T> solution,
                    DenseBlasVectorView<DenseBlasRealType<T>> forward_error,
                    DenseBlasVectorView<DenseBlasRealType<T>> backward_error) {
  const auto a = Spans(original);
  const auto f = Spans(factors);
  return std::array{a[0],
                    a[1],
                    a[2],
                    f[0],
                    f[1],
                    f[2],
                    f[3],
                    pivots.reachable_storage(),
                    rhs.reachable_storage(),
                    solution.reachable_storage(),
                    forward_error.reachable_storage(),
                    backward_error.reachable_storage()};
}

template <typename T, typename FactorElement, typename Pivot>
Result<LapackWorkspacePlan> QueryExpert(
    const ReferenceLapackProvider& provider, std::string_view routine,
    DenseBlasTranspose transpose, std::int64_t branch,
    bool estimate_without_rhs, LapackTridiagonalView<const T> original,
    LapackTridiagonalLuStorage<FactorElement> factors, Pivot pivots,
    DenseBlasMatrixView<const T> rhs, DenseBlasMatrixView<T> solution,
    DenseBlasVectorView<DenseBlasRealType<T>> forward_error,
    DenseBlasVectorView<DenseBlasRealType<T>> backward_error) {
  const extent_t order = original.order();
  if (factors.primary().order() != order ||
      rhs.columns() != solution.columns()) {
    return Status(ErrorCode::kShape);
  }
  Status status = Transpose(transpose);
  if (!status.ok()) {
    return status;
  }
  status = Storage(provider, original);
  if (!status.ok()) {
    return status;
  }
  status = Storage(provider, factors);
  if (!status.ok()) {
    return status;
  }
  status = PivotMetadata(provider, pivots, order);
  if (!status.ok()) {
    return status;
  }
  status = Matrix(provider, rhs, order);
  if (!status.ok()) {
    return status;
  }
  status = Matrix(provider, solution, order);
  if (!status.ok()) {
    return status;
  }
  status = Vector(provider, forward_error, rhs.columns());
  if (!status.ok()) {
    return status;
  }
  status = Vector(provider, backward_error, rhs.columns());
  if (!status.ok()) {
    return status;
  }
  status = Disjoint(ExpertOperands(original, factors, pivots, rhs, solution,
                                   forward_error, backward_error));
  if (!status.ok()) {
    return status;
  }
  status = internal_tridiagonal_counts::Expert(order, rhs.columns(),
                                               estimate_without_rhs, kLimit);
  if (!status.ok()) {
    return status;
  }
  const auto identity = LapackPlanIdentity::Create(
      routine, Kind<T>(),
      std::array{order, rhs.columns(), Leading(rhs), Leading(solution)},
      std::array<std::int64_t, 6>{static_cast<std::int64_t>(transpose), branch,
                                  static_cast<std::int64_t>(rhs.layout()),
                                  rhs.leading_dimension(),
                                  static_cast<std::int64_t>(solution.layout()),
                                  solution.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (order != 0 && (estimate_without_rhs || rhs.columns() != 0)) {
    status = AddEstimator<T>(order, true, plan);
    if (status.ok()) {
      status = internal_lapack_layout::AddPacking(rhs, plan);
    }
    if (status.ok()) {
      status = internal_lapack_layout::AddPacking(solution, plan);
    }
  }
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

// A driver's output-only X must not be packed by reading its ignored input.
template <typename T>
T* Output(DenseBlasMatrixView<T> matrix, T*& cursor) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor || matrix.rows() == 0 ||
      matrix.columns() == 0) {
    return matrix.data();
  }
  T* result = cursor;
  cursor += matrix.rows() * matrix.columns();
  return result;
}

template <typename Real>
void EmptyErrors(DenseBlasVectorView<Real> forward_error,
                 DenseBlasVectorView<Real> backward_error) {
  for (extent_t i = 0; i < forward_error.size(); ++i) {
    forward_error.data()[i] = 0;
    backward_error.data()[i] = 0;
  }
}

template <typename Real>
Status Errors(DenseBlasVectorView<Real> forward_error,
              DenseBlasVectorView<Real> backward_error, LapackReport& report) {
  Status result = Status::Ok();
  for (extent_t i = 0; i < forward_error.size(); ++i) {
    for (const Real value :
         {forward_error.data()[i], backward_error.data()[i]}) {
      const Status status = Diagnostic(value, report);
      if (status.code() == ErrorCode::kProvider) {
        return status;
      }
      if (!status.ok()) {
        result = status;
      }
    }
  }
  return result;
}

}  // namespace asc::internal_tridiagonal

#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIDIAGONAL_EXPERT_H_
