#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iostream>
#include <limits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_condition.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "asc/dense/providers/lapack_cholesky_equilibration.h"
#include "asc/dense/providers/lapack_cholesky_refinement.h"
#include "cholesky_expert_test_support.h"
#include "cholesky_test_support.h"

namespace {
using asc_cholesky_expert_test::CheckSuccess;
using asc_cholesky_expert_test::Scratch;
using asc_cholesky_expert_test::Vector;
using asc_cholesky_test::CheckFactor;
using asc_cholesky_test::CheckRatio;
using asc_cholesky_test::CheckSolution;
using asc_cholesky_test::Fill;
using asc_cholesky_test::FillRhs;
using asc_cholesky_test::kLayouts;
using asc_cholesky_test::kTriangles;
using asc_cholesky_test::Layout;
using asc_cholesky_test::Matrix;
using asc_cholesky_test::Narrow;
using asc_cholesky_test::NotANumber;
using asc_cholesky_test::Take;
using asc_cholesky_test::TestContext;
using asc_cholesky_test::Triangle;
using asc_cholesky_test::Widen;
using asc_cholesky_test::WithoutAllocation;
using Equed = asc::LapackCholeskyEquilibration;
std::size_t g_blocked = 0;
std::size_t g_scaled_partials = 0;
std::size_t g_wide_stride = 0;

template <typename T>
auto WideView(T& value) {
  return Take(asc::DenseBlasMatrixView<T>::Create(
      &value, 1, 1, Layout::kRowMajor,
      std::numeric_limits<asc::stride_t>::max(),
      {&value, sizeof(value), asc::MemorySpace::kHost}));
}

template <typename T>
void WideFactorModes(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  T a = Narrow<T>({4, 0});
  T af{};
  T b = Narrow<T>({6, 0});
  T x{};
  const auto av = WideView(a);
  const auto afv = WideView(af);
  const auto bv = WideView(b);
  const auto xv = WideView(x);
  const auto ac = static_cast<asc::DenseBlasMatrixView<const T>>(av);
  const auto afc = static_cast<asc::DenseBlasMatrixView<const T>>(afv);
  std::array<Real, 1> s{11};
  std::array<Real, 1> ferr{12};
  std::array<Real, 1> berr{13};
  Real rcond = 14;
  Scratch<T> scratch;
  asc::LapackReport report;
  Equed equed = Equed::kDiagonal;
  const auto equilibrated = Take(asc::QueryPosvxEquilibratedWorkspace(
      provider, Triangle::kUpper, av, afv, equed, Vector(s, 1), bv, xv,
      Vector(ferr, 1), Vector(berr, 1), rcond));
  ASC_DENSE_TEST_CHECK(
      test, asc::PosvxEquilibrated(provider, Triangle::kUpper, av, afv, equed,
                                   Vector(s, 1), bv, xv, Vector(ferr, 1),
                                   Vector(berr, 1), rcond, equilibrated,
                                   scratch.view(equilibrated), report)
                .ok());
  ASC_DENSE_TEST_EQ(test, equed, Equed::kNone);
  ASC_DENSE_TEST_EQ(test, x, Narrow<T>({1.5L, 0}));
  const auto supplied = Take(asc::QueryPosvxFactoredWorkspace(
      provider, Triangle::kUpper, ac, afc, equed, Vector(s, 1), bv, xv,
      Vector(ferr, 1), Vector(berr, 1), rcond));
  ASC_DENSE_TEST_CHECK(
      test,
      asc::PosvxFactored(provider, Triangle::kUpper, ac, afc, equed,
                         Vector(s, 1), bv, xv, Vector(ferr, 1), Vector(berr, 1),
                         rcond, supplied, scratch.view(supplied), report)
          .ok());
  ASC_DENSE_TEST_EQ(test, x, Narrow<T>({1.5L, 0}));
}

template <typename T>
void WideStride(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  T a = Narrow<T>({4, 0});
  T af = Narrow<T>({2, 0});
  T b = Narrow<T>({6, 0});
  T x{};
  const auto av = WideView(a);
  const auto afv = WideView(af);
  const auto bv = WideView(b);
  const auto xv = WideView(x);
  const auto ac = static_cast<asc::DenseBlasMatrixView<const T>>(av);
  const auto afc = static_cast<asc::DenseBlasMatrixView<const T>>(afv);
  const auto bc = static_cast<asc::DenseBlasMatrixView<const T>>(bv);
  std::array<Real, 1> s{11};
  std::array<Real, 1> ferr{12};
  std::array<Real, 1> berr{13};
  Real rcond = 14;
  Real scond = 15;
  Real amax = 16;
  Scratch<T> scratch;
  asc::LapackReport report;
  const auto condition = Take(asc::QueryPoconWorkspace(
      provider, Triangle::kUpper, afc, Real{4}, rcond));
  ASC_DENSE_TEST_CHECK(
      test, asc::Pocon(provider, Triangle::kUpper, afc, Real{4}, rcond,
                       condition, scratch.view(condition), report)
                .ok());
  ASC_DENSE_TEST_EQ(test, rcond, Real{1});
  const auto equilibration =
      Take(asc::QueryPoequWorkspace(provider, ac, Vector(s, 1), scond, amax));
  ASC_DENSE_TEST_CHECK(
      test, asc::Poequ(provider, ac, Vector(s, 1), scond, amax, equilibration,
                       scratch.view(equilibration), report)
                .ok());
  ASC_DENSE_TEST_EQ(test, s[0], Real{0.5});
  const auto radix =
      Take(asc::QueryPoequbWorkspace(provider, ac, Vector(s, 1), scond, amax));
  ASC_DENSE_TEST_CHECK(
      test, asc::Poequb(provider, ac, Vector(s, 1), scond, amax, radix,
                        scratch.view(radix), report)
                .ok());
  ASC_DENSE_TEST_EQ(test, s[0], Real{0.5});
  const auto refinement =
      Take(asc::QueryPorfsWorkspace(provider, Triangle::kUpper, ac, afc, bc, xv,
                                    Vector(ferr, 1), Vector(berr, 1)));
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Porfs(provider, Triangle::kUpper, ac, afc, bc, xv, Vector(ferr, 1),
                 Vector(berr, 1), refinement, scratch.view(refinement), report)
          .ok());
  ASC_DENSE_TEST_EQ(test, x, Narrow<T>({1.5L, 0}));
  const auto driver =
      Take(asc::QueryPosvxWorkspace(provider, Triangle::kUpper, ac, afv, bc, xv,
                                    Vector(ferr, 1), Vector(berr, 1), rcond));
  // The scalar refinement has exact zero residual and denominator 12.
  // With NZ=2 and LAPACK EPS=epsilon/2, its FERR is 2*epsilon.
  ASC_DENSE_TEST_NEAR(test, ferr[0],
                      Real{2} * std::numeric_limits<Real>::epsilon(), Real{0},
                      Real{8} * std::numeric_limits<Real>::epsilon());
  ASC_DENSE_TEST_EQ(test, berr[0], Real{0});
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Posvx(provider, Triangle::kUpper, ac, afv, bc, xv, Vector(ferr, 1),
                 Vector(berr, 1), rcond, driver, scratch.view(driver), report)
          .ok());
  ASC_DENSE_TEST_EQ(test, x, Narrow<T>({1.5L, 0}));
  WideFactorModes<T>(test, provider);
  ++g_wide_stride;
}

