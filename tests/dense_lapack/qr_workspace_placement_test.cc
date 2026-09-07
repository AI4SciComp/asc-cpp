#include <complex>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_qr.h"
#include "qr_faults.h"
#include "qr_test_support.h"

namespace {
using asc_qr_test::ConstVector;
using asc_qr_test::ForeignCalls;
using asc_qr_test::kPacking;
using asc_qr_test::kScalar;
using asc_qr_test::Layout;
using asc_qr_test::Matrix;
using asc_qr_test::Routine;
using asc_qr_test::Scratch;
using asc_qr_test::Take;
using asc_qr_test::TestContext;
using asc_qr_test::Vector;
using asc_qr_test::WithoutAllocation;

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider, Routine routine,
           Matrix<T>& matrix, const Matrix<T>& reflectors, std::vector<T>& tau,
           asc::LapackReport& report) {
  if (routine == Routine::kGeqrf) {
    return asc::QueryGeqrfWorkspace(provider, matrix.view(), Vector(tau, 3),
                                    report);
  }
  if (routine == Routine::kGeqr2) {
    return asc::QueryGeqr2Workspace(provider, matrix.view(), Vector(tau, 3),
                                    report);
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    if (routine == Routine::kGenerate) {
      return asc::QueryUngqrWorkspace(provider, matrix.view(),
                                      ConstVector(tau, 3), report);
    }
    return asc::QueryUnmqrWorkspace(
        provider, asc::DenseBlasSide::kLeft, asc::DenseBlasTranspose::kNone,
        reflectors.const_view(), ConstVector(tau, 3), matrix.view(), report);
  } else {
    if (routine == Routine::kGenerate) {
      return asc::QueryOrgqrWorkspace(provider, matrix.view(),
                                      ConstVector(tau, 3), report);
    }
    return asc::QueryOrmqrWorkspace(
        provider, asc::DenseBlasSide::kLeft, asc::DenseBlasTranspose::kNone,
        reflectors.const_view(), ConstVector(tau, 3), matrix.view(), report);
  }
}

template <typename T>
auto Execute(const asc::ReferenceLapackProvider& provider, Routine routine,
             Matrix<T>& matrix, const Matrix<T>& reflectors,
             std::vector<T>& tau, const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  if (routine == Routine::kGeqrf) {
    return asc::Geqrf(provider, matrix.view(), Vector(tau, 3), plan, workspace,
                      report);
  }
  if (routine == Routine::kGeqr2) {
    return asc::Geqr2(provider, matrix.view(), Vector(tau, 3), plan, workspace,
                      report);
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    if (routine == Routine::kGenerate) {
      return asc::Ungqr(provider, matrix.view(), ConstVector(tau, 3), plan,
                        workspace, report);
    }
    return asc::Unmqr(provider, asc::DenseBlasSide::kLeft,
                      asc::DenseBlasTranspose::kNone, reflectors.const_view(),
                      ConstVector(tau, 3), matrix.view(), plan, workspace,
                      report);
  } else {
    if (routine == Routine::kGenerate) {
      return asc::Orgqr(provider, matrix.view(), ConstVector(tau, 3), plan,
                        workspace, report);
    }
    return asc::Ormqr(provider, asc::DenseBlasSide::kLeft,
                      asc::DenseBlasTranspose::kNone, reflectors.const_view(),
                      ConstVector(tau, 3), matrix.view(), plan, workspace,
                      report);
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  Matrix<T> matrix(3, 3, Layout::kRowMajor);
  Matrix<T> reflectors(3, 3, Layout::kRowMajor);
  std::vector<T> tau(3);
  const auto before = matrix.bytes();
  const auto old_reflectors = reflectors.bytes();
  const auto old_tau = tau;
  for (const auto routine : {Routine::kGeqrf, Routine::kGeqr2,
                             Routine::kGenerate, Routine::kApply}) {
    asc::LapackReport report;
    const auto plan =
        Take(Query(provider, routine, matrix, reflectors, tau, report));
    Scratch<T> scratch(plan, true);
    const auto original = scratch.view();
    for (const auto role : {kScalar, kPacking}) {
      for (const auto placement :
           {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
            asc::MemorySpace::kManaged}) {
        auto workspace = original;
        const auto region = workspace.regions[role];
        ASC_DENSE_TEST_CHECK(test, region.size() != 0);
        workspace.regions[role] = {region.data(), region.size(), placement};
        const auto calls = ForeignCalls();
        const auto status = WithoutAllocation(test, [&] {
          return Execute(provider, routine, matrix, reflectors, tau, plan,
                         workspace, report);
        });
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
        ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
        ASC_DENSE_TEST_CHECK(test, !report.called_provider);
        ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
        ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kNotRun);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kUnchanged);
        matrix.CheckSame(test, before);
        reflectors.CheckSame(test, old_reflectors);
        ASC_DENSE_TEST_EQ(test, tau, old_tau);
      }
    }
  }
}
}  // namespace

int main() {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Run<float>(test, provider);
  Run<double>(test, provider);
  Run<std::complex<float>>(test, provider);
  Run<std::complex<double>>(test, provider);
  return test.Finish();
}
