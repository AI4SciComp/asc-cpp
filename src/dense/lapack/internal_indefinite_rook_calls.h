#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_CALLS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_CALLS_H_

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "asc/dense/lapack/types.h"
#include "internal_indefinite.h"
#include "internal_indefinite_rook_prototypes.h"

namespace asc::internal_indefinite_rook {

enum class Routine : std::uint8_t { kTrf, kTf2, kTrs };

template <typename T>
constexpr LapackScalarKind ScalarKind() {
  if constexpr (std::is_same_v<T, float>) {
    return LapackScalarKind::kF32;
  } else if constexpr (std::is_same_v<T, double>) {
    return LapackScalarKind::kF64;
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return LapackScalarKind::kC64;
  } else {
    return LapackScalarKind::kC128;
  }
}

template <typename T>
std::string_view Name(Routine routine, bool hermitian) {
  const auto index = static_cast<std::size_t>(routine);
  if constexpr (std::is_same_v<T, float>) {
    constexpr std::array<std::string_view, 3> kNames{
        "ssytrf_rook", "ssytf2_rook", "ssytrs_rook"};
    return kNames[index];
  } else if constexpr (std::is_same_v<T, double>) {
    constexpr std::array<std::string_view, 3> kNames{
        "dsytrf_rook", "dsytf2_rook", "dsytrs_rook"};
    return kNames[index];
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    constexpr std::array<std::string_view, 3> kSymmetric{
        "csytrf_rook", "csytf2_rook", "csytrs_rook"};
    constexpr std::array<std::string_view, 3> kHermitian{
        "chetrf_rook", "chetf2_rook", "chetrs_rook"};
    return hermitian ? kHermitian[index] : kSymmetric[index];
  } else {
    constexpr std::array<std::string_view, 3> kSymmetric{
        "zsytrf_rook", "zsytf2_rook", "zsytrs_rook"};
    constexpr std::array<std::string_view, 3> kHermitian{
        "zhetrf_rook", "zhetf2_rook", "zhetrs_rook"};
    return hermitian ? kHermitian[index] : kSymmetric[index];
  }
}

template <typename T>
void TrfCall(bool hermitian, char uplo, lapack_int n, T* matrix, lapack_int lda,
             lapack_int* pivots, T* work, lapack_int lwork, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrf_rook(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrf_rook(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chetrf_rook(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
    } else {
      LAPACK_csytrf_rook(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
    }
  } else {
    if (hermitian) {
      LAPACK_zhetrf_rook(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
    } else {
      LAPACK_zsytrf_rook(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
    }
  }
}

template <typename T>
void Tf2Call(bool hermitian, char uplo, lapack_int n, T* matrix, lapack_int lda,
             lapack_int* pivots, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    ssytf2_rook_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
  } else if constexpr (std::is_same_v<T, double>) {
    dsytf2_rook_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      chetf2_rook_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
    } else {
      csytf2_rook_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
    }
  } else {
    if (hermitian) {
      zhetf2_rook_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
    } else {
      zsytf2_rook_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
    }
  }
}

template <typename T>
void TrsCall(bool hermitian, char uplo, lapack_int n, lapack_int nrhs,
             const T* matrix, lapack_int lda, const lapack_int* pivots, T* rhs,
             lapack_int ldb, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrs_rook(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb,
                       &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrs_rook(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb,
                       &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chetrs_rook(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb,
                         &info);
    } else {
      LAPACK_csytrs_rook(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb,
                         &info);
    }
  } else {
    if (hermitian) {
      LAPACK_zhetrs_rook(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb,
                         &info);
    } else {
      LAPACK_zsytrs_rook(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb,
                         &info);
    }
  }
}

}  // namespace asc::internal_indefinite_rook

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_CALLS_H_
