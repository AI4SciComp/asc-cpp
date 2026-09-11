#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_expert.h"
#include "installed_lu/normal_return_guard.h"
#include "internal_tridiagonal.h"
#include "positive_tridiagonal_expert_entry.h"
#include "positive_tridiagonal_refinement_test_support.h"
#include "tridiagonal_test_support.h"
namespace {
namespace probe = asc::internal_ptsvx_test;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::Vector;
bool BadProtocol(probe::Fault fault, asc::extent_t nrhs, bool supplied) {
  using Fault = probe::Fault;
  return fault == Fault::kNoInfo || fault == Fault::kNegativeInfo ||
         fault == Fault::kBeyondInfo || fault == Fault::kPivotCondition ||
         (fault == Fault::kPartialInfo && sizeof(lapack_int) == 8) ||
         fault == Fault::kNoCondition || fault == Fault::kNegativeCondition ||
         (fault == Fault::kPositiveInfo && supplied) ||
         (nrhs > 0 &&
          (fault == Fault::kNoFerr || fault == Fault::kNegativeFerr ||
           fault == Fault::kNoBerr || fault == Fault::kNegativeBerr));
}
template <typename T>
void Published(TestContext& test, probe::Fault fault, bool nonfinite,
               const asc::Status& status, const asc::LapackReport& report,
               asc::DenseBlasRealType<T> condition,
               asc::DenseBlasVectorView<asc::DenseBlasRealType<T>> fv,
               asc::DenseBlasVectorView<asc::DenseBlasRealType<T>> bv,
               const asc_tridiagonal_test::Rhs<T>& x, asc::extent_t nrhs) {
  using R = asc::DenseBlasRealType<T>;
  using Fault = probe::Fault;
  const bool warning = nonfinite || fault == Fault::kConditionWarning;
  ASC_DENSE_TEST_EQ(test, status.code(),
                    warning ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99),
                    fault == Fault::kConditionWarning ? 4 : 0);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    nonfinite ? asc::LapackOutputValidity::kDocumentedPartial
                              : asc::LapackOutputValidity::kComplete);
  if (fault == Fault::kWrite || fault == Fault::kConditionWarning) {
    ASC_DENSE_TEST_EQ(test, condition, R{0.5});
    for (asc::extent_t j = 0; j < nrhs; ++j) {
      ASC_DENSE_TEST_EQ(test, fv.data()[j], R{0.125});
      ASC_DENSE_TEST_EQ(test, bv.data()[j], R{0.0625});
      for (asc::extent_t i = 0; i < 3; ++i) {
        ASC_DENSE_TEST_EQ(test, x.At(i, j), T{2});
      }
    }
  }
}
template <typename T, typename F>
void RunCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc_ptrfs_test::Fixture<T>& fixture, F factor,
             asc::DenseBlasLayout layout, asc::extent_t nrhs,
             probe::Fault fault) {
  using R = asc::DenseBlasRealType<T>;
  using Fault = probe::Fault;
  constexpr bool kSupplied =
      std::is_same_v<F, asc::ReferencePositiveDefiniteTridiagonalFactorView<T>>;
  asc_tridiagonal_test::Rhs<T> b(3, nrhs, layout);
  asc_tridiagonal_test::Rhs<T> x(3, nrhs,
                                 layout == asc_tridiagonal_test::kRow
                                     ? asc_tridiagonal_test::kColumn
                                     : asc_tridiagonal_test::kRow);
  asc_ptrfs_test::Initialize(fixture, b, x);
  const auto old_x = x.data;
  const auto old_b = b.data;
  const auto old_ef = fixture.ef;
  const auto old_df = fixture.df;
  std::array<R, 4> ferr{-3, -3, -3, -3};
  std::array<R, 4> berr{-5, -5, -5, -5};
  R condition = -7;
  const auto fv = Vector(ferr, nrhs);
  const auto bv = Vector(berr, nrhs);
  const auto plan = Take(asc::QueryPtsvxWorkspace(
      provider, fixture.Original(), factor, std::as_const(b).View(), x.View(),
      condition, fv, bv));
  asc_ptrfs_test::Storage<T> storage;
  const auto workspace = storage.Workspace(plan);
  probe::SetFault(fault);
  asc::LapackReport report;
  const auto status =
      asc::Ptsvx(provider, fixture.Original(), factor, std::as_const(b).View(),
                 x.View(), condition, fv, bv, plan, workspace, report);
  ASC_DENSE_TEST_EQ(test, probe::EntryCount(), std::size_t{1});
  ASC_DENSE_TEST_CHECK(
      test, report.called_provider && report.native_info.has_value());
  const bool bad = BadProtocol(fault, nrhs, kSupplied);
  const bool pivot = fault == Fault::kPositiveInfo && !kSupplied;
  const bool nonfinite =
      fault == Fault::kNanCondition || fault == Fault::kInfiniteCondition ||
      (fault == Fault::kNanDiagonal && !kSupplied) ||
      (nrhs > 0 &&
       (fault == Fault::kNanSolution || fault == Fault::kInfiniteSolution ||
        fault == Fault::kNanFerr || fault == Fault::kInfiniteBerr));
  if (bad || pivot) {
    ASC_DENSE_TEST_EQ(
        test, status.code(),
        bad ? asc::ErrorCode::kProvider : asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, x.data, old_x);
    ASC_DENSE_TEST_EQ(test, condition, pivot ? R{0} : R{-7});
    for (std::size_t i = 0; i < ferr.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, ferr[i], R{-3});
      ASC_DENSE_TEST_EQ(test, berr[i], R{-5});
    }
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      pivot
                          ? asc::LapackOutputValidity::kDocumentedPartial
                          : (kSupplied ? asc::LapackOutputValidity::kUnchanged
                                       : asc::LapackOutputValidity::kUnusable));
    if (pivot) {
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-99), 1);
    }
  } else {
    Published(test, fault, nonfinite, status, report, condition, fv, bv, x,
              nrhs);
  }
  ASC_DENSE_TEST_EQ(test, b.data, old_b);
  if constexpr (kSupplied) {
    ASC_DENSE_TEST_EQ(test, fixture.df, old_df);
    ASC_DENSE_TEST_EQ(test, fixture.ef, old_ef);
  }
  b.Guards(test);
  x.Guards(test);
  storage.Guards(test, workspace);
  ASC_DENSE_TEST_EQ(test, ferr.front(), R{-3});
  ASC_DENSE_TEST_EQ(test, ferr.back(), R{-3});
  ASC_DENSE_TEST_EQ(test, berr.front(), R{-5});
  ASC_DENSE_TEST_EQ(test, berr.back(), R{-5});
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Fault = probe::Fault;
  for (const auto layout :
       {asc_tridiagonal_test::kColumn, asc_tridiagonal_test::kRow}) {
    for (const asc::extent_t nrhs : {0, 2}) {
      for (const auto fault : {Fault::kNone,
                               Fault::kWrite,
                               Fault::kNoInfo,
                               Fault::kPartialInfo,
                               Fault::kNegativeInfo,
                               Fault::kPositiveInfo,
                               Fault::kBeyondInfo,
                               Fault::kPivotCondition,
                               Fault::kConditionWarning,
                               Fault::kNoCondition,
                               Fault::kNegativeCondition,
                               Fault::kNoFerr,
                               Fault::kNegativeFerr,
                               Fault::kNoBerr,
                               Fault::kNegativeBerr,
                               Fault::kNanCondition,
                               Fault::kInfiniteCondition,
                               Fault::kNanSolution,
                               Fault::kInfiniteSolution,
                               Fault::kNanFerr,
                               Fault::kInfiniteBerr,
                               Fault::kNanDiagonal}) {
        asc_ptrfs_test::Fixture<T> supplied(3, 0, asc_ptrfs_test::kUpper);
        asc_ptrfs_test::Fixture<T> fresh(3, 0, asc_ptrfs_test::kLower);
        RunCase(test, provider, supplied, supplied.Factor(provider), layout,
                nrhs, fault);
        const auto output =
            Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
                Vector(fresh.df, 3), Vector(fresh.ef, 2)));
        RunCase(test, provider, fresh, output, layout, nrhs, fault);
      }
    }
  }
  probe::SetFault(Fault::kNone);
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
