
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_refinement.h"
#include "installed_lu/normal_return_guard.h"
#include "positive_tridiagonal_refinement_entry.h"
#include "positive_tridiagonal_refinement_test_support.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_ptrfs_entry::Fault;
using asc_ptrfs_test::Fixture;
using asc_ptrfs_test::Initialize;
using asc_ptrfs_test::Storage;
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Vector;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
template <typename T>
using Real = asc::DenseBlasRealType<T>;

constexpr bool IsDefect(Fault fault) {
  return fault != Fault::kNone && fault != Fault::kNormalWrite &&
         fault != Fault::kNanFerr && fault != Fault::kInfiniteBerr &&
         fault != Fault::kNanSolution;
}
template <typename T>
void Published(TestContext& test, Fault fault, const asc::Status& status,
               const asc::LapackReport& report, const Rhs<T>& solution,
               const std::array<Real<T>, 5>& ferr,
               const std::array<Real<T>, 5>& berr) {
  const bool finite = fault == Fault::kNone || fault == Fault::kNormalWrite;
  ASC_DENSE_TEST_EQ(test, status.code(),
                    finite ? asc::ErrorCode::kOk : asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), 0);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    finite ? asc::LapackOutputValidity::kComplete
                           : asc::LapackOutputValidity::kDocumentedPartial);
  if (!finite) {
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
  }
  for (asc::extent_t j = 0; j < 3; ++j) {
    for (asc::extent_t i = 0; i < 2; ++i) {
      if (fault == Fault::kNanSolution && j == 2 && i == 1) {
        ASC_DENSE_TEST_CHECK(test,
                             std::isnan(ToWide(solution.At(i, j)).real()));
      } else if (fault != Fault::kNone) {
        ASC_DENSE_TEST_EQ(test, solution.At(i, j), T{7});
      } else {
        const auto expected = ToWide(asc_ptrfs_test::Exact<T>(i, j));
        ASC_DENSE_TEST_CHECK(test,
                             std::abs(ToWide(solution.At(i, j)) - expected) <=
                                 64 * std::numeric_limits<Real<T>>::epsilon() *
                                     std::abs(expected));
      }
    }
    const auto at = static_cast<std::size_t>(j + 1);
    if (fault == Fault::kNanFerr && j == 2) {
      ASC_DENSE_TEST_CHECK(test, std::isnan(ferr[at]));
    } else if (fault != Fault::kNone) {
      ASC_DENSE_TEST_EQ(test, ferr[at], Real<T>{0.25});
    }
    if (fault == Fault::kInfiniteBerr && j == 2) {
      ASC_DENSE_TEST_EQ(test, berr[at],
                        std::numeric_limits<Real<T>>::infinity());
    } else if (fault != Fault::kNone) {
      ASC_DENSE_TEST_EQ(test, berr[at], Real<T>{0.125});
    }
  }
}
template <typename T>
void One(TestContext& test, const asc::ReferenceLapackProvider& provider,
         asc::DenseBlasTriangle triangle, asc::DenseBlasLayout b_layout,
         asc::DenseBlasLayout x_layout, Fault fault) {
  const Fixture<T> fixture(2, 0, triangle);
  const auto original = fixture.Original();
  const auto factor = fixture.Factor(provider);
  Rhs<T> rhs(2, 3, b_layout);
  Rhs<T> solution(2, 3, x_layout);
  Initialize(fixture, rhs, solution);
  const auto x_before = solution.data;
  const auto rhs_before = rhs.data;
  std::array<Real<T>, 5> ferr{};
  std::array<Real<T>, 5> berr{};
  ferr.fill(Real<T>{-81});
  berr.fill(Real<T>{-83});
  const auto f = Vector(ferr, 3);
  const auto b = Vector(berr, 3);
  const auto plan = Take(asc::QueryPtrfsWorkspace(provider, original, factor,
                                                  std::as_const(rhs).View(),
                                                  solution.View(), f, b));
  Storage<T> storage;
  const auto workspace = storage.Workspace(plan);
  asc::LapackReport report;
  asc_ptrfs_entry::Reset();
  asc_ptrfs_entry::SetFault(fault);
  const auto status =
      asc::Ptrfs(provider, original, factor, std::as_const(rhs).View(),
                 solution.View(), f, b, plan, workspace, report);
  ASC_DENSE_TEST_EQ(test, asc_ptrfs_entry::Calls(), 1U);
  ASC_DENSE_TEST_CHECK(
      test, report.called_provider && report.native_info.has_value());
  if (IsDefect(fault)) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, solution.data, x_before);
    for (std::size_t i = 0; i < ferr.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, ferr[i], Real<T>{-81});
      ASC_DENSE_TEST_EQ(test, berr[i], Real<T>{-83});
    }
  } else {
    Published(test, fault, status, report, solution, ferr, berr);
  }
  ASC_DENSE_TEST_EQ(test, rhs.data, rhs_before);
  ASC_DENSE_TEST_EQ(test, ferr.front(), Real<T>{-81});
  ASC_DENSE_TEST_EQ(test, ferr.back(), Real<T>{-81});
  ASC_DENSE_TEST_EQ(test, berr.front(), Real<T>{-83});
  ASC_DENSE_TEST_EQ(test, berr.back(), Real<T>{-83});
  solution.Guards(test);
  storage.Guards(test, workspace);
  asc_ptrfs_entry::Reset();
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kLower, kUpper}) {
    for (const auto b : {kColumn, kRow}) {
      for (const auto x : {kColumn, kRow}) {
        for (const auto fault :
             {Fault::kNone, Fault::kNoInfo, Fault::kPartialInfo,
              Fault::kNegativeInfo, Fault::kPositiveInfo, Fault::kNoFerr,
              Fault::kNoBerr, Fault::kNegativeFerr, Fault::kNegativeBerr,
              Fault::kNanFerr, Fault::kInfiniteBerr, Fault::kNanSolution,
              Fault::kNormalWrite}) {
          One<T>(test, provider, triangle, b, x, fault);
        }
      }
    }
  }
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
