#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iostream>
#include <limits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "asc/dense/providers/lapack_cholesky_refinement.h"
#include "cholesky_expert_test_support.h"
#include "cholesky_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_cholesky_expert_test::CheckSuccess;
using asc_cholesky_expert_test::Scratch;
using asc_cholesky_expert_test::Vector;
using asc_cholesky_test::CheckFactor;
using asc_cholesky_test::CheckRatio;
using asc_cholesky_test::CheckSolution;
using asc_cholesky_test::Coefficient;
using asc_cholesky_test::Fill;
using asc_cholesky_test::FillRhs;
using asc_cholesky_test::kLayouts;
using asc_cholesky_test::kTriangles;
using asc_cholesky_test::Layout;
using asc_cholesky_test::Lower;
using asc_cholesky_test::Matrix;
using asc_cholesky_test::Narrow;
using asc_cholesky_test::Solution;
using asc_cholesky_test::Take;
using asc_cholesky_test::TestContext;
using asc_cholesky_test::Triangle;
using asc_cholesky_test::Wide;
using asc_cholesky_test::Widen;
using asc_cholesky_test::WithoutAllocation;
using Equed = asc::LapackCholeskyEquilibration;
std::size_t g_refinements = 0;
std::size_t g_drivers = 0;
std::size_t g_factored = 0;
std::size_t g_reused = 0;
std::size_t g_partial = 0;

template <typename T>
void Factor(TestContext& test, const asc::ReferenceLapackProvider& provider,
            Matrix<T>& factors, Triangle triangle) {
  Scratch<T> scratch;
  const auto plan =
      Take(asc::QueryPotrfWorkspace(provider, triangle, factors.view()));
  const auto workspace = scratch.view(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Potrf(provider, triangle,
                                                 factors.view(), plan,
                                                 workspace, report);
                             }).ok());
}

template <typename T>
void CheckErrors(TestContext& test, const Matrix<T>& x, const Matrix<T>& b,
                 std::size_t n, std::size_t nrhs, long double scale,
                 const std::array<asc::DenseBlasRealType<T>, 3>& ferr,
                 const std::array<asc::DenseBlasRealType<T>, 3>& berr) {
  using Real = asc::DenseBlasRealType<T>;
  const auto magnitude = [](Wide value) {
    return std::abs(value.real()) + std::abs(value.imag());
  };
  for (std::size_t j = 0; j < nrhs; ++j) {
    long double backward = 0;
    for (std::size_t i = 0; i < n; ++i) {
      Wide residual = Widen(b(i, j));
      long double denominator = magnitude(residual);
      for (std::size_t k = 0; k < n; ++k) {
        const auto a = Widen(Narrow<T>(Coefficient<T>(n, i, k) * scale));
        residual -= a * Widen(x(k, j));
        denominator += magnitude(a) * magnitude(Widen(x(k, j)));
      }
      if (denominator != 0) {
        backward = std::max(backward, magnitude(residual) / denominator);
      }
    }
    ASC_DENSE_TEST_CHECK(test, std::isfinite(ferr[j]) && ferr[j] >= 0);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(berr[j]) && berr[j] >= 0);
    // Independently accumulated high-precision residual need not equal the
    // provider's rounded working-precision residual bit for bit.
    ASC_DENSE_TEST_CHECK(
        test, backward <= berr[j] + 64 * std::numeric_limits<Real>::epsilon());
  }
}

