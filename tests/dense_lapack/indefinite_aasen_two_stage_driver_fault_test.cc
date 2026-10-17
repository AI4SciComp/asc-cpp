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
#include "indefinite_aasen_two_stage_driver_faults.h"
#include "indefinite_aasen_two_stage_driver_numerical_support.h"
#include "indefinite_aasen_two_stage_driver_test_support.h"
#include "indefinite_aasen_two_stage_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
namespace {
namespace aa = asc_aasen_two_stage_driver_test;
namespace faults = asc_aasen_two_stage_driver_fault_test;
namespace base = asc_indefinite_rook_test;
using faults::Fault;
template <typename T>
bool Accepted(Fault fault) {
  return fault == Fault::kPass || fault == Fault::kRealSingular ||
         (ASC_LAPACK_INTEGER_BITS == 32 && (fault == Fault::kWrite32BitInfo ||
                                            fault == Fault::kWrite32BitOuter ||
                                            fault == Fault::kWrite32BitBand)) ||
         (!asc::DenseBlasComplex<T> &&
          (fault == Fault::kRealOnlyNb || fault == Fault::kRealOnlyWitness ||
           fault == Fault::kRealOnlyWork));
}
template <typename T>
void Outcome(base::TestContext& test, Fault fault, int n,
             const asc::Status& status, const asc::LapackReport& report) {
  const bool accepted = Accepted<T>(fault);
  const bool singular = faults::SingularInput(fault);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), singular ? n : 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kAasen);
  ASC_DENSE_TEST_EQ(
      test, status.code(),
      accepted ? (singular ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk)
               : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(
      test, report.output_validity,
      accepted ? (singular ? asc::LapackOutputValidity::kDocumentedPartial
                           : asc::LapackOutputValidity::kComplete)
               : asc::LapackOutputValidity::kUnusable);
  std::int64_t info = singular ? n : 0;
  if (fault == Fault::kOmitInfo ||
      (fault == Fault::kWrite32BitInfo && ASC_LAPACK_INTEGER_BITS == 64)) {
    info = ASC_LAPACK_INTEGER_BITS == 32
               ? std::int64_t{std::numeric_limits<std::int32_t>::min()}
               : std::numeric_limits<std::int64_t>::min();
  } else if (fault == Fault::kNegativeInfo) {
    info = -7;
  } else if (fault == Fault::kOutOfRangeInfo) {
    info = n + 1;
  } else if (fault == Fault::kFalseSingular) {
    info = n;
  }
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(17), info);
  if (accepted && singular) {
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), n - 1);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
  }
}
template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          bool he, bool upper, bool row, bool rhs_row, int n, int nrhs,
          int band, int work, Fault fault) {
  aa::Sample<T> sample(n, nrhs, he, upper, row, rhs_row, band, work,
                       faults::SingularInput(fault));
  aa::Initialize(sample);
  const auto before_a = sample.a;
  const auto a = aa::Matrix(sample);
  const auto tb = aa::Vector(sample.tb, sample.ltb);
  const auto p = aa::Vector(sample.p, n);
  const auto q = aa::Vector(sample.q, n);
  const auto b = aa::Rhs(sample);
  const auto tri = upper ? base::kUpper : base::kLower;
  faults::SetFault(fault);
  const auto plan = base::Take(base::WithoutAllocation(
      test, [&] { return aa::Query(provider, tri, he, a, tb, p, q, b); }));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
  aa::factor::Scratch<T> scratch(plan, sample.work_entries);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return aa::Driver(provider, tri, he, a, tb, p, q, b, plan,
                      scratch.workspace, report);
  });
  Outcome<T>(test, fault, n, status, report);
  if (Accepted<T>(fault)) {
    aa::factor::VerifyActive(test, sample.original, row, sample.ltb,
                             sample.work_entries, sample.a, sample.tb, sample.p,
                             sample.q, sample.singular, false, report);
    if (nrhs != 0 && !sample.singular) {
      sample.Solution(test);
    }
  } else {
    for (auto value : sample.p) {
      ASC_DENSE_TEST_EQ(test, value, -71);
    }
    for (auto value : sample.q) {
      ASC_DENSE_TEST_EQ(test, value, -71);
    }
    if (he || row) {
      ASC_DENSE_TEST_CHECK(test,
                           base::EqualBytes(sample.a.data(), before_a.data(),
                                            sample.a.size() * sizeof(T)));
    }
  }
  if (sample.singular || nrhs == 0 || (!Accepted<T>(fault) && rhs_row)) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), sample.before_b.data(),
                               sample.b.size() * sizeof(T)));
  }
  aa::factor::OutputGuards(test, sample.original, row, sample.a, before_a,
                           sample.tb, sample.p, sample.q);
  sample.RhsGuards(test);
  scratch.Guards(test);
}
template <typename T>
int Run(bool he) {
  base::TestContext test;
  int cases = 0;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (int f = 0; f <= static_cast<int>(Fault::kRealOnlyWork); ++f) {
    for (bool upper : {false, true}) {
      for (bool row : {false, true}) {
        for (bool rhs_row : {false, true}) {
          for (int n : {1, 3}) {
            for (int nrhs : {0, 2}) {
              for (int band : {0, 2}) {
                for (int work : {0, 2}) {
                  Case<T>(test, provider, he, upper, row, rhs_row, n, nrhs,
                          band, work, static_cast<Fault>(f));
                  ++cases;
                }
              }
            }
          }
        }
      }
    }
  }
  std::printf("Two-stage driver fault cases=%d\n", cases);
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
