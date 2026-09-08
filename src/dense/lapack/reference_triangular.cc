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
#include "asc/dense/providers/lapack_triangular.h"
#include "internal_indefinite.h"
#include "internal_triangular.h"
#include "internal_triangular_counts.h"
#include "internal_triangular_prototypes.h"

namespace asc {
namespace {
namespace common = internal_indefinite;
namespace triangular = internal_triangular;
namespace counts = internal_triangular_counts;

template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kTrtri = "strtri";
  static constexpr std::string_view kTrti2 = "strti2";
  static constexpr std::string_view kTrtrs = "strtrs";
  static lapack_int Invert(bool blocked, char uplo, char diag, lapack_int n,
                           float* a, lapack_int lda) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    if (blocked) {
      LAPACK_strtri(&uplo, &diag, &n, a, &lda, &info);
    } else {
      LAPACK_strti2_base(&uplo, &diag, &n, a, &lda, &info, 1, 1);
    }
    return info;
  }
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int nrhs, const float* a, lapack_int lda,
                          float* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_strtrs(&uplo, &trans, &diag, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
};
template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kTrtri = "dtrtri";
  static constexpr std::string_view kTrti2 = "dtrti2";
  static constexpr std::string_view kTrtrs = "dtrtrs";
  static lapack_int Invert(bool blocked, char uplo, char diag, lapack_int n,
                           double* a, lapack_int lda) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    if (blocked) {
      LAPACK_dtrtri(&uplo, &diag, &n, a, &lda, &info);
    } else {
      LAPACK_dtrti2_base(&uplo, &diag, &n, a, &lda, &info, 1, 1);
    }
    return info;
  }
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int nrhs, const double* a, lapack_int lda,
                          double* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dtrtrs(&uplo, &trans, &diag, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kTrtri = "ctrtri";
  static constexpr std::string_view kTrti2 = "ctrti2";
  static constexpr std::string_view kTrtrs = "ctrtrs";
  static lapack_int Invert(bool blocked, char uplo, char diag, lapack_int n,
                           std::complex<float>* a, lapack_int lda) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    if (blocked) {
      LAPACK_ctrtri(&uplo, &diag, &n, a, &lda, &info);
    } else {
      LAPACK_ctrti2_base(&uplo, &diag, &n, a, &lda, &info, 1, 1);
    }
    return info;
  }
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int nrhs, const std::complex<float>* a,
                          lapack_int lda, std::complex<float>* b,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ctrtrs(&uplo, &trans, &diag, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kTrtri = "ztrtri";
  static constexpr std::string_view kTrti2 = "ztrti2";
  static constexpr std::string_view kTrtrs = "ztrtrs";
  static lapack_int Invert(bool blocked, char uplo, char diag, lapack_int n,
                           std::complex<double>* a, lapack_int lda) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    if (blocked) {
      LAPACK_ztrtri(&uplo, &diag, &n, a, &lda, &info);
    } else {
      LAPACK_ztrti2_base(&uplo, &diag, &n, a, &lda, &info, 1, 1);
    }
    return info;
  }
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int nrhs, const std::complex<double>* a,
                          lapack_int lda, std::complex<double>* b,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ztrtrs(&uplo, &trans, &diag, &n, &nrhs, a, &lda, b, &ldb, &info);
    return info;
  }
};