template <typename T>
void Blocked(TestContext& test, const asc::ReferenceLapackProvider& provider,
             Triangle triangle, Layout layout, long double scale) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(65, 65, layout);
  Matrix<T> af(65, 65, layout);
  Matrix<T> b(65, 2, layout);
  Matrix<T> x(65, 2, layout);
  Fill(a, 65, triangle, scale);
  FillRhs(b, 65, 2, scale);
  const auto before_a = a.bytes();
  std::array<Real, 2> ferr{12, 13};
  std::array<Real, 2> berr{14, 15};
  Real rcond = 16;
  const auto plan = Take(asc::QueryPosvxWorkspace(
      provider, triangle, a.const_view(), af.view(), b.const_view(), x.view(),
      Vector(ferr, 2), Vector(berr, 2), rcond));
  Scratch<T> scratch;
  const auto workspace = scratch.view(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Posvx(
                                   provider, triangle, a.const_view(),
                                   af.view(), b.const_view(), x.view(),
                                   Vector(ferr, 2), Vector(berr, 2), rcond,
                                   plan, workspace, report);
                             }).ok());
  CheckSuccess(test, provider, report, true);
  a.CheckSame(test, before_a);
  CheckFactor(test, af, 65, triangle, scale);
  CheckSolution(test, x, b, 65, 2, scale);
  ASC_DENSE_TEST_CHECK(test, rcond > 0 && rcond <= 1);
  ++g_blocked;
}

