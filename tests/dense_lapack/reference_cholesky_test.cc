#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky.h"
#include "cholesky_faults.h"
#include "cholesky_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "src/dense/lapack/internal_cholesky_limits.h"

namespace {
using asc_cholesky_test::Calls;
using asc_cholesky_test::CheckFactor;
using asc_cholesky_test::CheckInverse;
using asc_cholesky_test::CheckRatio;
using asc_cholesky_test::CheckSolution;
using asc_cholesky_test::Fault;
using asc_cholesky_test::Fill;
using asc_cholesky_test::FillRhs;
using asc_cholesky_test::kLayouts;
using asc_cholesky_test::kPacking;
using asc_cholesky_test::kTriangles;
using asc_cholesky_test::Layout;
using asc_cholesky_test::Matrix;
using asc_cholesky_test::Narrow;
using asc_cholesky_test::NotANumber;
using asc_cholesky_test::Routine;
using asc_cholesky_test::SameBits;
using asc_cholesky_test::Scratch;
using asc_cholesky_test::SetFault;
using asc_cholesky_test::Take;
using asc_cholesky_test::TestContext;
using asc_cholesky_test::Triangle;
using asc_cholesky_test::Wide;
using asc_cholesky_test::Widen;
using asc_cholesky_test::WithoutAllocation;
std::size_t g_reconstructions = 0;
std::size_t g_solves = 0;
std::size_t g_inverses = 0;
std::size_t g_drivers = 0;
std::size_t g_positive_failures = 0;
constexpr std::array kFactorRoutines{Routine::kPotrf, Routine::kPotrf2,
                                     Routine::kPotf2};

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider, Routine routine,
           Triangle triangle, asc::DenseBlasMatrixView<T> matrix) {
  switch (routine) {
    case Routine::kPotrf:
      return asc::QueryPotrfWorkspace(provider, triangle, matrix);
    case Routine::kPotrf2:
      return asc::QueryPotrf2Workspace(provider, triangle, matrix);
    case Routine::kPotf2:
      return asc::QueryPotf2Workspace(provider, triangle, matrix);
    default:
      return asc::QueryPotriWorkspace(provider, triangle, matrix);
  }
}

template <typename T>
asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                    Routine routine, Triangle triangle,
                    asc::DenseBlasMatrixView<T> matrix,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  switch (routine) {
    case Routine::kPotrf:
      return asc::Potrf(provider, triangle, matrix, plan, workspace, report);
    case Routine::kPotrf2:
      return asc::Potrf2(provider, triangle, matrix, plan, workspace, report);
    case Routine::kPotf2:
      return asc::Potf2(provider, triangle, matrix, plan, workspace, report);
    default:
      return asc::Potri(provider, triangle, matrix, plan, workspace, report);
  }
}

void CheckSuccess(TestContext& test,
                  const asc::ReferenceLapackProvider& provider,
                  const asc::LapackReport& report, bool called) {
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, report.called_provider, called);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), called);
  if (report.native_info.has_value()) {
    ASC_DENSE_TEST_EQ(test, *report.native_info, 0);
  }
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
}

