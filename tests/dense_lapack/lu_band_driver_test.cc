#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <utility>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
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
using Eq = asc::LapackEquilibration;
template <typename T>
void Padding(TestContext& test, const driver::Sample<T>& sample,
             const driver::Sample<T>& before) {
  std::vector<unsigned char> selected(sample.a.values.size());
  for (asc::extent_t j = 0; j < sample.original.n; ++j) {
    for (asc::extent_t i = std::max<asc::extent_t>(0, j - sample.original.ku);
         i < std::min(sample.original.n, j + sample.original.kl + 1); ++i) {
      selected[static_cast<std::size_t>(1 + j * sample.a.ld + sample.a.ku + i -
                                        j)] = 1;
    }
  }
  for (std::size_t i = 0; i < selected.size(); ++i) {
    if (selected[i] == 0) {
      ASC_DENSE_TEST_CHECK(
          test, base::SameScalarBytes(sample.a.values[i], before.a.values[i]));
    }
  }
  selected.assign(sample.b.values.size(), 0);
  for (asc::extent_t i = 0; i < sample.b.n; ++i) {
    for (asc::extent_t j = 0; j < sample.b.count; ++j) {
      selected[sample.b.Index(i, j)] = 1;
    }
  }
  for (std::size_t i = 0; i < selected.size(); ++i) {
    if (selected[i] == 0) {
      ASC_DENSE_TEST_CHECK(
          test, base::SameScalarBytes(sample.b.values[i], before.b.values[i]));
    }
  }
  sample.af.Padding(test, before.af.values);
  sample.pivots.Check(test);
  sample.rows.Check(test);
  sample.columns.Check(test);
  sample.ferr.Check(test);
  sample.berr.Check(test);
}
template <typename T>
void Reconstruct(TestContext& test, driver::Sample<T>& sample) {
  for (asc::extent_t i = 0; i < sample.original.n; ++i) {
    for (asc::extent_t j = 0; j < sample.original.n; ++j) {
      sample.af.original[static_cast<std::size_t>(i * sample.original.n + j)] =
          support::ToWide(std::as_const(sample.a).At(i, j));
    }
  }
  sample.af.Reconstruct(test, sample.pivots);
}
template <typename T>
Eq One(TestContext& test, const asc::ReferenceLapackProvider& provider,
       driver::Sample<T> sample, int mode, Eq supplied = Eq::kNone) {
  using Real = asc::DenseBlasRealType<T>;
  if (mode == 2) {
    sample.Supply(test, provider, supplied);
  }
  if (mode == 1) {
    sample.equed = std::bit_cast<Eq>(std::uint8_t{255});
  }
  const auto before = sample;
  const auto plan = Take(support::WithoutAllocation(
      test, [&] { return sample.Query(provider, mode); }));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  for (int iteration = 0; iteration < (mode == 2 ? 2 : 1); ++iteration) {
    sample.b.values = before.b.values;
    sample.x.values = before.x.values;
    asc::LapackReport report;
    const auto status = support::WithoutAllocation(test, [&] {
      return sample.Execute(provider, mode, plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_CHECK(test,
                         report.called_provider && report.native_info == 0);
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    ASC_DENSE_TEST_CHECK(test,
                         std::isfinite(sample.stats.reciprocal_condition) &&
                             sample.stats.reciprocal_condition > 0);
    ASC_DENSE_TEST_CHECK(test,
                         std::isfinite(sample.stats.reciprocal_pivot_growth) &&
                             sample.stats.reciprocal_pivot_growth > 0);
    sample.x.Check(test, sample.original, before.x.values);
    driver::Scaling(test, sample, before, mode);
    Padding(test, sample, before);
    scratch.Check(test);
    Reconstruct(test, sample);
    for (asc::extent_t j = 0; j < sample.b.count; ++j) {
      const auto f = sample.ferr.values[static_cast<std::size_t>(j + 1)];
      const auto b = sample.berr.values[static_cast<std::size_t>(j + 1)];
      ASC_DENSE_TEST_CHECK(
          test, std::isfinite(f) && f >= 0 && std::isfinite(b) && b >= 0);
      if (sample.original.n == 0) {
        ASC_DENSE_TEST_EQ(test, f, Real{0});
        ASC_DENSE_TEST_EQ(test, b, Real{0});
      }
    }
    if (sample.original.n == 0) {
      ASC_DENSE_TEST_EQ(test, sample.stats.reciprocal_condition, Real{1});
      ASC_DENSE_TEST_EQ(test, sample.stats.reciprocal_pivot_growth, Real{1});
    }
  }
  return sample.equed;
}
template <typename T>
base::Band<T> EquilibrationFixture(Eq selected) {
  base::Band<T> band(3, 3, 2, 2);
  for (asc::extent_t i = 0; i < 3; ++i) {
    for (asc::extent_t j = 0; j < 3; ++j) {
      long double value = i == j ? 1 : 0;
      if (driver::Columns(selected)) {
        value = i == j ? std::ldexp(1.0L, -4 * static_cast<int>(j)) : 0;
        if (j == 0) {
          value = 1;
        }
      }
      if (driver::Rows(selected)) {
        value = std::ldexp(value, -4 * static_cast<int>(i));
      }
      band.Put(i, j, support::Value<T>(value));
    }
  }
  return band;
}
template <typename T>
void Modes(TestContext& test, const asc::ReferenceLapackProvider& provider,
           const base::Band<T>& original, asc::DenseBlasTranspose trans,
           asc::DenseBlasLayout b_layout, asc::DenseBlasLayout x_layout,
           asc::extent_t nrhs) {
  for (const int mode : {0, 1, 2}) {
    if (mode == 2) {
      for (const auto eq : {Eq::kNone, Eq::kRows, Eq::kColumns, Eq::kBoth}) {
        One(test, provider,
            driver::Sample<T>(original, nrhs, trans, b_layout, x_layout), mode,
            eq);
      }
    } else {
      One(test, provider,
          driver::Sample<T>(original, nrhs, trans, b_layout, x_layout), mode);
    }
  }
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const int exponent = sizeof(Real) == sizeof(float) ? 100 : 800;
  std::size_t profiles = 0;
  for (const auto shape :
       {std::array{0, 0, 0}, std::array{0, 5, 6}, std::array{1, 0, 0},
        std::array{1, 5, 6}, std::array{3, 2, 2}, std::array{7, 2, 1}}) {
    for (const int power : {0, -exponent, exponent}) {
      const base::Band<T> original(shape[0], shape[0], shape[1], shape[2],
                                   power);
      for (const auto trans : support::kOperations) {
        for (const auto b_layout : {base::kColumn, base::kRow}) {
          for (const auto x_layout : {base::kColumn, base::kRow}) {
            for (const asc::extent_t nrhs : {0, 1, 3}) {
              Modes(test, provider, original, trans, b_layout, x_layout, nrhs);
              ++profiles;
            }
          }
        }
      }
    }
  }
  for (const auto eq : {Eq::kNone, Eq::kRows, Eq::kColumns, Eq::kBoth}) {
    for (const auto trans : support::kOperations) {
      for (const auto b_layout : {base::kColumn, base::kRow}) {
        for (const auto x_layout : {base::kColumn, base::kRow}) {
          const auto selected =
              One(test, provider,
                  driver::Sample<T>(EquilibrationFixture<T>(eq), 3, trans,
                                    b_layout, x_layout),
                  1);
          ASC_DENSE_TEST_EQ(test, selected, eq);
        }
      }
    }
  }
  std::printf(
      "%zu profiles x N/E/F(N/R/C/B), FACT=F repeated reuse, plus 48 forced "
      "actual E equilibration branch cases\n",
      profiles);
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
