#if defined(ASC_RK_SOLVE_EMITTED)
#include ASC_RK_SOLVE_SSYTRS_3_HEADER
#include ASC_RK_SOLVE_DSYTRS_3_HEADER
#include ASC_RK_SOLVE_CSYTRS_3_HEADER
#include ASC_RK_SOLVE_ZSYTRS_3_HEADER
#include ASC_RK_SOLVE_CHETRS_3_HEADER
#include ASC_RK_SOLVE_ZHETRS_3_HEADER
#undef ssytrs_3_
#undef dsytrs_3_
#undef csytrs_3_
#undef zsytrs_3_
#undef chetrs_3_
#undef zhetrs_3_
#endif
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
#include "installed_lu/normal_return_guard.h"
template <class T>
struct Arg {
  using Type = T;
};
template <class T>
struct Arg<T*> {
  using Type = std::remove_const_t<T>*;
};
template <class T>
struct Sig;
template <class R, class... A>
struct Sig<R(A...)> {
  using Type = R(typename Arg<A>::Type...);
};
#if defined(ASC_RK_SOLVE_EMITTED)
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_ssytrs_3_base)>::Type,
                             typename Sig<decltype(emitted_ssytrs_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_dsytrs_3_base)>::Type,
                             typename Sig<decltype(emitted_dsytrs_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_csytrs_3_base)>::Type,
                             typename Sig<decltype(emitted_csytrs_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zsytrs_3_base)>::Type,
                             typename Sig<decltype(emitted_zsytrs_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_chetrs_3_base)>::Type,
                             typename Sig<decltype(emitted_chetrs_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zhetrs_3_base)>::Type,
                             typename Sig<decltype(emitted_zhetrs_3)>::Type>);
