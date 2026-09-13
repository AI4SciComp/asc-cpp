#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_RK_CALLS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_RK_CALLS_H_

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "asc/dense/lapack/types.h"
#include "internal_indefinite.h"
#include "internal_indefinite_rk_prototypes.h"

namespace asc::internal_indefinite_rk {

enum class Routine : std::uint8_t { kTrf, kTf2 };

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
    constexpr std::array<std::string_view, 2> kNames{"ssytrf_rk", "ssytf2_rk"};
    return kNames[index];
  } else if constexpr (std::is_same_v<T, double>) {
    constexpr std::array<std::string_view, 2> kNames{"dsytrf_rk", "dsytf2_rk"};
    return kNames[index];
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    constexpr std::array<std::string_view, 2> kSymmetric{"csytrf_rk",
                                                         "csytf2_rk"};
    constexpr std::array<std::string_view, 2> kHermitian{"chetrf_rk",
                                                         "chetf2_rk"};
    return hermitian ? kHermitian[index] : kSymmetric[index];
  } else {
    constexpr std::array<std::string_view, 2> kSymmetric{"zsytrf_rk",
                                                         "zsytf2_rk"};
    constexpr std::array<std::string_view, 2> kHermitian{"zhetrf_rk",
                                                         "zhetf2_rk"};
    return hermitian ? kHermitian[index] : kSymmetric[index];
  }
}

template <typename T>
void TrfCall(bool hermitian, char uplo, lapack_int n, T* matrix, lapack_int lda,
             T* off_diagonal, lapack_int* pivots, T* work, lapack_int lwork,
             lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrf_rk(&uplo, &n, matrix, &lda, off_diagonal, pivots, work,
                     &lwork, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrf_rk(&uplo, &n, matrix, &lda, off_diagonal, pivots, work,
                     &lwork, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chetrf_rk(&uplo, &n, matrix, &lda, off_diagonal, pivots, work,
                       &lwork, &info);
    } else {
      LAPACK_csytrf_rk(&uplo, &n, matrix, &lda, off_diagonal, pivots, work,
                       &lwork, &info);
    }
  } else {
    if (hermitian) {
      LAPACK_zhetrf_rk(&uplo, &n, matrix, &lda, off_diagonal, pivots, work,
                       &lwork, &info);
    } else {
      LAPACK_zsytrf_rk(&uplo, &n, matrix, &lda, off_diagonal, pivots, work,
                       &lwork, &info);
    }
  }
}

template <typename T>
void Tf2Call(bool hermitian, char uplo, lapack_int n, T* matrix, lapack_int lda,
             T* off_diagonal, lapack_int* pivots, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    ssytf2_rk_(&uplo, &n, matrix, &lda, off_diagonal, pivots, &info,
               std::size_t{1});
  } else if constexpr (std::is_same_v<T, double>) {
    dsytf2_rk_(&uplo, &n, matrix, &lda, off_diagonal, pivots, &info,
               std::size_t{1});
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      chetf2_rk_(&uplo, &n, matrix, &lda, off_diagonal, pivots, &info,
                 std::size_t{1});
    } else {
      csytf2_rk_(&uplo, &n, matrix, &lda, off_diagonal, pivots, &info,
                 std::size_t{1});
    }
  } else {
    if (hermitian) {
      zhetf2_rk_(&uplo, &n, matrix, &lda, off_diagonal, pivots, &info,
                 std::size_t{1});
    } else {
      zsytf2_rk_(&uplo, &n, matrix, &lda, off_diagonal, pivots, &info,
                 std::size_t{1});
    }
  }
}

}  // namespace asc::internal_indefinite_rk

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_RK_CALLS_H_
