#include <complex>
#include <cstdint>
#include <limits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "least_squares_faults.h"
#include "least_squares_test_support.h"

namespace {
using asc_least_squares_test::Execute;
using asc_least_squares_test::Fault;
using asc_least_squares_test::ForeignCalls;
using asc_least_squares_test::kPacking;
using asc_least_squares_test::Layout;
using asc_least_squares_test::Matrix;
using asc_least_squares_test::Query;
using asc_least_squares_test::Routine;
using asc_least_squares_test::Scratch;
using asc_least_squares_test::SetFault;
using asc_least_squares_test::Take;
using asc_least_squares_test::TestContext;
using asc_least_squares_test::WithoutAllocation;

template <typename T>
void HugeStrides(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 Routine routine) {
  constexpr auto kHuge = std::numeric_limits<asc::extent_t>::max();
  const auto foreign_limit =
      provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64
          ? std::numeric_limits<std::int32_t>::max()
          : kHuge;
  const auto adjoint = asc::DenseBlasComplex<T>
                           ? asc::DenseBlasTranspose::kConjugateTranspose
                           : asc::DenseBlasTranspose::kTranspose;
  for (const auto layout : {Layout::kRowMajor, Layout::kColumnMajor}) {
    for (const auto trans : {asc::DenseBlasTranspose::kNone, adjoint}) {
      T a{2};
      T b{6};
      const auto av = Take(asc::DenseBlasMatrixView<T>::Create(
          &a, 1, 1, layout, layout == Layout::kRowMajor ? kHuge : foreign_limit,
          {&a, sizeof(T), asc::MemorySpace::kHost}));
      const auto bv = Take(asc::DenseBlasMatrixView<T>::Create(
          &b, 1, 1, Layout::kColumnMajor, kHuge,
          {&b, sizeof(T), asc::MemorySpace::kHost}));
      asc::LapackReport report;
      const auto result = WithoutAllocation(test, [&] {
        return Query(provider, routine, trans, av, bv, report);
      });
      ASC_DENSE_TEST_CHECK(test, result.ok());
      if (!result.ok()) {
        continue;
      }
      Scratch<T> scratch(*result, true);
      const auto workspace = scratch.view();
      const auto status = WithoutAllocation(test, [&] {
        return Execute(provider, routine, trans, av, bv, *result, workspace,
                       report);
      });
      ASC_DENSE_TEST_CHECK(test, status.ok());
      ASC_DENSE_TEST_EQ(test, b, T{3});
      ASC_DENSE_TEST_CHECK(test, report.called_provider);
    }
  }
  const auto a = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, 0, foreign_limit, Layout::kColumnMajor, 1,
      {nullptr, 0, asc::MemorySpace::kHost}));
  const auto b = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, foreign_limit, 0, Layout::kRowMajor, 1,
      {nullptr, 0, asc::MemorySpace::kHost}));
  asc::LapackReport report;
  const auto plan = Take(
      Query(provider, routine, asc::DenseBlasTranspose::kNone, a, b, report));
  Scratch<T> scratch(plan, false);
  const auto workspace = scratch.view();
  const auto calls = ForeignCalls();
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, routine, asc::DenseBlasTranspose::kNone, a, b,
                   plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
}

template <typename T>
void Publication(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 Routine routine) {
  for (const auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
    Matrix<T> a(2, 3, layout);
    Matrix<T> b(3, 2, layout);
    const auto old_a = a.bytes();
    const auto old_b = b.bytes();
    asc::LapackReport report;
    const auto plan =
        Take(Query(provider, routine, asc::DenseBlasTranspose::kNone, a.view(),
                   b.view(), report));
    Scratch<T> scratch(plan, true);
    const auto workspace = scratch.view();
    SetFault(routine, Fault::kNegative);
    const auto failed = WithoutAllocation(test, [&] {
      return Execute(provider, routine, asc::DenseBlasTranspose::kNone,
                     a.view(), b.view(), plan, workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, failed.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.native_info, -8);
    ASC_DENSE_TEST_EQ(test, report.native_argument, 8);
    b.CheckSame(test, old_b);
    if (layout == Layout::kRowMajor) {
      a.CheckSame(test, old_a);
    } else {
      ASC_DENSE_TEST_EQ(test, a(0, 0), T{-881});
    }
    SetFault(routine, Fault::kPositive);
    const auto partial = WithoutAllocation(test, [&] {
      return Execute(provider, routine, asc::DenseBlasTranspose::kNone,
                     a.view(), b.view(), plan, workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, partial.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info, 1);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 0);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_EQ(test, a(0, 0), T{-881});
    ASC_DENSE_TEST_EQ(test, b(0, 0), T{-882});
    for (asc::extent_t j = 0; j < 2; ++j) {
      ASC_DENSE_TEST_EQ(test, b(2, j), old_b[b.Offset(2, j)]);
    }
    a.CheckPadding(test, old_a);
    b.CheckPadding(test, old_b);
    SetFault(routine, Fault::kNone);
  }
}

template <typename T>
void MetadataAlias(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   Routine routine) {
  Matrix<T> a(2, 2, Layout::kRowMajor);
  Matrix<T> b(2, 2, Layout::kColumnMajor);
  asc::LapackReport report;
  const auto plan =
      Take(Query(provider, routine, asc::DenseBlasTranspose::kNone, a.view(),
                 b.view(), report));
  Scratch<T> scratch(plan, true);
  auto workspace = scratch.view();
  const auto old_report = report;
  const auto calls = ForeignCalls();
  const auto old_a = a.bytes();
  const auto old_b = b.bytes();
  // The invalid region aliases a real live report; no pretend huge storage or
  // type-punned numerical access is used. Rejection precedes report reset.
  workspace.regions[kPacking] = {&report, sizeof(report),
                                 asc::MemorySpace::kHost};
  const auto failed = WithoutAllocation(test, [&] {
    return Execute(provider, routine, asc::DenseBlasTranspose::kNone, a.view(),
                   b.view(), plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, failed.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, report.native_info, old_report.native_info);
  ASC_DENSE_TEST_EQ(test, report.called_provider, old_report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.routine, old_report.routine);
  ASC_DENSE_TEST_EQ(test, report.outcome, old_report.outcome);
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
  a.CheckSame(test, old_a);
  b.CheckSame(test, old_b);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto routine :
       {Routine::kGels, Routine::kGelst, Routine::kGetsls}) {
    HugeStrides<T>(test, provider, routine);
    Publication<T>(test, provider, routine);
    MetadataAlias<T>(test, provider, routine);
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Run<float>(test, provider);
  Run<double>(test, provider);
  Run<std::complex<float>>(test, provider);
  Run<std::complex<double>>(test, provider);
  return test.Finish();
}
