#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_INVERSE_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_INVERSE_NATIVE_H_

#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"

namespace asc_packed_inverse_test {
template <typename T>
void Native(bool hermitian, const char* triangle, const lapack_int* n, T* a,
            const lapack_int* pivots, T* work, lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssptri(triangle, n, a, pivots, work, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsptri(triangle, n, a, pivots, work, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chptri(triangle, n, a, pivots, work, info);
    } else {
      LAPACK_csptri(triangle, n, a, pivots, work, info);
    }
  } else {
    if (hermitian) {
      LAPACK_zhptri(triangle, n, a, pivots, work, info);
    } else {
      LAPACK_zsptri(triangle, n, a, pivots, work, info);
    }
  }
}
}  // namespace asc_packed_inverse_test

#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_INVERSE_NATIVE_H_
