#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "asc/dense/providers/lapack_cholesky_band_refinement.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_estimation_test::WorkspaceStorage;

template <typename Real>
asc::DenseBlasVectorView<Real> Vector(std::vector<Real>& values,
                                      asc::extent_t count) {
  return Take(asc::DenseBlasVectorView<Real>::Create(
      values.data() + 1, count, 1,
      {values.data(), values.size() * sizeof(Real), kHost}));
}
long double Abs1(Wide value) {
  return std::abs(value.real()) + std::abs(value.imag());
}
long double Ratio(long double numerator, long double denominator) {
  if (denominator != 0) {
    return numerator / denominator;
  }
  return numerator == 0 ? 0 : std::numeric_limits<long double>::infinity();
}

template <typename T>
asc::DenseBlasMatrixView<const T> ConstView(const RhsData<T>& rhs) {
  return Take(asc::DenseBlasMatrixView<const T>::Create(
      rhs.values.data() + 1, rhs.n, rhs.count, rhs.layout, rhs.ld,
      {rhs.values.data(), rhs.values.size() * sizeof(T), kHost}));
}

template <typename T>
void CheckEstimates(TestContext& test, const BandData<T>& a,
                    const RhsData<T>& x,
                    const std::vector<asc::DenseBlasRealType<T>>& ferr,
                    const std::vector<asc::DenseBlasRealType<T>>& berr) {
  using Real = asc::DenseBlasRealType<T>;
  const auto eps = std::numeric_limits<Real>::epsilon();
  const long double safe1 =
      std::min(a.n + 1, 2 * a.kd + 2) *
      static_cast<long double>(std::numeric_limits<Real>::min());
  const long double safe2 = safe1 / (eps / 2);
  for (asc::extent_t j = 0; j < x.count; ++j) {
    long double error = 0;
    long double norm = 0;
    long double backward = 0;
    long double guarded_backward = 0;
    for (asc::extent_t i = 0; i < x.n; ++i) {
      const auto xi = ToWide(x.values[x.Index(i, j)]);
      const auto expected =
          x.expected[static_cast<std::size_t>(i * x.count + j)];
      error = std::max(error, Abs1(xi - expected));
      norm = std::max(norm, Abs1(xi));
      const auto b = x.original[static_cast<std::size_t>(i * x.count + j)];
      Wide residual = b;
      long double denominator = Abs1(b);
      for (asc::extent_t k = 0; k < x.n; ++k) {
        const auto aik = a.original[static_cast<std::size_t>(i * x.n + k)];
        const auto xk = ToWide(x.values[x.Index(k, j)]);
        residual -= aik * xk;
        denominator += Abs1(aik) * Abs1(xk);
      }
      const auto ratio = Ratio(Abs1(residual), denominator);
      backward = std::max(backward, ratio);
      const auto guarded = denominator > safe2 ? Abs1(residual) / denominator
                                               : (Abs1(residual) + safe1) /
                                                     (denominator + safe1);
      guarded_backward = std::max(guarded_backward, guarded);
    }
    const auto f = ferr[static_cast<std::size_t>(j + 1)];
    const auto b = berr[static_cast<std::size_t>(j + 1)];
    ASC_DENSE_TEST_CHECK(test, std::isfinite(f) && f >= 0);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(b) && b >= 0);
    ASC_DENSE_TEST_CHECK(
        test, backward <= 16 * std::max<asc::extent_t>(1, x.n) * eps);
    // Source safe-minimum guards yield one for an exact zero component,
    // unlike the mathematical unguarded backward error. Compare both metrics
    // independently; a zero component must not be masked by a loose bound.
    ASC_DENSE_TEST_CHECK(test, std::abs(b - guarded_backward) <=
                                   16 * std::max<asc::extent_t>(1, x.n) * eps);
    ASC_DENSE_TEST_CHECK(test, norm == 0 ? error == 0 : error / norm <= f);
    if (x.n == 0) {
      ASC_DENSE_TEST_EQ(test, f, Real{0});
      ASC_DENSE_TEST_EQ(test, b, Real{0});
    }
  }
}

template <typename T>
void Perturb(RhsData<T>& x) {
  using Real = asc::DenseBlasRealType<T>;
  for (asc::extent_t i = 0; i < x.n; ++i) {
    for (asc::extent_t j = 0; j < x.count; ++j) {
      x.values[x.Index(i, j)] *= Real{0.99};
    }
  }
}