template <typename T>
void SuccessCase(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Routine routine,
                 std::size_t n, Triangle triangle, Layout layout,
                 long double scale) {
  Matrix<T> matrix(static_cast<asc::extent_t>(n), static_cast<asc::extent_t>(n),
                   layout);
  Fill(matrix, n, triangle, scale);
  const auto before = matrix.bytes();
  Scratch<T> scratch;
  const auto plan = Take(WithoutAllocation(
      test, [&] { return Query(provider, routine, triangle, matrix.view()); }));
  matrix.CheckSame(test, before);
  ASC_DENSE_TEST_EQ(
      test, plan.regions[kPacking].minimum_entries,
      static_cast<asc::extent_t>(layout == Layout::kRowMajor ? n * n : 0));
  auto workspace = scratch.view(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return Execute(provider, routine, triangle,
                                              matrix.view(), plan, workspace,
                                              report);
                             }).ok());
  CheckSuccess(test, provider, report, true);
  std::string_view expected_name = "potf2";
  if (routine == Routine::kPotrf) {
    expected_name = "potrf";
  } else if (routine == Routine::kPotrf2) {
    expected_name = "potrf2";
  }
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()).substr(1),
                    std::string_view(expected_name));
  CheckFactor(test, matrix, n, triangle, scale);
  ++g_reconstructions;
  matrix.CheckUntouched(test, before, triangle);
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      matrix.const_view(), triangle, report));
  for (const auto rhs_layout : kLayouts) {
    for (const std::size_t nrhs : {1U, 3U}) {
      Matrix<T> rhs(static_cast<asc::extent_t>(n),
                    static_cast<asc::extent_t>(nrhs), rhs_layout);
      FillRhs(rhs, n, nrhs, scale);
      const auto original_rhs = rhs;
      const auto factor_bytes = matrix.bytes();
      const auto solve_plan = Take(WithoutAllocation(test, [&] {
        return asc::QueryPotrsWorkspace(provider, factor, rhs.view());
      }));
      workspace = scratch.view(solve_plan);
      ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                   return asc::Potrs(provider, factor,
                                                     rhs.view(), solve_plan,
                                                     workspace, report);
                                 }).ok());
      CheckSuccess(test, provider, report, true);
      ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
      CheckSolution(test, rhs, original_rhs, n, nrhs, scale);
      ++g_solves;
      matrix.CheckSame(test, factor_bytes);
      rhs.CheckUntouched(test, original_rhs.bytes(), triangle, true);
    }
  }
  const auto factor_bytes = matrix.bytes();
  const auto inverse_plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPotriWorkspace(provider, triangle, matrix.view());
  }));
  workspace = scratch.view(inverse_plan);
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Potri(provider, triangle,
                                                 matrix.view(), inverse_plan,
                                                 workspace, report);
                             }).ok());
  CheckSuccess(test, provider, report, true);
  CheckInverse(test, matrix, n, triangle, scale);
  ++g_inverses;
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_CHECK(test, !asc::LapackCholeskyFactorView<T>::Create(
                                  matrix.const_view(), triangle, report)
                                  .ok());
  matrix.CheckUntouched(test, factor_bytes, triangle);
}

template <typename T>
void DriverCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
                Triangle triangle, Layout layout, Layout rhs_layout,
                long double scale, std::size_t nrhs) {
  Matrix<T> matrix(3, 3, layout);
  Fill(matrix, 3, triangle, scale);
  const auto before = matrix.bytes();
  Matrix<T> rhs(3, static_cast<asc::extent_t>(nrhs), rhs_layout);
  FillRhs(rhs, 3, nrhs, scale);
  const auto original = rhs;
  Scratch<T> scratch;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPosvWorkspace(provider, triangle, matrix.view(),
                                   rhs.view());
  }));
  const auto workspace = scratch.view(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Posv(provider, triangle,
                                                matrix.view(), rhs.view(), plan,
                                                workspace, report);
                             }).ok());
  CheckSuccess(test, provider, report, true);
  CheckFactor(test, matrix, 3, triangle, scale);
  CheckSolution(test, rhs, original, 3, nrhs, scale);
  ASC_DENSE_TEST_CHECK(test, asc::LapackCholeskyFactorView<T>::Create(
                                 matrix.const_view(), triangle, report)
                                 .ok());
  matrix.CheckUntouched(test, before, triangle);
  rhs.CheckUntouched(test, original.bytes(), triangle, true);
  ++g_drivers;
}

