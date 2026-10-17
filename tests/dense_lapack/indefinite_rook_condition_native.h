#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_CONDITION_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_CONDITION_NATIVE_H_

#include <complex>
#include <cstddef>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rook_condition_prototypes.h"
#include "asc/dense/blas.h"

namespace asc_rook_condition_test {
// Test-only direct calls retain the actual addresses of guarded ABI objects.
template <typename T>
void Native(bool hermitian, char* uplo, lapack_int* n, T* a, lapack_int* lda,
            lapack_int* pivots, asc::DenseBlasRealType<T>* norm,
            asc::DenseBlasRealType<T>* condition, T* work, lapack_int* iwork,
            lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    ssycon_rook_(uplo, n, a, lda, pivots, norm, condition, work, iwork, info,
                 std::size_t{1});
  } else if constexpr (std::is_same_v<T, double>) {
    dsycon_rook_(uplo, n, a, lda, pivots, norm, condition, work, iwork, info,
                 std::size_t{1});
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    const auto call = hermitian ? checon_rook_ : csycon_rook_;
    call(uplo, n, a, lda, pivots, norm, condition, work, info, std::size_t{1});
  } else {
    const auto call = hermitian ? zhecon_rook_ : zsycon_rook_;
    call(uplo, n, a, lda, pivots, norm, condition, work, info, std::size_t{1});
  }
}
}  // namespace asc_rook_condition_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_CONDITION_NATIVE_H_