template <typename T>
void Refine(TestContext& test, const asc::ReferenceLapackProvider& provider,
            Triangle triangle, const std::array<Layout, 4>& layouts,
            long double scale) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(3, 3, layouts[0]);
  Matrix<T> af(3, 3, layouts[1]);
  Matrix<T> b(3, 2, layouts[2]);
  Matrix<T> x(3, 2, layouts[3]);
  Fill(a, 3, triangle, scale);
  Fill(af, 3, triangle, scale);
  FillRhs(b, 3, 2, scale);
  Factor(test, provider, af, triangle);
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      x(i, j) = Narrow<T>(Solution<T>(i, j) + Wide{0.125L, 0});
    }
  }
  const auto before_a = a.bytes();
  const auto before_af = af.bytes();
  const auto before_b = b.bytes();
  const auto before_x = x.bytes();
  std::array<Real, 3> ferr{71, 72, 73};
  std::array<Real, 3> berr{81, 82, 83};
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPorfsWorkspace(provider, triangle, a.const_view(),
                                    af.const_view(), b.const_view(), x.view(),
                                    Vector(ferr, 2), Vector(berr, 2));
  }));
  Scratch<T> scratch;
  const auto workspace = scratch.view(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Porfs(
                                   provider, triangle, a.const_view(),
                                   af.const_view(), b.const_view(), x.view(),
                                   Vector(ferr, 2), Vector(berr, 2), plan,
                                   workspace, report);
                             }).ok());
  CheckSuccess(test, provider, report, true);
  a.CheckSame(test, before_a);
  af.CheckSame(test, before_af);
  b.CheckSame(test, before_b);
  x.CheckUntouched(test, before_x, triangle, true);
  CheckSolution(test, x, b, 3, 2, scale);
  CheckErrors(test, x, b, 3, 2, scale, ferr, berr);
  ASC_DENSE_TEST_EQ(test, ferr[2], Real{73});
  ASC_DENSE_TEST_EQ(test, berr[2], Real{83});
  ++g_refinements;
}

template <typename T>
void Reuse(TestContext& test, const asc::ReferenceLapackProvider& provider,
           Matrix<T>& af, Triangle triangle, Layout layout,
           const asc::LapackReport& factor_report, long double scale) {
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      af.const_view(), triangle, factor_report));
  Matrix<T> rhs(3, 2, layout);
  FillRhs(rhs, 3, 2, scale);
  const auto original = rhs;
  Scratch<T> scratch;
  const auto plan =
      Take(asc::QueryPotrsWorkspace(provider, factor, rhs.view()));
  const auto workspace = scratch.view(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Potrs(provider, factor, rhs.view(),
                                                 plan, workspace, report);
                             }).ok());
  CheckSolution(test, rhs, original, 3, 2, scale);
  ++g_reused;
}

template <typename T>
void NewDriver(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Triangle triangle, const std::array<Layout, 4>& layouts,
               long double scale) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(3, 3, layouts[0]);
  Matrix<T> af(3, 3, layouts[1]);
  Matrix<T> b(3, 2, layouts[2]);
  Matrix<T> x(3, 2, layouts[3]);
  Fill(a, 3, triangle, scale);
  FillRhs(b, 3, 2, scale);
  const auto before_a = a.bytes();
  const auto before_af = af.bytes();
  const auto before_b = b.bytes();
  const auto before_x = x.bytes();
  std::array<Real, 3> ferr{71, 72, 73};
  std::array<Real, 3> berr{81, 82, 83};
  Real rcond = 97;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPosvxWorkspace(provider, triangle, a.const_view(),
                                    af.view(), b.const_view(), x.view(),
                                    Vector(ferr, 2), Vector(berr, 2), rcond);
  }));
  const asc::extent_t packed =
      (layouts[0] == Layout::kRowMajor || asc::DenseBlasComplex<T> ? 9 : 0) +
      (layouts[1] == Layout::kRowMajor ? 9 : 0) +
      (layouts[2] == Layout::kRowMajor ? 6 : 0) +
      (layouts[3] == Layout::kRowMajor ? 6 : 0);
  ASC_DENSE_TEST_EQ(
      test, plan.regions[asc_cholesky_expert_test::kPacking].minimum_entries,
      packed);
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
  b.CheckSame(test, before_b);
  af.CheckUntouched(test, before_af, triangle);
  x.CheckUntouched(test, before_x, triangle, true);
  CheckFactor(test, af, 3, triangle, scale);
  CheckSolution(test, x, b, 3, 2, scale);
  CheckErrors(test, x, b, 3, 2, scale, ferr, berr);
  ASC_DENSE_TEST_CHECK(test, rcond > 0 && rcond <= 1);
  Reuse(test, provider, af, triangle, layouts[2], report, scale);
  ++g_drivers;
}

