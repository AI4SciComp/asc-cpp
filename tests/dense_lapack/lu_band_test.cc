#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_band.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_factor_test_support.h"
#include "lu_band_test_support.h"

namespace {
namespace band_test = asc_lu_band_test;
using band_test::Band;
using band_test::Pivots;
using band_test::Scratch;
using band_test::Take;
using band_test::TestContext;
using band_test::WithoutAllocation;

template <typename T>
void SolveCases(TestContext& test, const asc::ReferenceLapackProvider& provider,
                const Band<T>& band, const Pivots& pivots,
                asc::ReferenceLuBandFactorView<T> factor) {
  const auto factored = band.values;
  const auto native_swaps = pivots.values;
  for (const auto layout : {band_test::kColumn, band_test::kRow}) {
    for (const auto operation : band_test::kOperations) {
      for (const asc::extent_t nrhs : {0, 1, 3}) {
        band_test::Rhs<T> rhs(band, nrhs, layout, operation);
        const auto rhs_before = rhs.values;
        const auto destination = rhs.View();
        const auto solve_plan = Take(WithoutAllocation(test, [&] {
          return asc::QueryGbtrsWorkspace(provider, operation, factor,
                                          destination);
        }));
        Scratch<T> solve_scratch(solve_plan);
        const auto solve_workspace = solve_scratch.View();
        asc::LapackReport solve_report;
        const auto solved = WithoutAllocation(test, [&] {
          return asc::Gbtrs(provider, operation, factor, destination,
                            solve_plan, solve_workspace, solve_report);
        });
        ASC_DENSE_TEST_CHECK(test, solved.ok());
        ASC_DENSE_TEST_CHECK(test, solve_report.called_provider &&
                                       solve_report.native_info == 0);
        ASC_DENSE_TEST_EQ(test, solve_report.outcome,
                          asc::LapackOutcome::kSuccess);
        ASC_DENSE_TEST_EQ(test, solve_report.output_validity,
                          asc::LapackOutputValidity::kComplete);
        ASC_DENSE_TEST_CHECK(test, band_test::SameBytes(factored, band.values));
        ASC_DENSE_TEST_EQ(test, native_swaps, pivots.values);
        rhs.Check(test, band, rhs_before);
        solve_scratch.Check(test);
        pivots.Check(test);
        std::printf(
            "gbtrs numerical n=%lld kl=%lld ku=%lld operation=%d rhs_layout=%d "
            "nrhs=%lld residual=checked\n",
            static_cast<long long>(band.n), static_cast<long long>(band.kl),
            static_cast<long long>(band.ku), static_cast<int>(operation),
            static_cast<int>(layout), static_cast<long long>(nrhs));
      }
    }
  }
}

template <typename T>
void Exercise(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::extent_t m, asc::extent_t n, asc::extent_t kl,
              asc::extent_t ku, int exponent = 0, bool noncommuting = false) {
  Band<T> band(m, n, kl, ku, exponent);
  if (noncommuting) {
    constexpr std::array<int, 9> kA{0, 2, 1, 1, 0, 1, 2, 1, 1};
    for (asc::extent_t i = 0; i < 3; ++i) {
      for (asc::extent_t j = 0; j < 3; ++j) {
        band.Put(i, j,
                 band_test::Value<T>(kA[static_cast<std::size_t>(i * 3 + j)],
                                     i == j ? 0 : (i - j) / 8.0L));
      }
    }
  }
  Pivots pivots(std::min(m, n));
  const auto before = band.values;
  const auto pivot_before = pivots.values;
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc_lu_band_test::QueryFactor(provider, matrix, swaps);
  }));
  ASC_DENSE_TEST_CHECK(test, band_test::SameBytes(before, band.values));
  ASC_DENSE_TEST_EQ(test, pivot_before, pivots.values);
  Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc_lu_band_test::Factor(provider, matrix, swaps, plan, workspace,
                                    report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  band.Padding(test, before);
  pivots.Check(test);
  scratch.Check(test);
  if (!status.ok()) {
    return;
  }
  if (std::min(m, n) == 0) {
    ASC_DENSE_TEST_CHECK(test, band_test::SameBytes(before, band.values));
  }
  if (noncommuting) {
    ASC_DENSE_TEST_EQ(test, pivots.values[1], 3);
    ASC_DENSE_TEST_EQ(test, pivots.values[2], 3);
  }
  band.Reconstruct(test, pivots);
  const auto factor = Take(WithoutAllocation(test, [&] {
    return asc::ReferenceLuBandFactorView<T>::Create(
        provider, band.ConstView(), pivots.ConstView(), report);
  }));
  const std::string_view origin(report.routine.data());
  ASC_DENSE_TEST_CHECK(
      test, origin.ends_with(band_test::g_unblocked ? "gbtf2" : "gbtrf"));
  ASC_DENSE_TEST_EQ(test, factor.originating_routine(), origin);
  std::printf(
      "%s numerical m=%lld n=%lld kl=%lld ku=%lld exponent=%d "
      "reconstruction=checked\n",
      band_test::g_unblocked ? "gbtf2" : "gbtrf", static_cast<long long>(m),
      static_cast<long long>(n), static_cast<long long>(kl),
      static_cast<long long>(ku), exponent);
  if (m != n) {
    return;
  }
  SolveCases(test, provider, band, pivots, factor);
}

