#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_INVERSE_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_INVERSE_NATIVE_H_
#include <algorithm>
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "indefinite_block_inverse_test_support.h"
namespace asc_block_inverse_test {
template <typename T>
void NativeWithSize(bool hermitian, const char* uplo, const lapack_int* n, T* a,
                    const lapack_int* lda, const lapack_int* pivots, T* work,
                    const lapack_int* size, lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    if (g_block_size) {
      LAPACK_ssytri2x(uplo, n, a, lda, pivots, work, size, info);
    } else {
      LAPACK_ssytri2(uplo, n, a, lda, pivots, work, size, info);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    if (g_block_size) {
      LAPACK_dsytri2x(uplo, n, a, lda, pivots, work, size, info);
    } else {
      LAPACK_dsytri2(uplo, n, a, lda, pivots, work, size, info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      if (g_block_size) {
        LAPACK_chetri2x(uplo, n, a, lda, pivots, work, size, info);
      } else {
        LAPACK_chetri2(uplo, n, a, lda, pivots, work, size, info);
      }
    } else {
      if (g_block_size) {
        LAPACK_csytri2x(uplo, n, a, lda, pivots, work, size, info);
      } else {
        LAPACK_csytri2(uplo, n, a, lda, pivots, work, size, info);
      }
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (hermitian) {
      if (g_block_size) {
        LAPACK_zhetri2x(uplo, n, a, lda, pivots, work, size, info);
      } else {
        LAPACK_zhetri2(uplo, n, a, lda, pivots, work, size, info);
      }
    } else {
      if (g_block_size) {
        LAPACK_zsytri2x(uplo, n, a, lda, pivots, work, size, info);
      } else {
        LAPACK_zsytri2(uplo, n, a, lda, pivots, work, size, info);
      }
    }
  }
}
template <typename T>
void Native(bool hermitian, const char* uplo, const lapack_int* n, T* a,
            const lapack_int* lda, const lapack_int* pivots, T* work,
            lapack_int* info) {
  const auto size = static_cast<lapack_int>(
      g_block_size != 0
          ? g_block_size
          : std::max<asc::extent_t>(1, Entries<T>(*n, hermitian)));
  NativeWithSize(hermitian, uplo, n, a, lda, pivots, work, &size, info);
}
}  // namespace asc_block_inverse_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_INVERSE_NATIVE_H_