template <typename T>
void PartialEquilibrated(TestContext& test,
                         const asc::ReferenceLapackProvider& provider,
                         Triangle triangle, Layout layout, bool last) {
  using Real = asc::DenseBlasRealType<T>;
  const auto scale = static_cast<Real>(
      std::ldexp(1.L, -(std::numeric_limits<Real>::max_exponent - 8)));
  Matrix<T> a(3, 3, layout);
  Matrix<T> af(3, 3, layout);
  Matrix<T> b(3, 1, layout);
  Matrix<T> x(3, 1, layout);
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      const bool selected = triangle == Triangle::kUpper ? i <= j : i >= j;
      a(i, j) = selected ? T{} : NotANumber<T>();
    }
    a(i, i) = Narrow<T>({(i == 0 && !last ? -1.L : 4.L) * scale, 0});
    if constexpr (asc::DenseBlasComplex<T>) {
      a(i, i).imag(std::numeric_limits<Real>::quiet_NaN());
    }
    b(i, 0) = Narrow<T>({scale, 0});
  }
  if (last) {
    const auto row = triangle == Triangle::kUpper ? 1U : 2U;
    const auto col = triangle == Triangle::kUpper ? 2U : 1U;
    a(row, col) = Narrow<T>({8.L * scale, 0});
  }
  const auto original = a;
  const auto original_b = b;
  const auto before_x = x.bytes();
  std::array<Real, 3> s{11, 12, 13};
  std::array<Real, 1> ferr{21};
  std::array<Real, 1> berr{22};
  Real rcond = 23;
  // Deliberate unread output sentinel in a fixed-underlying-type enum.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  Equed equed = static_cast<Equed>(255);
  const auto plan = Take(asc::QueryPosvxEquilibratedWorkspace(
      provider, triangle, a.view(), af.view(), equed, Vector(s, 3), b.view(),
      x.view(), Vector(ferr, 1), Vector(berr, 1), rcond));
  Scratch<T> scratch;
  const auto workspace = scratch.view(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::PosvxEquilibrated(provider, triangle, a.view(), af.view(),
                                  equed, Vector(s, 3), b.view(), x.view(),
                                  Vector(ferr, 1), Vector(berr, 1), rcond, plan,
                                  workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-9999), last ? 3 : 1);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-9999),
                    last ? 2 : 0);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    asc::LapackOutcome::kNotPositiveDefinite);
  ASC_DENSE_TEST_EQ(test, rcond, Real{0});
  ASC_DENSE_TEST_EQ(test, equed, last ? Equed::kDiagonal : Equed::kNone);
  ASC_DENSE_TEST_EQ(test, ferr[0], Real{21});
  ASC_DENSE_TEST_EQ(test, berr[0], Real{22});
  x.CheckSame(test, before_x);
  if (!last) {
    a.CheckSame(test, original.bytes());
    b.CheckSame(test, original_b.bytes());
  } else {
    for (std::size_t i = 0; i < 3; ++i) {
      ASC_DENSE_TEST_EQ(test, b(i, 0), s[i] * original_b(i, 0));
      for (std::size_t j = 0; j < 3; ++j) {
        if (triangle == Triangle::kUpper ? i <= j : i >= j) {
          auto before = Widen(original(i, j));
          if (i == j) {
            before.imag(0);
          }
          const auto expected = before * static_cast<long double>(s[i]) *
                                static_cast<long double>(s[j]);
          CheckRatio(test, std::abs(Widen(a(i, j)) - expected),
                     std::abs(expected) + 1,
                     32 * std::numeric_limits<Real>::epsilon());
        }
      }
    }
  }
  ++g_scaled_partials;
}

