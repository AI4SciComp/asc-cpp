#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_REFINEMENT_OPERANDS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_REFINEMENT_OPERANDS_H_

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_packed_refinement.h"
#include "indefinite_packed_refinement_test_support.h"

namespace asc_packed_refinement_test {
// Views are separately replaceable for validation and shared-input workers.
template <typename T>
struct Operands {
  using Real = asc::DenseBlasRealType<T>;
  asc::DenseBlasPackedMatrixView<const T> a;
  asc::DenseBlasPackedMatrixView<const T> af;
  asc::RawLapackPivotView pivots;
  asc::DenseBlasMatrixView<const T> b;
  asc::DenseBlasMatrixView<T> x;
  asc::DenseBlasVectorView<Real> ferr;
  asc::DenseBlasVectorView<Real> berr;
  asc::DenseBlasTriangle triangle;
  bool hermitian;

  explicit Operands(Fixture<T>& sample)
      : a(sample.Original()),
        af(sample.system.a.ConstView()),
        pivots(sample.system.Pivots()),
        b(sample.Rhs()),
        x(sample.Solution()),
        ferr(sample.Forward()),
        berr(sample.Backward()),
        triangle(sample.system.a.triangle),
        hermitian(sample.system.a.hermitian) {}

  auto Query(const asc::ReferenceLapackProvider& provider) const {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (hermitian) {
        return asc::QueryHprfsWorkspace(provider, triangle, a, af, pivots, b, x,
                                        ferr, berr);
      }
    }
    return asc::QuerySprfsWorkspace(provider, triangle, a, af, pivots, b, x,
                                    ferr, berr);
  }
  asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                      const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& work,
                      asc::LapackReport& report) const {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (hermitian) {
        return asc::Hprfs(provider, triangle, a, af, pivots, b, x, ferr, berr,
                          plan, work, report);
      }
    }
    return asc::Sprfs(provider, triangle, a, af, pivots, b, x, ferr, berr, plan,
                      work, report);
  }
};
}  // namespace asc_packed_refinement_test
#endif
