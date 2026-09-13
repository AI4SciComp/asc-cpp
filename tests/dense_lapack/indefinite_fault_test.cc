#include <array>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite.h"
#include "indefinite_faults.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_indefinite_test::Factor;
using asc_indefinite_test::Fault;
using asc_indefinite_test::ForeignRoutine;
using asc_indefinite_test::kColumn;
using asc_indefinite_test::kLower;
using asc_indefinite_test::kRow;
using asc_indefinite_test::kSymmetric;
using asc_indefinite_test::kUpper;
using asc_indefinite_test::Matrix;
using asc_indefinite_test::Pivots;
using asc_indefinite_test::QueryFactor;
using asc_indefinite_test::QuerySolve;
using asc_indefinite_test::Raw;
using asc_indefinite_test::Scratch;
using asc_indefinite_test::SetFault;
using asc_indefinite_test::Solve;
using asc_indefinite_test::Take;
using asc_indefinite_test::TestContext;
using asc_indefinite_test::WithoutAllocation;

void FactorFaults(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool blocked : {false, true}) {
        for (const auto fault :
             {Fault::kNegativeInfo, Fault::kExcessInfo, Fault::kPivotMinimum,
              Fault::kBrokenPair, Fault::kWorkNan}) {
          if (!blocked && fault == Fault::kWorkNan) {
            continue;
          }
          std::array<double, 8> a{-31, 0, 1, -31, 1, 0, -31, -31};
          std::array<asc::index_t, 4> pivots{-37, -37, -37, -37};
          const auto original = a;
          const auto matrix = Matrix(a, 2, 2, layout, 3);
          const auto pivot = Pivots(pivots, 2);
          const auto plan = Take(
              QueryFactor(provider, triangle, false, blocked, matrix, pivot));
          Scratch<double> scratch;
          const auto workspace = scratch.Workspace(plan);
          asc::LapackReport report;
          SetFault(blocked ? ForeignRoutine::kTrf : ForeignRoutine::kTf2,
                   fault);
          const auto status = WithoutAllocation(test, [&] {
            return Factor(provider, triangle, false, blocked, matrix, pivot,
                          plan, workspace, report);
          });
          SetFault(ForeignRoutine::kTrf, Fault::kNone);
          ASC_DENSE_TEST_CHECK(test, !status.ok());
          ASC_DENSE_TEST_CHECK(
              test, report.called_provider && report.native_info.has_value());
          ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999),
                            fault == Fault::kNegativeInfo ? -4
                            : fault == Fault::kExcessInfo ? 3
                                                          : 0);
          ASC_DENSE_TEST_EQ(test, report.output_validity,
                            asc::LapackOutputValidity::kUnusable);
          ASC_DENSE_TEST_EQ(test, pivots,
                            (std::array<asc::index_t, 4>{-37, -37, -37, -37}));
          if (layout == kRow || fault == Fault::kNegativeInfo) {
            ASC_DENSE_TEST_EQ(test, a, original);
          }
        }
      }
    }
  }
}

void SolveFaults(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : {kColumn, kRow}) {
    std::array<double, 8> a{-31, 0, 1, -31, 1, 0, -31, -31};
    std::array<asc::index_t, 4> pivots{};
    const auto matrix = Matrix(a, 2, 2, kColumn, 3);
    const auto pivot = Pivots(pivots, 2);
    const auto plan =
        Take(QueryFactor(provider, kUpper, false, false, matrix, pivot));
    Scratch<double> scratch;
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(
        test, Factor(provider, kUpper, false, false, matrix, pivot, plan,
                     scratch.Workspace(plan), report)
                  .ok());
    const auto factor =
        Take(asc::ReferenceBunchKaufmanFactorView<double>::Create(
            provider, Matrix(std::as_const(a), 2, 2, kColumn, 3), kUpper,
            kSymmetric, Raw(pivots, 2), report));
    for (const auto fault : {Fault::kNegativeInfo, Fault::kExcessInfo}) {
      std::array<double, 8> rhs{-41, 2, 3, -41, -41, -41, -41, -41};
      if (layout == kRow) {
        rhs[4] = 3;
      }
      const auto original = rhs;
      const auto b = Matrix(rhs, 2, 1, layout, 3);
      const auto solve_plan = Take(QuerySolve(provider, false, factor, b));
      const auto workspace = scratch.Workspace(solve_plan);
      SetFault(ForeignRoutine::kTrs, fault);
      const auto status = WithoutAllocation(test, [&] {
        return Solve(provider, false, factor, b, solve_plan, workspace, report);
      });
      SetFault(ForeignRoutine::kTrf, Fault::kNone);
      ASC_DENSE_TEST_CHECK(test, !status.ok());
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999),
                        fault == Fault::kNegativeInfo ? -8 : 1);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      if (layout == kRow || fault == Fault::kNegativeInfo) {
        ASC_DENSE_TEST_EQ(test, rhs, original);
      }
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  FactorFaults(test, provider);
  SolveFaults(test, provider);
  return test.Finish();
}
