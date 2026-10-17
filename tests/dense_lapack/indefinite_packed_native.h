#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_NATIVE_H_
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_packed_indefinite_test {
template <typename T>
void Native(bool he, const char* uplo, const lapack_int* n, T* a,
            lapack_int* pivots, lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssptrf(uplo, n, a, pivots, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsptrf(uplo, n, a, pivots, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chptrf(uplo, n, a, pivots, info);
    } else {
      LAPACK_csptrf(uplo, n, a, pivots, info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhptrf(uplo, n, a, pivots, info);
    } else {
      LAPACK_zsptrf(uplo, n, a, pivots, info);
    }
  }
}
}  // namespace asc_packed_indefinite_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_NATIVE_H_