template <typename T>
void NoRightSides(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(1, 1, Layout::kColumnMajor);
  Matrix<T> af(1, 1, Layout::kRowMajor);
  Matrix<T> b(1, 0, Layout::kRowMajor);
  Matrix<T> x(1, 0, Layout::kColumnMajor);
  a(0, 0) = Narrow<T>({4, 0});
  af(0, 0) = T{};
  std::array<Real, 1> ferr{11};
  std::array<Real, 1> berr{12};
  Real rcond = 13;
  asc::LapackReport report;
  const auto refinement = Take(asc::QueryPorfsWorkspace(
      provider, Triangle::kUpper, a.const_view(), af.const_view(),
      b.const_view(), x.view(), Vector(ferr, 0), Vector(berr, 0)));
  ASC_DENSE_TEST_CHECK(
      test, asc::Porfs(provider, Triangle::kUpper, a.const_view(),
                       af.const_view(), b.const_view(), x.view(),
                       Vector(ferr, 0), Vector(berr, 0), refinement, {}, report)
                .ok());
  CheckSuccess(test, provider, report, false);
  const auto driver = Take(asc::QueryPosvxWorkspace(
      provider, Triangle::kUpper, a.const_view(), af.view(), b.const_view(),
      x.view(), Vector(ferr, 0), Vector(berr, 0), rcond));
  Scratch<T> scratch;
  const auto workspace = scratch.view(driver);
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Posvx(
                                   provider, Triangle::kUpper, a.const_view(),
                                   af.view(), b.const_view(), x.view(),
                                   Vector(ferr, 0), Vector(berr, 0), rcond,
                                   driver, workspace, report);
                             }).ok());
  CheckSuccess(test, provider, report, true);
  ASC_DENSE_TEST_EQ(test, af(0, 0), Narrow<T>({2, 0}));
  ASC_DENSE_TEST_EQ(test, rcond, Real{1});
  ASC_DENSE_TEST_EQ(test, ferr[0], Real{11});
  ASC_DENSE_TEST_EQ(test, berr[0], Real{12});
}

template <typename T>
void All(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr int kExponent = std::numeric_limits<Real>::max_exponent / 3;
  WideStride<T>(test, provider);
  NoRightSides<T>(test, provider);
  for (const auto triangle : kTriangles) {
    for (const auto layout : kLayouts) {
      for (const auto scale :
           {1.L, std::ldexp(1.L, -kExponent), std::ldexp(1.L, kExponent)}) {
        Blocked<T>(test, provider, triangle, layout, scale);
      }
      PartialEquilibrated<T>(test, provider, triangle, layout, false);
      PartialEquilibrated<T>(test, provider, triangle, layout, true);
    }
  }
}
}  // namespace

int main() {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  All<float>(test, provider);
  All<double>(test, provider);
  All<std::complex<float>>(test, provider);
  All<std::complex<double>>(test, provider);
  std::cout << "Cholesky expert boundary: blocked_scaled_drivers=" << g_blocked
            << " equilibrated_partial_results=" << g_scaled_partials
            << " wide_ASC_stride_cases=" << g_wide_stride << '\n';
  return test.Finish();
}
