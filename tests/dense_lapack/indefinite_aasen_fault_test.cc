#include <complex>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_faults.h"
#include "indefinite_aasen_fixture.h"
#include "indefinite_aasen_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_test;
namespace faults = asc_aasen_fault_test;
using base::TestContext;
using faults::Fault;
template <typename T>
bool Outcome(TestContext& test, Fault fault, const asc::Status& status,
             const asc::LapackReport& report) {
  const bool pass =
      fault == Fault::kPass ||
      (ASC_LAPACK_INTEGER_BITS == 32 && (fault == Fault::kWrite32BitInfo ||
                                         fault == Fault::kWrite32BitPivots)) ||
      (!asc::DenseBlasComplex<T> && fault == Fault::kRealOnlyWork);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kAasen);
  ASC_DENSE_TEST_EQ(test, status.code(),
                    pass ? asc::ErrorCode::kOk : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    pass ? asc::LapackOutputValidity::kComplete
                         : asc::LapackOutputValidity::kUnusable);
  std::int64_t expected = 0;
  if (fault == Fault::kOmitInfo || (fault == Fault::kWrite32BitInfo && !pass)) {
    expected = ASC_LAPACK_INTEGER_BITS == 32
                   ? std::int64_t{std::numeric_limits<std::int32_t>::min()}
                   : std::numeric_limits<std::int64_t>::min();
  } else if (fault == Fault::kNegativeInfo) {
    expected = -7;
  } else if (fault == Fault::kPositiveInfo) {
    expected = 1;
  }
  ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(17), expected);
  return pass;
}
template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          bool he, asc::DenseBlasTriangle tri, asc::DenseBlasLayout layout,
          int n, bool preferred, Fault fault) {
  aa::Sample<T> sample(n, he, tri, layout, 0, false);
  const auto before_pivots = sample.pivots;
  const auto pivots = base::Pivots(sample.pivots, n);
  const auto plan =
      base::Take(aa::Query(provider, tri, he, sample.View(), pivots));
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(
      plan, preferred ? -1 : plan.regions[base::kScalar].minimum_entries);
  asc::LapackReport report;
  faults::SetFault(fault);
  const auto status = base::WithoutAllocation(test, [&] {
    return aa::Factor(provider, tri, he, sample.View(), pivots, plan, workspace,
                      report);
  });
  if (Outcome<T>(test, fault, status, report)) {
    sample.Reconstruction(test);
  } else {
    ASC_DENSE_TEST_EQ(test, sample.pivots, before_pivots);
    if (he || layout == base::kRow) {
      ASC_DENSE_TEST_CHECK(
          test, base::EqualBytes(sample.a.data(), sample.original.data(),
                                 sizeof(sample.a)));
    }
  }
  sample.Guards(test);
  scratch.Guards(test, workspace);
}
template <typename T>
int Run(bool he) {
  TestContext test;
  int cases = 0;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (int raw = 0; raw <= static_cast<int>(Fault::kRealOnlyWork); ++raw) {
    for (const auto tri : {base::kUpper, base::kLower}) {
      for (const auto layout : {base::kColumn, base::kRow}) {
        for (int n : {1, 3}) {
          for (bool preferred : {false, true}) {
            Case<T>(test, provider, he, tri, layout, n, preferred,
                    static_cast<Fault>(raw));
            ++cases;
          }
        }
      }
    }
  }
  std::printf("Aasen fault cases=%d\n", cases);
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