template <typename T>
void Numerical(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const int exponent = sizeof(Real) == sizeof(float) ? 100 : 900;
  for (const auto triangle : kTriangles) {
    for (const auto layout : kLayouts) {
      for (const auto routine : kFactorRoutines) {
        for (const auto scale :
             {1.L, std::ldexp(1.L, -exponent), std::ldexp(1.L, exponent)}) {
          SuccessCase<T>(test, provider, routine, 3, triangle, layout, scale);
        }
        SuccessCase<T>(test, provider, routine, 65, triangle, layout, 1);
      }
      for (const auto rhs_layout : kLayouts) {
        for (const auto scale :
             {1.L, std::ldexp(1.L, -exponent), std::ldexp(1.L, exponent)}) {
          for (const std::size_t nrhs : {0U, 1U, 3U}) {
            DriverCase<T>(test, provider, triangle, layout, rhs_layout, scale,
                          nrhs);
          }
        }
      }
    }
  }
}

template <typename T>
void FactorFailureCase(TestContext& test,
                       const asc::ReferenceLapackProvider& provider,
                       Routine routine, Triangle triangle, Layout layout,
                       std::size_t n, std::size_t failed, long double bad) {
  Matrix<T> matrix(static_cast<asc::extent_t>(n), static_cast<asc::extent_t>(n),
                   layout);
  Fill(matrix, n, triangle, 1);
  for (std::size_t row = 0; row < n; ++row) {
    for (std::size_t col = 0; col < n; ++col) {
      if (triangle == Triangle::kLower ? row >= col : row <= col) {
        long double entry = 0;
        if (row == col) {
          entry = row == failed ? bad : 4;
        }
        matrix(row, col) = Narrow<T>({entry, 0});
        if constexpr (asc::DenseBlasComplex<T>) {
          if (row == col) {
            matrix(row, col).imag(
                std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
          }
        }
      }
    }
  }
  const auto before = matrix.bytes();
  const auto plan = Take(Query(provider, routine, triangle, matrix.view()));
  Scratch<T> scratch;
  const auto workspace = scratch.view(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, routine, triangle, matrix.view(), plan, workspace,
                   report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.native_info,
                    static_cast<std::int64_t>(failed + 1));
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index,
                    static_cast<asc::index_t>(failed));
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    asc::LapackOutcome::kNotPositiveDefinite);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  ASC_DENSE_TEST_CHECK(test, !asc::LapackCholeskyFactorView<T>::Create(
                                  matrix.const_view(), triangle, report)
                                  .ok());
  for (std::size_t i = 0; i < failed; ++i) {
    ASC_DENSE_TEST_EQ(test, Widen(matrix(i, i)).real(), 2.L);
  }
  if (std::isnan(bad)) {
    ASC_DENSE_TEST_CHECK(test,
                         std::isnan(Widen(matrix(failed, failed)).real()));
  } else {
    ASC_DENSE_TEST_EQ(test, Widen(matrix(failed, failed)).real(), bad);
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    if (layout == Layout::kRowMajor) {
      for (std::size_t i = 0; i < n; ++i) {
        ASC_DENSE_TEST_CHECK(
            test,
            SameBits(matrix(i, i).imag(), before[matrix.Offset(i, i)].imag()));
      }
    }
  }
  matrix.CheckUntouched(test, before, triangle);
  ++g_positive_failures;
}

