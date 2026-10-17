#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_refinement_faults.h"
#include "indefinite_packed_refinement_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "src/dense/lapack/internal_indefinite.h"
#include "tests/dense/test_support.h"
namespace {
namespace base = asc_indefinite_test;
namespace refinement = asc_packed_refinement_test;
namespace faults = asc_packed_refinement_fault_test;
using faults::Fault;
constexpr std::array kFaults{Fault::kPass,
                             Fault::kOmitInfo,
                             Fault::kWrite16BitInfo,
                             Fault::kWrite32BitInfo,
                             Fault::kNegativeInfo,
                             Fault::kPositiveInfo,
                             Fault::kMaximumInfo,
                             Fault::kChangedPivot,
                             Fault::kOmitErrors,
                             Fault::kOmitFirstForward,
                             Fault::kOmitLastBackward,
                             Fault::kNegativeForward,
                             Fault::kNegativeBackward,
                             Fault::kNanForward,
                             Fault::kInfBackward,
                             Fault::kNegativeInfForward,
                             Fault::kZeroErrors,
                             Fault::kTwoErrors,
                             Fault::kSubnormalErrors,
                             Fault::kNegativeZeroErrors,
                             Fault::kNanX,
                             Fault::kInfX,
                             Fault::kInvalidInfoNegativeError,
                             Fault::kChangedPivotNanError};
bool InfoDefect(Fault fault) {
  switch (fault) {
    case Fault::kOmitInfo:
    case Fault::kWrite16BitInfo:
    case Fault::kNegativeInfo:
    case Fault::kPositiveInfo:
    case Fault::kMaximumInfo:
    case Fault::kChangedPivot:
    case Fault::kInvalidInfoNegativeError:
    case Fault::kChangedPivotNanError:
      return true;
    case Fault::kWrite32BitInfo:
      return sizeof(lapack_int) != 4;
    default:
      return false;
  }
}
bool Negative(Fault fault) {
  return fault == Fault::kNegativeForward ||
         fault == Fault::kNegativeBackward ||
         fault == Fault::kNegativeInfForward;
}
bool Nonfinite(Fault fault) {
  return fault == Fault::kOmitErrors || fault == Fault::kOmitFirstForward ||
         fault == Fault::kOmitLastBackward || fault == Fault::kNanForward ||
         fault == Fault::kInfBackward;
}
void Report(base::TestContext& test, Fault fault, const asc::Status& status,
            const asc::LapackReport& report) {
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_CHECK(test, faults::SeedWasFullWidth() &&
                                 faults::ErrorSeedsWereNan() &&
                                 faults::NativeGuardsPass());
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, faults::LastPublishedInfo());
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value() &&
                                 !report.diagnostic_index.has_value());
  if (InfoDefect(fault)) {
    const auto info = faults::LastPublishedInfo();
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      info < 0 ? asc::LapackOutcome::kProviderArgument
                               : asc::LapackOutcome::kPartialResult);
    const bool argument =
        info < 0 && info != std::numeric_limits<lapack_int>::min();
    ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(), argument);
    if (argument) {
      ASC_DENSE_TEST_EQ(test, report.native_argument, -info);
    }
  } else if (Negative(fault)) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kPartialResult);
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  } else if (Nonfinite(fault)) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  }
}
template <typename Real>
bool Matches(Real actual, long double wide) {
  if (std::isnan(wide)) {
    return std::isnan(actual);
  }
  const auto converted = static_cast<Real>(wide);
  return base::EqualBytes(&actual, &converted, sizeof(actual));
}
template <typename T>
bool Matches(T actual, std::complex<long double> wide) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return Matches(actual.real(), wide.real()) &&
           Matches(actual.imag(), wide.imag());
  } else {
    return Matches(actual, wide.real());
  }
}
template <typename T>
void NativeBaseline(base::TestContext& test,
                    const refinement::Fixture<T>& sample) {
  using Real = asc::DenseBlasRealType<T>;
  auto native = sample;
  for (int j = 0; j < native.system.nrhs; ++j) {
    native.ferr[j + 1] = static_cast<Real>(faults::LastNativeForward(j));
    native.berr[j + 1] = static_cast<Real>(faults::LastNativeBackward(j));
    for (int i = 0; i < native.system.a.n; ++i) {
      const auto value = faults::LastNativeX(i, j);
      native.x[native.Offset(i, j)] =
          base::Value<T>(value.real(), value.imag());
    }
  }
  native.Mathematics(test);
}
template <typename T>
void Published(base::TestContext& test, Fault fault,
               const refinement::Fixture<T>& sample) {
  using Real = asc::DenseBlasRealType<T>;
  for (int j = 0; j < sample.system.nrhs; ++j) {
    if (InfoDefect(fault)) {
      ASC_DENSE_TEST_EQ(test, sample.ferr[j + 1], Real{-293});
      ASC_DENSE_TEST_EQ(test, sample.berr[j + 1], Real{-307});
    } else {
      ASC_DENSE_TEST_CHECK(
          test, Matches(sample.ferr[j + 1], faults::LastPublishedForward(j)));
      ASC_DENSE_TEST_CHECK(
          test, Matches(sample.berr[j + 1], faults::LastPublishedBackward(j)));
    }
    for (int i = 0; i < sample.system.a.n; ++i) {
      if (sample.solution_layout == base::kRow &&
          (InfoDefect(fault) || Negative(fault))) {
        ASC_DENSE_TEST_EQ(test, sample.x[sample.Offset(i, j)],
                          sample.initial_x[sample.Offset(i, j)]);
      } else {
        ASC_DENSE_TEST_CHECK(test, Matches(sample.x[sample.Offset(i, j)],
                                           faults::LastPublishedX(i, j)));
      }
    }
  }
}
template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          refinement::Fixture<T>& sample, Fault fault) {
  sample.Prepare(test, provider);
  faults::SetFault(fault);
  const auto before_original = sample.original;
  const auto before_x = sample.x;
  const auto before_ferr = sample.ferr;
  const auto before_berr = sample.berr;
  const auto plan = base::Take(base::WithoutAllocation(
      test, [&] { return refinement::Query(provider, sample); }));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
  ASC_DENSE_TEST_EQ(test, sample.x, before_x);
  ASC_DENSE_TEST_EQ(test, sample.ferr, before_ferr);
  ASC_DENSE_TEST_EQ(test, sample.berr, before_berr);
  refinement::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return refinement::Refine(provider, sample, plan, workspace, report);
  });
  const bool active = sample.system.a.n != 0 && sample.system.nrhs != 0;
  if (active) {
    Report(test, fault, status, report);
    NativeBaseline(test, sample);
    Published(test, fault, sample);
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, sample.x, before_x);
    sample.Mathematics(test);
  }
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(sample.original.data(), before_original.data(),
                             sizeof(before_original)));
  sample.Padding(test);
  scratch.Guards(test, workspace);
}
template <typename T>
int Run(bool hermitian) {
  base::TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (int n : {0, 1, 2, 5}) {
    for (int nrhs : {0, 1, 3}) {
      for (auto triangle : {base::kUpper, base::kLower}) {
        for (unsigned layouts : {0U, 15U, 5U, 10U}) {
          const auto layout = [&](unsigned bit) {
            return (layouts & (1U << bit)) != 0 ? base::kRow : base::kColumn;
          };
          for (auto fault : kFaults) {
            refinement::Fixture<T> sample(
                n, nrhs, hermitian, triangle, layout(0), layout(1), layout(2),
                layout(3), 0, layouts % 2 == 0 ? 3 : 0);
            std::printf(
                "Packed refinement fault n=%d nrhs=%d tri=%d layouts=%u "
                "fault=%d\n",
                n, nrhs, static_cast<int>(triangle), layouts,
                static_cast<int>(fault));
            Case(test, provider, sample, fault);
            ++cases;
          }
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, 2304);
  std::printf("Packed refinement fault cases=%d\n", cases);
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
