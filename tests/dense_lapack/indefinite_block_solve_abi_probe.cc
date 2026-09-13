#if defined(ASC_BLOCK_SOLVE_EMITTED)
#include ASC_BLOCK_SOLVE_SSYTRS2_HEADER
#include ASC_BLOCK_SOLVE_DSYTRS2_HEADER
#include ASC_BLOCK_SOLVE_CSYTRS2_HEADER
#include ASC_BLOCK_SOLVE_ZSYTRS2_HEADER
#include ASC_BLOCK_SOLVE_CHETRS2_HEADER
#include ASC_BLOCK_SOLVE_ZHETRS2_HEADER
#undef ssytrs2_
#undef dsytrs2_
#undef csytrs2_
#undef zsytrs2_
#undef chetrs2_
#undef zhetrs2_
#endif
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
#include "indefinite_block_solve_native.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_BLOCK_SOLVE_EMITTED)
#include <type_traits>
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
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_ssytrs2_base)>::Type,
                             typename Sig<decltype(emitted_ssytrs2)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_dsytrs2_base)>::Type,
                             typename Sig<decltype(emitted_dsytrs2)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_csytrs2_base)>::Type,
                             typename Sig<decltype(emitted_csytrs2)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zsytrs2_base)>::Type,
                             typename Sig<decltype(emitted_zsytrs2)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_chetrs2_base)>::Type,
                             typename Sig<decltype(emitted_chetrs2)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zhetrs2_base)>::Type,
                             typename Sig<decltype(emitted_zhetrs2)>::Type>);
#endif
namespace {
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
// Exact object bytes, including signed zero and NaN payloads, are intentional.
bool EqualBytes(const void* first, const void* second, std::size_t size) {
  return std::memcmp(first, second, size) == 0;
}
template <typename T>
T Expected(lapack_int i, lapack_int j) {
  return Value<T>(i + 1 + j, static_cast<long double>(i - j) / 4);
}
template <typename T>
void Fill(bool hermitian, char tri, lapack_int order, lapack_int rhs,
          std::array<T, 14>& a, std::array<T, 14>& b,
          std::array<lapack_int, 5>& piv) {
  for (lapack_int j = 0; j < order; ++j) {
    for (lapack_int i = 0; i < order; ++i) {
      if (tri == 'U' ? i <= j : i >= j) {
        a[1 + j * 4 + i] = T{};
      }
    }
  }
  const lapack_int first = order - 2;
  if (order >= 2) {
    const lapack_int paired = tri == 'U' ? -(first + 1) : -(first + 2);
    piv[1 + first] = paired;
    piv[2 + first] = paired;
    a[tri == 'U' ? 1 + (first + 1) * 4 + first : 1 + first * 4 + first + 1] =
        Value<T>(3, 4);
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
  std::array<T, 6> work;
  a.fill(Value<T>(-71, 13));
  b.fill(Value<T>(-73, 17));
  work.fill(Value<T>(-79, 19));
  Fill(hermitian, tri, order, rhs, a, b, piv);
  const auto before_a = a;
  const auto before_b = b;
  const auto before_p = piv;
  const auto before_w = work;
  asc_block_solve_test::Native(hermitian, &uplo[1], &n[1], &nrhs[1], &a[1],
                               &lda[1], &piv[1], &b[1], &ldb[1], &work[1],
                               &info[1]);
  bool pass = info == std::array<lapack_int, 3>{kGuard, 0, kGuard} &&
              piv == before_p &&
              EqualBytes(a.data(), before_a.data(), sizeof(a)) &&
              n == std::array<lapack_int, 3>{kGuard, order, kGuard} &&
              nrhs == std::array<lapack_int, 3>{kGuard, rhs, kGuard} &&
              lda == std::array<lapack_int, 3>{kGuard, 4, kGuard} &&
              ldb == std::array<lapack_int, 3>{kGuard, 4, kGuard} &&
              uplo == std::array<char, 3>{'a', tri, 'z'};
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
  for (std::size_t k = 0; k < work.size(); ++k) {
    if (k == 0 || k > static_cast<std::size_t>(order) || order == 0 ||
        rhs == 0) {
      pass = (work[k] == before_w[k]) && pass;
    }
  }
  std::printf(
      "TRS2 ABI real_bytes=%zu complex=%d he=%d tri=%c n=%lld nrhs=%lld "
      "restored=%d pass=%d\n",
      sizeof(R), static_cast<int>(asc::DenseBlasComplex<T>),
      static_cast<int>(hermitian), tri, static_cast<long long>(order),
      static_cast<long long>(rhs), static_cast<int>(a == before_a),
      static_cast<int>(pass));
  return pass;
}
}  // namespace
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
  std::printf("TRS2 guarded native cases=%d passed=%d\n", cases,
              static_cast<int>(pass));
  return pass ? 0 : 1;
}
