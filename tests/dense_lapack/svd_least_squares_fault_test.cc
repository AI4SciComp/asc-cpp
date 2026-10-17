#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "least_squares_test_support.h"
#include "svd_least_squares_faults.h"
#include "svd_least_squares_test_support.h"

namespace {
using asc_svd_least_squares_test::Execute;
using asc_svd_least_squares_test::Fault;
using asc_svd_least_squares_test::ForeignCalls;
using asc_svd_least_squares_test::Guarded;
using asc_svd_least_squares_test::Integer;
using asc_svd_least_squares_test::Layout;
using asc_svd_least_squares_test::Matrix;
using asc_svd_least_squares_test::Query;
using asc_svd_least_squares_test::QueryCalls;
using asc_svd_least_squares_test::Routine;
using asc_svd_least_squares_test::SameBits;
using asc_svd_least_squares_test::Scratch;
using asc_svd_least_squares_test::SetFault;
using asc_svd_least_squares_test::Take;
using asc_svd_least_squares_test::TestContext;
using asc_svd_least_squares_test::WithoutAllocation;

template <typename T>
void QueryFailure(TestContext& test,
                  const asc::ReferenceLapackProvider& provider, Routine routine,
                  Fault fault) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(3, 2, Layout::kRowMajor);
  Matrix<T> b(3, 2, Layout::kColumnMajor);
  Guarded<Real> s(2);
  const auto a_before = a.bytes();
  const auto b_before = b.bytes();
  const auto s_before = s.bytes();
  asc::LapackReport report;
  SetFault(routine, fault);
  const auto before_calls = ForeignCalls();
  const auto before_queries = QueryCalls();
  const auto result = WithoutAllocation(test, [&] {
    return Query(provider, routine, a.view(), b.view(), s.vector(), Real{-1},
                 report);
  });
  SetFault(routine, Fault::kNone);
  ASC_DENSE_TEST_CHECK(test, !result.ok());
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), before_calls + 1);
  ASC_DENSE_TEST_EQ(test, QueryCalls(), before_queries + 1);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  a.CheckSame(test, a_before);
  b.CheckSame(test, b_before);
  s.CheckSame(test, s_before);
}

bool FailureReport(TestContext& test, const asc::Status& status,
                   const asc::LapackReport& report, Fault fault) {
  asc::index_t expected_info = 0;
  if (fault == Fault::kPositive) {
    expected_info = 1;
  } else if (fault == Fault::kImpossiblePositive) {
    expected_info = std::numeric_limits<Integer>::max();
  } else if (fault == Fault::kNegative) {
    expected_info = -8;
  } else if (fault == Fault::kMinimumInteger) {
    expected_info = std::numeric_limits<Integer>::min();
  }
  ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
  if (report.native_info.has_value()) {
    ASC_DENSE_TEST_EQ(test, *report.native_info, expected_info);
  }
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  if (fault == Fault::kNegative) {
    ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(0), 8);
  } else {
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  }
  const bool partial = fault == Fault::kPositive;
  ASC_DENSE_TEST_EQ(
      test, status.code(),
      partial ? asc::ErrorCode::kNumerical : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    partial ? asc::LapackOutputValidity::kDocumentedPartial
                            : asc::LapackOutputValidity::kUnusable);
  if (partial) {
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kNonconvergence);
  }
  return partial;
}

