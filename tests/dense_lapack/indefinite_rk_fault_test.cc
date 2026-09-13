#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_faults.h"
#include "indefinite_rk_fixture.h"
#include "indefinite_rk_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace support = asc_rk_test;
namespace fault = asc_rk_fault_test;
using support::Take;
using support::TestContext;
constexpr std::array kFaults{fault::Fault::kPass,
                             fault::Fault::kOmitInfo,
                             fault::Fault::kWrite16BitInfo,
                             fault::Fault::kWrite32BitInfo,
                             fault::Fault::kNegativeInfo,
                             fault::Fault::kLargeInfo,
                             fault::Fault::kZeroPivot,
                             fault::Fault::kBrokenPair,
                             fault::Fault::kWrongDirection,
                             fault::Fault::kChangedE,
                             fault::Fault::kChangedFactorSlot,
                             fault::Fault::kChangedWork};
bool IsDefect(fault::Fault selected) {
  if (selected == fault::Fault::kPass) {
    return false;
  }
  if (selected == fault::Fault::kWrite32BitInfo) {
    return sizeof(lapack_int) != 4;
  }
  return true;
}

template <typename T>
void CheckCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
               const support::Sample<T>& input,
               const support::Sample<T>& baseline,
               const asc::LapackReport& baseline_report, bool blocked,
               fault::Fault selected, int pair) {
  auto sample = input;
  const auto routine = blocked ? fault::Routine::kTrf : fault::Routine::kTf2;
  fault::SetFault(routine, selected);
  const auto a = sample.View();
  const auto e = support::OffDiagonal(sample.e, sample.n);
  const auto pivots = support::Pivots(sample.pivots, sample.n);
  const auto plan = Take(support::WithoutAllocation(test, [&] {
    return support::QueryFactor(provider, sample.triangle, sample.hermitian,
                                blocked, a, e, pivots);
  }));
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  ASC_DENSE_TEST_CHECK(
      test,
      support::EqualBytes(sample.a.data(), input.a.data(), sizeof(sample.a)));
  ASC_DENSE_TEST_EQ(test, sample.e, input.e);
  ASC_DENSE_TEST_EQ(test, sample.pivots, input.pivots);
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return support::Factor(provider, sample.triangle, sample.hermitian, blocked,
                           a, e, pivots, plan, workspace, report);
  });
  const bool empty = sample.n == 0;
  const bool defect = !empty && IsDefect(selected);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), empty ? 0U : 1U);
  ASC_DENSE_TEST_EQ(test, report.called_provider, !empty);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), !empty);
  if (empty) {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
  } else {
    ASC_DENSE_TEST_CHECK(test, fault::SeedWasFullWidth());
    ASC_DENSE_TEST_EQ(test, fault::LastNativeInfo(),
                      baseline_report.native_info.value_or(-1));
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0),
                      fault::LastPublishedInfo());
    if (defect) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index);
    } else {
      ASC_DENSE_TEST_EQ(test, status.code(),
                        baseline_report.native_info == 0
                            ? asc::ErrorCode::kOk
                            : asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_EQ(test, report.outcome, baseline_report.outcome);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        baseline_report.output_validity);
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index,
                        baseline_report.diagnostic_index);
    }
  }
  auto expected_a = baseline.a;
  auto expected_e = baseline.e;
  if (!empty && selected == fault::Fault::kChangedE) {
    expected_e[sample.triangle == support::kUpper ? 1 : sample.n] = T{7};
  }
  if (!empty && selected == fault::Fault::kChangedFactorSlot) {
    ASC_DENSE_TEST_CHECK(test, pair >= 0 && pair + 1 < sample.n);
    const int i = sample.triangle == support::kUpper ? pair : pair + 1;
    const int j = sample.triangle == support::kUpper ? pair + 1 : pair;
    expected_a[sample.Offset(i, j)] = T{9};
  }
  const bool withheld =
      empty || (defect && (sample.hermitian || sample.layout == support::kRow));
  const auto& expected = withheld ? input.a : expected_a;
  ASC_DENSE_TEST_CHECK(
      test,
      support::EqualBytes(sample.a.data(), expected.data(), sizeof(sample.a)));
  ASC_DENSE_TEST_CHECK(test,
                       support::EqualBytes(sample.e.data(), expected_e.data(),
                                           sizeof(sample.e)));
  ASC_DENSE_TEST_EQ(test, sample.pivots,
                    empty || defect ? input.pivots : baseline.pivots);
  sample.Guards(test);
  scratch.Guards(test, workspace);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  int empty_cases = 0;
  const std::array<int, 7> orders{0, 1, 2, 3, 67, 1, 67};
  for (const auto triangle : {support::kUpper, support::kLower}) {
    for (const auto layout : {support::kColumn, support::kRow}) {
      for (bool blocked : {false, true}) {
        for (std::size_t fixture = 0; fixture < orders.size(); ++fixture) {
          support::Sample<T> input(orders[fixture], hermitian, triangle, layout,
                                   0, fixture >= 5);
          if (fixture == 2) {
            input.a[input.Offset(0, 0)] = T{};
            input.a[input.Offset(1, 1)] = T{};
            const int i = triangle == support::kUpper ? 0 : 1;
            const int j = triangle == support::kUpper ? 1 : 0;
            input.a[input.Offset(i, j)] = support::Value<T>(2, 1);
            input.original = input.a;
          }
          auto baseline = input;
          const auto routine =
              blocked ? fault::Routine::kTrf : fault::Routine::kTf2;
          fault::SetFault(routine, fault::Fault::kPass);
          const auto a = baseline.View();
          const auto e = support::OffDiagonal(baseline.e, baseline.n);
          const auto pivots = support::Pivots(baseline.pivots, baseline.n);
          const auto plan = Take(support::QueryFactor(
              provider, triangle, hermitian, blocked, a, e, pivots));
          support::Scratch<T> scratch;
          asc::LapackReport report;
          const auto status =
              support::Factor(provider, triangle, hermitian, blocked, a, e,
                              pivots, plan, scratch.Workspace(plan), report);
          ASC_DENSE_TEST_EQ(
              test, status.code(),
              fixture >= 5 ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk);
          int pair = -1;
          for (int i = 0; i + 1 < input.n; ++i) {
            if (baseline.pivots[i + 1] < 0) {
              pair = i;
              break;
            }
          }
          if (fixture == 2 || fixture == 3 || fixture == 4 || fixture == 6) {
            ASC_DENSE_TEST_CHECK(test, pair >= 0);
          }
          for (const auto selected : kFaults) {
            if ((!blocked && selected == fault::Fault::kChangedWork) ||
                (input.n != 0 && pair < 0 &&
                 selected == fault::Fault::kChangedFactorSlot)) {
              continue;
            }
            CheckCase(test, provider, input, baseline, report, blocked,
                      selected, pair);
            ++cases;
            if (input.n == 0) {
              ++empty_cases;
            }
          }
        }
      }
    }
  }
  std::printf("RK fault cases=%d empty_noncalls=%d\n", cases, empty_cases);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false);
  }
  if (scalar == "d") {
    return Run<double>(false);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true);
  }
  return 2;
}
