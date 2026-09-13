#include <array>
#include <complex>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "installed_lu/normal_return_guard.h"
template <class T>
void Call(bool he, const char* u, const lapack_int* n, T* a,
          const lapack_int* lda, const T* e, const lapack_int* ip, T* w,
          const lapack_int* lw, lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytri_3(u, n, a, lda, e, ip, w, lw, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytri_3(u, n, a, lda, e, ip, w, lw, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chetri_3(u, n, a, lda, e, ip, w, lw, info);
    } else {
      LAPACK_csytri_3(u, n, a, lda, e, ip, w, lw, info);
    }
  } else {
    if (he) {
      LAPACK_zhetri_3(u, n, a, lda, e, ip, w, lw, info);
    } else {
      LAPACK_zsytri_3(u, n, a, lda, e, ip, w, lw, info);
    }
  }
}
template <class T>
bool Cases(const char* name, bool he, bool mathematical) {
  bool result = true;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 13;
  for (char u : {'U', 'L'}) {
    for (lapack_int order : {0, 1, 2, 7, 67}) {
      std::array<lapack_int, 3> ns{kGuard, order, kGuard};
      std::array<lapack_int, 3> ld{kGuard, order != 0 ? order : 1, kGuard};
      std::array<lapack_int, 3> lw{kGuard, -1, kGuard};
      std::array<lapack_int, 3> ip{kGuard, 1, kGuard};
      std::array<lapack_int, 3> info{
          kGuard, std::numeric_limits<lapack_int>::min(), kGuard};
      std::array<T, 3> a{T{11}, T{13}, T{17}};
      std::array<T, 3> ev{T{19}, T{23}, T{29}};
      std::array<T, 10> w;
      w.fill(T{-73});
      const auto old = w;
      const auto aa = a;
      const auto ee = ev;
      const auto pp = ip;
      Call(he, &u, &ns[1], &a[1], &ld[1], &ev[1], &ip[1], &w[1], &lw[1],
           &info[1]);
      const bool special = std::is_same_v<T, float> ||
                           std::is_same_v<T, double> ||
                           (std::is_same_v<T, std::complex<float>> && he);
      const lapack_int expected = order == 0 && special ? 1 : (order + 2) * 4;
      bool guards =
          ns == std::array<lapack_int, 3>{kGuard, order, kGuard} &&
          ld == std::array<lapack_int, 3>{kGuard, order != 0 ? order : 1,
                                          kGuard} &&
          lw == std::array<lapack_int, 3>{kGuard, -1, kGuard} && ip == pp &&
          a == aa && ev == ee &&
          info == std::array<lapack_int, 3>{kGuard, 0, kGuard};
      for (unsigned i = 0; i < w.size(); ++i) {
        if (i != 1) {
          guards = guards && w[i] == old[i];
        }
      }
      bool query_ok = w[1] == T(expected);
      std::printf(
          "%s u=%c n=%lld query=%Lg expected=%lld guards=%d query_ok=%d\n",
          name, u, static_cast<long long>(order),
          static_cast<long double>(std::real(w[1])),
          static_cast<long long>(expected), guards, query_ok);
      result = result && guards && query_ok;
      if (order == 0) {
        w.fill(T{-73});
        lw[1] = expected;
        info[1] = std::numeric_limits<lapack_int>::min();
        Call(he, &u, &ns[1], &a[1], &ld[1], &ev[1], &ip[1], &w[1], &lw[1],
             &info[1]);
        guards = ns == std::array<lapack_int, 3>{kGuard, order, kGuard} &&
                 ld == std::array<lapack_int, 3>{kGuard, 1, kGuard} &&
                 lw == std::array<lapack_int, 3>{kGuard, expected, kGuard} &&
                 ip == pp && a == aa && ev == ee &&
                 info == std::array<lapack_int, 3>{kGuard, 0, kGuard};
        for (unsigned i = 0; i < w.size(); ++i) {
          if (i != 1) {
            guards = guards && w[i] == old[i];
          }
        }
        const bool source_ok = w[1] == (special ? T(expected) : T{-73});
        const bool contract_ok = w[1] == T(expected);
        std::printf(
            "%s u=%c n=0 execute_work=%Lg guards=%d source_ok=%d "
            "contract_ok=%d\n",
            name, u, static_cast<long double>(std::real(w[1])), guards,
            source_ok, contract_ok);
        result =
            result && guards && source_ok && (!mathematical || contract_ok);
      }
    }
  }
  return result;
}
int main(int argc, char** argv) {
  asc_lapack_test::NormalReturnGuard normal;
  if (argc != 3) {
    return 2;
  }
  const bool math = std::string_view(argv[1]) == "--contract";
  const bool all = std::string_view(argv[2]) == "all";
  bool ok = true;
  if (all || std::string_view(argv[2]) == "dsytri_3") {
    ok = Cases<double>("dsytri_3", false, math) && ok;
  }
  if (all) {
    ok = Cases<float>("ssytri_3", false, math) && ok;
    ok = Cases<std::complex<float>>("csytri_3", false, math) && ok;
    ok = Cases<std::complex<double>>("zsytri_3", false, math) && ok;
    ok = Cases<std::complex<float>>("chetri_3", true, math) && ok;
    ok = Cases<std::complex<double>>("zhetri_3", true, math) && ok;
  }
  std::printf("normal_return=1 success=%d\n", static_cast<int>(ok));
  return ok ? 0 : 1;
}
