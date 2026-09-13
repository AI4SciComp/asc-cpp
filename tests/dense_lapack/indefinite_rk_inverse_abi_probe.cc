#if defined(ASC_RK_INVERSE_EMITTED)
#include ASC_RK_INVERSE_SSYTRI_3_HEADER
#include ASC_RK_INVERSE_DSYTRI_3_HEADER
#include ASC_RK_INVERSE_CSYTRI_3_HEADER
#include ASC_RK_INVERSE_ZSYTRI_3_HEADER
#include ASC_RK_INVERSE_CHETRI_3_HEADER
#include ASC_RK_INVERSE_ZHETRI_3_HEADER
#include ASC_RK_INVERSE_SSYTRI_3X_HEADER
#include ASC_RK_INVERSE_DSYTRI_3X_HEADER
#include ASC_RK_INVERSE_CSYTRI_3X_HEADER
#include ASC_RK_INVERSE_ZSYTRI_3X_HEADER
#include ASC_RK_INVERSE_CHETRI_3X_HEADER
#include ASC_RK_INVERSE_ZHETRI_3X_HEADER
#undef ssytri_3_
#undef dsytri_3_
#undef csytri_3_
#undef zsytri_3_
#undef chetri_3_
#undef zhetri_3_
#undef ssytri_3x_
#undef dsytri_3x_
#undef csytri_3x_
#undef zsytri_3x_
#undef chetri_3x_
#undef zhetri_3x_
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rk_inverse_prototypes.h"
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
template <class>
struct Sig;
template <class R, class... A>
struct Sig<R(A...)> {
  using Type = R(typename Arg<A>::Type...);
};
#if defined(ASC_RK_INVERSE_EMITTED)
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_ssytri_3_base)>::Type,
                             typename Sig<decltype(emitted_ssytri_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_dsytri_3_base)>::Type,
                             typename Sig<decltype(emitted_dsytri_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_csytri_3_base)>::Type,
                             typename Sig<decltype(emitted_csytri_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zsytri_3_base)>::Type,
                             typename Sig<decltype(emitted_zsytri_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_chetri_3_base)>::Type,
                             typename Sig<decltype(emitted_chetri_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zhetri_3_base)>::Type,
                             typename Sig<decltype(emitted_zhetri_3)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(ssytri_3x_)>::Type,
                             typename Sig<decltype(emitted_ssytri_3x)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(dsytri_3x_)>::Type,
                             typename Sig<decltype(emitted_dsytri_3x)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(csytri_3x_)>::Type,
                             typename Sig<decltype(emitted_csytri_3x)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(zsytri_3x_)>::Type,
                             typename Sig<decltype(emitted_zsytri_3x)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(chetri_3x_)>::Type,
                             typename Sig<decltype(emitted_chetri_3x)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(zhetri_3x_)>::Type,
                             typename Sig<decltype(emitted_zhetri_3x)>::Type>);

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
bool Bytes(const void* a, const void* b, std::size_t n) {
  return std::memcmp(a, b, n) == 0;
}
template <class T>
void Call(bool he, bool explicit_block, char* u, lapack_int* n, T* a,
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
template <class T>
bool Accurate(const std::array<T, 14>& a, bool he, char tri, lapack_int order) {
  using R = asc::DenseBlasRealType<T>;
  bool accurate = true;
  for (lapack_int j = 0; j < order; ++j) {
    for (lapack_int i = 0; i < order; ++i) {
      if (tri == 'U' ? i <= j : i >= j) {
        std::complex<long double> expected{};
        if (i == 0 && j == 0 && (order == 1 || order == 3)) {
          expected = {.25L, 0};
        }
        if (order >= 2 && i != j && i >= order - 2 && j >= order - 2) {
          const auto off = asc::DenseBlasComplex<T>
                               ? std::complex<long double>{3, 4}
                               : std::complex<long double>{3, 0};
          expected = 1.L / (he ? std::conj(off) : off);
        }
        const T actual = a[1 + j * 4 + i];
        const std::complex<long double> wide{
            static_cast<long double>(std::real(actual)),
            static_cast<long double>(std::imag(actual))};
        accurate = std::isfinite(std::real(actual)) &&
                   std::isfinite(std::imag(actual)) &&
                   std::abs(wide - expected) <=
                       32 * std::numeric_limits<R>::epsilon() *
                           std::max(1.L, std::abs(expected)) &&
                   accurate;
      }
    }
  }
  return accurate;
}

template <class T>
void Initialize(std::array<T, 14>& a, std::array<T, 6>& ev,
                std::array<lapack_int, 5>& piv, lapack_int order, char tri,
                bool singular) {
  for (lapack_int j = 0; j < order; ++j) {
    for (lapack_int i = 0; i < order; ++i) {
      if (tri == 'U' ? i <= j : i >= j) {
        a[1 + j * 4 + i] = T{};
      }
    }
  }
  if (order >= 2) {
    const lapack_int first = order - 2;
    piv[1 + first] = -(first + 1);
    piv[2 + first] = -(first + 2);
    ev[1 + (tri == 'U' ? first + 1 : first)] = Value<T>(3, 4);
  }
  if (order == 1 || order == 3) {
    a[1] = singular ? T{} : T{4};
  }
}

template <class T>
bool Case(const char* name, bool he, char tri, lapack_int order, bool singular,
          lapack_int block, bool mathematical) {
  using R = asc::DenseBlasRealType<T>;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 79;
  const bool x = block != 0;
  const lapack_int nb = x ? block : 1;
  const bool special = std::is_same_v<T, float> || std::is_same_v<T, double> ||
                       (std::is_same_v<T, std::complex<float>> && he);
  lapack_int count = 0;
  if (order != 0) {
    count = (order + nb + 1) * (nb + 3);
  } else if (!x) {
    count = special ? 1 : 8;
  }
  std::array<lapack_int, 3> ns{kGuard, order, kGuard};
  std::array<lapack_int, 3> ld{kGuard, 4, kGuard};
  std::array<lapack_int, 3> size{kGuard, x ? nb : count, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  std::array<lapack_int, 5> piv{kGuard, 1, 2, 3, kGuard};
  std::array<char, 3> u{'a', tri, 'z'};
  std::array<T, 14> a;
  a.fill(Value<T>(-71, 13));
  std::array<T, 6> ev;
  ev.fill(Value<T>(std::numeric_limits<R>::quiet_NaN(), 19));
  std::array<T, 5002> w;
  w.fill(Value<T>(-79, 17));
  Initialize(a, ev, piv, order, tri, singular);
  const auto aa = a;
  const auto ee = ev;
  const auto ww = w;
  const auto pp = piv;
  const auto ss = size;
  Call(he, x, &u[1], &ns[1], &a[1], &ld[1], &ev[1], &piv[1], &w[1], &size[1],
       &info[1]);
  bool guards =
      Bytes(ev.data(), ee.data(), sizeof(ev)) && piv == pp && size == ss &&
      ns == std::array<lapack_int, 3>{kGuard, order, kGuard} &&
      ld == std::array<lapack_int, 3>{kGuard, 4, kGuard} &&
      info == std::array<lapack_int, 3>{kGuard, singular ? 1 : 0, kGuard} &&
      u == std::array<char, 3>{'a', tri, 'z'};
  for (std::size_t k = 0; k < w.size(); ++k) {
    if (k == 0 || k > static_cast<std::size_t>(count) ||
        (order == 0 && (x || !special))) {
      guards = Bytes(&w[k], &ww[k], sizeof(T)) && guards;
    }
  }
  for (std::size_t k = 0; k < a.size(); ++k) {
    bool changed = false;
    for (lapack_int j = 0; j < order; ++j) {
      for (lapack_int i = 0; i < order; ++i) {
        if ((tri == 'U' ? i <= j : i >= j) &&
            k == std::size_t{1} + static_cast<std::size_t>(j) * 4 +
                     static_cast<std::size_t>(i)) {
          changed = true;
        }
      }
    }
    if (!changed || singular) {
      guards = Bytes(&a[k], &aa[k], sizeof(T)) && guards;
    }
  }
  if (!x && (order != 0 || special)) {
    guards = w[1] == T(count) && guards;
  }
  if (singular) {
    for (lapack_int i = 0; i < order; ++i) {
      if (x || i != 0) {
        guards = Bytes(&w[1 + i], &ee[1 + i], sizeof(T)) && guards;
      }
    }
  }
  const bool accurate = singular || Accurate(a, he, tri, order);
  std::printf(
      "%s%s u=%c n=%lld singular=%d nb=%lld info=%lld guards=%d accurate=%d\n",
      name, x ? "x" : "", tri, static_cast<long long>(order), singular,
      static_cast<long long>(nb), static_cast<long long>(info[1]), guards,
      accurate);
  return guards && (!mathematical || accurate);
}
template <class T>
bool Cases(const char* name, bool he, bool math) {
  bool ok = true;
  for (char tri : {'U', 'L'}) {
    for (lapack_int nb : {0, 1, 2, 3, 64}) {
      for (lapack_int n : {0, 1, 2, 3}) {
        ok = Case<T>(name, he, tri, n, false, nb, math) && ok;
      }
      for (lapack_int n : {1, 3}) {
        ok = Case<T>(name, he, tri, n, true, nb, math) && ok;
      }
    }
  }
  return ok;
}
int main(int argc, char** argv) {
  asc_lapack_test::NormalReturnGuard normal;
  if (argc != 2) {
    return 2;
  }
  const bool math = std::string_view(argv[1]) == "--mathematical";
  bool ok = true;
  ok = Cases<float>("ssytri_3", false, math) && ok;
  ok = Cases<double>("dsytri_3", false, math) && ok;
  ok = Cases<std::complex<float>>("csytri_3", false, math) && ok;
  ok = Cases<std::complex<double>>("zsytri_3", false, math) && ok;
  ok = Cases<std::complex<float>>("chetri_3", true, math) && ok;
  ok = Cases<std::complex<double>>("zhetri_3", true, math) && ok;
  std::printf("normal_return=1 success=%d\n", static_cast<int>(ok));
  return ok ? 0 : 1;
}
