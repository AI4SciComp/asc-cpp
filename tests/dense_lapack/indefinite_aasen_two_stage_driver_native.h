#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_NATIVE_H_
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_aasen_two_stage_driver_test {
template <typename T>
void NativePointers(bool he, const char* tri, const lapack_int* n,
                    const lapack_int* nrhs, T* a, const lapack_int* lda, T* tb,
                    const lapack_int* ltb, lapack_int* p, lapack_int* q, T* b,
                    const lapack_int* ldb, T* work, const lapack_int* lwork,
                    lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssysv_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb, work,
                           lwork, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsysv_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb, work,
                           lwork, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chesv_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb, work,
                             lwork, info);
    } else {
      LAPACK_csysv_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb, work,
                             lwork, info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhesv_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb, work,
                             lwork, info);
    } else {
      LAPACK_zsysv_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb, work,
                             lwork, info);
    }
  }
}
}  // namespace asc_aasen_two_stage_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_NATIVE_H_
