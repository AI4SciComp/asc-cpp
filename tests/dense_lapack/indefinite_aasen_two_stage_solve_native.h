#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_NATIVE_H_
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_aasen_two_stage_solve_test {
template <typename T>
void NativePointers(bool he, const char* tri, const lapack_int* n,
                    const lapack_int* nrhs, const T* a, const lapack_int* lda,
                    const T* band, const lapack_int* ltb, const lapack_int* p,
                    const lapack_int* q, T* b, const lapack_int* ldb,
                    lapack_int* info) {
  auto* tb = const_cast<T*>(band);
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrs_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrs_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chetrs_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                              info);
    } else {
      LAPACK_csytrs_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                              info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhetrs_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                              info);
    } else {
      LAPACK_zsytrs_aa_2stage(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                              info);
    }
  }
}
template <typename T>
void Native(bool he, char tri, lapack_int n, lapack_int nrhs, const T* a,
            lapack_int lda, const T* tb, lapack_int ltb, const lapack_int* p,
            const lapack_int* q, T* b, lapack_int ldb, lapack_int& info) {
  NativePointers(he, &tri, &n, &nrhs, a, &lda, tb, &ltb, p, q, b, &ldb, &info);
}
}  // namespace asc_aasen_two_stage_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_NATIVE_H_
