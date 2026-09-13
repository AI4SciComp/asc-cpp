#include <algorithm>
#include <array>
#include <complex>
#include <cstdint>
#include <limits>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_solve.h"
#include "internal_indefinite.h"
#include "internal_packed_cholesky_solve_counts.h"
#include "internal_packed_triangular.h"

namespace asc {
namespace {
namespace common = internal_indefinite;
namespace packed = internal_packed_triangular;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kRoutine = "spptrs";
  static lapack_int Solve(char uplo, lapack_int n, lapack_int nrhs,
                          const float* a, float* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_spptrs(&uplo, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kRoutine = "dpptrs";
  static lapack_int Solve(char uplo, lapack_int n, lapack_int nrhs,
                          const double* a, double* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dpptrs(&uplo, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kRoutine = "cpptrs";
  static lapack_int Solve(char uplo, lapack_int n, lapack_int nrhs,
                          const std::complex<float>* a, std::complex<float>* b,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cpptrs(&uplo, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kRoutine = "zpptrs";
  static lapack_int Solve(char uplo, lapack_int n, lapack_int nrhs,
                          const std::complex<double>* a,
                          std::complex<double>* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zpptrs(&uplo, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasPackedMatrixView<const T> a,
                                  DenseBlasMatrixView<T> b) {
  if (!common::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (b.rows() != a.order()) {
    return Status(ErrorCode::kShape);
  }
  const std::array operands{a.reachable_storage(), b.reachable_storage()};
  for (const auto operand : operands) {
    Status status = common::Accessible(provider, operand);
    if (!status.ok()) {
      return status;
    }
  }
  Status status = common::Disjoint(operands);
  if (!status.ok()) {
    return status;
  }
  const extent_t ldb =
      b.columns() == 0 ? std::max<extent_t>(1, b.rows()) : common::Leading(b);
  status = internal_packed_cholesky_solve_counts::Solve(
      a.order(), b.columns(), ldb, common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      Native<T>::kRoutine, Native<T>::kKind,
      std::array{a.order(), b.columns(), ldb},
      std::array<std::int64_t, 4>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(a.layout()),
                                  static_cast<std::int64_t>(b.layout()),
                                  b.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (a.order() == 0 || b.columns() == 0) {
    return plan;
  }
  status = packed::Packing(a, plan);
  if (!status.ok()) {
    return status;
  }
  status = common::Packing(b, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan) : status;
}

template <typename T>
Status Solve(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasPackedMatrixView<const T> a,
             DenseBlasMatrixView<T> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{a.reachable_storage(), b.reachable_storage()};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kRoutine, report);
  auto expected = Query(provider, triangle, a, b);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (a.order() == 0 || b.columns() == 0) {
    return common::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  const auto* ap =
      packed::Pack(a, triangle, DenseBlasDiagonal::kNonUnit, cursor);
  auto* rhs = common::PackRhs(b, cursor);
  report.called_provider = true;
  const auto info = Native<T>::Solve(
      common::Uplo(triangle), static_cast<lapack_int>(a.order()),
      static_cast<lapack_int>(b.columns()), ap, rhs,
      static_cast<lapack_int>(common::Leading(b)));
  report.native_info = info;
  if (info != 0) {
    return common::Defect(info, report);
  }
  common::PublishRhs(rhs, b);
  return common::Complete(report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryPptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> a, DenseBlasMatrixView<float> b) {
  return Query(provider, triangle, a, b);
}
Status Pptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const float> a,
             DenseBlasMatrixView<float> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, triangle, a, b, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> a, DenseBlasMatrixView<double> b) {
  return Query(provider, triangle, a, b);
}
Status Pptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const double> a,
             DenseBlasMatrixView<double> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, triangle, a, b, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b) {
  return Query(provider, triangle, a, b);
}
Status Pptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<float>> a,
             DenseBlasMatrixView<std::complex<float>> b,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, triangle, a, b, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> a,
    DenseBlasMatrixView<std::complex<double>> b) {
  return Query(provider, triangle, a, b);
}
Status Pptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<double>> a,
             DenseBlasMatrixView<std::complex<double>> b,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, triangle, a, b, plan, workspace, report);
}

}  // namespace asc