template <typename T>
void ExecutionFailure(TestContext& test,
                      const asc::ReferenceLapackProvider& provider,
                      Routine routine, Fault fault, asc::extent_t m,
                      asc::extent_t n, Layout a_layout, Layout b_layout) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(m, n, a_layout);
  Matrix<T> b(std::max(m, n), 3, b_layout);
  Guarded<Real> s(static_cast<std::size_t>(std::min(m, n)));
  const auto a_before = a.bytes();
  const auto b_before = b.bytes();
  const auto s_before = s.bytes();
  asc::LapackReport report;
  const auto plan = Take(Query(provider, routine, a.view(), b.view(),
                               s.vector(), Real{-1}, report));
  Scratch<T> scratch(plan, false);
  const auto workspace = scratch.view();
  asc::index_t rank = -37;
  const auto before_calls = ForeignCalls();
  const auto before_queries = QueryCalls();
  SetFault(routine, fault);
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, routine, a.view(), b.view(), s.vector(), Real{-1},
                   rank, plan, workspace, report);
  });
  SetFault(routine, Fault::kNone);
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), before_calls + 1);
  ASC_DENSE_TEST_EQ(test, QueryCalls(), before_queries);
  ASC_DENSE_TEST_EQ(test, rank, -37);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  const bool partial = FailureReport(test, status, report, fault);
  for (asc::extent_t i = 0; i < m; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      const T expected = partial || a_layout == Layout::kColumnMajor
                             ? T{-881}
                             : a_before[a.Offset(i, j)];
      ASC_DENSE_TEST_CHECK(test, SameBits(a(i, j), expected));
    }
  }
  for (asc::extent_t i = 0; i < std::max(m, n); ++i) {
    for (asc::extent_t j = 0; j < 3; ++j) {
      const T expected = partial && i < m ? T{-882} : b_before[b.Offset(i, j)];
      ASC_DENSE_TEST_CHECK(test, SameBits(b(i, j), expected));
    }
  }
  if (partial) {
    for (asc::extent_t i = 0; i < std::min(m, n); ++i) {
      ASC_DENSE_TEST_EQ(test, s.data()[i], Real{-883});
    }
  } else {
    s.CheckSame(test, s_before);
  }
  a.CheckPadding(test, a_before);
  b.CheckPadding(test, b_before);
  s.CheckGuards(test);
  scratch.CheckGuards(test);
}

template <typename T>
int Run(const asc::ReferenceLapackProvider& provider, std::string_view scalar) {
  TestContext test;
  for (const Routine routine : {Routine::kGelss, Routine::kGelsd}) {
    for (const Fault fault :
         {Fault::kNegative, Fault::kMinimumInteger, Fault::kPositive,
          Fault::kImpossiblePositive, Fault::kQueryZero, Fault::kQueryNan,
          Fault::kQueryInfinity}) {
      QueryFailure<T>(test, provider, routine, fault);
    }
    if constexpr (asc::DenseBlasComplex<T>) {
      QueryFailure<T>(test, provider, routine, Fault::kQueryImaginary);
      if (routine == Routine::kGelsd) {
        QueryFailure<T>(test, provider, routine, Fault::kRealQueryZero);
        QueryFailure<T>(test, provider, routine, Fault::kRealQueryNan);
      }
    }
    if (routine == Routine::kGelsd) {
      QueryFailure<T>(test, provider, routine, Fault::kIntegerQueryZero);
    }
    for (const auto shape : {std::array<asc::extent_t, 2>{5, 3}, {3, 5}}) {
      for (const auto a_layout : {Layout::kRowMajor, Layout::kColumnMajor}) {
        for (const auto b_layout : {Layout::kRowMajor, Layout::kColumnMajor}) {
          for (const Fault fault :
               {Fault::kNegative, Fault::kMinimumInteger, Fault::kPositive,
                Fault::kImpossiblePositive, Fault::kRankNegative,
                Fault::kRankTooLarge}) {
            ExecutionFailure<T>(test, provider, routine, fault, shape[0],
                                shape[1], a_layout, b_layout);
          }
        }
      }
    }
  }
  const int result = test.Finish();
  if (result == 0) {
    std::cout << "SVD_LS_FAULTS scalar=" << scalar
              << " actual_query_boundaries=" << QueryCalls()
              << " wrapped_foreign_boundaries=" << ForeignCalls()
              << " evidence=query_rejection,raw_info,staged_publication "
                 "injected_not_source_nonconvergence=1\n";
  }
  return result;
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(provider, scalar);
  }
  if (scalar == "d") {
    return Run<double>(provider, scalar);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(provider, scalar);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(provider, scalar);
  }
  return 2;
}
