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
#include "asc/dense/lapack/triangular_band_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_triangular_band.h"
#include "internal_indefinite.h"
#include "internal_triangular.h"
#include "internal_triangular_band.h"
#include "internal_triangular_band_counts.h"

namespace asc {
namespace {
namespace common = internal_indefinite;
namespace band = internal_triangular_band;
namespace triangular = internal_triangular;
namespace counts = internal_triangular_band_counts;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kTbtrs = "stbtrs";
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int kd, lapack_int nrhs, const float* a,
                          lapack_int ldab, float* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_stbtrs(&uplo, &trans, &diag, &n, &kd, &nrhs, a, &ldab, b, &ldb,
                  &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kTbtrs = "dtbtrs";
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int kd, lapack_int nrhs, const double* a,
                          lapack_int ldab, double* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dtbtrs(&uplo, &trans, &diag, &n, &kd, &nrhs, a, &ldab, b, &ldb,
                  &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kTbtrs = "ctbtrs";
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int kd, lapack_int nrhs,
                          const std::complex<float>* a, lapack_int ldab,
                          std::complex<float>* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ctbtrs(&uplo, &trans, &diag, &n, &kd, &nrhs, a, &ldab, b, &ldb,
                  &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kTbtrs = "ztbtrs";
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int kd, lapack_int nrhs,
                          const std::complex<double>* a, lapack_int ldab,
                          std::complex<double>* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ztbtrs(&uplo, &trans, &diag, &n, &kd, &nrhs, a, &ldab, b, &ldb,
                  &info);
    return info;
  }
};

template <typename T>
Result<LapackWorkspacePlan> QuerySolve(const ReferenceLapackProvider& provider,
                                       DenseBlasDiagonal diagonal,
                                       DenseBlasTranspose operation,
                                       LapackTriangularBandView<const T> a,
                                       DenseBlasMatrixView<T> b) {
  if (!triangular::Diagonal(diagonal) || !triangular::Operation(operation)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (b.rows() != a.order()) {
    return Status(ErrorCode::kShape);
  }
  const std::array operands{a.storage().reachable_storage(),
                            b.reachable_storage()};
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
  const extent_t ldab = band::Leading(a);
  status =
      counts::Solve(a.order(), a.bandwidth(), b.columns(), ldab, ldb,
                    a.triangle(), diagonal, operation, common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      Native<T>::kTbtrs, Native<T>::kKind,
      std::array{a.order(), a.bandwidth(), b.columns(), ldab, ldb},
      std::array<std::int64_t, 7>{static_cast<std::int64_t>(a.triangle()),
                                  static_cast<std::int64_t>(diagonal),
                                  static_cast<std::int64_t>(operation),
                                  static_cast<std::int64_t>(a.layout()),
                                  static_cast<std::int64_t>(b.layout()),
                                  a.storage().leading_dimension(),
                                  b.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (b.columns() != 0 || diagonal == DenseBlasDiagonal::kNonUnit) {
    status = band::Packing(a, plan);
    if (!status.ok()) {
      return status;
    }
  }
  status = common::Packing(b, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan) : status;
}

template <typename T>
Status Solve(const ReferenceLapackProvider& provider,
             DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
             LapackTriangularBandView<const T> a, DenseBlasMatrixView<T> b,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const std::array operands{a.storage().reachable_storage(),
                            b.reachable_storage()};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kTbtrs, report);
  auto expected = QuerySolve(provider, diagonal, operation, a, b);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (a.order() == 0) {
    return common::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  const auto* ap = b.columns() == 0 && diagonal == DenseBlasDiagonal::kUnit
                       ? a.storage().data()
                       : band::Pack(a, diagonal, cursor, b.columns() == 0);
  auto* rhs = common::PackRhs(b, cursor);
  T dummy{};
  if (b.columns() == 0) {
    rhs = &dummy;
  }
  report.called_provider = true;
  const lapack_int info = Native<T>::Solve(
      common::Uplo(a.triangle()), triangular::Trans(operation),
      triangular::Diag(diagonal), static_cast<lapack_int>(a.order()),
      static_cast<lapack_int>(a.bandwidth()),
      static_cast<lapack_int>(b.columns()), ap,
      static_cast<lapack_int>(band::Leading(a)), rhs,
      static_cast<lapack_int>(b.columns() == 0 ? std::max<extent_t>(1, b.rows())
                                               : common::Leading(b)));
  status = triangular::Info(info, a.order(),
                            diagonal == DenseBlasDiagonal::kNonUnit, report);
  if (status.ok()) {
    common::PublishRhs(rhs, b);
  }
  return status;
}

}  // namespace

Result<LapackWorkspacePlan> QueryTbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
    DenseBlasTranspose operation, LapackTriangularBandView<const float> a,
    DenseBlasMatrixView<float> b) {
  return QuerySolve(provider, diagonal, operation, a, b);
}
Status Tbtrs(const ReferenceLapackProvider& provider,
             DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
             LapackTriangularBandView<const float> a,
             DenseBlasMatrixView<float> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, diagonal, operation, a, b, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryTbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
    DenseBlasTranspose operation, LapackTriangularBandView<const double> a,
    DenseBlasMatrixView<double> b) {
  return QuerySolve(provider, diagonal, operation, a, b);
}
Status Tbtrs(const ReferenceLapackProvider& provider,
             DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
             LapackTriangularBandView<const double> a,
             DenseBlasMatrixView<double> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, diagonal, operation, a, b, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryTbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
    DenseBlasTranspose operation,
    LapackTriangularBandView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b) {
  return QuerySolve(provider, diagonal, operation, a, b);
}
Status Tbtrs(const ReferenceLapackProvider& provider,
             DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
             LapackTriangularBandView<const std::complex<float>> a,
             DenseBlasMatrixView<std::complex<float>> b,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, diagonal, operation, a, b, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryTbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
    DenseBlasTranspose operation,
    LapackTriangularBandView<const std::complex<double>> a,
    DenseBlasMatrixView<std::complex<double>> b) {
  return QuerySolve(provider, diagonal, operation, a, b);
}
Status Tbtrs(const ReferenceLapackProvider& provider,
             DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
             LapackTriangularBandView<const std::complex<double>> a,
             DenseBlasMatrixView<std::complex<double>> b,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, diagonal, operation, a, b, plan, workspace, report);
}

}  // namespace asc
