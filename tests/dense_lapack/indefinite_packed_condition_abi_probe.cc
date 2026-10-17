#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#if defined(ASC_CONDITION_SSPCON_HEADER)
#include <type_traits>
#endif

#include "asc/dense/blas.h"
#include "src/dense/lapack/internal_indefinite.h"
#include "tests/dense/test_support.h"
#include "tests/dense_lapack/indefinite_packed_condition_native.h"
#include "tests/dense_lapack/indefinite_packed_fixture.h"
#include "tests/dense_lapack/indefinite_packed_native.h"
#include "tests/dense_lapack/installed_lu/normal_return_guard.h"
#if defined(ASC_CONDITION_SSPCON_HEADER)
#define sspcon_ emitted_sspcon
#include ASC_CONDITION_SSPCON_HEADER
#undef sspcon_
#define dspcon_ emitted_dspcon
#include ASC_CONDITION_DSPCON_HEADER
#undef dspcon_
#define cspcon_ emitted_cspcon
#include ASC_CONDITION_CSPCON_HEADER
#undef cspcon_
#define zspcon_ emitted_zspcon
#include ASC_CONDITION_ZSPCON_HEADER
#undef zspcon_
#define chpcon_ emitted_chpcon
#include ASC_CONDITION_CHPCON_HEADER
#undef chpcon_
#define zhpcon_ emitted_zhpcon
#include ASC_CONDITION_ZHPCON_HEADER
#undef zhpcon_
#endif
namespace {
namespace packed = asc_packed_indefinite_test;
#if defined(ASC_CONDITION_SSPCON_HEADER)
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
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_sspcon_base)>::Type,
                             typename Sig<decltype(emitted_sspcon)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_dspcon_base)>::Type,
                             typename Sig<decltype(emitted_dspcon)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_cspcon_base)>::Type,
                             typename Sig<decltype(emitted_cspcon)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zspcon_base)>::Type,
                             typename Sig<decltype(emitted_zspcon)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_chpcon_base)>::Type,
                             typename Sig<decltype(emitted_chpcon)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zhpcon_base)>::Type,
                             typename Sig<decltype(emitted_zhpcon)>::Type>);

#endif

template <typename T>
void Initialize(packed::Sample<T>& sample, int exponent, int kind) {
  const auto scale = std::ldexp(1.0L, exponent);
  for (int j = 0; j < sample.n; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      auto value = packed::Value<T>(i == j && kind != 1 ? 4 * scale : 0);
      if (kind == 3 && sample.n >= 2 && i < 2 && j < 2) {
        value = i == j ? T{} : packed::Value<T>(2 * scale, 0.5L * scale);
        if constexpr (asc::DenseBlasComplex<T>) {
          if (sample.hermitian && i < j) {
            value = std::conj(value);
          }
        }
      }
      sample.full[i * sample.n + j] = packed::ToWide(value);
      if (sample.Selected(i, j)) {
        sample.a[sample.Offset(i, j)] = value;
      }
    }
  }
  sample.original = sample.a;
}

template <typename T>
void Case(packed::TestContext& test, bool he, char triangle, int order,
          int exponent, int kind) {
  using Real = asc::DenseBlasRealType<T>;
  packed::Sample<T> sample(order, he,
                           triangle == 'U' ? packed::kUpper : packed::kLower,
                           packed::kColumn, exponent);
  Initialize(sample, exponent, kind);
  const lapack_int n = order;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;
  std::array<lapack_int, 68> pivots;
  std::array<lapack_int, 68> iwork;
  pivots.fill(kGuard);
  iwork.fill(kGuard);
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  packed::Native(he, &triangle, &n, sample.a.data() + 1, pivots.data() + 1,
                 &info[1]);
  const int singular_index = triangle == 'U' ? order : 1;
  const int expected_factor_info = order != 0 && kind == 1 ? singular_index : 0;
  ASC_DENSE_TEST_EQ(test, info[1], expected_factor_info);
  for (int i = 0; i < order; ++i) {
    sample.pivots[i + 1] = pivots[i + 1];
  }
  sample.Reconstruction(test);
  const auto before = sample.a;
  const auto before_pivots = pivots;
  long double norm = 0;
  for (int j = 0; j < order; ++j) {
    long double column = 0;
    for (int i = 0; i < order; ++i) {
      column += std::abs(sample.full[i * order + j]);
    }
    norm = std::max(norm, column);
  }
  const Real anorm = static_cast<Real>(norm);
  std::array<T, 132> work;
  work.fill(packed::Value<T>(-103, 11));
  std::array<Real, 3> rcond{Real{-107}, std::numeric_limits<Real>::quiet_NaN(),
                            Real{-109}};
  info[1] = std::numeric_limits<lapack_int>::min();
  asc_packed_condition_test::Native(
      he, &triangle, &n, sample.a.data() + 1, pivots.data() + 1, &anorm,
      &rcond[1], work.data() + 1, iwork.data() + 1, &info[1]);
  ASC_DENSE_TEST_EQ(test, info[0], kGuard);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  ASC_DENSE_TEST_EQ(test, info[2], kGuard);
  ASC_DENSE_TEST_EQ(test, pivots, before_pivots);
  ASC_DENSE_TEST_CHECK(
      test, packed::EqualBytes(sample.a.data(), before.data(), sizeof(before)));
  ASC_DENSE_TEST_EQ(test, rcond.front(), Real{-107});
  ASC_DENSE_TEST_EQ(test, rcond.back(), Real{-109});
  for (std::size_t i = 0; i < work.size(); ++i) {
    if (i == 0 || i >= 2 * static_cast<std::size_t>(order) + 1) {
      ASC_DENSE_TEST_EQ(test, work[i], packed::Value<T>(-103, 11));
    }
  }
  for (std::size_t i = 0; i < iwork.size(); ++i) {
    if (asc::DenseBlasComplex<T> || i == 0 ||
        i >= static_cast<std::size_t>(order) + 1) {
      ASC_DENSE_TEST_EQ(test, iwork[i], kGuard);
    }
  }
  long double expected = kind == 1 ? 0 : 1;
  if (kind == 3 && order > 2) {
    expected = std::abs(packed::ToWide(packed::Value<T>(2, 0.5L))) / 4;
  }
  if (order == 0) {
    expected = 1;
  }
  ASC_DENSE_TEST_CHECK(test, std::isfinite(rcond[1]));
  ASC_DENSE_TEST_CHECK(
      test, std::abs(static_cast<long double>(rcond[1]) - expected) <=
                128 * std::numeric_limits<Real>::epsilon() *
                    std::max(1.0L, expected));
  sample.Guards(test);
}
template <typename T>
int Run(packed::TestContext& test, bool he) {
  int cases = 0;
  for (int n : {0, 1, 2, 5, 17, 65}) {
    for (char triangle : {'U', 'L'}) {
      for (int exponent : {-20, 0, 20}) {
        for (int kind : {0, 1, 3}) {
          Case<T>(test, he, triangle, n, exponent, kind);
          ++cases;
        }
      }
    }
  }
  return cases;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  packed::TestContext test;
  int cases = Run<float>(test, false) + Run<double>(test, false);
  cases += Run<std::complex<float>>(test, false) +
           Run<std::complex<double>>(test, false);
  cases += Run<std::complex<float>>(test, true) +
           Run<std::complex<double>>(test, true);
  ASC_DENSE_TEST_EQ(test, cases, 648);
  const auto code = test.Finish();
  std::printf("Packed condition guarded native cases=%d failed=%d\n", cases,
              code);
  return code;
}