template <typename T>
void CheckEquilibrated(TestContext& test, const Matrix<T>& a,
                       const Matrix<T>& af, Triangle triangle,
                       const std::array<asc::DenseBlasRealType<T>, 3>& scales,
                       long double scale, bool applied) {
  using Real = asc::DenseBlasRealType<T>;
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      auto expected = Coefficient<T>(3, i, j) * scale;
      if (applied) {
        expected *= static_cast<long double>(scales[i]) * scales[j];
      }
      Wide product{};
      for (std::size_t k = 0; k < 3; ++k) {
        product +=
            Lower(af, triangle, i, k) * std::conj(Lower(af, triangle, j, k));
      }
      CheckRatio(test, std::abs(product - expected), std::abs(expected) + scale,
                 512 * std::numeric_limits<Real>::epsilon());
      if (triangle == Triangle::kLower ? i >= j : i <= j) {
        auto actual = Widen(a(i, j));
        if (i == j) {
          actual.imag(0);
        }
        CheckRatio(test, std::abs(actual - expected),
                   std::abs(expected) + scale,
                   128 * std::numeric_limits<Real>::epsilon());
      }
    }
  }
}

template <typename T>
void EquilibratedDriver(TestContext& test,
                        const asc::ReferenceLapackProvider& provider,
                        Triangle triangle, const std::array<Layout, 4>& layouts,
                        long double scale, bool applied) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(3, 3, layouts[0]);
  Matrix<T> af(3, 3, layouts[1]);
  Matrix<T> b(3, 2, layouts[2]);
  Matrix<T> x(3, 2, layouts[3]);
  Fill(a, 3, triangle, scale);
  FillRhs(b, 3, 2, scale);
  const auto original_b = b;
  const auto before_a = a.bytes();
  const auto before_af = af.bytes();
  std::array<Real, 3> s{11, 12, 13};
  std::array<Real, 3> ferr{71, 72, 73};
  std::array<Real, 3> berr{81, 82, 83};
  Real rcond = 97;
  // Deliberate unread output sentinel in a fixed-underlying-type enum.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  Equed equed = static_cast<Equed>(255);
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPosvxEquilibratedWorkspace(
        provider, triangle, a.view(), af.view(), equed, Vector(s, 3), b.view(),
        x.view(), Vector(ferr, 2), Vector(berr, 2), rcond);
  }));
  Scratch<T> scratch;
  auto workspace = scratch.view(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::PosvxEquilibrated(
                                   provider, triangle, a.view(), af.view(),
                                   equed, Vector(s, 3), b.view(), x.view(),
                                   Vector(ferr, 2), Vector(berr, 2), rcond,
                                   plan, workspace, report);
                             }).ok());
  CheckSuccess(test, provider, report, true);
  ASC_DENSE_TEST_EQ(test, equed, applied ? Equed::kDiagonal : Equed::kNone);
  af.CheckUntouched(test, before_af, triangle);
  if (applied) {
    a.CheckUntouched(test, before_a, triangle);
  } else {
    a.CheckSame(test, before_a);
  }
  CheckEquilibrated(test, a, af, triangle, s, scale, applied);
  CheckSolution(test, x, original_b, 3, 2, scale);
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      ASC_DENSE_TEST_EQ(test, b(i, j),
                        applied ? s[i] * original_b(i, j) : original_b(i, j));
    }
  }
  ++g_drivers;
  b = original_b;
  const auto saved_a = a.bytes();
  const auto saved_af = af.bytes();
  const auto saved_s = s;
  const auto supplied = Take(asc::QueryPosvxFactoredWorkspace(
      provider, triangle, a.const_view(), af.const_view(), equed, Vector(s, 3),
      b.view(), x.view(), Vector(ferr, 2), Vector(berr, 2), rcond));
  workspace = scratch.view(supplied);
  ASC_DENSE_TEST_CHECK(
      test, WithoutAllocation(test, [&] {
              return asc::PosvxFactored(
                  provider, triangle, a.const_view(), af.const_view(), equed,
                  Vector(s, 3), b.view(), x.view(), Vector(ferr, 2),
                  Vector(berr, 2), rcond, supplied, workspace, report);
            }).ok());
  CheckSuccess(test, provider, report, true);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  CheckSolution(test, x, original_b, 3, 2, scale);
  a.CheckSame(test, saved_a);
  af.CheckSame(test, saved_af);
  ASC_DENSE_TEST_EQ(test, s, saved_s);
  ++g_factored;
}

