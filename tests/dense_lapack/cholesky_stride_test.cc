#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_dense_test::TestContext;

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}

template <typename T>
void EmptyStrides(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  constexpr asc::extent_t kWide =
      static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  for (const auto triangle :
       {asc::DenseBlasTriangle::kLower, asc::DenseBlasTriangle::kUpper}) {
    const auto matrix = Take(asc::DenseBlasMatrixView<T>::Create(
        nullptr, 0, 0, asc::DenseBlasLayout::kRowMajor, kWide,
        {nullptr, 0, asc::MemorySpace::kHost}));
    const auto rhs = Take(asc::DenseBlasMatrixView<T>::Create(
        nullptr, 0, 3, asc::DenseBlasLayout::kRowMajor, kWide + 1,
        {nullptr, 0, asc::MemorySpace::kHost}));
    const std::array plans{
        asc::QueryPotrfWorkspace(provider, triangle, matrix),
        asc::QueryPotrf2Workspace(provider, triangle, matrix),
        asc::QueryPotf2Workspace(provider, triangle, matrix),
        asc::QueryPotriWorkspace(provider, triangle, matrix),
        asc::QueryPosvWorkspace(provider, triangle, matrix, rhs)};
    for (const auto& plan : plans) {
      ASC_DENSE_TEST_CHECK(test, plan.ok());
    }
    if (!plans[0].ok()) {
      continue;
    }
    asc::LapackWorkspace workspace;
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(test, asc::Potrf(provider, triangle, matrix, *plans[0],
                                          workspace, report)
                                   .ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
        asc::DenseBlasMatrixView<const T>(matrix), triangle, report));
    const auto solve = asc::QueryPotrsWorkspace(provider, factor, rhs);
    ASC_DENSE_TEST_CHECK(test, solve.ok());
    if (solve.ok()) {
      ASC_DENSE_TEST_CHECK(
          test,
          asc::Potrs(provider, factor, rhs, *solve, workspace, report).ok());
      ASC_DENSE_TEST_CHECK(test,
                           !report.called_provider && !report.native_info);
    }
    const auto other_stride = Take(asc::DenseBlasMatrixView<T>::Create(
        nullptr, 0, 0, asc::DenseBlasLayout::kRowMajor, kWide + 2,
        {nullptr, 0, asc::MemorySpace::kHost}));
    ASC_DENSE_TEST_EQ(test,
                      asc::Potrf(provider, triangle, other_stride, *plans[0],
                                 workspace, report)
                          .code(),
                      asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  }
}

template <typename T>
void SingletonStrides(TestContext& test,
                      const asc::ReferenceLapackProvider& provider) {
  constexpr auto kWide = std::numeric_limits<asc::extent_t>::max();
  std::array<T, 1> values{T{4}};
  std::array<T, 1> rhs_values{T{8}};
  const auto matrix = Take(asc::DenseBlasMatrixView<T>::Create(
      values.data(), 1, 1, asc::DenseBlasLayout::kRowMajor, kWide,
      {values.data(), sizeof(values), asc::MemorySpace::kHost}));
  const auto rhs = Take(asc::DenseBlasMatrixView<T>::Create(
      rhs_values.data(), 1, 1, asc::DenseBlasLayout::kRowMajor, kWide - 1,
      {rhs_values.data(), sizeof(rhs_values), asc::MemorySpace::kHost}));
  std::array<T, 2> packing{};
  asc::LapackWorkspace workspace;
  workspace.regions[static_cast<std::size_t>(
      asc::LapackWorkspaceKind::kLayoutConversion)] = {
      packing.data(), sizeof(packing), asc::MemorySpace::kHost};
  asc::LapackReport report;
  const auto triangle = asc::DenseBlasTriangle::kLower;
  const auto plan = asc::QueryPotrfWorkspace(provider, triangle, matrix);
  ASC_DENSE_TEST_CHECK(test, plan.ok());
  if (!plan.ok()) {
    return;
  }
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Potrf(provider, triangle, matrix, *plan, workspace, report).ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test, values[0], T{2});
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      asc::DenseBlasMatrixView<const T>(matrix), triangle, report));
  const auto solve = Take(asc::QueryPotrsWorkspace(provider, factor, rhs));
  ASC_DENSE_TEST_CHECK(
      test, asc::Potrs(provider, factor, rhs, solve, workspace, report).ok());
  ASC_DENSE_TEST_EQ(test, rhs_values[0], T{2});
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  EmptyStrides<float>(test, provider);
  EmptyStrides<double>(test, provider);
  EmptyStrides<std::complex<float>>(test, provider);
  EmptyStrides<std::complex<double>>(test, provider);
  SingletonStrides<float>(test, provider);
  SingletonStrides<double>(test, provider);
  SingletonStrides<std::complex<float>>(test, provider);
  SingletonStrides<std::complex<double>>(test, provider);
  return test.Finish();
}