template <typename T>
void PositiveFailures(TestContext& test,
                      const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : kTriangles) {
    for (const auto layout : kLayouts) {
      for (const auto routine : kFactorRoutines) {
        for (const std::size_t failed : {0U, 2U}) {
          for (const long double bad :
               {0.L, -1.L, std::numeric_limits<long double>::quiet_NaN()}) {
            FactorFailureCase<T>(test, provider, routine, triangle, layout, 3,
                                 failed, bad);
          }
        }
        FactorFailureCase<T>(test, provider, routine, triangle, layout, 65, 0,
                             0);
        FactorFailureCase<T>(test, provider, routine, triangle, layout, 65, 64,
                             0);
      }
      // Raw POTRI deliberately accepts a zero diagonal, preserving foreign
      // singularity INFO and proving the upstream pre-inversion zero scan.
      Matrix<T> inverse(3, 3, layout);
      Fill(inverse, 3, triangle, 1);
      for (std::size_t i = 0; i < 3; ++i) {
        inverse(i, i) = Narrow<T>({i == 2 ? 0.L : 2.L, 0});
      }
      const auto before = inverse.bytes();
      const auto plan =
          Take(asc::QueryPotriWorkspace(provider, triangle, inverse.view()));
      Scratch<T> scratch;
      const auto workspace = scratch.view(plan);
      asc::LapackReport report;
      const auto status = WithoutAllocation(test, [&] {
        return asc::Potri(provider, triangle, inverse.view(), plan, workspace,
                          report);
      });
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_EQ(test, report.native_info, 3);
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 2);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnchanged);
      ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
      inverse.CheckSame(test, before);
      for (const auto rhs_layout : kLayouts) {
        Matrix<T> matrix(3, 3, layout);
        Fill(matrix, 3, triangle, 1);
        matrix(0, 0) = Narrow<T>({-1, 0});
        Matrix<T> rhs(3, 2, rhs_layout);
        FillRhs(rhs, 3, 2, 1);
        const auto rhs_before = rhs.bytes();
        const auto driver_plan = Take(asc::QueryPosvWorkspace(
            provider, triangle, matrix.view(), rhs.view()));
        const auto driver_workspace = scratch.view(driver_plan);
        const auto failure = WithoutAllocation(test, [&] {
          return asc::Posv(provider, triangle, matrix.view(), rhs.view(),
                           driver_plan, driver_workspace, report);
        });
        ASC_DENSE_TEST_EQ(test, failure.code(), asc::ErrorCode::kNumerical);
        ASC_DENSE_TEST_EQ(test, report.native_info, 1);
        ASC_DENSE_TEST_EQ(test, report.outcome,
                          asc::LapackOutcome::kNotPositiveDefinite);
        rhs.CheckSame(test, rhs_before);
      }
    }
  }
}

template <typename T>
void ScalarCases(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Routine routine,
                 Triangle triangle, Layout layout) {
  Scratch<T> scratch;
  asc::LapackWorkspace workspace;
  asc::LapackReport report;
  for (const auto value :
       {std::numeric_limits<asc::DenseBlasRealType<T>>::denorm_min(),
        std::numeric_limits<asc::DenseBlasRealType<T>>::max() / 4}) {
    Matrix<T> scalar(1, 1, layout);
    scalar(0, 0) = Narrow<T>({value, 0});
    if constexpr (asc::DenseBlasComplex<T>) {
      scalar(0, 0).imag(
          std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
    }
    const auto scalar_plan =
        Take(Query(provider, routine, triangle, scalar.view()));
    workspace = scratch.view(scalar_plan);
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return Execute(provider, routine, triangle,
                                                scalar.view(), scalar_plan,
                                                workspace, report);
                               }).ok());
    CheckSuccess(test, provider, report, true);
    const auto root = Widen(scalar(0, 0));
    CheckRatio(test, std::abs(root.real() * root.real() - value), value,
               8 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon());
    ASC_DENSE_TEST_EQ(test, root.imag(), 0);
    // An empty RHS must not read or pack factor payload. Poison only
    // after obtaining its successful borrowed provenance.
    const auto scalar_factor = Take(asc::LapackCholeskyFactorView<T>::Create(
        scalar.const_view(), triangle, report));
    Matrix<T> empty_rhs(1, 0, layout);
    const auto no_rhs_plan = Take(
        asc::QueryPotrsWorkspace(provider, scalar_factor, empty_rhs.view()));
    workspace = scratch.view(no_rhs_plan);
    scalar(0, 0) = NotANumber<T>();
    const auto bytes = scalar.bytes();
    ASC_DENSE_TEST_CHECK(
        test, asc::Potrs(provider, scalar_factor, empty_rhs.view(), no_rhs_plan,
                         workspace, report)
                  .ok());
    CheckSuccess(test, provider, report, false);
    scalar.CheckSame(test, bytes);
  }
}