template <typename T>
void Singular(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::extent_t n, asc::extent_t zero) {
  Band<T> band(n, n, 2, 1);
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = std::max<asc::extent_t>(0, i - 2);
         j < std::min(n, i + 2); ++j) {
      band.Put(i, j, band_test::Value<T>(i == j && i != zero ? 2 : 0));
    }
  }
  Pivots pivots(n);
  const auto before = band.values;
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan =
      Take(asc_lu_band_test::QueryFactor(provider, matrix, swaps));
  Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc_lu_band_test::Factor(provider, matrix, swaps, plan, workspace,
                                    report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_CHECK(
      test, report.called_provider && report.native_info == zero + 1);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index, zero);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  band.Padding(test, before);
  band.Reconstruct(test, pivots);
  pivots.Check(test);
  scratch.Check(test);
  const auto rejected = WithoutAllocation(test, [&] {
    return asc::ReferenceLuBandFactorView<T>::Create(
        provider, band.ConstView(), pivots.ConstView(), report);
  });
  ASC_DENSE_TEST_EQ(test, rejected.status().code(),
                    asc::ErrorCode::kInvalidState);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  constexpr std::array<std::array<asc::extent_t, 4>, 18> kShapes{
      {{0, 0, 0, 0},
       {0, 7, 3, 4},
       {7, 0, 4, 3},
       {1, 1, 0, 0},
       {1, 1, 4, 6},
       {3, 3, 2, 2},
       {7, 7, 0, 3},
       {7, 7, 3, 0},
       {7, 7, 1, 2},
       {9, 6, 3, 1},
       {6, 9, 1, 4},
       {129, 129, 40, 67},
       {131, 117, 32, 66},
       {117, 131, 40, 67},
       {8, 8, 2, 65},
       {1, 1, 32, 65},
       {1, 4, 32, 65},
       {4, 1, 32, 65}}};
  for (const auto& shape : kShapes) {
    Exercise<T>(test, provider, shape[0], shape[1], shape[2], shape[3]);
  }
  Exercise<T>(test, provider, 3, 3, 2, 2, 0, true);
  constexpr int kExponent = sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
  Exercise<T>(test, provider, 7, 7, 2, 3, kExponent);
  Exercise<T>(test, provider, 7, 7, 2, 3, -kExponent);
  Singular<T>(test, provider, 1, 0);
  for (const asc::extent_t zero : {0, 3, 6}) {
    Singular<T>(test, provider, 7, zero);
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (!asc_lu_band_test::SelectFactor(argc, argv)) {
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
