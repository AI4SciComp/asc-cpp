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
#include "asc/dense/providers/lapack_lu_driver.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_driver_test_support.h"
#include "lu_band_expert_test_support.h"
#include "lu_band_test_support.h"
namespace {
namespace support = asc_lu_band_expert_test;
namespace base = asc_lu_band_test;
namespace driver = asc_lu_band_driver_test;
using support::Take;
using support::TestContext;
template <typename T>
base::Band<T> Diagonal(asc::extent_t n, asc::DenseBlasRealType<T> last) {
  base::Band<T> band(n, n, 4, 6);
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      if (i != j) {
        band.Put(i, j, T{});
      } else {
        band.Put(i, j, support::Value<T>(i == n - 1 ? last : 1));
      }
    }
  }
  return band;
}
template <typename T>
void Singular(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::DenseBlasTranspose trans, asc::DenseBlasLayout b_layout,
              asc::DenseBlasLayout x_layout, asc::extent_t nrhs, int mode) {
  using Real = asc::DenseBlasRealType<T>;
  driver::Sample<T> sample(Diagonal<T>(3, Real{0}), nrhs, trans, b_layout,
                           x_layout);
  const auto before = sample;
  const auto plan = Take(sample.Query(provider, mode));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return sample.Execute(provider, mode, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 3);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 2);
  ASC_DENSE_TEST_EQ(test, sample.stats.reciprocal_condition, Real{0});
  ASC_DENSE_TEST_EQ(test, sample.stats.reciprocal_pivot_growth, Real{1});
  ASC_DENSE_TEST_CHECK(test,
                       support::SameBytes(sample.x.values, before.x.values));
  ASC_DENSE_TEST_EQ(test, sample.ferr.values, before.ferr.values);
  ASC_DENSE_TEST_EQ(test, sample.berr.values, before.berr.values);
  ASC_DENSE_TEST_CHECK(test,
                       support::SameBytes(sample.b.values, before.b.values));
  sample.af.Reconstruct(test, sample.pivots);
  sample.af.Padding(test, before.af.values);
  sample.pivots.Check(test);
  sample.rows.Check(test);
  sample.columns.Check(test);
  scratch.Check(test);
}
template <typename T>
void Warning(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasTranspose trans, asc::DenseBlasLayout b_layout,
             asc::DenseBlasLayout x_layout, int mode, bool extreme) {
  using Real = asc::DenseBlasRealType<T>;
  const auto value = extreme ? std::numeric_limits<Real>::min() / Real{8}
                             : std::numeric_limits<Real>::epsilon() / Real{8};
  driver::Sample<T> sample(Diagonal<T>(extreme ? 1 : 3, value), 3, trans,
                           b_layout, x_layout);
  if (mode == 2) {
    sample.Supply(test, provider, asc::LapackEquilibration::kNone);
  }
  const auto before = sample;
  const auto plan = Take(sample.Query(provider, mode));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return sample.Execute(provider, mode, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(
      test, report.called_provider && report.native_info.has_value());
  if (mode == 1) {
    ASC_DENSE_TEST_CHECK(test, status.ok() && report.native_info == 0);
  } else {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info, sample.original.n + 1);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    if (extreme) {
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 0);
    } else {
      ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    }
  }
  sample.x.Check(test, sample.original, before.x.values);
  driver::Scaling(test, sample, before, mode);
  scratch.Check(test);
  sample.pivots.Check(test);
  if (extreme) {
    // Independent singleton condition is exactly one despite its absolute
    // scale.
    ASC_DENSE_TEST_CHECK(test,
                         std::isfinite(sample.stats.reciprocal_condition));
    ASC_DENSE_TEST_CHECK(
        test, std::abs(sample.stats.reciprocal_condition - Real{1}) <=
                  Real{64} * std::numeric_limits<Real>::epsilon());
    if (sample.stats.reciprocal_condition != Real{1}) {
      std::printf(
          "GBSVX FACT=%c TRANS=%d RCOND=%.21Lg independent=1 INFO=%lld\n",
          std::array{'N', 'E', 'F'}[static_cast<std::size_t>(mode)],
          static_cast<int>(trans),
          static_cast<long double>(sample.stats.reciprocal_condition),
          static_cast<long long>(report.native_info.value_or(-1)));
    }
  }
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         bool extreme) {
  for (const auto trans : support::kOperations) {
    for (const auto b_layout : {base::kColumn, base::kRow}) {
      for (const auto x_layout : {base::kColumn, base::kRow}) {
        if (!extreme) {
          for (const asc::extent_t nrhs : {0, 3}) {
            for (const int mode : {0, 1}) {
              Singular<T>(test, provider, trans, b_layout, x_layout, nrhs,
                          mode);
            }
          }
        }
        for (const int mode : {0, 1, 2}) {
          Warning<T>(test, provider, trans, b_layout, x_layout, mode, extreme);
        }
      }
    }
  }
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
