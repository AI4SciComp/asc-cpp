#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_CONDITION_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_CONDITION_NATIVE_H_
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
namespace asc_rk_condition_test {
template <class T>
void Native(bool he, const char* u, const lapack_int* n, const T* a,
            const lapack_int* lda, const T* e, const lapack_int* piv,
            const asc::DenseBlasRealType<T>* norm,
            asc::DenseBlasRealType<T>* rcond, T* work, lapack_int* iw,
            lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssycon_3(u, n, a, lda, e, piv, norm, rcond, work, iw, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsycon_3(u, n, a, lda, e, piv, norm, rcond, work, iw, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_checon_3(u, n, a, lda, e, piv, norm, rcond, work, info);
    } else {
      LAPACK_csycon_3(u, n, a, lda, e, piv, norm, rcond, work, info);
    }
  } else {
    if (he) {
      LAPACK_zhecon_3(u, n, a, lda, e, piv, norm, rcond, work, info);
    } else {
      LAPACK_zsycon_3(u, n, a, lda, e, piv, norm, rcond, work, info);
    }
  }
}
}  // namespace asc_rk_condition_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_CONDITION_NATIVE_H_
