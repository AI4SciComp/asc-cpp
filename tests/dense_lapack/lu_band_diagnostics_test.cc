#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_band.h"
#include "asc/dense/providers/lapack_lu_band_refinement.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_driver_test_support.h"
#include "lu_band_expert_test_support.h"
#include "lu_band_test_support.h"

namespace {
namespace base = asc_lu_band_test;
namespace driver = asc_lu_band_driver_test;
namespace support = asc_lu_band_expert_test;
using support::Take;
using support::TestContext;
using Eq = asc::LapackEquilibration;

long double Magnitude(support::Wide value) {
  return std::abs(value.real()) + std::abs(value.imag());
}

template <typename T>
base::Band<T> Diagonal(bool rational) {
  base::Band<T> band(3, 3, 0, 0);
  for (asc::extent_t i = 0; i < 3; ++i) {
    const long double value =
        rational ? std::array{3, 7, 11}[i] : std::array{2, 8, 32}[i];
    if constexpr (asc::DenseBlasComplex<T>) {
      band.Put(i, i,
               i == 1 ? support::Value<T>(value) : support::Value<T>(0, value));
    } else {
      band.Put(i, i, support::Value<T>(value));
    }
  }
  return band;
}

template <typename T>
void Statistics(TestContext& test, const driver::Sample<T>& sample) {
  using Real = asc::DenseBlasRealType<T>;
  long double minimum = std::numeric_limits<long double>::max();
  long double maximum = 0;
  for (asc::extent_t i = 0; i < 3; ++i) {
    const long double diagonal = Magnitude(support::ToWide(sample.a.At(i, i)));
    minimum = std::min(minimum, diagonal);
    maximum = std::max(maximum, diagonal);
  }
  // A is the documented equilibrated matrix. Its diagonal inverse norm is
  // exact in either one/infinity norm, independently of the native estimator.
  const long double expected = minimum / maximum;
  const long double actual = sample.stats.reciprocal_condition;
  ASC_DENSE_TEST_CHECK(test, std::isfinite(actual));
  ASC_DENSE_TEST_CHECK(
      test, std::abs(actual - expected) <=
                64 * std::numeric_limits<Real>::epsilon() * expected);
  // Diagonal elimination has U=A and cannot grow an entry, also after scaling.
  ASC_DENSE_TEST_CHECK(
      test, std::abs(sample.stats.reciprocal_pivot_growth - Real{1}) <=
                Real{64} * std::numeric_limits<Real>::epsilon());
}

template <typename T>
void Driver(TestContext& test, const asc::ReferenceLapackProvider& provider,
            asc::DenseBlasTranspose trans, asc::DenseBlasLayout b_layout,
            asc::DenseBlasLayout x_layout, int mode, Eq equed, bool corrupt) {
  driver::Sample<T> sample(Diagonal<T>(false), 3, trans, b_layout, x_layout);
  if (mode == 2) {
    sample.Supply(test, provider, equed);
  }
  const auto before = sample;
  const auto plan = Take(support::WithoutAllocation(
      test, [&] { return sample.Query(provider, mode); }));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return sample.Execute(provider, mode, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && report.called_provider && report.native_info == 0);
  sample.x.Check(test, sample.original, before.x.values);
  driver::Scaling(test, sample, before, mode);
  sample.pivots.Check(test);
  sample.rows.Check(test);
  sample.columns.Check(test);
  sample.ferr.Check(test);
  sample.berr.Check(test);
  scratch.Check(test);
  if (corrupt) {
    // Deliberately wrong but finite-positive estimates establish that the new
    // numerical oracle rejects values admitted by the previous sign checks.
    sample.stats.reciprocal_condition = 0.75;
    sample.stats.reciprocal_pivot_growth = 0.5;
  }
  Statistics(test, sample);
}

template <typename T>
void RationalRightHandSides(const base::Band<T>& band, base::Rhs<T>& b,
                            base::Rhs<T>& x) {
  for (asc::extent_t i = 0; i < 3; ++i) {
    for (asc::extent_t j = 0; j < 3; ++j) {
      const auto value = support::Value<T>((j + 1) * (i + 2) + 1, j + 2);
      b.values[b.Index(i, j)] = value;
      const auto at = static_cast<std::size_t>(i * 3 + j);
      b.original[at] = support::ToWide(value);
      x.original[at] = b.original[at];
      // A and B are exact integers/pure imaginary integers, so each true
      // solution is an independent long-double rational division.
      x.expected[at] = b.original[at] / b.Coefficient(band, i, i);
      x.values[x.Index(i, j)] =
          support::Value<T>(x.expected[at].real() + 1.0L / 128,
                            x.expected[at].imag() - 1.0L / 256);
    }
  }
}

template <typename T>
void ForwardError(TestContext& test, const base::Rhs<T>& x,
                  const support::Vector<asc::DenseBlasRealType<T>>& ferr) {
  using Real = asc::DenseBlasRealType<T>;
  for (asc::extent_t j = 0; j < 3; ++j) {
    long double error = 0;
    long double norm = 0;
    for (asc::extent_t i = 0; i < 3; ++i) {
      const auto actual = support::ToWide(x.values[x.Index(i, j)]);
      const auto truth = x.expected[static_cast<std::size_t>(i * 3 + j)];
      error = std::max(error, Magnitude(actual - truth));
      norm = std::max(norm, Magnitude(actual));
    }
    const long double observed = error / norm;
    const long double estimate = ferr.values[static_cast<std::size_t>(j + 1)];
    ASC_DENSE_TEST_CHECK(test, std::isfinite(observed) && observed > 0);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(estimate) && estimate > 0);
    // FERR is an estimate, not a universal certified bound. These regular
    // diagonal fixtures specifically require it to cover their measured error;
    // the tiny additive allowance is verifier long-double rounding only.
    ASC_DENSE_TEST_CHECK(
        test,
        observed <= estimate * (1 + 64 * std::numeric_limits<Real>::epsilon()) +
                        8 * std::numeric_limits<long double>::epsilon());
  }
}

