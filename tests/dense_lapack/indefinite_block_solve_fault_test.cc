#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_block_solve_faults.h"
#include "indefinite_block_solve_fixture.h"
#include "indefinite_block_solve_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace support = asc_indefinite_test;
namespace faults = asc_block_solve_fault_test;
using asc_block_solve_test::RightHandSides;
using asc_block_solve_test::Sample;
using faults::Fault;
using support::Take;
using support::TestContext;
bool Accepted(Fault fault) {
  return fault == Fault::kPass || fault == Fault::kChangedWork ||
         (fault == Fault::kWrite32BitInfo && sizeof(lapack_int) == 4);
}
void Report(TestContext& test, const asc::Status& status,
            const asc::LapackReport& report, Fault fault) {
  const bool accepted = Accepted(fault);
  const auto info = faults::LastPublishedInfo();
  ASC_DENSE_TEST_EQ(test, status.code(),
                    accepted ? asc::ErrorCode::kOk : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_CHECK(test, faults::SeedWasFullWidth());
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), info);
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  if (accepted) {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
  } else {
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      info < 0 ? asc::LapackOutcome::kProviderArgument
                               : asc::LapackOutcome::kPartialResult);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
  }
  const bool argument =
      info < 0 && info != std::numeric_limits<lapack_int>::min();
  ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(), argument);
  if (argument) {
    ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(-999), -info);
  }
}
template <typename T>
void Execute(TestContext& test, const asc::ReferenceLapackProvider& provider,
             const Sample<T>& sample, int nrhs, asc::DenseBlasLayout b_layout,
             Fault fault) {
  RightHandSides<T> rhs(sample, nrhs, b_layout);
  const auto before_a = sample.a;
  const auto before_pivots = sample.pivots;
  const auto raw = support::Raw(sample.pivots, sample.n);
  faults::SetFault(fault);
  const auto plan = Take(support::WithoutAllocation(test, [&] {
    return asc_block_solve_test::Query(provider, sample.triangle,
                                       sample.hermitian, sample.ConstView(),
                                       raw, rhs.View());
  }));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return asc_block_solve_test::Solve(
        provider, sample.triangle, sample.hermitian, sample.ConstView(), raw,
        rhs.View(), plan, workspace, report);
  });
  if (nrhs == 0) {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
  } else {
    Report(test, status, report, fault);
  }
  ASC_DENSE_TEST_CHECK(
      test,
      support::EqualBytes(sample.a.data(), before_a.data(), sizeof(before_a)));
  ASC_DENSE_TEST_EQ(test, sample.pivots, before_pivots);
  if (nrhs == 0 || (b_layout == support::kRow && !Accepted(fault))) {
    ASC_DENSE_TEST_CHECK(
        test, support::EqualBytes(rhs.values.data(), rhs.before.data(),
                                  sizeof(rhs.before)));
  } else {
    rhs.Verify(test, sample);
  }
  sample.Guards(test);
  rhs.Guards(test);
  scratch.Guards(test, workspace);
}
template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto context = asc::ExecutionContext::Serial();
  const auto provider = Take(asc::ReferenceLapackProvider::Create(context));
  int cases = 0;
  for (const auto triangle : {support::kUpper, support::kLower}) {
    for (const auto al : {support::kColumn, support::kRow}) {
      for (const bool blocked : {false, true}) {
        Sample<T> sample(3, hermitian, triangle, al, 0, false);
        support::Scratch<T> scratch;
        const auto plan = Take(support::QueryFactor(
            provider, triangle, hermitian, blocked, sample.View(),
            support::Pivots(sample.pivots, sample.n)));
        const auto work = scratch.Workspace(plan);
        asc::LapackReport report;
        ASC_DENSE_TEST_CHECK(
            test, support::Factor(provider, triangle, hermitian, blocked,
                                  sample.View(),
                                  support::Pivots(sample.pivots, sample.n),
                                  plan, work, report)
                      .ok());
        for (const auto bl : {support::kColumn, support::kRow}) {
          for (const int nrhs : {0, 1, 3}) {
            for (const auto fault :
                 {Fault::kPass, Fault::kOmitInfo, Fault::kWrite16BitInfo,
                  Fault::kWrite32BitInfo, Fault::kNegativeInfo,
                  Fault::kPositiveInfo, Fault::kChangedPivot,
                  Fault::kChangedFactor, Fault::kChangedWork}) {
              Execute(test, provider, sample, nrhs, bl, fault);
              ++cases;
            }
          }
        }
        scratch.Guards(test, work);
      }
    }
  }
  std::printf("block solve fault cases=%d\n", cases);
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
