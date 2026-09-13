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
#include "indefinite_aasen_driver_faults.h"
#include "indefinite_aasen_driver_test_support.h"
#include "indefinite_aasen_solve_fixture.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_solve_test;
namespace faults = asc_aasen_driver_fault_test;
namespace driver = asc_aasen_driver_test;
using faults::Fault;
template <typename T>
bool CheckReport(base::TestContext& test, int n, int nrhs, Fault fault,
                 const asc::Status& status, const asc::LapackReport& report) {
  const bool singular_input = fault >= Fault::kSingularSentinelWitness;
  const bool success =
      fault == Fault::kPass ||
      (ASC_LAPACK_INTEGER_BITS == 32 &&
       (fault == Fault::kWrite32BitInfo || fault == Fault::kClobberUpperPivot ||
        fault == Fault::kWrite32BitPivot)) ||
      (!asc::DenseBlasComplex<T> &&
       (fault == Fault::kNonzeroImagRecommendation ||
        fault == Fault::kRealOnlyRecommendation)) ||
      (nrhs == 0 && singular_input && fault != Fault::kSingularWrongInfo);
  const bool numerical =
      nrhs != 0 && (fault == Fault::kConsistentSingular ||
                    (!asc::DenseBlasComplex<T> &&
                     fault == Fault::kSingularRealOnlyWitness) ||
                    (n == 1 && fault == Fault::kSingularWrongInfo));
  auto expected = asc::ErrorCode::kProvider;
  if (success) {
    expected = asc::ErrorCode::kOk;
  } else if (numerical) {
    expected = asc::ErrorCode::kNumerical;
  }
  ASC_DENSE_TEST_EQ(test, status.code(), expected);
  ASC_DENSE_TEST_EQ(test, report.called_provider, true);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), true);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(),
                    nrhs != 0 && singular_input ? n : 0);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kAasen);
  ASC_DENSE_TEST_EQ(
      test, report.output_validity,
      success ? asc::LapackOutputValidity::kComplete
              : (numerical ? asc::LapackOutputValidity::kDocumentedPartial
                           : asc::LapackOutputValidity::kUnusable));
  {
    std::int64_t info = nrhs != 0 && singular_input ? n : 0;
    if (fault == Fault::kOmitInfo ||
        (fault == Fault::kWrite32BitInfo && ASC_LAPACK_INTEGER_BITS == 64)) {
      info = ASC_LAPACK_INTEGER_BITS == 32
                 ? std::int64_t{std::numeric_limits<std::int32_t>::min()}
                 : std::numeric_limits<std::int64_t>::min();
    } else if (fault == Fault::kNegativeInfo) {
      info = -10;
    } else if (fault == Fault::kInfoBeyondOrder) {
      info = n + 1;
    } else if (fault == Fault::kFalseSingular ||
               fault == Fault::kSingularWrongInfo) {
      info = 1;
    } else if (fault == Fault::kConsistentSingular) {
      info = n;
    }
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-91), info);
  }
  if (numerical) {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), n - 1);
  }
  return success;
}
template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          bool he, asc::DenseBlasTriangle tri, asc::DenseBlasLayout al,
          asc::DenseBlasLayout bl, int n, int nrhs, bool minimum, Fault fault) {
  const bool singular_input = fault >= Fault::kSingularSentinelWitness;
  aa::Sample<T> sample(n, nrhs, he, tri, al, bl, 0, singular_input);
  const auto before_a = sample.a;
  const auto before_pivots = sample.pivots;
  const auto pivots = base::Pivots(sample.pivots, n);
  const auto plan = base::Take(
      driver::Query(provider, tri, he, sample.View(), pivots, sample.Rhs()));
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(
      plan, minimum ? plan.regions[base::kScalar].minimum_entries : -1);
  asc::LapackReport report;
  faults::SetFault(fault);
  const auto status = base::WithoutAllocation(test, [&] {
    return driver::Driver(provider, tri, he, sample.View(), pivots,
                          sample.Rhs(), plan, workspace, report);
  });
  const bool success = CheckReport<T>(test, n, nrhs, fault, status, report);
  if (status.code() == asc::ErrorCode::kProvider) {
    ASC_DENSE_TEST_EQ(test, sample.pivots, before_pivots);
    if (he || al == base::kRow) {
      ASC_DENSE_TEST_CHECK(
          test,
          base::EqualBytes(sample.a.data(), before_a.data(), sizeof(before_a)));
    }
  }
  if (success && nrhs != 0) {
    sample.Solution(test);
    sample.Reconstruction(test);
  }
  if (nrhs == 0 || (!success && bl == base::kRow)) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), sample.original_b.data(),
                               sizeof(sample.b)));
  }
  sample.Guards(test);
  sample.RhsGuards(test);
  scratch.Guards(test, workspace);
}
template <typename T>
int Run(bool he) {
  base::TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (int raw = 0; raw <= static_cast<int>(Fault::kSingularWrongInfo); ++raw) {
    for (auto tri : {base::kUpper, base::kLower}) {
      for (auto al : {base::kColumn, base::kRow}) {
        for (auto bl : {base::kColumn, base::kRow}) {
          for (int n : {1, 3}) {
            for (int nrhs : {0, 2}) {
              for (bool minimum : {false, true}) {
                Case<T>(test, provider, he, tri, al, bl, n, nrhs, minimum,
                        static_cast<Fault>(raw));
                ++cases;
              }
            }
          }
        }
      }
    }
  }
  std::printf("Aasen driver fault cases=%d failed=%d\n", cases, test.Finish());
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
