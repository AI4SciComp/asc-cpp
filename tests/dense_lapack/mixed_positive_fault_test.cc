#include <cmath>
#include <complex>
#include <cstddef>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_mixed_general.h"
#include "installed_lu/normal_return_guard.h"
#include "mixed_positive_faults.h"
#include "mixed_positive_test_support.h"

namespace {
namespace support = asc_mixed_positive_test;
namespace fault = asc_mixed_positive_fault;
using support::Take;
using support::TestContext;
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         asc::DenseBlasTriangle uplo) {
  for (const auto mode :
       {fault::Mode::kNegativeInfo, fault::Mode::kExcessInfo,
        fault::Mode::kUnwrittenInfo, fault::Mode::kPartialInfo,
        fault::Mode::kInvalidIter, fault::Mode::kUnwrittenIter,
        fault::Mode::kPartialIter, fault::Mode::kWriteThenNegative,
        fault::Mode::kUnwrittenX, fault::Mode::kImplementationFallback}) {
    for (const auto al : {support::kRow, support::kColumn}) {
      for (const auto xl : {support::kRow, support::kColumn}) {
        support::Problem<T> p(3, 2, al, support::kColumn, xl, uplo);
        auto before = p;
        if constexpr (asc::DenseBlasComplex<T>) {
          // The successful synthetic working fallback publishes the
          // normalized factor diagonal; this is its documented output.
          if (mode == fault::Mode::kImplementationFallback) {
            for (asc::extent_t i = 0; i < 3; ++i) {
              before.a.At(i, i).imag(0);
            }
          }
        }
        const auto plan = Take(p.Query(provider));
        support::Scratch<T> scratch;
        const auto workspace = scratch.Workspace(plan);
        asc::LapackMixedSolveStatistics statistics;
        asc::LapackReport report;
        fault::Reset(mode);
        const auto status =
            p.Run(provider, plan, workspace, statistics, report);
        const bool unwritten_x = mode == fault::Mode::kUnwrittenX;
        const bool implementation =
            mode == fault::Mode::kImplementationFallback;
        ASC_DENSE_TEST_EQ(test, status.code(),
                          implementation ? asc::ErrorCode::kOk
                          : unwritten_x  ? asc::ErrorCode::kNumerical
                                         : asc::ErrorCode::kProvider);
        ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{1});
        ASC_DENSE_TEST_CHECK(test,
                             report.called_provider && report.native_info);
        if (unwritten_x) {
          ASC_DENSE_TEST_CHECK(
              test, std::isnan(support::ToWide(p.x.At(0, 0)).real()));
          ASC_DENSE_TEST_EQ(test, report.outcome,
                            asc::LapackOutcome::kAccuracyWarning);
        } else if (implementation) {
          ASC_DENSE_TEST_EQ(test, statistics.fallback,
                            asc::LapackMixedFallback::kImplementation);
          ASC_DENSE_TEST_EQ(test, p.x.At(0, 0), T{1});
        } else {
          ASC_DENSE_TEST_EQ(test, p.x.data, before.x.data);
          ASC_DENSE_TEST_EQ(test, report.output_validity,
                            asc::LapackOutputValidity::kUnusable);
        }
        if (!asc::DenseBlasComplex<T> && al == support::kColumn &&
            mode == fault::Mode::kWriteThenNegative) {
          ASC_DENSE_TEST_EQ(test, p.a.At(0, 0), T{-31});
        } else {
          ASC_DENSE_TEST_EQ(test, p.a.data, before.a.data);
        }
        ASC_DENSE_TEST_EQ(test, p.b.data, before.b.data);
        p.a.Guards(test);
        p.x.Guards(test);
        scratch.Guards(test, workspace);
        auto stale = plan;
        ++stale.regions[support::kLayout].minimum_entries;
        fault::Reset(mode);
        ASC_DENSE_TEST_CHECK(
            test, !p.Run(provider, stale, workspace, statistics, report).ok());
        ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
      }
    }
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "d") {
    Run<double>(test, provider, asc::DenseBlasTriangle::kUpper);
    Run<double>(test, provider, asc::DenseBlasTriangle::kLower);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider, asc::DenseBlasTriangle::kUpper);
    Run<std::complex<double>>(test, provider, asc::DenseBlasTriangle::kLower);
  } else {
    return 2;
  }
  return test.Finish();
}