template <typename T>
void Partial(TestContext& test, const asc::ReferenceLapackProvider& provider,
             Layout layout, Triangle triangle, int bad) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(3, 3, layout);
  Matrix<T> af(3, 3, layout);
  Matrix<T> b(3, 2, layout);
  Matrix<T> x(3, 2, layout);
  Fill(a, 3, triangle, 1);
  FillRhs(b, 3, 2, 1);
  a(static_cast<std::size_t>(bad), static_cast<std::size_t>(bad)) =
      Narrow<T>({-1, 0});
  if constexpr (asc::DenseBlasComplex<T>) {
    a(static_cast<std::size_t>(bad), static_cast<std::size_t>(bad))
        .imag(std::numeric_limits<Real>::quiet_NaN());
  }
  const auto before_x = x.bytes();
  const auto before_af = af.bytes();
  const auto before_a = a.bytes();
  const auto before_b = b.bytes();
  std::array<Real, 3> ferr{71, 72, 73};
  std::array<Real, 3> berr{81, 82, 83};
  Real rcond = 97;
  const auto plan = Take(asc::QueryPosvxWorkspace(
      provider, triangle, a.const_view(), af.view(), b.const_view(), x.view(),
      Vector(ferr, 2), Vector(berr, 2), rcond));
  Scratch<T> scratch;
  const auto workspace = scratch.view(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Posvx(provider, triangle, a.const_view(), af.view(),
                      b.const_view(), x.view(), Vector(ferr, 2),
                      Vector(berr, 2), rcond, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-9999), bad + 1);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-9999), bad);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    asc::LapackOutcome::kNotPositiveDefinite);
  ASC_DENSE_TEST_EQ(test, rcond, Real{0});
  ASC_DENSE_TEST_EQ(test, ferr[0], Real{71});
  ASC_DENSE_TEST_EQ(test, berr[0], Real{81});
  a.CheckSame(test, before_a);
  b.CheckSame(test, before_b);
  x.CheckSame(test, before_x);
  af.CheckUntouched(test, before_af, triangle);
  if constexpr (asc::DenseBlasComplex<T>) {
    if (layout == Layout::kRowMajor) {
      for (std::size_t i = 0; i < 3; ++i) {
        ASC_DENSE_TEST_EQ(test, af(i, i).imag(),
                          before_af[af.Offset(i, i)].imag());
      }
    }
  }
  ASC_DENSE_TEST_CHECK(test, !asc::LapackCholeskyFactorView<T>::Create(
                                  af.const_view(), triangle, report)
                                  .ok());
  ++g_partial;
}

template <typename T>
void All(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr int kSmall = -(std::numeric_limits<Real>::max_exponent - 8);
  for (const auto triangle : kTriangles) {
    for (const auto a : kLayouts) {
      for (const auto af : kLayouts) {
        for (const auto b : kLayouts) {
          for (const auto x : kLayouts) {
            const std::array layouts{a, af, b, x};
            Refine<T>(test, provider, triangle, layouts, 1);
            NewDriver<T>(test, provider, triangle, layouts, 1);
            EquilibratedDriver<T>(test, provider, triangle, layouts, 1, false);
            EquilibratedDriver<T>(test, provider, triangle, layouts,
                                  std::ldexp(1.L, kSmall), true);
          }
        }
      }
      for (const int bad : {0, 2}) {
        Partial<T>(test, provider, a, triangle, bad);
      }
    }
  }
}

}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  All<float>(test, provider);
  All<double>(test, provider);
  All<std::complex<float>>(test, provider);
  All<std::complex<double>>(test, provider);
  std::cout << "Cholesky expert solves: refinements=" << g_refinements
            << " new_or_equilibrated_drivers=" << g_drivers
            << " supplied_factor_drivers=" << g_factored
            << " direct_factor_reuses=" << g_reused
            << " positive_factor_failures=" << g_partial << '\n';
  return test.Finish();
}
