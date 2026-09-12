#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_NATIVE_H_
#include <complex>
#include <cstddef>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rk_prototypes.h"
namespace asc_rk_test {
inline void NativeTf2([[maybe_unused]] bool hermitian, char* uplo,
                      lapack_int* n, float* a, lapack_int* lda, float* e,
                      lapack_int* pivots, lapack_int* info) {
  const auto call = ssytf2_rk_;
  call(uplo, n, a, lda, e, pivots, info, std::size_t{1});
}
inline void NativeTrf([[maybe_unused]] bool hermitian, char* uplo,
                      lapack_int* n, float* a, lapack_int* lda, float* e,
                      lapack_int* pivots, float* work, lapack_int* lwork,
                      lapack_int* info) {
  const auto call = LAPACK_ssytrf_rk_base;
  call(uplo, n, a, lda, e, pivots, work, lwork, info, std::size_t{1});
}
inline void NativeTf2([[maybe_unused]] bool hermitian, char* uplo,
                      lapack_int* n, double* a, lapack_int* lda, double* e,
                      lapack_int* pivots, lapack_int* info) {
  const auto call = dsytf2_rk_;
  call(uplo, n, a, lda, e, pivots, info, std::size_t{1});
}
inline void NativeTrf([[maybe_unused]] bool hermitian, char* uplo,
                      lapack_int* n, double* a, lapack_int* lda, double* e,
                      lapack_int* pivots, double* work, lapack_int* lwork,
                      lapack_int* info) {
  const auto call = LAPACK_dsytrf_rk_base;
  call(uplo, n, a, lda, e, pivots, work, lwork, info, std::size_t{1});
}
inline void NativeTf2([[maybe_unused]] bool hermitian, char* uplo,
                      lapack_int* n, std::complex<float>* a, lapack_int* lda,
                      std::complex<float>* e, lapack_int* pivots,
                      lapack_int* info) {
  const auto call = hermitian ? chetf2_rk_ : csytf2_rk_;
  call(uplo, n, a, lda, e, pivots, info, std::size_t{1});
}
inline void NativeTrf([[maybe_unused]] bool hermitian, char* uplo,
                      lapack_int* n, std::complex<float>* a, lapack_int* lda,
                      std::complex<float>* e, lapack_int* pivots,
                      std::complex<float>* work, lapack_int* lwork,
                      lapack_int* info) {
  const auto call = hermitian ? LAPACK_chetrf_rk_base : LAPACK_csytrf_rk_base;
  call(uplo, n, a, lda, e, pivots, work, lwork, info, std::size_t{1});
}
inline void NativeTf2([[maybe_unused]] bool hermitian, char* uplo,
                      lapack_int* n, std::complex<double>* a, lapack_int* lda,
                      std::complex<double>* e, lapack_int* pivots,
                      lapack_int* info) {
  const auto call = hermitian ? zhetf2_rk_ : zsytf2_rk_;
  call(uplo, n, a, lda, e, pivots, info, std::size_t{1});
}
inline void NativeTrf([[maybe_unused]] bool hermitian, char* uplo,
                      lapack_int* n, std::complex<double>* a, lapack_int* lda,
                      std::complex<double>* e, lapack_int* pivots,
                      std::complex<double>* work, lapack_int* lwork,
                      lapack_int* info) {
  const auto call = hermitian ? LAPACK_zhetrf_rk_base : LAPACK_zsytrf_rk_base;
  call(uplo, n, a, lda, e, pivots, work, lwork, info, std::size_t{1});
}
}  // namespace asc_rk_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_NATIVE_H_
