#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "band_cholesky_test_support.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)

void CheckReport(TestContext& test, const asc::LapackReport& report,
                 const asc::ReferenceLapackProvider& provider) {
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
}

template <typename T>
void SolveReuse(TestContext& test, const asc::ReferenceLapackProvider& provider,
                BandData<T>& band) {
  const auto n = band.n;
  const auto kd = band.kd;
  const auto layout = band.layout;
  asc::LapackReport report;
  const auto factors = band.values;
  for (const auto rhs_layout : {kColumn, kRow}) {
    // Two and then three RHS exercise reuse of one factor; zero RHS still
    // executes the source quick return without reading factors.
    for (const asc::extent_t count : {2, 3, 0}) {
      RhsData<T> rhs(band, count, rhs_layout);
      const auto rhs_before = rhs.values;
      const auto solve_plan = Take(WithoutAllocation(test, [&] {
        return asc::QueryPbtrsWorkspace(provider, band.ConstView(), rhs.View());
      }));
      ASC_DENSE_TEST_CHECK(test, SameBytes(rhs_before, rhs.values));
      ASC_DENSE_TEST_CHECK(test, SameBytes(factors, band.values));
      const auto expected_count =
          n == 0 || count == 0 ? 0
                               : (layout == kRow ? n * (kd + 1) : 0) +
                                     (rhs_layout == kRow ? n * count : 0);
      ASC_DENSE_TEST_EQ(test, solve_plan.regions[kLayout].minimum_entries,
                        expected_count);
      Storage<T> solve_storage(solve_plan);
      auto solve_workspace = solve_storage.View();
      const auto solved = WithoutAllocation(test, [&] {
        return asc::Pbtrs(provider, band.ConstView(), rhs.View(), solve_plan,
                          solve_workspace, report);
      });
      ASC_DENSE_TEST_CHECK(test, solved.ok());
      CheckReport(test, report, provider);
      ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()).substr(1),
                        "pbtrs");
      rhs.Check(test, band, rhs_before);
      ASC_DENSE_TEST_CHECK(test, SameBytes(factors, band.values));
      solve_storage.Check(test);
    }
  }
}

template <typename T>
void Exercise(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::extent_t n, asc::extent_t kd,
              asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
              bool blocked, int scale = 0, bool poison_ignored = false) {
  BandData<T> band(n, kd, triangle, layout, scale);
  if (poison_ignored) {
    const auto nan = std::numeric_limits<long double>::quiet_NaN();
    for (std::size_t i = 0; i < band.values.size(); ++i) {
      if (band.selected[i] == 0) {
        band.values[i] = Value<T>(nan, nan);
      }
    }
    if constexpr (asc::DenseBlasComplex<T>) {
      for (asc::extent_t i = 0; i < n; ++i) {
        band.values[band.Index(i, i)].imag(
            std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
      }
    }
  }
  const auto before = band.values;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return blocked ? asc::QueryPbtrfWorkspace(provider, band.View())
                   : asc::QueryPbtf2Workspace(provider, band.View());
  }));
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries,
                    layout == kRow ? n * (kd + 1) : 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries,
                    plan.regions[kLayout].preferred_entries);
  Storage<T> storage(plan);
  auto workspace = storage.View();
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return blocked ? asc::Pbtrf(provider, band.View(), plan, workspace, report)
                   : asc::Pbtf2(provider, band.View(), plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  if (!status.ok()) {
    std::fprintf(stderr, "Factor n=%lld kd=%lld layout=%d blocked=%d code=%d\n",
                 static_cast<long long>(n), static_cast<long long>(kd),
                 static_cast<int>(layout), blocked,
                 static_cast<int>(status.code()));
    return;
  }
  CheckReport(test, report, provider);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    std::optional{asc::LapackFactorFamily::kCholesky});
  const std::string_view name(report.routine.data());
  ASC_DENSE_TEST_EQ(test, name.substr(1), blocked ? "pbtrf" : "pbtf2");
  band.CheckFactor(test);
  band.CheckPadding(test, before);
  storage.Check(test);
  SolveReuse(test, provider, band);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  constexpr int kScale =
      std::is_same_v<asc::DenseBlasRealType<T>, float> ? 50 : 450;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool blocked : {false, true}) {
        for (const auto shape : std::array<std::array<asc::extent_t, 2>, 6>{
                 {{0, 0}, {1, 0}, {5, 0}, {6, 2}, {4, 9}, {96, 65}}}) {
          Exercise<T>(test, provider, shape[0], shape[1], triangle, layout,
                      blocked);
        }
        Exercise<T>(test, provider, 6, 2, triangle, layout, blocked, kScale);
        Exercise<T>(test, provider, 6, 2, triangle, layout, blocked, -kScale);
        Exercise<T>(test, provider, 6, 2, triangle, layout, blocked, 0, true);
        Exercise<T>(test, provider, 96, 65, triangle, layout, blocked, 0, true);
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
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
