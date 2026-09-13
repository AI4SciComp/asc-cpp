#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_solve_faults.h"
#include "indefinite_packed_solve_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"

namespace {
namespace base = asc_indefinite_test;
namespace solve = asc_packed_solve_test;
namespace faults = asc_packed_solve_fault_test;
using faults::Fault;

bool Accepted(Fault fault) {
  return fault == Fault::kPass ||
         (fault == Fault::kWrite32BitInfo && sizeof(lapack_int) == 4);
}

void Report(base::TestContext& test, const asc::Status& status,
            const asc::LapackReport& report, Fault fault) {
  const auto info = faults::LastPublishedInfo();
  const bool accepted = Accepted(fault);
  ASC_DENSE_TEST_EQ(test, status.code(),
                    accepted ? asc::ErrorCode::kOk : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_CHECK(test, faults::SeedWasFullWidth());
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, info);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value() &&
                                 !report.diagnostic_index.has_value());
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    accepted ? asc::LapackOutputValidity::kComplete
                             : asc::LapackOutputValidity::kUnusable);
  if (accepted) {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  } else {
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      info < 0 ? asc::LapackOutcome::kProviderArgument
                               : asc::LapackOutcome::kPartialResult);
  }
  const bool argument =
      info < 0 && info != std::numeric_limits<lapack_int>::min();
  ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(), argument);
  if (argument) {
    ASC_DENSE_TEST_EQ(test, report.native_argument, -info);
  }
}

template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          solve::Fixture<T> sample, Fault fault) {
  sample.Prepare(test, provider);
  faults::SetFault(fault);
  const auto factors = sample.a.ConstView();
  const auto pivots = sample.Pivots();
  const auto rhs = sample.Rhs();
  const auto plan = solve::Take(base::WithoutAllocation(test, [&] {
    return solve::Query(provider, sample.a.triangle, sample.a.hermitian,
                        factors, pivots, rhs);
  }));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
  base::Scratch<T> scratch;
  const auto work = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return solve::Solve(provider, sample.a.triangle, sample.a.hermitian,
                        factors, pivots, rhs, plan, work, report);
  });
  const bool active = sample.a.n != 0 && sample.nrhs != 0;
  if (active) {
    Report(test, status, report, fault);
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
  }
  if (!active || (!Accepted(fault) && sample.rhs_layout == solve::kRow)) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.rhs.data(), sample.original_rhs.data(),
                               sizeof(sample.rhs)));
  } else {
    sample.Mathematics(test);
  }
  sample.UnchangedInputs(test);
  sample.Padding(test);
  scratch.Guards(test, work);
}

template <typename T>
int Run(bool hermitian) {
  base::TestContext test;
  const auto provider = solve::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (int n : {0, 1, 2, 5}) {
    for (int nrhs : {0, 1, 3}) {
      for (auto triangle : {solve::kUpper, solve::kLower}) {
        for (auto al : {solve::kColumn, solve::kRow}) {
          for (auto bl : {solve::kColumn, solve::kRow}) {
            for (auto fault :
                 {Fault::kPass, Fault::kOmitInfo, Fault::kWrite16BitInfo,
                  Fault::kWrite32BitInfo, Fault::kNegativeInfo,
                  Fault::kPositiveInfo, Fault::kMaximumInfo}) {
              Case(
                  test, provider,
                  solve::Fixture<T>(n, nrhs, hermitian, triangle, al, bl, 0, 3),
                  fault);
              ++cases;
            }
          }
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, 672);
  std::printf("Packed solve native fault cases=%d\n", cases);
  return test.Finish();
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
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
