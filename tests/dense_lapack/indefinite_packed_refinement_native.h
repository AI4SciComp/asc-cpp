#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_REFINEMENT_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_REFINEMENT_NATIVE_H_
#include <complex>
#include <type_traits>

#include "asc/dense/blas.h"
#include "src/dense/lapack/internal_indefinite.h"
namespace asc_packed_refinement_test {
template <typename T>
void Native(bool he, const char* triangle, const lapack_int* n,
            const lapack_int* nrhs, const T* a, const T* factors,
            const lapack_int* pivots, const T* b, const lapack_int* ldb, T* x,
            const lapack_int* ldx, asc::DenseBlasRealType<T>* ferr,
            asc::DenseBlasRealType<T>* berr, T* work,
            asc::DenseBlasRealType<T>* rwork, lapack_int* iwork,
            lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssprfs(triangle, n, nrhs, a, factors, pivots, b, ldb, x, ldx, ferr,
                  berr, work, iwork, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsprfs(triangle, n, nrhs, a, factors, pivots, b, ldb, x, ldx, ferr,
                  berr, work, iwork, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chprfs(triangle, n, nrhs, a, factors, pivots, b, ldb, x, ldx, ferr,
                    berr, work, rwork, info);
    } else {
      LAPACK_csprfs(triangle, n, nrhs, a, factors, pivots, b, ldb, x, ldx, ferr,
                    berr, work, rwork, info);
    }
  } else {
    if (he) {
      LAPACK_zhprfs(triangle, n, nrhs, a, factors, pivots, b, ldb, x, ldx, ferr,
                    berr, work, rwork, info);
    } else {
      LAPACK_zsprfs(triangle, n, nrhs, a, factors, pivots, b, ldb, x, ldx, ferr,
                    berr, work, rwork, info);
    }
  }
}
}  // namespace asc_packed_refinement_test
#endif
