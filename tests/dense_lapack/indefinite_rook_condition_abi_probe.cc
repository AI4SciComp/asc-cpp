#if defined(ASC_ROOK_CONDITION_EMITTED_PROTOTYPES)
// Untouched GNU emissions, renamed only inside this test translation unit.
#include ASC_ROOK_CONDITION_EMITTED_SSYCON_ROOK_HEADER
#include ASC_ROOK_CONDITION_EMITTED_DSYCON_ROOK_HEADER
#include ASC_ROOK_CONDITION_EMITTED_CSYCON_ROOK_HEADER
#include ASC_ROOK_CONDITION_EMITTED_ZSYCON_ROOK_HEADER
#include ASC_ROOK_CONDITION_EMITTED_CHECON_ROOK_HEADER
#include ASC_ROOK_CONDITION_EMITTED_ZHECON_ROOK_HEADER
#undef ssycon_rook_
#undef dsycon_rook_
#undef csycon_rook_
#undef zsycon_rook_
#undef checon_rook_
#undef zhecon_rook_
#endif
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
#include "indefinite_rook_condition_native.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_ROOK_CONDITION_EMITTED_PROTOTYPES)
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite_rook_condition_prototypes.h"
static_assert(std::is_same_v<decltype(ssycon_rook_),
                             decltype(asc_probe_emitted_ssycon_rook)>);
static_assert(std::is_same_v<decltype(dsycon_rook_),
                             decltype(asc_probe_emitted_dsycon_rook)>);
static_assert(std::is_same_v<decltype(csycon_rook_),
                             decltype(asc_probe_emitted_csycon_rook)>);
static_assert(std::is_same_v<decltype(zsycon_rook_),
                             decltype(asc_probe_emitted_zsycon_rook)>);
static_assert(std::is_same_v<decltype(checon_rook_),
                             decltype(asc_probe_emitted_checon_rook)>);
static_assert(std::is_same_v<decltype(zhecon_rook_),
                             decltype(asc_probe_emitted_zhecon_rook)>);
#endif
namespace {
template <typename T>
T Value(int real, int imaginary = 0) {
  if constexpr (asc::DenseBlasComplex<T>) {
    using Real = asc::DenseBlasRealType<T>;
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
bool Probe(bool hermitian, char triangle, int mode) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 79;
  const lapack_int nonempty_order = mode == 3 ? 2 : 1;
  const lapack_int order = mode == 0 ? 0 : nonempty_order;
  const Real pair_norm = asc::DenseBlasComplex<T> ? Real{5} : Real{3};
  std::array<char, 3> uplo{'a', triangle, 'z'};
  std::array<lapack_int, 3> n{kGuard, order, kGuard};
  std::array<lapack_int, 3> lda{kGuard, 2, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  std::array<lapack_int, 4> pivots{kGuard, mode == 3 ? -1 : 1, -2, kGuard};
  std::array<lapack_int, 4> iwork{kGuard, kGuard, kGuard, kGuard};
  std::array<T, 6> a{Value<T>(-173, 23),
                     mode == 1 ? T{4} : T{},
                     Value<T>(3, 4),
                     Value<T>(3, hermitian ? -4 : 4),
                     T{},
                     Value<T>(-173, 23)};
  std::array<T, 6> work;
  work.fill(Value<T>(-179, 29));
  std::array<Real, 3> norm{Real{-181}, mode == 3 ? pair_norm : Real{4},
                           Real{-191}};
  std::array<Real, 3> condition{Real{-193}, Real{-1}, Real{-197}};
  const auto a_before = a;
  const auto pivots_before = pivots;
  const auto norm_before = norm;
  asc_rook_condition_test::Native(hermitian, &uplo[1], &n[1], &a[1], &lda[1],
                                  &pivots[1], &norm[1], &condition[1], &work[1],
                                  &iwork[1], &info[1]);
  const Real expected = mode == 2 ? Real{} : Real{1};
  const bool output = info[1] == 0 && std::isfinite(condition[1]) &&
                      std::abs(condition[1] - expected) <=
                          32 * std::numeric_limits<Real>::epsilon();
  const bool inputs = a == a_before && pivots == pivots_before &&
                      norm == norm_before &&
                      uplo == std::array<char, 3>{'a', triangle, 'z'} &&
                      n == std::array<lapack_int, 3>{kGuard, order, kGuard} &&
                      lda == std::array<lapack_int, 3>{kGuard, 2, kGuard};
  bool guards = info.front() == kGuard && info.back() == kGuard &&
                condition.front() == Real{-193} &&
                condition.back() == Real{-197} && iwork.front() == kGuard &&
                iwork.back() == kGuard && work.front() == Value<T>(-179, 29);
  for (std::size_t i = 1 + 2 * static_cast<std::size_t>(n[1]); i < work.size();
       ++i) {
    guards = guards && work[i] == Value<T>(-179, 29);
  }
  for (std::size_t i =
           1 + (asc::DenseBlasComplex<T> ? 0 : static_cast<std::size_t>(n[1]));
       i < iwork.size(); ++i) {
    guards = guards && iwork[i] == kGuard;
  }
  std::printf(
      "ABI real-bytes=%zu complex=%d hermitian=%d triangle=%c mode=%d "
      "info=%lld rcond=%Lg pass=%d\n",
      sizeof(Real), static_cast<int>(asc::DenseBlasComplex<T>),
      static_cast<int>(hermitian), triangle, mode,
      static_cast<long long>(info[1]), static_cast<long double>(condition[1]),
      static_cast<int>(output && inputs && guards));
  return output && inputs && guards;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  bool passed = true;
  int cases = 0;
  for (const char triangle : {'U', 'L'}) {
    for (const int mode : {0, 1, 2, 3}) {
      passed = Probe<float>(false, triangle, mode) && passed;
      passed = Probe<double>(false, triangle, mode) && passed;
      passed = Probe<std::complex<float>>(false, triangle, mode) && passed;
      passed = Probe<std::complex<double>>(false, triangle, mode) && passed;
      passed = Probe<std::complex<float>>(true, triangle, mode) && passed;
      passed = Probe<std::complex<double>>(true, triangle, mode) && passed;
      cases += 6;
    }
  }
  std::printf("direct rook condition ABI cases=%d\n", cases);
  return passed ? 0 : 1;
}
