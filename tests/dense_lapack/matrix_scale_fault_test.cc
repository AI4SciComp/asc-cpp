#include <complex>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "matrix_scale_entry.h"
#include "matrix_scale_test_support.h"
#include "tridiagonal_test_support.h"

namespace {
namespace support = asc_tridiagonal_test;
namespace fault = asc_scale_entry;
using asc_scale_test::Problem;
using asc_scale_test::SameBytes;
using support::Take;
using support::TestContext;

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          char type, asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  const bool band = type == 'B' || type == 'Q' || type == 'Z';
  Problem<T> p(type, 3, type == 'B' || type == 'Q' ? 3 : 2, 1, 1, layout);
  const auto original = p.data;
  const auto plan = Take(p.Query(provider));
  const auto workspace = p.Workspace(plan);
  const auto scratch = p.packed;
  for (const auto mode :
       {fault::Mode::kPass, fault::Mode::kMissingInfo,
        fault::Mode::kPositiveInfo, fault::Mode::kNegativeInfo,
        fault::Mode::kPartialInfo}) {
    p.data = original;
    p.packed = scratch;
    fault::Reset(mode);
    ASC_DENSE_TEST_CHECK(test, p.Query(provider).ok());
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
    asc::LapackReport report;
    auto stale = plan;
    ++stale.regions[support::kLayout].minimum_entries;
    ASC_DENSE_TEST_CHECK(
        test,
        !p.Run(provider, Real{1}, Real{2}, stale, workspace, report).ok());
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
    ASC_DENSE_TEST_CHECK(test, SameBytes(p.data, original));
    ASC_DENSE_TEST_CHECK(test, SameBytes(p.packed, scratch));
    const auto status =
        p.Run(provider, Real{1}, Real{2}, plan, workspace, report);
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
    const auto entry = fault::Last();
    ASC_DENSE_TEST_EQ(test, entry.type, type);
    ASC_DENSE_TEST_EQ(test, entry.rows, p.rows);
    ASC_DENSE_TEST_EQ(test, entry.columns, p.columns);
    ASC_DENSE_TEST_EQ(test, entry.lower, band ? 1 : 0);
    ASC_DENSE_TEST_EQ(test, entry.upper, band ? 1 : 0);
    auto leading = p.leading;
    if (layout == support::kRow) {
      leading = band ? 2 : p.rows;
    }
    ASC_DENSE_TEST_EQ(test, entry.leading, leading);
    ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info);
    if (mode == fault::Mode::kPass) {
      ASC_DENSE_TEST_CHECK(test, status.ok());
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
      ASC_DENSE_TEST_EQ(test, p.data[p.Offset(0, 0)],
                        original[p.Offset(0, 0)] * Real{2});
    } else {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_CHECK(test, report.native_info.value_or(0) != 0);
      if (layout == support::kRow) {
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kUnchanged);
        ASC_DENSE_TEST_CHECK(test, SameBytes(p.data, original));
      } else {
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kUnusable);
        ASC_DENSE_TEST_EQ(test, p.data[p.Offset(0, 0)], T{17});
      }
      if (mode == fault::Mode::kNegativeInfo) {
        ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(0), 4);
      } else {
        ASC_DENSE_TEST_CHECK(test, !report.native_argument);
      }
    }
  }
}

template <typename T>
int Run() {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (const auto layout : {support::kColumn, support::kRow}) {
    for (const auto type : {'G', 'L', 'U', 'H', 'B', 'Q', 'Z'}) {
      if (type != 'Z' || layout == support::kColumn) {
        Case<T>(test, provider, type, layout);
      }
    }
  }
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>();
  }
  if (scalar == "d") {
    return Run<double>();
  }
  if (scalar == "c") {
    return Run<std::complex<float>>();
  }
  if (scalar == "z") {
    return Run<std::complex<double>>();
  }
  return 2;
}