template <typename T>
void EmptyAndScalar(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : kTriangles) {
    for (const auto layout : kLayouts) {
      for (const auto routine : kFactorRoutines) {
        Matrix<T> empty(0, 0, layout);
        const auto empty_plan =
            Take(Query(provider, routine, triangle, empty.view()));
        Scratch<T> scratch;
        auto workspace = scratch.view(empty_plan);
        asc::LapackReport report;
        ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                     return Execute(provider, routine, triangle,
                                                    empty.view(), empty_plan,
                                                    workspace, report);
                                   }).ok());
        CheckSuccess(test, provider, report, false);
        const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
            empty.const_view(), triangle, report));
        Matrix<T> rhs(0, 3, layout);
        const auto solve_plan =
            Take(asc::QueryPotrsWorkspace(provider, factor, rhs.view()));
        workspace = scratch.view(solve_plan);
        ASC_DENSE_TEST_CHECK(test, asc::Potrs(provider, factor, rhs.view(),
                                              solve_plan, workspace, report)
                                       .ok());
        CheckSuccess(test, provider, report, false);
        const auto inverse_plan =
            Take(asc::QueryPotriWorkspace(provider, triangle, empty.view()));
        workspace = scratch.view(inverse_plan);
        ASC_DENSE_TEST_CHECK(test, asc::Potri(provider, triangle, empty.view(),
                                              inverse_plan, workspace, report)
                                       .ok());
        CheckSuccess(test, provider, report, false);
        const auto driver_plan = Take(asc::QueryPosvWorkspace(
            provider, triangle, empty.view(), rhs.view()));
        workspace = scratch.view(driver_plan);
        ASC_DENSE_TEST_CHECK(
            test, asc::Posv(provider, triangle, empty.view(), rhs.view(),
                            driver_plan, workspace, report)
                      .ok());
        CheckSuccess(test, provider, report, false);
        ScalarCases<T>(test, provider, routine, triangle, layout);
      }
    }
  }
}

void Defensive(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  Matrix<double> matrix(3, 3, Layout::kRowMajor);
  Fill(matrix, 3, Triangle::kLower, 1);
  const auto before = matrix.bytes();
  const auto plan =
      Take(asc::QueryPotrfWorkspace(provider, Triangle::kLower, matrix.view()));
  Scratch<double> scratch;
  auto workspace = scratch.view(plan);
  asc::LapackReport report;
  const auto reject = [&](const asc::LapackWorkspacePlan& supplied,
                          const asc::LapackWorkspace& storage) {
    const auto count = Calls();
    report.called_provider = true;
    report.native_info = 77;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Potrf(provider, Triangle::kLower, matrix.view(), supplied,
                        storage, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, count, Calls());
    matrix.CheckSame(test, before);
  };
  auto stale = Take(
      asc::QueryPotrf2Workspace(provider, Triangle::kLower, matrix.view()));
  reject(stale, workspace);
  stale =
      Take(asc::QueryPotrfWorkspace(provider, Triangle::kUpper, matrix.view()));
  reject(stale, workspace);
  stale = plan;
  stale.regions[kPacking].minimum_entries = 0;
  reject(stale, workspace);
  stale = plan;
  stale.total_byte_limit = 1;
  reject(stale, workspace);
  auto short_workspace = workspace;
  short_workspace.regions[kPacking] = {
      scratch.values.data(), 8 * sizeof(double), asc::MemorySpace::kHost};
  reject(plan, short_workspace);
  auto misaligned = workspace;
  auto* raw = reinterpret_cast<std::byte*>(scratch.values.data());
  misaligned.regions[kPacking] = {raw + 1, 9 * sizeof(double),
                                  asc::MemorySpace::kHost};
  reject(plan, misaligned);
  auto device = workspace;
  device.regions[kPacking] = {scratch.values.data(), 9 * sizeof(double),
                              asc::MemorySpace::kDevice};
  reject(plan, device);
  auto overlap = workspace;
  overlap.regions[kPacking] = {matrix.view().data(), 9 * sizeof(double),
                               asc::MemorySpace::kHost};
  reject(plan, overlap);
  overlap = workspace;
  overlap.regions[0] = workspace.regions[kPacking];
  reject(plan, overlap);
  const auto count = Calls();
  report.native_info = 991;
  auto metadata_alias = workspace;
  metadata_alias.regions[kPacking] = {&report, sizeof(report),
                                      asc::MemorySpace::kHost};
  ASC_DENSE_TEST_CHECK(
      test, !asc::Potrf(provider, Triangle::kLower, matrix.view(), plan,
                        metadata_alias, report)
                 .ok());
  ASC_DENSE_TEST_EQ(test, report.native_info, 991);
  ASC_DENSE_TEST_EQ(test, Calls(), count);
  matrix.CheckSame(test, before);
}

