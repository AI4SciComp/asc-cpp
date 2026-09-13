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
#include "../../src/dense/lapack/internal_indefinite_rk_prototypes.h"
#include "asc/dense/blas.h"
#include "installed_lu/normal_return_guard.h"
#if defined(RK_SSYTF2_RK_HEADER)
#define ssytf2_rk_ emitted_ssytf2_rk
#include RK_SSYTF2_RK_HEADER
#undef ssytf2_rk_
#define ssytrf_rk_ emitted_ssytrf_rk
#include RK_SSYTRF_RK_HEADER
#undef ssytrf_rk_
#define dsytf2_rk_ emitted_dsytf2_rk
#include RK_DSYTF2_RK_HEADER
#undef dsytf2_rk_
#define dsytrf_rk_ emitted_dsytrf_rk
#include RK_DSYTRF_RK_HEADER
#undef dsytrf_rk_
#define csytf2_rk_ emitted_csytf2_rk
#include RK_CSYTF2_RK_HEADER
#undef csytf2_rk_
#define csytrf_rk_ emitted_csytrf_rk
#include RK_CSYTRF_RK_HEADER
#undef csytrf_rk_
#define zsytf2_rk_ emitted_zsytf2_rk
#include RK_ZSYTF2_RK_HEADER
#undef zsytf2_rk_
#define zsytrf_rk_ emitted_zsytrf_rk
#include RK_ZSYTRF_RK_HEADER
#undef zsytrf_rk_
#define chetf2_rk_ emitted_chetf2_rk
#include RK_CHETF2_RK_HEADER
#undef chetf2_rk_
#define chetrf_rk_ emitted_chetrf_rk
#include RK_CHETRF_RK_HEADER
#undef chetrf_rk_
#define zhetf2_rk_ emitted_zhetf2_rk
#include RK_ZHETF2_RK_HEADER
#undef zhetf2_rk_
#define zhetrf_rk_ emitted_zhetrf_rk
#include RK_ZHETRF_RK_HEADER
#undef zhetrf_rk_
#endif
namespace {
#if defined(RK_SSYTF2_RK_HEADER)
template <typename T>
struct Arg {
  using Type = T;
};
template <typename T>
struct Arg<T*> {
  using Type = std::remove_const_t<T>*;
};
template <typename T>
struct Sig;
template <typename R, typename... A>
struct Sig<R(A...)> {
  using Type = R(typename Arg<A>::Type...);
};
static_assert(
    std::is_same_v<decltype(ssytf2_rk_), decltype(emitted_ssytf2_rk)>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_ssytrf_rk_base)>::Type,
                   typename Sig<decltype(emitted_ssytrf_rk)>::Type>);
static_assert(
    std::is_same_v<decltype(dsytf2_rk_), decltype(emitted_dsytf2_rk)>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_dsytrf_rk_base)>::Type,
                   typename Sig<decltype(emitted_dsytrf_rk)>::Type>);
static_assert(
    std::is_same_v<decltype(csytf2_rk_), decltype(emitted_csytf2_rk)>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_csytrf_rk_base)>::Type,
                   typename Sig<decltype(emitted_csytrf_rk)>::Type>);
static_assert(
    std::is_same_v<decltype(zsytf2_rk_), decltype(emitted_zsytf2_rk)>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zsytrf_rk_base)>::Type,
                   typename Sig<decltype(emitted_zsytrf_rk)>::Type>);
static_assert(
    std::is_same_v<decltype(chetf2_rk_), decltype(emitted_chetf2_rk)>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_chetrf_rk_base)>::Type,
                   typename Sig<decltype(emitted_chetrf_rk)>::Type>);
static_assert(
    std::is_same_v<decltype(zhetf2_rk_), decltype(emitted_zhetf2_rk)>);
static_assert(
    std::is_same_v<typename Sig<decltype(LAPACK_zhetrf_rk_base)>::Type,
                   typename Sig<decltype(emitted_zhetrf_rk)>::Type>);
#endif
template <typename T>
T Value(long double real, long double imaginary = 0) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<asc::DenseBlasRealType<T>>(real),
            static_cast<asc::DenseBlasRealType<T>>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}