template <typename T>
void General(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::extent_t n, asc::extent_t kd, asc::extent_t nrhs,
             asc::DenseBlasTriangle triangle,
             const std::array<asc::DenseBlasLayout, 4>& layouts,
             int exponent = 0) {
  using Real = asc::DenseBlasRealType<T>;
  BandData<T> a(n, kd, triangle, layouts[0], exponent);
  BandData<T> af(n, kd, triangle, layouts[1], exponent);
  if constexpr (asc::DenseBlasComplex<T>) {
    for (asc::extent_t i = 0; i < n; ++i) {
      a.values[a.Index(i, i)].imag(std::numeric_limits<Real>::quiet_NaN());
    }
  }
  RhsData<T> b(a, nrhs, layouts[2]);
  RhsData<T> x(a, nrhs, layouts[3]);
  auto factor_plan = Take(asc::QueryPbtrfWorkspace(provider, af.View()));
  Storage<T> factor_storage(factor_plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, asc::Pbtrf(provider, af.View(), factor_plan,
                                        factor_storage.View(), report)
                                 .ok());
  af.CheckFactor(test);
  const auto solve_plan =
      Take(asc::QueryPbtrsWorkspace(provider, af.ConstView(), x.View()));
  Storage<T> solve_storage(solve_plan);
  const auto solve_before = x.values;
  ASC_DENSE_TEST_CHECK(
      test, asc::Pbtrs(provider, af.ConstView(), x.View(), solve_plan,
                       solve_storage.View(), report)
                .ok());
  x.Check(test, a, solve_before);
  Perturb(x);
  const auto a_before = a.values;
  const auto af_before = af.values;
  const auto b_before = b.values;
  const auto x_before = x.values;
  std::vector<Real> ferr(static_cast<std::size_t>(nrhs + 2), Real{-293});
  std::vector<Real> berr(static_cast<std::size_t>(nrhs + 2), Real{-307});
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPbrfsWorkspace(provider, a.ConstView(), af.ConstView(),
                                    ConstView(b), x.View(), Vector(ferr, nrhs),
                                    Vector(berr, nrhs));
  }));
  ASC_DENSE_TEST_CHECK(test, SameBytes(x_before, x.values));
  WorkspaceStorage<T> storage(plan);
  const auto status = WithoutAllocation(test, [&] {
    return asc::Pbrfs(provider, a.ConstView(), af.ConstView(), ConstView(b),
                      x.View(), Vector(ferr, nrhs), Vector(berr, nrhs), plan,
                      storage.View(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  x.Check(test, a, x_before);
  CheckEstimates(test, a, x, ferr, berr);
  ASC_DENSE_TEST_CHECK(test, SameBytes(a_before, a.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(af_before, af.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(b_before, b.values));
  ASC_DENSE_TEST_EQ(test, ferr.front(), Real{-293});
  ASC_DENSE_TEST_EQ(test, ferr.back(), Real{-293});
  ASC_DENSE_TEST_EQ(test, berr.front(), Real{-307});
  ASC_DENSE_TEST_EQ(test, berr.back(), Real{-307});
  storage.Check(test);
  factor_storage.Check(test);
  solve_storage.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  constexpr int kScale =
      std::is_same_v<asc::DenseBlasRealType<T>, float> ? 50 : 450;
  for (const auto triangle : {kUpper, kLower}) {
    for (int bits = 0; bits < 16; ++bits) {
      const std::array layouts{
          (bits & 1) ? kRow : kColumn, (bits & 2) ? kRow : kColumn,
          (bits & 4) ? kRow : kColumn, (bits & 8) ? kRow : kColumn};
      for (const auto shape : std::array<std::array<asc::extent_t, 2>, 5>{
               {{0, 0}, {1, 0}, {4, 0}, {6, 2}, {4, 9}}}) {
        for (const asc::extent_t count : {0, 2}) {
          General<T>(test, provider, shape[0], shape[1], count, triangle,
                     layouts);
        }
      }
      General<T>(test, provider, 6, 2, 3, triangle, layouts, kScale);
      General<T>(test, provider, 6, 2, 3, triangle, layouts, -kScale);
    }
    General<T>(test, provider, 96, 65, 2, triangle,
               {kRow, kColumn, kRow, kColumn});
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
