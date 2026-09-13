#include <array>
#include <cmath>
#include <complex>
#include <limits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_condition.h"
#include "installed_lu/normal_return_guard.h"
#include "internal_tridiagonal.h"
#include "positive_tridiagonal_condition_entry.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_ptcon_entry::Fault;
using asc_tridiagonal_test::Scratch;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::Vector;
template <typename T>
void Check(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array<Real, 3> d{-83, 2, -83};
  const std::array<T, 3> e{};
  for (const auto triangle :
       {asc::DenseBlasTriangle::kLower, asc::DenseBlasTriangle::kUpper}) {
    const auto factor =
        Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::FromRaw(
            provider, triangle, Vector(d, 1), Vector(e, 0)));
    Real output = -19;
    const auto plan =
        Take(asc::QueryPtconWorkspace(provider, factor, Real{2}, output));
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    for (const auto fault :
         {Fault::kNoInfo, Fault::kPartialInfo, Fault::kNegativeInfo,
          Fault::kPositiveInfo, Fault::kNoOutput, Fault::kNegativeOutput,
          Fault::kNanOutput, Fault::kInfiniteOutput, Fault::kNone}) {
      asc_ptcon_entry::Reset();
      asc_ptcon_entry::SetFault(fault);
      output = -19;
      asc::LapackReport report;
      const auto status = asc::Ptcon(provider, factor, Real{2}, output, plan,
                                     workspace, report);
      ASC_DENSE_TEST_EQ(test, asc_ptcon_entry::Calls(), 1U);
      ASC_DENSE_TEST_CHECK(test, report.called_provider);
      ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
      if (fault == Fault::kNone) {
        ASC_DENSE_TEST_CHECK(test, status.ok());
        ASC_DENSE_TEST_EQ(test, output, Real{1});
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), 0);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kComplete);
      } else if (fault == Fault::kNanOutput ||
                 fault == Fault::kInfiniteOutput) {
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
        ASC_DENSE_TEST_CHECK(test, fault == Fault::kNanOutput
                                       ? std::isnan(output)
                                       : std::isinf(output));
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), 0);
        ASC_DENSE_TEST_EQ(test, report.outcome,
                          asc::LapackOutcome::kAccuracyWarning);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kDocumentedPartial);
      } else {
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
        ASC_DENSE_TEST_EQ(test, output, Real{-19});
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kUnchanged);
        if (fault == Fault::kNoInfo || fault == Fault::kPartialInfo) {
          ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99),
                            std::numeric_limits<lapack_int>::min());
          ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
        } else if (fault == Fault::kNegativeInfo) {
          ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), -4);
          ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(-99), 4);
        } else if (fault == Fault::kPositiveInfo) {
          ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), 3);
        } else {
          ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), 0);
        }
      }
      scratch.Guards(test, workspace);
    }
  }
  asc_ptcon_entry::Reset();
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Check<float>(test, provider);
  Check<double>(test, provider);
  Check<std::complex<float>>(test, provider);
  Check<std::complex<double>>(test, provider);
  return test.Finish();
}
