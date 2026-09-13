#include <array>
#include <complex>
#include <cstddef>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_expert.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "band_expert_driver_fault_support.h"
#include "band_expert_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_driver_test::FaultCalls;
using asc_band_driver_test::Fixture;
using asc_band_driver_test::ResetFault;
using asc_band_estimation_test::WorkspaceStorage;

template <typename T>
struct Operands {
  using Real = asc::DenseBlasRealType<T>;
  std::array<std::array<T, 8>, 8> backing{};
  std::array<T*, 8> pointers{};
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  asc::LapackCholeskyEquilibration equilibration =
      asc::LapackCholeskyEquilibration::kNone;
  Operands(asc::DenseBlasTriangle part, asc::DenseBlasLayout order,
           std::size_t first, std::size_t second)
      : triangle(part), layout(order) {
    for (std::size_t i = 0; i < backing.size(); ++i) {
      backing[i].fill(T{13});
      pointers[i] = backing[i].data();
    }
    pointers[second] = pointers[first];
  }
  template <typename Element>
  auto Band(std::size_t i) {
    return Take(asc::LapackPositiveDefiniteBandView<Element>::Create(
        pointers[i], 1, 0, triangle, layout, 1,
        {pointers[i], sizeof(backing[i]), kHost}));
  }
  template <typename Element>
  auto Matrix(std::size_t i) {
    return Take(asc::DenseBlasMatrixView<Element>::Create(
        pointers[i], 1, 1, layout, 1,
        {pointers[i], sizeof(backing[i]), kHost}));
  }
  template <typename Element = Real>
  auto Vector(std::size_t i) {
    return Take(asc::DenseBlasVectorView<Element>::Create(
        reinterpret_cast<Real*>(pointers[i]), 1, 1,
        {pointers[i], sizeof(backing[i]), kHost}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider, char mode) {
    auto& rcond = *reinterpret_cast<Real*>(pointers[6]);
    if (mode == 'N') {
      return asc::QueryPbsvxWorkspace(provider, Band<const T>(0), Band<T>(1),
                                      Matrix<const T>(2), Matrix<T>(3),
                                      Vector(4), Vector(5), rcond);
    }
    if (mode == 'E') {
      return asc::QueryPbsvxEquilibratedWorkspace(
          provider, Band<T>(0), Band<T>(1), equilibration, Vector(7),
          Matrix<T>(2), Matrix<T>(3), Vector(4), Vector(5), rcond);
    }
    return asc::QueryPbsvxFactoredWorkspace(
        provider, Band<const T>(0), Band<const T>(1), equilibration,
        Vector<const Real>(7), Matrix<T>(2), Matrix<T>(3), Vector(4), Vector(5),
        rcond);
  }
  auto Execute(const asc::ReferenceLapackProvider& provider, char mode,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace,
               asc::LapackReport& report) {
    auto& rcond = *reinterpret_cast<Real*>(pointers[6]);
    if (mode == 'N') {
      return asc::Pbsvx(provider, Band<const T>(0), Band<T>(1),
                        Matrix<const T>(2), Matrix<T>(3), Vector(4), Vector(5),
                        rcond, plan, workspace, report);
    }
    if (mode == 'E') {
      return asc::PbsvxEquilibrated(provider, Band<T>(0), Band<T>(1),
                                    equilibration, Vector(7), Matrix<T>(2),
                                    Matrix<T>(3), Vector(4), Vector(5), rcond,
                                    plan, workspace, report);
    }
    return asc::PbsvxFactored(provider, Band<const T>(0), Band<const T>(1),
                              equilibration, Vector<const Real>(7),
                              Matrix<T>(2), Matrix<T>(3), Vector(4), Vector(5),
                              rcond, plan, workspace, report);
  }
};

template <typename T>
void Alias(TestContext& test, const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
           char mode, std::size_t first, std::size_t second) {
  Operands<T> operands(triangle, layout, first, second);
  const auto before = operands.backing;
  ASC_DENSE_TEST_EQ(
      test,
      WithoutAllocation(test, [&] { return operands.Query(provider, mode); })
          .status()
          .code(),
      asc::ErrorCode::kInvalidArgument);
  Fixture<T> good(1, 0, 1, triangle, {layout, layout, layout, layout}, mode);
  const auto plan = Take(good.Query(provider));
  WorkspaceStorage<T> storage(plan);
  ResetFault();
  asc::LapackReport report;
  ASC_DENSE_TEST_EQ(test,
                    WithoutAllocation(test,
                                      [&] {
                                        return operands.Execute(
                                            provider, mode, plan,
                                            storage.View(), report);
                                      })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kNotRun);
  ASC_DENSE_TEST_EQ(test, before, operands.backing);
  storage.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const char mode : {'N', 'E', 'F'}) {
        const std::size_t count = mode == 'N' ? 7 : 8;
        for (std::size_t first = 0; first < count; ++first) {
          for (std::size_t second = first + 1; second < count; ++second) {
            Alias<T>(test, provider, triangle, layout, mode, first, second);
          }
        }
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
