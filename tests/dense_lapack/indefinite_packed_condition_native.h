#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_CONDITION_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_CONDITION_NATIVE_H_
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
namespace asc_packed_condition_test {
template <typename T>
void Native(bool he, const char* triangle, const lapack_int* n, const T* a,
            const lapack_int* pivots, const asc::DenseBlasRealType<T>* norm,
            asc::DenseBlasRealType<T>* rcond, T* work, lapack_int* iwork,
            lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sspcon(triangle, n, a, pivots, norm, rcond, work, iwork, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dspcon(triangle, n, a, pivots, norm, rcond, work, iwork, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chpcon(triangle, n, a, pivots, norm, rcond, work, info);
    } else {
      LAPACK_cspcon(triangle, n, a, pivots, norm, rcond, work, info);
    }
  } else {
    if (he) {
      LAPACK_zhpcon(triangle, n, a, pivots, norm, rcond, work, info);
    } else {
      LAPACK_zspcon(triangle, n, a, pivots, norm, rcond, work, info);
    }
  }
}

}  // namespace asc_packed_condition_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_CONDITION_NATIVE_H_
