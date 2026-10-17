#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_INVERSE_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_INVERSE_NATIVE_H_
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite_rk_inverse_prototypes.h"
#include "indefinite_rk_inverse_test_support.h"
namespace asc_rk_inverse_test {
template <class T>
void NativeCall(bool he, bool explicit_block, char* u, lapack_int* n, T* a,
                lapack_int* lda, T* e, lapack_int* ip, T* w, lapack_int* size,
                lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    if (explicit_block) {
      ssytri_3x_(u, n, a, lda, e, ip, w, size, info, 1);
    } else {
      LAPACK_ssytri_3(u, n, a, lda, e, ip, w, size, info);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    if (explicit_block) {
      dsytri_3x_(u, n, a, lda, e, ip, w, size, info, 1);
    } else {
      LAPACK_dsytri_3(u, n, a, lda, e, ip, w, size, info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      if (explicit_block) {
        chetri_3x_(u, n, a, lda, e, ip, w, size, info, 1);
      } else {
        LAPACK_chetri_3(u, n, a, lda, e, ip, w, size, info);
      }
    } else {
      if (explicit_block) {
        csytri_3x_(u, n, a, lda, e, ip, w, size, info, 1);
      } else {
        LAPACK_csytri_3(u, n, a, lda, e, ip, w, size, info);
      }
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      if (explicit_block) {
        zhetri_3x_(u, n, a, lda, e, ip, w, size, info, 1);
      } else {
        LAPACK_zhetri_3(u, n, a, lda, e, ip, w, size, info);
      }
    } else {
      if (explicit_block) {
        zsytri_3x_(u, n, a, lda, e, ip, w, size, info, 1);
      } else {
        LAPACK_zsytri_3(u, n, a, lda, e, ip, w, size, info);
      }
    }
  }
}
template <typename T>
void Native(bool hermitian, const char* triangle, const lapack_int* n, T* a,
            const lapack_int* lda, const T* e, lapack_int* pivots, T* work,
            lapack_int* info) {
  char uplo = *triangle;
  lapack_int order = *n;
  lapack_int leading = *lda;
  lapack_int size = static_cast<lapack_int>(
      g_block_size == 0 ? Entries<T>(order, hermitian) : g_block_size);
  NativeCall(hermitian, g_block_size != 0, &uplo, &order, a, &leading,
             const_cast<T*>(e), pivots, work, &size, info);
}
}  // namespace asc_rk_inverse_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_INVERSE_NATIVE_H_