#endif
template <class T>
T Value(long double x, long double y = 0) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return T{static_cast<asc::DenseBlasRealType<T>>(x),
             static_cast<asc::DenseBlasRealType<T>>(y)};
  } else {
    return static_cast<T>(x);
  }
}
template <class T>
T Adj(T x, bool hermitian) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return hermitian ? std::conj(x) : x;
  } else {
    return x;
  }
}
template <class T>
void Call(bool hermitian, const char* uplo, const lapack_int* n,
          const lapack_int* nrhs, const T* a, const lapack_int* lda,
          const T* extra, const lapack_int* piv, T* b, const lapack_int* ldb,
          lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrs_3(uplo, n, nrhs, a, lda, extra, piv, b, ldb, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrs_3(uplo, n, nrhs, a, lda, extra, piv, b, ldb, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chetrs_3(uplo, n, nrhs, a, lda, extra, piv, b, ldb, info);
    } else {
      LAPACK_csytrs_3(uplo, n, nrhs, a, lda, extra, piv, b, ldb, info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (hermitian) {
      LAPACK_zhetrs_3(uplo, n, nrhs, a, lda, extra, piv, b, ldb, info);
    } else {
      LAPACK_zsytrs_3(uplo, n, nrhs, a, lda, extra, piv, b, ldb, info);
    }
  }
}
// Input immutability includes NaN payloads and signed zero, not value equality.
bool SameRepresentation(const void* a, const void* b, std::size_t bytes) {
  return std::memcmp(a, b, bytes) == 0;
}
template <class T>
T Expected(lapack_int i, lapack_int j) {
  return Value<T>(i + 1 + j, static_cast<long double>(i - j) / 4);
}
template <class T>
bool CheckRhs(const std::array<T, 14>& b, const std::array<T, 14>& before_b,
              lapack_int order, lapack_int rhs) {
  using R = asc::DenseBlasRealType<T>;
  bool pass = true;
  for (std::size_t k = 0; k < b.size(); ++k) {
    const int i = (static_cast<int>(k) - 1) % 4;
    const int j = (static_cast<int>(k) - 1) / 4;
    if (k != 0 && i < order && j < rhs) {
      pass = std::abs(b[k] - Expected<T>(i, j)) <=
                 32 * std::numeric_limits<R>::epsilon() *
                     std::abs(Expected<T>(i, j)) &&
             pass;
    } else {
      pass = (b[k] == before_b[k]) && pass;
    }
  }
  return pass;
}
template <class T>
bool Case(bool hermitian, char tri, lapack_int order, lapack_int rhs) {
  using R = asc::DenseBlasRealType<T>;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 79;
  std::array<lapack_int, 3> n{kGuard, order, kGuard};
  std::array<lapack_int, 3> nrhs{kGuard, rhs, kGuard};
  std::array<lapack_int, 3> lda{kGuard, 4, kGuard};
  std::array<lapack_int, 3> ldb{kGuard, 4, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  std::array<lapack_int, 5> piv{kGuard, 1, 2, 3, kGuard};
  std::array<char, 3> uplo{'a', tri, 'z'};
  std::array<T, 14> a;
  std::array<T, 14> b;
  std::array<T, 6> extra;
  a.fill(Value<T>(-71, 13));
  b.fill(Value<T>(-73, 17));
  extra.fill(Value<T>(std::numeric_limits<R>::quiet_NaN(), 19));
  for (lapack_int j = 0; j < order; ++j) {
    for (lapack_int i = 0; i < order; ++i) {
      if (tri == 'U' ? i <= j : i >= j) {
        a[1 + j * 4 + i] = T{};
      }
    }
  }
  const lapack_int first = order - 2;
  if (order >= 2) {
    piv[1 + first] = -(first + 1);
    piv[2 + first] = -(first + 2);
    extra[1 + (tri == 'U' ? first + 1 : first)] = Value<T>(3, 4);
  }
  if (order == 1 || order == 3) {
    a[1] = T{4};
  }
  for (lapack_int j = 0; j < rhs; ++j) {
    for (lapack_int i = 0; i < order; ++i) {
      T coefficient{};
      lapack_int k = i;
      if (order >= 2 && i >= first) {
        const T upper =
            tri == 'U' ? Value<T>(3, 4) : Adj(Value<T>(3, 4), hermitian);
        coefficient = i == first ? upper : Adj(upper, hermitian);
        k = i == first ? i + 1 : i - 1;
      } else {
        coefficient = T{4};
      }
      b[1 + j * 4 + i] = coefficient * Expected<T>(k, j);
    }
  }
  const auto before_a = a;
  const auto before_b = b;
  const auto before_p = piv;
  const auto before_e = extra;
  Call(hermitian, &uplo[1], &n[1], &nrhs[1], &a[1], &lda[1], &extra[1], &piv[1],
       &b[1], &ldb[1], &info[1]);
  bool pass =
      SameRepresentation(extra.data(), before_e.data(), sizeof(extra)) &&
      info == std::array<lapack_int, 3>{kGuard, 0, kGuard} && piv == before_p &&
      SameRepresentation(a.data(), before_a.data(), sizeof(a)) &&
      n == std::array<lapack_int, 3>{kGuard, order, kGuard} &&
      nrhs == std::array<lapack_int, 3>{kGuard, rhs, kGuard} &&
      lda == std::array<lapack_int, 3>{kGuard, 4, kGuard} &&
      ldb == std::array<lapack_int, 3>{kGuard, 4, kGuard} &&
      uplo == std::array<char, 3>{'a', tri, 'z'};
  pass = CheckRhs(b, before_b, order, rhs) && pass;

  std::printf(
      "TRS_3 ABI real_bytes=%zu complex=%d he=%d tri=%c n=%lld nrhs=%lld "
      "A_unchanged=%d pass=%d\n",
      sizeof(R), static_cast<int>(asc::DenseBlasComplex<T>),
      static_cast<int>(hermitian), tri, static_cast<long long>(order),
      static_cast<long long>(rhs), static_cast<int>(a == before_a),
      static_cast<int>(pass));
  return pass;
}
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  bool pass = true;
  int cases = 0;
  for (char tri : {'U', 'L'}) {
    for (lapack_int n : {0, 1, 2, 3}) {
      for (lapack_int nrhs : {0, 1, 2}) {
        pass = Case<float>(false, tri, n, nrhs) && pass;
        pass = Case<double>(false, tri, n, nrhs) && pass;
        pass = Case<std::complex<float>>(false, tri, n, nrhs) && pass;
        pass = Case<std::complex<double>>(false, tri, n, nrhs) && pass;
        pass = Case<std::complex<float>>(true, tri, n, nrhs) && pass;
        pass = Case<std::complex<double>>(true, tri, n, nrhs) && pass;
        cases += 6;
      }
    }
  }
  std::printf("RK TRS_3 guarded native cases=%d passed=%d\n", cases,
              static_cast<int>(pass));
  return pass ? 0 : 1;
}
