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
#include "asc/dense/providers/lapack_lu_band_condition.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_expert_test_support.h"
#include "lu_band_test_support.h"
namespace {
namespace support = asc_lu_band_expert_test;
namespace base = asc_lu_band_test;
using support::Take;
using support::TestContext;

template <typename T>
void Estimate(TestContext& test, const asc::ReferenceLapackProvider& provider,
              const base::Band<T>& band, const base::Pivots& pivots,
              asc::DenseBlasRealType<T> anorm, long double expected) {
  using Real = asc::DenseBlasRealType<T>;
  const auto factors = band.ConstView();
  const auto swaps =
      Take(asc::ReferenceLuBandPivotView::Create(pivots.ConstView()));
  const auto before = band.values;
  const auto pivot_before = pivots.values;
  for (const auto norm :
       {asc::LapackConditionNorm::kOne, asc::LapackConditionNorm::kInfinity}) {
    std::array<Real, 3> output{Real{-251}, Real{-257}, Real{-263}};
    const auto plan = Take(support::WithoutAllocation(test, [&] {
      return asc::QueryGbconWorkspace(provider, norm, factors, swaps, anorm,
                                      output[1]);
    }));
    support::Scratch<T> scratch(plan);
    const auto workspace = scratch.View();
    asc::LapackReport report;
    const auto status = support::WithoutAllocation(test, [&] {
      return asc::Gbcon(provider, norm, factors, swaps, anorm, output[1], plan,
                        workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_CHECK(test,
                         report.called_provider && report.native_info == 0);
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    ASC_DENSE_TEST_CHECK(test, std::isfinite(output[1]));
    const long double error =
        std::abs(static_cast<long double>(output[1]) - expected);
    ASC_DENSE_TEST_CHECK(
        test,
        error <= 64 * std::numeric_limits<Real>::epsilon() *
                     std::max(expected, static_cast<long double>(output[1])));
    if (error > 64 * std::numeric_limits<Real>::epsilon() *
                    std::max(expected, static_cast<long double>(output[1]))) {
      std::printf(
          "GBCON norm=%d n=%lld anorm=%.21Lg rcond=%.21Lg independent=%.21Lg\n",
          static_cast<int>(norm), static_cast<long long>(band.n),
          static_cast<long double>(anorm), static_cast<long double>(output[1]),
          expected);
    }
    ASC_DENSE_TEST_EQ(test, output.front(), Real{-251});
    ASC_DENSE_TEST_EQ(test, output.back(), Real{-263});
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(band.values, before));
    ASC_DENSE_TEST_EQ(test, pivots.values, pivot_before);
    scratch.Check(test);
    pivots.Check(test);
  }
}

template <typename T>
void One(TestContext& test, const asc::ReferenceLapackProvider& provider,
         asc::extent_t n, asc::extent_t kl, asc::extent_t ku,
         asc::DenseBlasRealType<T> scale, bool singular = false) {
  using Real = asc::DenseBlasRealType<T>;
  base::Band<T> band(n, n, kl, ku);
  Real minimum = std::numeric_limits<Real>::max();
  Real maximum = 0;
  for (asc::extent_t j = 0; j < n; ++j) {
    for (asc::extent_t i = std::max<asc::extent_t>(0, j - ku);
         i < std::min(n, j + kl + 1); ++i) {
      band.Put(i, j, T{});
    }
    const auto row = kl >= n - 1 && ku >= n - 1 ? (j + 1) % n : j;
    const Real value =
        singular && j == n - 1 ? Real{0} : scale * static_cast<Real>(1 + j % 4);
    T coefficient = support::Value<T>(value);
    if constexpr (asc::DenseBlasComplex<T>) {
      if (j % 2 != 0) {
        coefficient = support::Value<T>(0, value);
      }
    }
    band.Put(row, j, coefficient);
    minimum = std::min(minimum, value);
    maximum = std::max(maximum, value);
  }
  base::Pivots pivots(n);
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan = Take(asc::QueryGbtrfWorkspace(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return asc::Gbtrf(provider, matrix, swaps, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, singular
                                 ? status.code() == asc::ErrorCode::kNumerical
                                 : status.ok());
  band.Reconstruct(test, pivots);
  scratch.Check(test);
  const long double expected =
      n == 0 ? 1 : static_cast<long double>(minimum) / maximum;
  Estimate(test, provider, band, pivots, maximum, expected);
  Estimate(test, provider, band, pivots, Real{0}, n == 0 ? 1 : 0);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         bool extreme) {
  using Real = asc::DenseBlasRealType<T>;
  if (extreme) {
    for (const Real scale : {std::numeric_limits<Real>::min() / Real{8},
                             std::numeric_limits<Real>::min(),
                             std::numeric_limits<Real>::max() / Real{8}}) {
      One<T>(test, provider, 1, 4, 6, scale);
    }
    return;
  }
  const int exponent = sizeof(Real) == sizeof(float) ? 100 : 800;
  for (const auto shape :
       {std::array{0, 0, 0}, std::array{0, 4, 7}, std::array{1, 0, 0},
        std::array{1, 5, 6}, std::array{3, 2, 2}, std::array{7, 2, 1},
        std::array{7, 8, 9}}) {
    for (const int power : {0, -exponent, exponent}) {
      One<T>(test, provider, shape[0], shape[1], shape[2],
             std::ldexp(Real{1}, power));
    }
  }
  One<T>(test, provider, 7, 2, 1, Real{1}, true);
  std::printf(
      "21 monomial condition profiles plus singular, both norms and ANORM=0, "
      "independent exact inverse norm\n");
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "regular" && mode != "extreme") {
    return 2;
  }
  const auto run = [&]<typename T>() {
    Run<T>(test, provider, mode == "extreme");
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
