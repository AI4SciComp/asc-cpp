#if defined(ASC_RK_CONDITION_EMITTED)
#include ASC_RK_CONDITION_SSYCON_3_HEADER
#include ASC_RK_CONDITION_DSYCON_3_HEADER
#include ASC_RK_CONDITION_CSYCON_3_HEADER
#include ASC_RK_CONDITION_ZSYCON_3_HEADER
#include ASC_RK_CONDITION_CHECON_3_HEADER
#include ASC_RK_CONDITION_ZHECON_3_HEADER
#undef ssycon_3_
#undef dsycon_3_
#undef csycon_3_
#undef zsycon_3_
#undef checon_3_
#undef zhecon_3_
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
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
#if defined(ASC_RK_CONDITION_EMITTED)
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_ssycon_3_base)>::Type,
                             typename Sig<decltype(emitted_ssycon_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_dsycon_3_base)>::Type,
                             typename Sig<decltype(emitted_dsycon_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_csycon_3_base)>::Type,
                             typename Sig<decltype(emitted_csycon_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zsycon_3_base)>::Type,
                             typename Sig<decltype(emitted_zsycon_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_checon_3_base)>::Type,
                             typename Sig<decltype(emitted_checon_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zhecon_3_base)>::Type,
                             typename Sig<decltype(emitted_zhecon_3)>::Type>);
