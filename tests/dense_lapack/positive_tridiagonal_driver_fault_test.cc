#include <cmath>
#include <complex>
#include <cstddef>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_driver.h"
#include "installed_lu/normal_return_guard.h"
#include "internal_tridiagonal.h"
#include "positive_tridiagonal_driver_entry.h"
#include "positive_tridiagonal_refinement_test_support.h"
#include "tridiagonal_test_support.h"
namespace {
namespace probe = asc::internal_ptsv_test;
using asc_ptrfs_test::Fixture;
using asc_ptrfs_test::kLower;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::Scratch;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::Vector;

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Fault = probe::Fault;
  for (const auto layout :
       {asc_tridiagonal_test::kColumn, asc_tridiagonal_test::kRow}) {
    for (const asc::extent_t nrhs : {0, 2}) {
      for (const auto fault :
           {Fault::kNone, Fault::kWrite, Fault::kNoInfo, Fault::kPartialInfo,
            Fault::kNegativeInfo, Fault::kPositiveInfo, Fault::kBeyondInfo,
            Fault::kNanSolution, Fault::kInfiniteSolution, Fault::kNanDiagonal,
            Fault::kInfiniteDiagonal}) {
        Fixture<T> fixture(3, 0, kLower);
        Rhs<T> rhs(3, nrhs, layout);
        const auto matrix =
            Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
                Vector(fixture.d, 3), Vector(fixture.e, 2)));
        const auto before = rhs.data;
        const auto plan =
            Take(asc::QueryPtsvWorkspace(provider, matrix, rhs.View()));
        Scratch<T> storage;
        const auto workspace = storage.Workspace(plan);
        probe::SetFault(fault);
        asc::LapackReport report;
        const auto status =
            asc::Ptsv(provider, matrix, rhs.View(), plan, workspace, report);
        ASC_DENSE_TEST_EQ(test, probe::EntryCount(), std::size_t{1});
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        const bool bad_info =
            fault == Fault::kNoInfo || fault == Fault::kNegativeInfo ||
            fault == Fault::kBeyondInfo ||
            (fault == Fault::kPartialInfo && sizeof(lapack_int) == 8);
        const bool bad_pivot = fault == Fault::kPositiveInfo;
        const bool nonfinite =
            fault == Fault::kNanDiagonal || fault == Fault::kInfiniteDiagonal ||
            (nrhs > 0 && (fault == Fault::kNanSolution ||
                          fault == Fault::kInfiniteSolution));
        if (bad_info || bad_pivot) {
          ASC_DENSE_TEST_EQ(test, status.code(),
                            bad_info ? asc::ErrorCode::kProvider
                                     : asc::ErrorCode::kNumerical);
          ASC_DENSE_TEST_EQ(test, rhs.data, before);
          ASC_DENSE_TEST_EQ(
              test, report.output_validity,
              bad_info ? asc::LapackOutputValidity::kUnusable
                       : asc::LapackOutputValidity::kDocumentedPartial);
        } else {
          ASC_DENSE_TEST_EQ(
              test, status.code(),
              nonfinite ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk);
          ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), 0);
          ASC_DENSE_TEST_EQ(test, report.output_validity,
                            nonfinite
                                ? asc::LapackOutputValidity::kDocumentedPartial
                                : asc::LapackOutputValidity::kComplete);
          if (fault == Fault::kWrite ||
              (fault == Fault::kPartialInfo && sizeof(lapack_int) == 4)) {
            for (asc::extent_t j = 0; j < nrhs; ++j) {
              for (asc::extent_t i = 0; i < 3; ++i) {
                ASC_DENSE_TEST_EQ(test, rhs.At(i, j), T{2});
              }
            }
          }
        }
        rhs.Guards(test);
        storage.Guards(test, workspace);
      }
    }
  }
  probe::SetFault(probe::Fault::kNone);
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Run<float>(test, provider);
  Run<double>(test, provider);
  Run<std::complex<float>>(test, provider);
  Run<std::complex<double>>(test, provider);
  return test.Finish();
}
