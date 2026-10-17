#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_inverse_faults.h"
#include "indefinite_packed_inverse_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"
namespace {
namespace base = asc_indefinite_test;
namespace inverse = asc_packed_inverse_test;
namespace faults = asc_packed_inverse_fault_test;
using base::TestContext;
using faults::Fault;
bool Valid(Fault fault) {
  return fault == Fault::kPass ||
         (fault == Fault::kWrite32BitInfo && sizeof(lapack_int) == 4);
}
void Outcome(TestContext& test, Fault fault, const asc::Status& status,
             const asc::LapackReport& report, asc::index_t expected_info) {
  const bool valid = Valid(fault);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_CHECK(test, faults::SeedWasFullWidth());
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), expected_info);
  ASC_DENSE_TEST_CHECK(
      test, report.called_provider && report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999),
                    faults::LastPublishedInfo());
  ASC_DENSE_TEST_EQ(test, status.code(),
                    valid ? (expected_info == 0 ? asc::ErrorCode::kOk
                                                : asc::ErrorCode::kNumerical)
                          : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  if (valid) {
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      expected_info == 0 ? asc::LapackOutcome::kSuccess
                                         : asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      expected_info == 0
                          ? asc::LapackOutputValidity::kComplete
                          : asc::LapackOutputValidity::kDocumentedPartial);
    if (expected_info > 0) {
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                        expected_info - 1);
    }
  } else {
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    const auto info = faults::LastPublishedInfo();
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      info < 0 ? asc::LapackOutcome::kProviderArgument
                               : asc::LapackOutcome::kPartialResult);
    const bool argument =
        info < 0 && info != std::numeric_limits<lapack_int>::min();
    ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(), argument);
    if (argument) {
      ASC_DENSE_TEST_EQ(test, report.native_argument, -info);
    }
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  }
}

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          inverse::Sample<T>& sample, Fault fault) {
  const auto expected_info = inverse::Prepare(test, provider, sample);
  faults::SetFault(fault);
  const auto raw = base::Raw(sample.pivots, sample.n);
  const auto before = sample.a;
  const auto pivots_before = sample.pivots;
  const auto plan = inverse::Take(base::WithoutAllocation(test, [&] {
    return inverse::Query(provider, sample.triangle, sample.hermitian,
                          sample.View(), raw);
  }));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return inverse::Inverse(provider, sample.triangle, sample.hermitian,
                            sample.View(), raw, plan, workspace, report);
  });
  if (sample.n != 0) {
    Outcome(test, fault, status, report, expected_info);
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
  }
  ASC_DENSE_TEST_EQ(test, sample.pivots, pivots_before);
  if (sample.n == 0 || expected_info > 0 ||
      (!Valid(fault) && sample.layout == base::kRow)) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.a.data(), before.data(), sizeof(before)));
  } else {
    inverse::Mathematics(test, sample);
  }
  sample.Guards(test);
  scratch.Guards(test, workspace);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (int n : {0, 1, 2, 5}) {
        for (int kind : {0, 1, 2, 3}) {
          for (const Fault fault :
               {Fault::kPass, Fault::kOmitInfo, Fault::kWrite16BitInfo,
                Fault::kWrite32BitInfo, Fault::kNegativeInfo,
                Fault::kMaximumInfo, Fault::kOutOfRangeInfo,
                Fault::kContradictoryInfo, Fault::kWrongIndex,
                Fault::kChangedPivot}) {
            inverse::Sample<T> sample(n, hermitian, triangle, layout, 0);
            sample.Reset(kind);
            Case(test, provider, sample, fault);
            ++cases;
          }
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, 640);
  std::printf("Packed inverse native fault cases=%d\n", cases);
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
