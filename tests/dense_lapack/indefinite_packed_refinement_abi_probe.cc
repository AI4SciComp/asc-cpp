#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "src/dense/lapack/internal_indefinite.h"
#include "tests/dense/test_support.h"
#include "tests/dense_lapack/indefinite_packed_fixture.h"
#include "tests/dense_lapack/indefinite_packed_native.h"
#include "tests/dense_lapack/installed_lu/normal_return_guard.h"
#if defined(ASC_REFINEMENT_SSPRFS_HEADER)
#include <type_traits>
#endif
#include "asc/dense/blas.h"
#include "tests/dense_lapack/indefinite_packed_refinement_native.h"
#if defined(ASC_REFINEMENT_SSPRFS_HEADER)
#define ssprfs_ emitted_ssprfs
#include ASC_REFINEMENT_SSPRFS_HEADER
#undef ssprfs_
#define dsprfs_ emitted_dsprfs
#include ASC_REFINEMENT_DSPRFS_HEADER
#undef dsprfs_
#define csprfs_ emitted_csprfs
#include ASC_REFINEMENT_CSPRFS_HEADER
#undef csprfs_
#define zsprfs_ emitted_zsprfs
#include ASC_REFINEMENT_ZSPRFS_HEADER
#undef zsprfs_
#define chprfs_ emitted_chprfs
#include ASC_REFINEMENT_CHPRFS_HEADER
#undef chprfs_
#define zhprfs_ emitted_zhprfs
#include ASC_REFINEMENT_ZHPRFS_HEADER
#undef zhprfs_
#endif
namespace {
namespace packed = asc_packed_indefinite_test;
#if defined(ASC_REFINEMENT_SSPRFS_HEADER)
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
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_ssprfs_base)>::Type,
                             typename Sig<decltype(emitted_ssprfs)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_dsprfs_base)>::Type,
                             typename Sig<decltype(emitted_dsprfs)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_csprfs_base)>::Type,
                             typename Sig<decltype(emitted_csprfs)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zsprfs_base)>::Type,
                             typename Sig<decltype(emitted_zsprfs)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_chprfs_base)>::Type,
                             typename Sig<decltype(emitted_chprfs)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zhprfs_base)>::Type,
                             typename Sig<decltype(emitted_zhprfs)>::Type>);

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

