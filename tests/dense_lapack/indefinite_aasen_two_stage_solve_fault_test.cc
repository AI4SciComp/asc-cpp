#include <complex>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_solve_faults.h"
#include "indefinite_aasen_two_stage_solve_fixture.h"
#include "indefinite_aasen_two_stage_solve_test_support.h"
#include "indefinite_aasen_two_stage_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_two_stage_solve_test;
namespace faults = asc_aasen_two_stage_solve_fault_test;
using faults::Fault;
bool Accepted(int n, int nrhs, Fault fault) {
  return nrhs == 0 || fault == Fault::kPass ||
         (ASC_LAPACK_INTEGER_BITS == 32 && (fault == Fault::kWrite32BitInfo ||
                                            fault == Fault::kUpperOuterPivot ||
                                            fault == Fault::kUpperBandPivot)) ||
         (n == 1 && (fault == Fault::kChangedOuterPivot ||
                     fault == Fault::kChangedBandPivot));
}
std::int64_t ExpectedInfo(int n, Fault fault) {
  if (fault == Fault::kOmitInfo ||
      (fault == Fault::kWrite32BitInfo && ASC_LAPACK_INTEGER_BITS == 64)) {
    return ASC_LAPACK_INTEGER_BITS == 32
               ? std::int64_t{std::numeric_limits<std::int32_t>::min()}
               : std::numeric_limits<std::int64_t>::min();
  }
  if (fault == Fault::kNegativeInfo) {
    return -10;
  }
  if (fault == Fault::kPositiveInfo) {
    return 1;
  }
  if (fault == Fault::kInfoAtOrder) {
    return n;
  }
  if (fault == Fault::kInfoBeyondOrder) {
    return n + 1;
  }
  return 0;
}
void CheckReport(base::TestContext& test, int n, int nrhs, Fault fault,
                 const asc::Status& status, const asc::LapackReport& report) {
  const bool active = nrhs != 0;
  const bool accepted = Accepted(n, nrhs, fault);
  ASC_DENSE_TEST_EQ(test, status.code(),
                    accepted ? asc::ErrorCode::kOk : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(test, report.called_provider, active);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), active ? 1U : 0U);
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), 0);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kAasen);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    accepted ? asc::LapackOutputValidity::kComplete
                             : asc::LapackOutputValidity::kUnusable);
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  if (active) {
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-91),
                      ExpectedInfo(n, fault));
  }
  if (accepted) {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  } else {
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      ExpectedInfo(n, fault) < 0
                          ? asc::LapackOutcome::kProviderArgument
                          : asc::LapackOutcome::kPartialResult);
  }
  ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(),
                    active && fault == Fault::kNegativeInfo);
  if (report.native_argument.has_value()) {
    ASC_DENSE_TEST_EQ(test, *report.native_argument, 10);
  }
}
template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          bool he, bool upper, bool row, bool rhs_row, int n, int nrhs,
          int capacity, Fault fault) {
  aa::Sample<T> sample(n, nrhs, he, upper, row, rhs_row, capacity, capacity);
  sample.Produce(test, false);
  const auto tri = upper ? base::kUpper : base::kLower;
  const auto a = aa::Factors(sample);
  const auto tb = aa::Vector(sample.tb, sample.ltb);
  const auto p = aa::Pivots(sample);
  const auto q = aa::Vector(sample.q, sample.n);
  const auto b = aa::Rhs(sample);
  const auto plan = base::Take(aa::Query(provider, tri, he, a, tb, p, q, b));
  asc_aasen_two_stage_test::Scratch<T> scratch(plan, 0);
  const auto before_a = sample.a;
  const auto before_tb = sample.tb;
  const auto before_p = sample.p;
  const auto before_q = sample.q;
  const auto before_integers = scratch.integers;
  const auto before_packed = scratch.packed;
  asc::LapackReport report;
  faults::SetFault(fault);
  const auto status = base::WithoutAllocation(test, [&] {
    return aa::Solve(provider, tri, he, a, tb, p, q, b, plan, scratch.workspace,
                     report);
  });
  CheckReport(test, n, nrhs, fault, status, report);
  ASC_DENSE_TEST_CHECK(test, base::EqualBytes(sample.a.data(), before_a.data(),
                                              sample.a.size() * sizeof(T)));
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(sample.tb.data(), before_tb.data(),
                                        sample.tb.size() * sizeof(T)));
  ASC_DENSE_TEST_EQ(test, sample.p, before_p);
  ASC_DENSE_TEST_EQ(test, sample.q, before_q);
  if (nrhs != 0 && Accepted(n, nrhs, fault)) {
    sample.Solution(test);
  }
  if (nrhs == 0 || (!Accepted(n, nrhs, fault) && rhs_row)) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), sample.before_b.data(),
                               sample.b.size() * sizeof(T)));
  }
  if (nrhs == 0) {
    ASC_DENSE_TEST_EQ(test, scratch.integers, before_integers);
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(scratch.packed.data(), before_packed.data(),
                               scratch.packed.size() * sizeof(T)));
  }
  sample.RhsGuards(test);
  scratch.Guards(test);
}
template <typename T>
int Run(bool he) {
  base::TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (int raw = 0; raw <= static_cast<int>(Fault::kChangedBandPivot); ++raw) {
    for (bool upper : {false, true}) {
      for (bool row : {false, true}) {
        for (bool rhs_row : {false, true}) {
          for (int n : {1, 3}) {
            for (int nrhs : {0, 2}) {
              for (int capacity : {0, 2}) {
                Case<T>(test, provider, he, upper, row, rhs_row, n, nrhs,
                        capacity, static_cast<Fault>(raw));
                ++cases;
              }
            }
          }
        }
      }
    }
  }
  std::printf("Two-stage Aasen solve fault cases=%d\n", cases);
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