template <typename T>
Result<LapackWorkspacePlan> QueryInverse(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<T> a, bool blocked) {
  if (!common::Triangle(triangle) || !triangular::Diagonal(diagonal)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (a.rows() != a.columns()) {
    return Status(ErrorCode::kShape);
  }
  Status status = common::Accessible(provider, a.reachable_storage());
  if (!status.ok()) {
    return status;
  }
  status = counts::Inverse(a.rows(), common::Leading(a), triangle, blocked,
                           common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      blocked ? Native<T>::kTrtri : Native<T>::kTrti2, Native<T>::kKind,
      std::array{a.rows(), common::Leading(a)},
      std::array<std::int64_t, 4>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(diagonal),
                                  static_cast<std::int64_t>(a.layout()),
                                  a.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  status = common::Packing(a, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan) : status;
}

template <typename T>
Status Inverse(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
               DenseBlasMatrixView<T> a, bool blocked,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{a.reachable_storage()};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, blocked ? Native<T>::kTrtri : Native<T>::kTrti2,
                report);
  auto expected = QueryInverse(provider, triangle, diagonal, a, blocked);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (a.rows() == 0) {
    return common::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  auto* packed = triangular::Pack(a, triangle, diagonal, cursor);
  report.called_provider = true;
  const lapack_int info = Native<T>::Invert(
      blocked, common::Uplo(triangle), triangular::Diag(diagonal),
      static_cast<lapack_int>(a.rows()), packed,
      static_cast<lapack_int>(common::Leading(a)));
  status = triangular::Info(info, a.rows(),
                            blocked && diagonal == DenseBlasDiagonal::kNonUnit,
                            report);
  if (status.ok()) {
    triangular::Publish(packed, a, triangle, diagonal);
  }
  return status;
}

template <typename T>
Result<LapackWorkspacePlan> QuerySolve(const ReferenceLapackProvider& provider,
                                       DenseBlasTriangle triangle,
                                       DenseBlasDiagonal diagonal,
                                       DenseBlasTranspose operation,
                                       DenseBlasMatrixView<const T> a,
                                       DenseBlasMatrixView<T> b) {
  if (!common::Triangle(triangle) || !triangular::Diagonal(diagonal) ||
      !triangular::Operation(operation)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (a.rows() != a.columns() || b.rows() != a.rows()) {
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
  const extent_t lda = triangular::SolveLeading(a, b.columns(), diagonal);
  const extent_t ldb =
      b.columns() == 0 ? std::max<extent_t>(1, b.rows()) : common::Leading(b);
  status = counts::Solve(a.rows(), b.columns(), lda, ldb, triangle, diagonal,
                         operation, common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      Native<T>::kTrtrs, Native<T>::kKind,
      std::array{a.rows(), b.columns(), lda, ldb},
      std::array<std::int64_t, 7>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(diagonal),
                                  static_cast<std::int64_t>(operation),
                                  static_cast<std::int64_t>(a.layout()),
                                  static_cast<std::int64_t>(b.layout()),
                                  a.leading_dimension(), b.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (b.columns() != 0) {
    status = common::Packing(a, plan);
    if (!status.ok()) {
      return status;
    }
    status = common::Packing(b, plan);
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}

template <typename T>
Status Solve(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation, DenseBlasMatrixView<const T> a,
             DenseBlasMatrixView<T> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{a.reachable_storage(), b.reachable_storage()};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kTrtrs, report);
  auto expected = QuerySolve(provider, triangle, diagonal, operation, a, b);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (a.rows() == 0) {
    return common::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  const auto* packed_a = b.columns() == 0
                             ? a.data()
                             : triangular::Pack(a, triangle, diagonal, cursor);
  auto* packed_b = common::PackRhs(b, cursor);
  T dummy{};
  if (b.columns() == 0) {
    packed_b = &dummy;
  }
  report.called_provider = true;
  const lapack_int info = Native<T>::Solve(
      common::Uplo(triangle), triangular::Trans(operation),
      triangular::Diag(diagonal), static_cast<lapack_int>(a.rows()),
      static_cast<lapack_int>(b.columns()), packed_a,
      static_cast<lapack_int>(
          triangular::SolveLeading(a, b.columns(), diagonal)),
      packed_b,
      static_cast<lapack_int>(b.columns() == 0 ? std::max<extent_t>(1, b.rows())
                                               : common::Leading(b)));
  status = triangular::Info(info, a.rows(),
                            diagonal == DenseBlasDiagonal::kNonUnit, report);
  if (status.ok()) {
    common::PublishRhs(packed_b, b);
  }
  return status;
}

}  // namespace
Result<LapackWorkspacePlan> QueryTrtriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<float> a) {
  return QueryInverse(provider, triangle, diagonal, a, true);
}
Status Trtri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<float> a, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, true, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryTrti2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<float> a) {
  return QueryInverse(provider, triangle, diagonal, a, false);
}
Status Trti2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<float> a, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, false, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryTrtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const float> a, DenseBlasMatrixView<float> b) {
  return QuerySolve(provider, triangle, diagonal, operation, a, b);
}
Status Trtrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation, DenseBlasMatrixView<const float> a,
             DenseBlasMatrixView<float> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, triangle, diagonal, operation, a, b, plan, workspace,
               report);
}
Result<LapackWorkspacePlan> QueryTrtriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<double> a) {
  return QueryInverse(provider, triangle, diagonal, a, true);
}
Status Trtri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<double> a, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, true, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryTrti2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<double> a) {
  return QueryInverse(provider, triangle, diagonal, a, false);
}
Status Trti2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<double> a, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, false, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryTrtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const double> a, DenseBlasMatrixView<double> b) {
  return QuerySolve(provider, triangle, diagonal, operation, a, b);
}
Status Trtrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation, DenseBlasMatrixView<const double> a,
             DenseBlasMatrixView<double> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, triangle, diagonal, operation, a, b, plan, workspace,
               report);
}
Result<LapackWorkspacePlan> QueryTrtriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<std::complex<float>> a) {
  return QueryInverse(provider, triangle, diagonal, a, true);
}
Status Trtri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<std::complex<float>> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, true, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryTrti2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<std::complex<float>> a) {
  return QueryInverse(provider, triangle, diagonal, a, false);
}
Status Trti2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<std::complex<float>> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, false, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryTrtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b) {
  return QuerySolve(provider, triangle, diagonal, operation, a, b);
}
Status Trtrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation,
             DenseBlasMatrixView<const std::complex<float>> a,
             DenseBlasMatrixView<std::complex<float>> b,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, triangle, diagonal, operation, a, b, plan, workspace,
               report);
}
Result<LapackWorkspacePlan> QueryTrtriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<std::complex<double>> a) {
  return QueryInverse(provider, triangle, diagonal, a, true);
}
Status Trtri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<std::complex<double>> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, true, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryTrti2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<std::complex<double>> a) {
  return QueryInverse(provider, triangle, diagonal, a, false);
}
Status Trti2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<std::complex<double>> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, false, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryTrtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const std::complex<double>> a,
    DenseBlasMatrixView<std::complex<double>> b) {
  return QuerySolve(provider, triangle, diagonal, operation, a, b);
}
Status Trtrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation,
             DenseBlasMatrixView<const std::complex<double>> a,
             DenseBlasMatrixView<std::complex<double>> b,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, triangle, diagonal, operation, a, b, plan, workspace,
               report);
}

}  // namespace asc