long double Abs1(packed::Wide value) {
  return std::abs(value.real()) + std::abs(value.imag());
}
template <typename T>
void CheckErrors(packed::TestContext& test, const packed::Sample<T>& sample,
                 int columns, lapack_int ldb, lapack_int ldx,
                 const std::array<T, 220>& b, const std::array<T, 220>& x,
                 const std::array<T, 195>& expected,
                 const std::array<asc::DenseBlasRealType<T>, 5>& ferr,
                 const std::array<asc::DenseBlasRealType<T>, 5>& berr) {
  using Real = asc::DenseBlasRealType<T>;
  const int order = sample.n;
  for (int j = 0; j < columns; ++j) {
    ASC_DENSE_TEST_CHECK(test, std::isfinite(ferr[j + 1]) && ferr[j + 1] >= 0 &&
                                   std::isfinite(berr[j + 1]) &&
                                   berr[j + 1] >= 0);
    if (order == 0) {
      ASC_DENSE_TEST_EQ(test, ferr[j + 1], Real{0});
      ASC_DENSE_TEST_EQ(test, berr[j + 1], Real{0});
      continue;
    }
    long double forward = 0;
    long double norm_x = 0;
    long double backward = 0;
    for (int i = 0; i < order; ++i) {
      const auto actual = packed::ToWide(x[1 + i + j * ldx]);
      forward = std::max(
          forward, Abs1(actual - packed::ToWide(expected[i + j * order])));
      norm_x = std::max(norm_x, Abs1(actual));
      auto residual = packed::ToWide(b[1 + i + j * ldb]);
      long double denominator = Abs1(residual);
      for (int k = 0; k < order; ++k) {
        const auto a = sample.full[i * order + k];
        const auto v = packed::ToWide(x[1 + k + j * ldx]);
        residual -= a * v;
        denominator += Abs1(a) * Abs1(v);
      }
      backward = std::max(backward, Abs1(residual) / denominator);
    }
    const long double tolerance = 128 * std::numeric_limits<Real>::epsilon();
    ASC_DENSE_TEST_CHECK(test, forward / norm_x <= tolerance);
    ASC_DENSE_TEST_CHECK(test, forward / norm_x <= ferr[j + 1] + tolerance);
    ASC_DENSE_TEST_CHECK(test, backward <= berr[j + 1] + tolerance);
  }
}
template <typename T>
void CheckWork(packed::TestContext& test, int order, int columns,
               const std::array<T, 330>& work,
               const std::array<asc::DenseBlasRealType<T>, 68>& rwork,
               const std::array<lapack_int, 68>& iwork,
               const std::array<asc::DenseBlasRealType<T>, 5>& ferr,
               const std::array<asc::DenseBlasRealType<T>, 5>& berr) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;
  for (std::size_t i = 0; i < ferr.size(); ++i) {
    if (i == 0 || i > static_cast<std::size_t>(columns)) {
      ASC_DENSE_TEST_EQ(test, ferr[i], Real{-137});
      ASC_DENSE_TEST_EQ(test, berr[i], Real{-139});
    }
  }
  const bool active = order != 0 && columns != 0;
  const auto scalar_entries =
      active
          ? static_cast<std::size_t>((asc::DenseBlasComplex<T> ? 2 : 3) * order)
          : 0;
  for (std::size_t i = 0; i < work.size(); ++i) {
    if (i == 0 || i > scalar_entries) {
      ASC_DENSE_TEST_EQ(test, work[i], packed::Value<T>(-103, 11));
    }
  }
  for (std::size_t i = 0; i < rwork.size(); ++i) {
    if (!active || !asc::DenseBlasComplex<T> || i == 0 ||
        i > static_cast<std::size_t>(order)) {
      ASC_DENSE_TEST_EQ(test, rwork[i], Real{-131});
    }
    if (!active || asc::DenseBlasComplex<T> || i == 0 ||
        i > static_cast<std::size_t>(order)) {
      ASC_DENSE_TEST_EQ(test, iwork[i], kGuard);
    }
  }
}
template <typename T>
void CheckPadding(packed::TestContext& test, int order, int columns,
                  lapack_int ldx, const std::array<T, 220>& x,
                  const std::array<T, 220>& before_x) {
  for (std::size_t k = 0; k < x.size(); ++k) {
    bool used = false;
    for (int j = 0; j < columns; ++j) {
      used = used || (k >= 1 + static_cast<std::size_t>(j * ldx) &&
                      k < 1 + static_cast<std::size_t>(j * ldx + order));
    }
    if (!used) {
      ASC_DENSE_TEST_EQ(test, x[k], before_x[k]);
    }
  }
}
template <typename T>
void Case(packed::TestContext& test, bool he, char triangle, int order,
          int columns, int exponent, int kind) {
  using Real = asc::DenseBlasRealType<T>;
  packed::Sample<T> sample(order, he,
                           triangle == 'U' ? packed::kUpper : packed::kLower,
                           packed::kColumn, exponent);
  Initialize(sample, exponent, kind);
  const lapack_int n = order;
  const lapack_int nrhs = columns;
  const lapack_int ldb = order + 3;
  const lapack_int ldx = order + 5;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;
  std::array<lapack_int, 68> pivots;
  std::array<lapack_int, 68> iwork;
  pivots.fill(kGuard);
  iwork.fill(kGuard);
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  packed::Native(he, &triangle, &n, sample.a.data() + 1, pivots.data() + 1,
                 &info[1]);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  for (int i = 0; i < order; ++i) {
    sample.pivots[i + 1] = pivots[i + 1];
  }
  sample.Reconstruction(test);
  const auto before_a = sample.a;
  const auto before_original = sample.original;
  const auto before_pivots = pivots;
  std::array<T, 220> b;
  std::array<T, 220> x;
  std::array<T, 330> work;
  std::array<Real, 68> rwork;
  b.fill(packed::Value<T>(-113, 17));
  x.fill(packed::Value<T>(-127, 19));
  work.fill(packed::Value<T>(-103, 11));
  rwork.fill(Real{-131});
  std::array<T, 195> expected{};
  for (int j = 0; j < columns; ++j) {
    for (int i = 0; i < order; ++i) {
      expected[i + j * order] =
          packed::Value<T>(1 + i % 3 + j, 0.25L * (1 + i % 2));
      x[1 + i + j * ldx] = expected[i + j * order] * Real{1.0625};
    }
  }
  for (int j = 0; j < columns; ++j) {
    for (int i = 0; i < order; ++i) {
      packed::Wide sum{};
      for (int k = 0; k < order; ++k) {
        sum += sample.full[i * order + k] *
               packed::ToWide(expected[k + j * order]);
      }
      b[1 + i + j * ldb] = packed::Value<T>(sum.real(), sum.imag());
    }
  }
  const auto before_b = b;
  const auto before_x = x;
  std::array<Real, 5> ferr;
  std::array<Real, 5> berr;
  ferr.fill(Real{-137});
  berr.fill(Real{-139});
  for (int j = 0; j < columns; ++j) {
    ferr[j + 1] = berr[j + 1] = std::numeric_limits<Real>::quiet_NaN();
  }
  info[1] = std::numeric_limits<lapack_int>::min();
  asc_packed_refinement_test::Native(
      he, &triangle, &n, &nrhs, sample.original.data() + 1, sample.a.data() + 1,
      pivots.data() + 1, b.data() + 1, &ldb, x.data() + 1, &ldx,
      ferr.data() + 1, berr.data() + 1, work.data() + 1, rwork.data() + 1,
      iwork.data() + 1, &info[1]);
  ASC_DENSE_TEST_EQ(test, info[0], kGuard);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  ASC_DENSE_TEST_EQ(test, info[2], kGuard);
  ASC_DENSE_TEST_EQ(test, pivots, before_pivots);
  ASC_DENSE_TEST_EQ(test, b, before_b);
  ASC_DENSE_TEST_EQ(test, sample.a, before_a);
  ASC_DENSE_TEST_EQ(test, sample.original, before_original);
  CheckPadding(test, order, columns, ldx, x, before_x);
  CheckErrors(test, sample, columns, ldb, ldx, b, x, expected, ferr, berr);
  CheckWork(test, order, columns, work, rwork, iwork, ferr, berr);
  sample.Guards(test);
}
template <typename T>
int Run(packed::TestContext& test, bool he) {
  int cases = 0;
  for (int n : {0, 1, 2, 5, 17, 65}) {
    for (int nrhs : {0, 1, 3}) {
      for (char triangle : {'U', 'L'}) {
        for (int exponent : {-20, 0, 20}) {
          for (int kind : {0, 3}) {
            Case<T>(test, he, triangle, n, nrhs, exponent, kind);
            ++cases;
          }
        }
      }
    }
  }
  return cases;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  packed::TestContext test;
  int cases = Run<float>(test, false) + Run<double>(test, false) +
              Run<std::complex<float>>(test, false) +
              Run<std::complex<double>>(test, false) +
              Run<std::complex<float>>(test, true) +
              Run<std::complex<double>>(test, true);
  ASC_DENSE_TEST_EQ(test, cases, 1296);
  const int result = test.Finish();
  std::printf("Packed refinement guarded native cases=%d failed=%d\n", cases,
              result);
  return result;
}
