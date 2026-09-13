#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_CALLS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_CALLS_H_

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "asc/dense/lapack/types.h"
#include "internal_indefinite.h"
#include "internal_indefinite_prototypes.h"

namespace asc::internal_indefinite {

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
    constexpr std::array<std::string_view, 3> kNames{"ssytrf", "ssytf2",
                                                     "ssytrs"};
    return kNames[index];
  } else if constexpr (std::is_same_v<T, double>) {
    constexpr std::array<std::string_view, 3> kNames{"dsytrf", "dsytf2",
                                                     "dsytrs"};
    return kNames[index];
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    constexpr std::array<std::string_view, 3> kSymmetric{"csytrf", "csytf2",
                                                         "csytrs"};
    constexpr std::array<std::string_view, 3> kHermitian{"chetrf", "chetf2",
                                                         "chetrs"};
    return hermitian ? kHermitian[index] : kSymmetric[index];
  } else {
    constexpr std::array<std::string_view, 3> kSymmetric{"zsytrf", "zsytf2",
                                                         "zsytrs"};
    constexpr std::array<std::string_view, 3> kHermitian{"zhetrf", "zhetf2",
                                                         "zhetrs"};
    return hermitian ? kHermitian[index] : kSymmetric[index];
  }
}

template <typename T>
void TrfCall(bool hermitian, char uplo, lapack_int n, T* matrix, lapack_int lda,
             lapack_int* pivots, T* work, lapack_int lwork, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrf(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrf(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chetrf(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
    } else {
      LAPACK_csytrf(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
    }
  } else {
    if (hermitian) {
      LAPACK_zhetrf(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
    } else {
      LAPACK_zsytrf(&uplo, &n, matrix, &lda, pivots, work, &lwork, &info);
    }
  }
}

template <typename T>
void Tf2Call(bool hermitian, char uplo, lapack_int n, T* matrix, lapack_int lda,
             lapack_int* pivots, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    ssytf2_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
  } else if constexpr (std::is_same_v<T, double>) {
    dsytf2_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      chetf2_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
    } else {
      csytf2_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
    }
  } else {
    if (hermitian) {
      zhetf2_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
    } else {
      zsytf2_(&uplo, &n, matrix, &lda, pivots, &info, std::size_t{1});
    }
  }
}

template <typename T>
void TrsCall(bool hermitian, char uplo, lapack_int n, lapack_int nrhs,
             const T* matrix, lapack_int lda, const lapack_int* pivots, T* rhs,
             lapack_int ldb, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrs(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrs(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chetrs(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb, &info);
    } else {
      LAPACK_csytrs(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb, &info);
    }
  } else {
    if (hermitian) {
      LAPACK_zhetrs(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb, &info);
    } else {
      LAPACK_zsytrs(&uplo, &n, &nrhs, matrix, &lda, pivots, rhs, &ldb, &info);
    }
  }
}

}  // namespace asc::internal_indefinite

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_CALLS_H_
