#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_band.h"
#include "asc/dense/providers/lapack_lu_band_refinement.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_expert_test_support.h"
#include "lu_band_test_support.h"
namespace {
namespace support = asc_lu_band_expert_test;
namespace base = asc_lu_band_test;
using support::Take;
using support::TestContext;
long double Magnitude(support::Wide value) {
  return std::abs(value.real()) + std::abs(value.imag());
}
template <typename T>
long double Backward(const base::Band<T>& band, const base::Rhs<T>& x,
                     asc::extent_t j) {
  long double result = 0;
  for (asc::extent_t i = 0; i < band.n; ++i) {
    const auto b = x.original[static_cast<std::size_t>(i * x.count + j)];
    auto residual = -b;
    long double denominator = Magnitude(b);
    for (asc::extent_t k = 0; k < band.n; ++k) {
      const auto a = x.Coefficient(band, i, k);
      const auto value = support::ToWide(x.values[x.Index(k, j)]);
      residual += a * value;
      denominator += Magnitude(a) * Magnitude(value);
    }
    const auto numerator = Magnitude(residual);
    if (denominator == 0) {
      result = std::max(
          result,
          numerator == 0 ? 0 : std::numeric_limits<long double>::infinity());
    } else {
      result = std::max(result, numerator / denominator);
    }
  }
  return result;
}
template <typename T>
void Diagnostics(TestContext& test, const base::Band<T>& band,
                 const base::Rhs<T>& x,
                 const support::Vector<asc::DenseBlasRealType<T>>& ferr,
                 const support::Vector<asc::DenseBlasRealType<T>>& berr) {
  using Real = asc::DenseBlasRealType<T>;
  for (asc::extent_t j = 0; j < x.count; ++j) {
    const auto f = ferr.values[static_cast<std::size_t>(j + 1)];
    const auto b = berr.values[static_cast<std::size_t>(j + 1)];
    ASC_DENSE_TEST_CHECK(
        test, std::isfinite(f) && f >= 0 && std::isfinite(b) && b >= 0);
    const auto residual = Backward(band, x, j);
    ASC_DENSE_TEST_CHECK(test,
                         residual <= 64 * std::numeric_limits<Real>::epsilon());
    ASC_DENSE_TEST_CHECK(test,
                         std::abs(static_cast<long double>(b) - residual) <=
                             64 * std::numeric_limits<Real>::epsilon());
    if (band.n == 0) {
      ASC_DENSE_TEST_EQ(test, f, Real{0});
    } else {
      ASC_DENSE_TEST_CHECK(
          test, f > 0 && f < 256 * std::numeric_limits<Real>::epsilon());
    }
  }
}
template <typename T>
void One(TestContext& test, const asc::ReferenceLapackProvider& provider,
         const base::Band<T>& band, const support::Compact<T>& original,
         const base::Pivots& pivots, asc::DenseBlasTranspose transpose,
         asc::DenseBlasLayout b_layout, asc::DenseBlasLayout x_layout,
         asc::extent_t nrhs, bool extreme = false) {
  using Real = asc::DenseBlasRealType<T>;
  base::Rhs<T> b(band, nrhs, b_layout, transpose);
  base::Rhs<T> x(band, nrhs, x_layout, transpose);
  for (asc::extent_t i = 0; i < band.n; ++i) {
    for (asc::extent_t j = 0; j < nrhs; ++j) {
      const auto expected = x.expected[static_cast<std::size_t>(i * nrhs + j)];
      x.values[x.Index(i, j)] = support::Value<T>(
          expected.real() + (extreme ? 0 : 1.0L / 128), expected.imag());
    }
  }
  const auto b_before = b.values;
  const auto x_before = x.values;
  const auto a_before = original.values;
  const auto af_before = band.values;
  support::Vector<Real> ferr(nrhs);
  support::Vector<Real> berr(nrhs);
  const auto a = original.ConstView();
  const auto af = band.ConstView();
  const auto swaps =
      Take(asc::ReferenceLuBandPivotView::Create(pivots.ConstView()));
  const auto bv = Take(asc::DenseBlasMatrixView<const T>::Create(
      b.values.data() + 1, band.n, nrhs, b_layout, b.ld,
      {b.values.data(), b.values.size() * sizeof(T), base::kHost}));
  const auto xv = x.View();
  const auto fv = ferr.View();
  const auto ev = berr.View();
  const auto plan = Take(support::WithoutAllocation(test, [&] {
    return asc::QueryGbrfsWorkspace(provider, transpose, a, af, swaps, bv, xv,
                                    fv, ev);
  }));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return asc::Gbrfs(provider, transpose, a, af, swaps, bv, xv, fv, ev, plan,
                      workspace, report);
  });
  if (extreme) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 0);
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
  }
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(original.values, a_before));
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(band.values, af_before));
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(b.values, b_before));
  x.Check(test, band, x_before);
  Diagnostics(test, band, x, ferr, berr);
  ferr.Check(test);
  berr.Check(test);
  scratch.Check(test);
  pivots.Check(test);
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const int exponent = sizeof(Real) == sizeof(float) ? 100 : 800;
  std::size_t count = 0;
  for (const auto shape :
       {std::array{0, 0, 0}, std::array{0, 5, 6}, std::array{1, 0, 0},
        std::array{1, 5, 6}, std::array{3, 2, 2}, std::array{7, 2, 1}}) {
    for (const int power : {0, -exponent, exponent}) {
      base::Band<T> band(shape[0], shape[0], shape[1], shape[2], power);
      const support::Compact<T> original(band);
      base::Pivots pivots(band.n);
      const auto matrix = band.View();
      const auto swaps = pivots.View();
      const auto plan = Take(asc::QueryGbtrfWorkspace(provider, matrix, swaps));
      support::Scratch<T> scratch(plan);
      const auto workspace = scratch.View();
      asc::LapackReport report;
      ASC_DENSE_TEST_CHECK(
          test,
          asc::Gbtrf(provider, matrix, swaps, plan, workspace, report).ok());
      band.Reconstruct(test, pivots);
      scratch.Check(test);
      for (const auto transpose : support::kOperations) {
        for (const auto b_layout : {base::kColumn, base::kRow}) {
          for (const auto x_layout : {base::kColumn, base::kRow}) {
            for (const asc::extent_t nrhs : {0, 1, 3}) {
              One(test, provider, band, original, pivots, transpose, b_layout,
                  x_layout, nrhs);
              ++count;
            }
          }
        }
      }
    }
  }
  std::printf(
      "%zu independent GBRFS residual/refinement/estimate cases, N/T/C and "
      "both independent B/X layouts\n",
      count);
}
template <typename T>
void Extreme(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  base::Band<T> band(1, 1, 0, 0);
  band.Put(0, 0, T{std::numeric_limits<Real>::min() / Real{8}});
  const support::Compact<T> original(band);
  base::Pivots pivots(1);
  pivots.values[1] = 1;  // Exact pinned singleton GBTRF output; no factor call.
  for (const auto transpose : support::kOperations) {
    for (const auto b_layout : {base::kColumn, base::kRow}) {
      for (const auto x_layout : {base::kColumn, base::kRow}) {
        One(test, provider, band, original, pivots, transpose, b_layout,
            x_layout, 3, true);
      }
    }
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2 && argc != 3) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  const std::string_view mode = argc == 3 ? argv[2] : "regular";
  if (mode != "regular" && mode != "extreme") {
    return 2;
  }
  const auto run = [&]<typename T>() {
    if (mode == "extreme") {
      Extreme<T>(test, provider);
    } else {
      Run<T>(test, provider);
    }
  };
  if (scalar == "s") {
    run.template operator()<float>();
  } else if (scalar == "d") {
    run.template operator()<double>();
  } else if (scalar == "c") {
    run.template operator()<std::complex<float>>();
  } else if (scalar == "z") {
    run.template operator()<std::complex<double>>();
  } else {
    return 2;
  }
  return test.Finish();
}