#endif
template <class T>
T Value(long double x, long double y = 0) {
  using R = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<R>(x), static_cast<R>(y)};
  } else {
    return static_cast<T>(x);
  }
}
bool Bytes(const void* a, const void* b, std::size_t size) {
  return std::memcmp(a, b, size) == 0;
}
template <class T>
void Call(bool he, const char* u, const lapack_int* n, const T* a,
          const lapack_int* lda, const T* e, const lapack_int* piv,
          const asc::DenseBlasRealType<T>* norm,
          asc::DenseBlasRealType<T>* rcond, T* work, lapack_int* iw,
          lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssycon_3(u, n, a, lda, e, piv, norm, rcond, work, iw, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsycon_3(u, n, a, lda, e, piv, norm, rcond, work, iw, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_checon_3(u, n, a, lda, e, piv, norm, rcond, work, info);
    } else {
      LAPACK_csycon_3(u, n, a, lda, e, piv, norm, rcond, work, info);
    }
  } else {
    if (he) {
      LAPACK_zhecon_3(u, n, a, lda, e, piv, norm, rcond, work, info);
    } else {
      LAPACK_zsycon_3(u, n, a, lda, e, piv, norm, rcond, work, info);
    }
  }
}
template <class T>
void InitializeRaw(std::array<T, 14>& a, std::array<T, 6>& e,
                   std::array<lapack_int, 5>& piv, char tri, lapack_int order,
                   bool singular) {
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
    e[1 + (tri == 'U' ? first + 1 : first)] = Value<T>(3, 4);
  }
  if (order == 1 || order == 3) {
    a[1] = singular ? T{} : T{4};
  }
}
template <class T>
bool Case(bool he, char tri, lapack_int order, bool zero_norm, bool singular,
          bool mathematical) {
  using R = asc::DenseBlasRealType<T>;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 79;
  std::array<lapack_int, 3> n{kGuard, order, kGuard};
  std::array<lapack_int, 3> lda{kGuard, 4, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  std::array<lapack_int, 5> piv{kGuard, 1, 2, 3, kGuard};
  std::array<lapack_int, 5> iw;
  iw.fill(kGuard);
  std::array<char, 3> u{'a', tri, 'z'};
  std::array<T, 14> a;
  a.fill(Value<T>(-71, 13));
  std::array<T, 6> e;
  e.fill(Value<T>(std::numeric_limits<R>::quiet_NaN(), 19));
  std::array<T, 10> work;
  work.fill(Value<T>(-79, 17));
  InitializeRaw(a, e, piv, tri, order, singular);
  const R off = asc::DenseBlasComplex<T> ? R{5} : R{3};
  R matrix_norm = 4;
  if (order == 2) {
    matrix_norm = off;
  }
  if (order > 2) {
    matrix_norm = std::max(off, R{4});
  }
  std::array<R, 3> norm{R{113}, zero_norm ? R{} : matrix_norm, R{117}};
  std::array<R, 3> condition{R{127}, R{-1}, R{131}};
  const auto before_a = a;
  const auto before_e = e;
  const auto before_p = piv;
  const auto before_w = work;
  const auto before_iw = iw;
  const auto before_norm = norm;
  Call(he, &u[1], &n[1], &a[1], &lda[1], &e[1], &piv[1], &norm[1],
       &condition[1], &work[1], &iw[1], &info[1]);
  bool guards = Bytes(a.data(), before_a.data(), sizeof(a)) &&
                Bytes(e.data(), before_e.data(), sizeof(e)) &&
                piv == before_p && norm == before_norm &&
                n == std::array<lapack_int, 3>{kGuard, order, kGuard} &&
                lda == std::array<lapack_int, 3>{kGuard, 4, kGuard} &&
                info == std::array<lapack_int, 3>{kGuard, 0, kGuard} &&
                u == std::array<char, 3>{'a', tri, 'z'} &&
                condition[0] == R{127} && condition[2] == R{131};
  const bool early = order == 0 || zero_norm || singular;
  for (std::size_t k = 0; k < work.size(); ++k) {
    if (early || k == 0 || k > 2U * static_cast<std::size_t>(order)) {
      guards = Bytes(&work[k], &before_w[k], sizeof(T)) && guards;
    }
  }
  for (std::size_t k = 0; k < iw.size(); ++k) {
    if (early || asc::DenseBlasComplex<T> || k == 0 ||
        k > static_cast<std::size_t>(order)) {
      guards = iw[k] == before_iw[k] && guards;
    }
  }
  R expected = 1;
  if (order != 0) {
    if (zero_norm || singular) {
      expected = 0;
    } else if (order >= 3) {
      expected = std::min(R{4}, off) / matrix_norm;
    }
  }
  const bool accurate = std::isfinite(condition[1]) &&
                        std::abs(condition[1] - expected) <=
                            64 * std::numeric_limits<R>::epsilon() * expected;
  std::printf(
      "CON_3 ABI real_bytes=%zu complex=%d he=%d tri=%c n=%lld zero_norm=%d "
      "singular=%d RCOND=%La expected=%La guards=%d accurate=%d\n",
      sizeof(R), static_cast<int>(asc::DenseBlasComplex<T>),
      static_cast<int>(he), tri, static_cast<long long>(order),
      static_cast<int>(zero_norm), static_cast<int>(singular),
      static_cast<long double>(condition[1]),
      static_cast<long double>(expected), static_cast<int>(guards),
      static_cast<int>(accurate));
  return guards && (!mathematical || accurate);
}
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view mode(argv[1]);
  if (mode != "guards" && mode != "mathematical") {
    return 2;
  }
  const bool mathematical = mode == "mathematical";
  bool pass = true;
  int cases = 0;
  for (char tri : {'U', 'L'}) {
    for (lapack_int n : {0, 1, 2, 3}) {
      for (bool zero : {false, true}) {
        for (bool singular : {false, true}) {
          if (singular && n != 1 && n != 3) {
            continue;
          }
          pass =
              Case<float>(false, tri, n, zero, singular, mathematical) && pass;
          pass =
              Case<double>(false, tri, n, zero, singular, mathematical) && pass;
          pass = Case<std::complex<float>>(false, tri, n, zero, singular,
                                           mathematical) &&
                 pass;
          pass = Case<std::complex<double>>(false, tri, n, zero, singular,
                                            mathematical) &&
                 pass;
          pass = Case<std::complex<float>>(true, tri, n, zero, singular,
                                           mathematical) &&
                 pass;
          pass = Case<std::complex<double>>(true, tri, n, zero, singular,
                                            mathematical) &&
                 pass;
          cases += 6;
        }
      }
    }
  }
  std::printf("RK CON_3 prerequisite mode=%s cases=%d passed=%d\n", argv[1],
              cases, static_cast<int>(pass));
  return pass ? 0 : 1;
}