template <typename T>
void Refine(TestContext& test, const asc::ReferenceLapackProvider& provider,
            asc::DenseBlasTranspose trans, asc::DenseBlasLayout b_layout,
            asc::DenseBlasLayout x_layout, bool corrupt) {
  using Real = asc::DenseBlasRealType<T>;
  auto band = Diagonal<T>(true);
  const support::Compact<T> original(band);
  base::Pivots pivots(3);
  const auto factor_plan =
      Take(asc::QueryGbtrfWorkspace(provider, band.View(), pivots.View()));
  support::Scratch<T> factor_scratch(factor_plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, asc::Gbtrf(provider, band.View(), pivots.View(), factor_plan,
                       factor_scratch.View(), report)
                .ok());
  band.Reconstruct(test, pivots);
  base::Rhs<T> b(band, 3, b_layout, trans);
  base::Rhs<T> x(band, 3, x_layout, trans);
  RationalRightHandSides(band, b, x);
  const auto before = x.values;
  const auto b_before = b.values;
  const auto bv = Take(asc::DenseBlasMatrixView<const T>::Create(
      b.values.data() + 1, 3, 3, b.layout, b.ld,
      {b.values.data(), b.values.size() * sizeof(T), base::kHost}));
  const auto swaps =
      Take(asc::ReferenceLuBandPivotView::Create(pivots.ConstView()));
  support::Vector<Real> ferr(3);
  support::Vector<Real> berr(3);
  const auto plan = Take(support::WithoutAllocation(test, [&] {
    return asc::QueryGbrfsWorkspace(provider, trans, original.ConstView(),
                                    band.ConstView(), swaps, bv, x.View(),
                                    ferr.View(), berr.View());
  }));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  const auto status = support::WithoutAllocation(test, [&] {
    return asc::Gbrfs(provider, trans, original.ConstView(), band.ConstView(),
                      swaps, bv, x.View(), ferr.View(), berr.View(), plan,
                      workspace, report);
  });
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && report.called_provider && report.native_info == 0);
  x.Check(test, band, before);
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(b.values, b_before));
  ferr.Check(test);
  berr.Check(test);
  pivots.Check(test);
  scratch.Check(test);
  factor_scratch.Check(test);
  if (corrupt) {
    for (asc::extent_t j = 0; j < 3; ++j) {
      // This still satisfies the previous positive/small FERR range check.
      ferr.values[static_cast<std::size_t>(j + 1)] =
          std::numeric_limits<Real>::min();
    }
  }
  ForwardError(test, x, ferr);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         std::string_view mode) {
  for (const auto trans : support::kOperations) {
    for (const auto b_layout : {base::kColumn, base::kRow}) {
      for (const auto x_layout : {base::kColumn, base::kRow}) {
        if (mode != "ferr-control") {
          Driver<T>(test, provider, trans, b_layout, x_layout, 0, Eq::kNone,
                    mode == "statistics-control");
          Driver<T>(test, provider, trans, b_layout, x_layout, 1, Eq::kNone,
                    mode == "statistics-control");
          for (const auto equed :
               {Eq::kNone, Eq::kRows, Eq::kColumns, Eq::kBoth}) {
            Driver<T>(test, provider, trans, b_layout, x_layout, 2, equed,
                      mode == "statistics-control");
          }
        }
        if (mode != "statistics-control") {
          Refine<T>(test, provider, trans, b_layout, x_layout,
                    mode == "ferr-control");
        }
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const std::string_view mode = argc == 2 ? argv[1] : "regular";
  if (argc > 2 || (mode != "regular" && mode != "statistics-control" &&
                   mode != "ferr-control")) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Run<float>(test, provider, mode);
  Run<double>(test, provider, mode);
  Run<std::complex<float>>(test, provider, mode);
  Run<std::complex<double>>(test, provider, mode);
  if (mode == "regular") {
    std::puts(
        "288 exact diagonal GBSVX statistics profiles and 48 rational GBRFS "
        "forward-error profiles");
  } else {
    std::printf("negative oracle control: %.*s\n",
                static_cast<int>(mode.size()), mode.data());
  }
  return test.Finish();
}
