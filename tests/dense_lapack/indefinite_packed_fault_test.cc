#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_faults.h"
#include "indefinite_packed_fixture.h"
#include "indefinite_packed_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
#include "tests/dense/test_support.h"
namespace {
namespace packed = asc_packed_indefinite_test;
namespace base = asc_indefinite_test;
namespace faults = asc_packed_indefinite_fault_test;
using faults::Fault;
bool Partial(Fault fault) {
  return fault == Fault::kNonzeroPartial || fault == Fault::kNanPartial;
}
bool Accepted(Fault fault) {
  return fault == Fault::kPass || Partial(fault) ||
         (ASC_LAPACK_INTEGER_BITS == 32 && (fault == Fault::kWrite32BitInfo ||
                                            fault == Fault::kWrite32BitPivots));
}
template <typename T>
void Case(packed::TestContext& test,
          const asc::ReferenceLapackProvider& provider, bool he,
          asc::DenseBlasTriangle tri, asc::DenseBlasLayout layout, int n,
          int kind, Fault fault) {
  packed::Sample<T> sample(n, he, tri, layout, 0);
  sample.Reset(kind);
  const auto before_p = sample.pivots;
  const auto a = sample.View();
  const auto p = base::Pivots(sample.pivots, n);
  faults::SetFault(fault);
  const auto plan = packed::Take(base::WithoutAllocation(
      test, [&] { return packed::Query(provider, tri, he, a, p); }));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return packed::Factor(provider, tri, he, a, p, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, faults::Calls(), n == 0 ? 0U : 1U);
  ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kBunchKaufman);
  if (n == 0) {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, sample.pivots, before_p);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
  } else {
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(99),
                      faults::LastInjectedInfo());
    ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(),
                      kind == 1 ? (tri == base::kUpper ? n : 1) : 0);
    if (Accepted(fault)) {
      const bool partial = Partial(fault) || kind == 1;
      ASC_DENSE_TEST_EQ(
          test, status.code(),
          partial ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        partial ? asc::LapackOutputValidity::kDocumentedPartial
                                : asc::LapackOutputValidity::kComplete);
      if (partial) {
        ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                          faults::LastInjectedInfo() - 1);
        ASC_DENSE_TEST_EQ(test, report.outcome,
                          Partial(fault) ? asc::LapackOutcome::kPartialResult
                                         : asc::LapackOutcome::kSingular);
      }
      if (!Partial(fault)) {
        sample.Reconstruction(test);
      }
    } else {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        faults::LastInjectedInfo() < 0
                            ? asc::LapackOutcome::kProviderArgument
                            : asc::LapackOutcome::kPartialResult);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      ASC_DENSE_TEST_EQ(test, sample.pivots, before_p);
      ASC_DENSE_TEST_CHECK(
          test, packed::EqualBytes(sample.a.data(), sample.original.data(),
                                   sizeof(sample.a)));
    }
  }
  sample.Guards(test);
  scratch.Guards(test, workspace);
}
template <typename T>
void ScalarInput(packed::TestContext& test,
                 const asc::ReferenceLapackProvider& provider, bool he,
                 asc::DenseBlasTriangle tri, asc::DenseBlasLayout layout,
                 int kind) {
  using Real = asc::DenseBlasRealType<T>;
  const Real nan = std::numeric_limits<Real>::quiet_NaN();
  packed::Sample<T> sample(1, he, tri, layout, 0);
  sample.a[1] = T{4};
  if (kind == 1 || kind == 3) {
    sample.a[1] = T{nan};
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    if (kind == 2 || kind == 3) {
      sample.a[1].imag(nan);
    }
  }
  sample.original = sample.a;
  const auto before_p = sample.pivots;
  const auto a = sample.View();
  const auto p = base::Pivots(sample.pivots, 1);
  faults::SetFault(Fault::kPass);
  const auto plan = packed::Take(base::WithoutAllocation(
      test, [&] { return packed::Query(provider, tri, he, a, p); }));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto before_scratch = scratch;
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return packed::Factor(provider, tri, he, a, p, plan, workspace, report);
  });
  const bool rejected =
      kind == 1 || kind == 3 || (asc::DenseBlasComplex<T> && !he && kind == 2);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), rejected ? 0U : 1U);
  ASC_DENSE_TEST_EQ(test, report.called_provider, !rejected);
  if (rejected) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kPartialResult);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), 0);
    ASC_DENSE_TEST_CHECK(
        test, packed::EqualBytes(sample.a.data(), sample.original.data(),
                                 sizeof(sample.a)));
    ASC_DENSE_TEST_EQ(test, sample.pivots, before_p);
    ASC_DENSE_TEST_EQ(test, scratch.scalar, before_scratch.scalar);
    ASC_DENSE_TEST_EQ(test, scratch.packed, before_scratch.packed);
    ASC_DENSE_TEST_EQ(test, scratch.pivot, before_scratch.pivot);
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_EQ(test, sample.a[1], T{4});
    ASC_DENSE_TEST_EQ(test, sample.pivots[1], 1);
  }
  sample.Guards(test);
  scratch.Guards(test, workspace);
}
template <typename T>
int Run(bool he) {
  packed::TestContext test;
  const auto provider = packed::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (int fault = 0; fault <= static_cast<int>(Fault::kPivotSuffix); ++fault) {
    for (auto tri : {base::kUpper, base::kLower}) {
      for (auto layout : {base::kColumn, base::kRow}) {
        for (int n : {0, 1, 2, 3, 8}) {
          for (int kind : {0, 1, 3}) {
            Case<T>(test, provider, he, tri, layout, n, kind,
                    static_cast<Fault>(fault));
            ++cases;
          }
        }
      }
    }
  }
  for (auto tri : {base::kUpper, base::kLower}) {
    for (auto layout : {base::kColumn, base::kRow}) {
      for (int kind = 0; kind < 4; ++kind) {
        ScalarInput<T>(test, provider, he, tri, layout, kind);
      }
    }
  }
  std::printf("Packed fault cases=%d scalar_nonentry_controls=16\n", cases);
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