void DefensiveDescriptors(TestContext& test,
                          const asc::ReferenceLapackProvider& provider) {
  Matrix<double> matrix(3, 3, Layout::kRowMajor);
  Fill(matrix, 3, Triangle::kLower, 1);
  const auto plan =
      Take(asc::QueryPotrfWorkspace(provider, Triangle::kLower, matrix.view()));
  Scratch<double> scratch;
  const auto workspace = scratch.view(plan);
  asc::LapackReport report;
  Matrix<double> nonsquare(3, 2, Layout::kColumnMajor);
  ASC_DENSE_TEST_EQ(
      test,
      asc::QueryPotrfWorkspace(provider, Triangle::kLower, nonsquare.view())
          .status()
          .code(),
      asc::ErrorCode::kShape);
  for (const auto placement :
       {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged,
        asc::MemorySpace::kPinnedHost}) {
    ASC_DENSE_TEST_EQ(test,
                      asc::QueryPotrfWorkspace(provider, Triangle::kLower,
                                               matrix.view(placement))
                          .status()
                          .code(),
                      asc::ErrorCode::kMemoryAccess);
  }
  // Deliberately invalid enum exercises the checked public boundary.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid_triangle = static_cast<Triangle>(89);
  ASC_DENSE_TEST_EQ(
      test,
      asc::QueryPotrfWorkspace(provider, invalid_triangle, matrix.view())
          .status()
          .code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_CHECK(
      test, !asc::QueryPosvWorkspace(provider, Triangle::kLower, matrix.view(),
                                     matrix.view())
                 .ok());

  ASC_DENSE_TEST_CHECK(test, asc::Potrf(provider, Triangle::kLower,
                                        matrix.view(), plan, workspace, report)
                                 .ok());
  auto wrong_identity = report;
  wrong_identity.provider.build_sha256[0] ^= std::byte{1};
  const auto foreign = Take(asc::LapackCholeskyFactorView<double>::Create(
      matrix.const_view(), Triangle::kLower, wrong_identity));
  Matrix<double> rhs(3, 1, Layout::kColumnMajor);
  ASC_DENSE_TEST_EQ(
      test,
      asc::QueryPotrsWorkspace(provider, foreign, rhs.view()).status().code(),
      asc::ErrorCode::kInvalidState);
  const auto factor = Take(asc::LapackCholeskyFactorView<double>::Create(
      matrix.const_view(), Triangle::kLower, report));
  ASC_DENSE_TEST_CHECK(
      test, !asc::QueryPotrsWorkspace(provider, factor, matrix.view()).ok());
  Matrix<double> wrong_rhs(2, 1, Layout::kRowMajor);
  ASC_DENSE_TEST_EQ(test,
                    asc::QueryPotrsWorkspace(provider, factor, wrong_rhs.view())
                        .status()
                        .code(),
                    asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(test,
                    asc::QueryPosvWorkspace(provider, Triangle::kLower,
                                            matrix.view(), wrong_rhs.view())
                        .status()
                        .code(),
                    asc::ErrorCode::kShape);
}

void ProviderDimensions(TestContext& test,
                        const asc::ReferenceLapackProvider& provider) {
  // A genuinely empty descriptor has no backing requirement, even with many
  // RHS columns. Nonempty extreme arithmetic is tested using pure integers.
  if (provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64) {
    const asc::extent_t n =
        static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()) +
        1;
    const auto huge = Take(asc::DenseBlasMatrixView<double>::Create(
        nullptr, 0, n, Layout::kColumnMajor, 1,
        {nullptr, 0, asc::MemorySpace::kHost}));
    ASC_DENSE_TEST_EQ(
        test,
        asc::QueryPosvWorkspace(provider, Triangle::kLower,
                                Take(asc::DenseBlasMatrixView<double>::Create(
                                    nullptr, 0, 0, Layout::kColumnMajor, 1,
                                    {nullptr, 0, asc::MemorySpace::kHost})),
                                huge)
            .status()
            .code(),
        asc::ErrorCode::kOverflow);
  }
}

void IntegerArithmetic(TestContext& test) {
  using asc::internal_cholesky_limits::CheckOrder;
  using asc::internal_cholesky_limits::CheckRightHandSides;
  for (const asc::extent_t maximum :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    ASC_DENSE_TEST_CHECK(test, CheckOrder(0, maximum, maximum, 64, true).ok());
    ASC_DENSE_TEST_CHECK(test,
                         CheckOrder(maximum - 64, 1, maximum, 64, false).ok());
    ASC_DENSE_TEST_EQ(test,
                      CheckOrder(maximum - 63, 1, maximum, 64, false).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test,
                         CheckOrder(maximum - 1, 1, maximum, 1, false).ok());
    ASC_DENSE_TEST_EQ(test, CheckOrder(maximum, 1, maximum, 1, false).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test,
                         CheckOrder(2, maximum / 2, maximum, 1, true).ok());
    ASC_DENSE_TEST_EQ(test,
                      CheckOrder(2, maximum / 2 + 1, maximum, 1, true).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(
        test, CheckOrder(2, maximum / 2 + 1, maximum, 1, false).ok());
    ASC_DENSE_TEST_CHECK(test, CheckRightHandSides(0, maximum, maximum).ok());
    ASC_DENSE_TEST_CHECK(test,
                         CheckRightHandSides(1, maximum - 1, maximum).ok());
    ASC_DENSE_TEST_EQ(test, CheckRightHandSides(1, maximum, maximum).code(),
                      asc::ErrorCode::kOverflow);
  }
  ASC_DENSE_TEST_EQ(test, CheckOrder(-1, 1, 128, 1, false).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, CheckOrder(1, 0, 128, 1, false).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, CheckOrder(1, 1, 128, 129, false).code(),
                    asc::ErrorCode::kInvalidArgument);
}

void CheckDefect(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 const asc::Status& status, const asc::LapackReport& report,
                 Fault fault) {
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnusable);
  if (fault == Fault::kNegative) {
    ASC_DENSE_TEST_EQ(test, report.native_info, -4);
    ASC_DENSE_TEST_EQ(test, report.native_argument, 4);
  } else if (fault == Fault::kMinimum) {
    ASC_DENSE_TEST_EQ(
        test, report.native_info,
        provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64
            ? static_cast<std::int64_t>(
                  std::numeric_limits<std::int32_t>::min())
            : std::numeric_limits<std::int64_t>::min());
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  } else {
    ASC_DENSE_TEST_EQ(test, report.native_info, 4);
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  }
}

void ProviderDefects(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : kLayouts) {
    for (const auto routine :
         {Routine::kPotrf, Routine::kPotrf2, Routine::kPotf2, Routine::kPotri,
          Routine::kPotrs, Routine::kPosv}) {
      for (const auto fault :
           {Fault::kNegative, Fault::kMinimum, Fault::kExcess}) {
        Matrix<double> matrix(3, 3, layout);
        Fill(matrix, 3, Triangle::kLower, 1);
        Scratch<double> scratch;
        asc::LapackReport report;
        if (routine == Routine::kPotrs || routine == Routine::kPotri) {
          const auto factor_plan = Take(asc::QueryPotrfWorkspace(
              provider, Triangle::kLower, matrix.view()));
          const auto factor_workspace = scratch.view(factor_plan);
          ASC_DENSE_TEST_CHECK(
              test, asc::Potrf(provider, Triangle::kLower, matrix.view(),
                               factor_plan, factor_workspace, report)
                        .ok());
        }
        std::optional<asc::LapackCholeskyFactorView<double>> factor;
        if (routine == Routine::kPotrs) {
          factor = Take(asc::LapackCholeskyFactorView<double>::Create(
              matrix.const_view(), Triangle::kLower, report));
        }
        Matrix<double> rhs(3, 2, layout);
        FillRhs(rhs, 3, 2, 1);
        const auto before = matrix.bytes();
        const auto rhs_before = rhs.bytes();
        const auto plan = Take([&]() -> asc::Result<asc::LapackWorkspacePlan> {
          if (routine == Routine::kPotrs) {
            return asc::QueryPotrsWorkspace(provider, *factor, rhs.view());
          }
          if (routine == Routine::kPosv) {
            return asc::QueryPosvWorkspace(provider, Triangle::kLower,
                                           matrix.view(), rhs.view());
          }
          return Query(provider, routine, Triangle::kLower, matrix.view());
        }());
        const auto workspace = scratch.view(plan);
        SetFault(routine, fault);
        const auto status = WithoutAllocation(test, [&] {
          if (routine == Routine::kPotrs) {
            return asc::Potrs(provider, *factor, rhs.view(), plan, workspace,
                              report);
          }
          if (routine == Routine::kPosv) {
            return asc::Posv(provider, Triangle::kLower, matrix.view(),
                             rhs.view(), plan, workspace, report);
          }
          return Execute(provider, routine, Triangle::kLower, matrix.view(),
                         plan, workspace, report);
        });
        SetFault(routine, Fault::kNone);
        CheckDefect(test, provider, status, report, fault);
        if (layout == Layout::kRowMajor) {
          matrix.CheckSame(test, before);
          rhs.CheckSame(test, rhs_before);
        }
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
  // The first foreign operation is inside the allocation observation scope.
  Numerical<float>(test, provider);
  Numerical<double>(test, provider);
  Numerical<std::complex<float>>(test, provider);
  Numerical<std::complex<double>>(test, provider);
  PositiveFailures<float>(test, provider);
  PositiveFailures<double>(test, provider);
  PositiveFailures<std::complex<float>>(test, provider);
  PositiveFailures<std::complex<double>>(test, provider);
  EmptyAndScalar<float>(test, provider);
  EmptyAndScalar<double>(test, provider);
  EmptyAndScalar<std::complex<float>>(test, provider);
  EmptyAndScalar<std::complex<double>>(test, provider);
  Defensive(test, provider);
  DefensiveDescriptors(test, provider);
  ProviderDimensions(test, provider);
  IntegerArithmetic(test);
  ProviderDefects(test, provider);
  std::cout << "Reference Cholesky: reconstructions=" << g_reconstructions
            << " solves=" << g_solves << " inverses=" << g_inverses
            << " drivers=" << g_drivers
            << " positive_factor_failures=" << g_positive_failures << '\n';
  return test.Finish();
}