template <typename T>
void Call(bool hermitian, bool blocked, char* uplo, lapack_int* n, T* a,
          lapack_int* lda, T* extra, lapack_int* pivots, T* work,
          lapack_int* lwork, lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    if (blocked) {
      LAPACK_ssytrf_rk(uplo, n, a, lda, extra, pivots, work, lwork, info);
    } else {
      ssytf2_rk_(uplo, n, a, lda, extra, pivots, info, std::size_t{1});
    }
  } else if constexpr (std::is_same_v<T, double>) {
    if (blocked) {
      LAPACK_dsytrf_rk(uplo, n, a, lda, extra, pivots, work, lwork, info);
    } else {
      dsytf2_rk_(uplo, n, a, lda, extra, pivots, info, std::size_t{1});
    }
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      if (blocked) {
        LAPACK_chetrf_rk(uplo, n, a, lda, extra, pivots, work, lwork, info);
      } else {
        chetf2_rk_(uplo, n, a, lda, extra, pivots, info, std::size_t{1});
      }
    } else {
      if (blocked) {
        LAPACK_csytrf_rk(uplo, n, a, lda, extra, pivots, work, lwork, info);
      } else {
        csytf2_rk_(uplo, n, a, lda, extra, pivots, info, std::size_t{1});
      }
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (hermitian) {
      if (blocked) {
        LAPACK_zhetrf_rk(uplo, n, a, lda, extra, pivots, work, lwork, info);
      } else {
        zhetf2_rk_(uplo, n, a, lda, extra, pivots, info, std::size_t{1});
      }
    } else {
      if (blocked) {
        LAPACK_zsytrf_rk(uplo, n, a, lda, extra, pivots, work, lwork, info);
      } else {
        zsytf2_rk_(uplo, n, a, lda, extra, pivots, info, std::size_t{1});
      }
    }
  }
}
constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;
template <typename T>
struct ProbeArrays {
  std::array<lapack_int, 6> piv{};
  std::array<T, 18> a{};
  std::array<T, 6> extra{};
  std::array<T, 258> work{};
  ProbeArrays(lapack_int order, char tri, int mode) {
    piv.fill(kGuard);
    a.fill(Value<T>(-71, 13));
    extra.fill(Value<T>(-79, 17));
    work.fill(Value<T>(-83, 19));
    for (lapack_int j = 0; j < order; ++j) {
      for (lapack_int i = 0; i < order; ++i) {
        if (tri == 'U' ? i <= j : i >= j) {
          a[1 + j * 4 + i] = T{};
        }
      }
    }
    if (mode == 1) {
      a[1] = T{4};
    }
    if (mode == 3) {
      a[tri == 'U' ? 5U : 2U] = Value<T>(3, 4);
    }
  }
};
inline lapack_int ExpectedInfo(int mode, char tri, bool query) {
  if (query) {
    return 0;
  }
  if (mode == 2) {
    return 1;
  }
  if (mode == 4) {
    return tri == 'U' ? 3 : 1;
  }
  return 0;
}
template <typename T>
bool ArrayGuards(lapack_int order, char tri, int mode, bool blocked, bool query,
                 const ProbeArrays<T>& output, const ProbeArrays<T>& before) {
  const auto& a = output.a;
  const auto& extra = output.extra;
  const auto& piv = output.piv;
  const auto& work = output.work;
  const auto& before_a = before.a;
  const auto& before_e = before.extra;
  const auto& before_p = before.piv;
  const auto& before_w = before.work;
  bool pass = true;
  for (std::size_t k = 0; k < a.size(); ++k) {
    const int i = (static_cast<int>(k) - 1) % 4;
    const int j = (static_cast<int>(k) - 1) / 4;
    const bool selected =
        k != 0 && i < order && j < order && (tri == 'U' ? i <= j : i >= j);
    if (!selected || query) {
      pass = (a[k] == before_a[k]) && pass;
    }
  }
  for (std::size_t k = 0; k < extra.size(); ++k) {
    if (k == 0 || k > static_cast<std::size_t>(order) || query) {
      pass = (extra[k] == before_e[k] && piv[k] == before_p[k]) && pass;
    }
  }
  for (std::size_t k = 0; k < work.size(); ++k) {
    if (!blocked || k != 1) {
      pass = (work[k] == before_w[k]) && pass;
    }
  }
  if (blocked) {
    pass = (work[1] == T{static_cast<asc::DenseBlasRealType<T>>(
                           std::max<lapack_int>(1, order * 64))}) &&
           pass;
  }
  if (!query && order != 0) {
    if (mode == 3) {
      pass = piv[1] == -1 && piv[2] == -2 &&
             extra[tri == 'U' ? 2U : 1U] == Value<T>(3, 4) &&
             extra[tri == 'U' ? 1U : 2U] == T{} &&
             a[tri == 'U' ? 5U : 2U] == T{} && pass;
    } else {
      for (lapack_int i = 0; i < order; ++i) {
        pass = piv[i + 1] == i + 1 && extra[i + 1] == T{} && pass;
      }
    }
  }
  return pass;
}
template <typename T>
bool Probe(bool he, bool blocked, char tri, int mode, bool query) {
  constexpr std::array<lapack_int, 5> kOrders{0, 1, 1, 2, 3};
  const lapack_int order = kOrders[static_cast<std::size_t>(mode)];
  const lapack_int size = query ? -1 : std::max<lapack_int>(1, order * 64);
  std::array<lapack_int, 3> n{kGuard, order, kGuard};
  std::array<lapack_int, 3> lda{kGuard, 4, kGuard};
  std::array<lapack_int, 3> lwork{kGuard, size, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  std::array<char, 3> uplo{'a', tri, 'z'};
  ProbeArrays<T> arrays(order, tri, mode);
  const auto before = arrays;
  Call(he, blocked, &uplo[1], &n[1], arrays.a.data() + 1, &lda[1],
       arrays.extra.data() + 1, arrays.piv.data() + 1, arrays.work.data() + 1,
       &lwork[1], &info[1]);
  const lapack_int expected = ExpectedInfo(mode, tri, query);
  bool pass = info == std::array<lapack_int, 3>{kGuard, expected, kGuard} &&
              n == std::array<lapack_int, 3>{kGuard, order, kGuard} &&
              lda == std::array<lapack_int, 3>{kGuard, 4, kGuard} &&
              lwork == std::array<lapack_int, 3>{kGuard, size, kGuard} &&
              uplo == std::array<char, 3>{'a', tri, 'z'};
  pass = ArrayGuards(order, tri, mode, blocked, query, arrays, before) && pass;
  std::printf(
      "RK ABI real_bytes=%zu complex=%d he=%d blocked=%d tri=%c n=%lld "
      "query=%d INFO=%lld pass=%d\n",
      sizeof(asc::DenseBlasRealType<T>),
      static_cast<int>(asc::DenseBlasComplex<T>), static_cast<int>(he),
      static_cast<int>(blocked), tri, static_cast<long long>(order),
      static_cast<int>(query), static_cast<long long>(info[1]),
      static_cast<int>(pass));
  return pass;
}
template <typename T>
bool EmptyContract(bool he, char tri) {
  lapack_int n = 0;
  lapack_int lda = 1;
  lapack_int lwork = 1;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  std::array<T, 5> a;
  std::array<T, 5> extra;
  std::array<T, 5> work;
  std::array<lapack_int, 5> piv;
  a.fill(T{-71});
  extra.fill(T{-73});
  work.fill(T{-79});
  piv.fill(-83);
  const auto before_a = a;
  const auto before_e = extra;
  const auto before_w = work;
  const auto before_p = piv;
  Call(he, false, &tri, &n, a.data() + 2, &lda, extra.data() + 2,
       piv.data() + 2, work.data() + 2, &lwork, &info);
  const bool unchanged = extra == before_e;
  const bool other =
      info == 0 && a == before_a && piv == before_p && work == before_w;
  std::printf(
      "RK TF2 empty contract real_bytes=%zu complex=%d he=%d tri=%c INFO=%lld "
      "E_left=%g E_first=%g E_unchanged=%d other_guards=%d\n",
      sizeof(asc::DenseBlasRealType<T>),
      static_cast<int>(asc::DenseBlasComplex<T>), static_cast<int>(he), tri,
      static_cast<long long>(info), static_cast<double>(std::real(extra[1])),
      static_cast<double>(std::real(extra[2])), static_cast<int>(unchanged),
      static_cast<int>(other));
  if (!unchanged) {
    std::puts("check failed: native empty E guard unchanged");
  }
  return unchanged && other;
}
template <typename T>
bool Run(bool he, bool empty, int& cases) {
  bool pass = true;
  for (char tri : {'U', 'L'}) {
    if (empty) {
      pass = EmptyContract<T>(he, tri) && pass;
      ++cases;
      continue;
    }
    for (bool blocked : {false, true}) {
      for (int mode = 0; mode < 5; ++mode) {
        // Direct TF2 N0 is isolated as the required failing native-empty
        // contract diagnostic, retaining guard assertions. ASC will use an
        // empty noncall.
        if (!blocked && mode == 0) {
          continue;
        }
        pass = Probe<T>(he, blocked, tri, mode, false) && pass;
        ++cases;
        if (blocked) {
          pass = Probe<T>(he, blocked, tri, mode, true) && pass;
          ++cases;
        }
      }
    }
  }
  return pass;
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2 || (std::string_view(argv[1]) != "active" &&
                    std::string_view(argv[1]) != "empty")) {
    return 2;
  }
  const bool empty = std::string_view(argv[1]) == "empty";
  bool pass = true;
  int cases = 0;
  pass = Run<float>(false, empty, cases) && pass;
  pass = Run<double>(false, empty, cases) && pass;
  pass = Run<std::complex<float>>(false, empty, cases) && pass;
  pass = Run<std::complex<double>>(false, empty, cases) && pass;
  pass = Run<std::complex<float>>(true, empty, cases) && pass;
  pass = Run<std::complex<double>>(true, empty, cases) && pass;
  std::printf("RK prerequisite mode=%s cases=%d passed=%d\n", argv[1], cases,
              static_cast<int>(pass));
  return pass ? 0 : 1;
}
