#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_SOLVE_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_SOLVE_NATIVE_H_

#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"

namespace asc_packed_solve_test {
template <typename T>
void Native(bool hermitian, const char* triangle, const lapack_int* n,
            const lapack_int* nrhs, const T* a, const lapack_int* pivots, T* b,
            const lapack_int* ldb, lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssptrs(triangle, n, nrhs, a, pivots, b, ldb, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsptrs(triangle, n, nrhs, a, pivots, b, ldb, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chptrs(triangle, n, nrhs, a, pivots, b, ldb, info);
    } else {
      LAPACK_csptrs(triangle, n, nrhs, a, pivots, b, ldb, info);
    }
  } else {
    if (hermitian) {
      LAPACK_zhptrs(triangle, n, nrhs, a, pivots, b, ldb, info);
    } else {
      LAPACK_zsptrs(triangle, n, nrhs, a, pivots, b, ldb, info);
    }
  }
}
}  // namespace asc_packed_solve_test

#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_SOLVE_NATIVE_H_
