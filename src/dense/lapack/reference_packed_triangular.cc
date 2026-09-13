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
#include "asc/dense/providers/lapack_triangular_packed.h"
#include "internal_indefinite.h"
#include "internal_packed_triangular.h"
#include "internal_packed_triangular_counts.h"
#include "internal_triangular.h"

namespace asc {
namespace {
namespace common = internal_indefinite;
namespace packed = internal_packed_triangular;
namespace triangular = internal_triangular;
namespace counts = internal_packed_triangular_counts;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kTptri = "stptri";
  static constexpr std::string_view kTptrs = "stptrs";
  static lapack_int Invert(char uplo, char diag, lapack_int n, float* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_stptri(&uplo, &diag, &n, a, &info);
    return info;
  }
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int nrhs, const float* a, float* b,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_stptrs(&uplo, &trans, &diag, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kTptri = "dtptri";
  static constexpr std::string_view kTptrs = "dtptrs";
  static lapack_int Invert(char uplo, char diag, lapack_int n, double* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dtptri(&uplo, &diag, &n, a, &info);
    return info;
  }
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int nrhs, const double* a, double* b,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dtptrs(&uplo, &trans, &diag, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kTptri = "ctptri";
  static constexpr std::string_view kTptrs = "ctptrs";
  static lapack_int Invert(char uplo, char diag, lapack_int n,
                           std::complex<float>* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ctptri(&uplo, &diag, &n, a, &info);
    return info;
  }
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int nrhs, const std::complex<float>* a,
                          std::complex<float>* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ctptrs(&uplo, &trans, &diag, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kTptri = "ztptri";
  static constexpr std::string_view kTptrs = "ztptrs";
  static lapack_int Invert(char uplo, char diag, lapack_int n,
                           std::complex<double>* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ztptri(&uplo, &diag, &n, a, &info);
    return info;
  }
  static lapack_int Solve(char uplo, char trans, char diag, lapack_int n,
                          lapack_int nrhs, const std::complex<double>* a,
                          std::complex<double>* b, lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ztptrs(&uplo, &trans, &diag, &n, &nrhs, a, b, &ldb, &info);
    return info;
  }
};

template <typename T>
Result<LapackWorkspacePlan> QueryInverse(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasPackedMatrixView<T> a) {
  if (!common::Triangle(triangle) || !triangular::Diagonal(diagonal)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  Status status = common::Accessible(provider, a.reachable_storage());
  if (!status.ok()) {
    return status;
  }
  status = counts::Inverse(a.order(), triangle, common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      Native<T>::kTptri, Native<T>::kKind, std::array{a.order()},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(diagonal),
                                  static_cast<std::int64_t>(a.layout())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  status = packed::Packing(a, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan) : status;
}

template <typename T>
Status Inverse(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
               DenseBlasPackedMatrixView<T> a, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{a.reachable_storage()};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kTptri, report);
  auto expected = QueryInverse(provider, triangle, diagonal, a);
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
  auto* ap = packed::Pack(a, triangle, diagonal, cursor);
  report.called_provider = true;
  const lapack_int info =
      Native<T>::Invert(common::Uplo(triangle), triangular::Diag(diagonal),
                        static_cast<lapack_int>(a.order()), ap);
  status = triangular::Info(info, a.order(),
                            diagonal == DenseBlasDiagonal::kNonUnit, report);
  if (status.ok()) {
    packed::Publish(ap, a, triangle, diagonal);
  }
  return status;
}

template <typename T>
Result<LapackWorkspacePlan> QuerySolve(const ReferenceLapackProvider& provider,
                                       DenseBlasTriangle triangle,
                                       DenseBlasDiagonal diagonal,
                                       DenseBlasTranspose operation,
                                       DenseBlasPackedMatrixView<const T> a,
                                       DenseBlasMatrixView<T> b) {
  if (!common::Triangle(triangle) || !triangular::Diagonal(diagonal) ||
      !triangular::Operation(operation)) {
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
  status = counts::Solve(a.order(), b.columns(), ldb, triangle, diagonal,
                         operation, common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      Native<T>::kTptrs, Native<T>::kKind,
      std::array{a.order(), b.columns(), ldb},
      std::array<std::int64_t, 6>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(diagonal),
                                  static_cast<std::int64_t>(operation),
                                  static_cast<std::int64_t>(a.layout()),
                                  static_cast<std::int64_t>(b.layout()),
                                  b.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (b.columns() != 0 || diagonal == DenseBlasDiagonal::kNonUnit) {
    status = packed::Packing(a, plan);
    if (!status.ok()) {
      return status;
    }
  }
  status = common::Packing(b, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan) : status;
}

template <typename T>
Status Solve(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation, DenseBlasPackedMatrixView<const T> a,
             DenseBlasMatrixView<T> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{a.reachable_storage(), b.reachable_storage()};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kTptrs, report);
  auto expected = QuerySolve(provider, triangle, diagonal, operation, a, b);
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
  const auto* ap =
      b.columns() == 0 && diagonal == DenseBlasDiagonal::kUnit
          ? a.data()
          : packed::Pack(a, triangle, diagonal, cursor, b.columns() == 0);
  auto* rhs = common::PackRhs(b, cursor);
  T dummy{};
  if (b.columns() == 0) {
    rhs = &dummy;
  }
  report.called_provider = true;
  const lapack_int info = Native<T>::Solve(
      common::Uplo(triangle), triangular::Trans(operation),
      triangular::Diag(diagonal), static_cast<lapack_int>(a.order()),
      static_cast<lapack_int>(b.columns()), ap, rhs,
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

Result<LapackWorkspacePlan> QueryTptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasPackedMatrixView<float> a) {
  return QueryInverse(provider, triangle, diagonal, a);
}
Status Tptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasPackedMatrixView<float> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryTptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasPackedMatrixView<const float> a, DenseBlasMatrixView<float> b) {
  return QuerySolve(provider, triangle, diagonal, operation, a, b);
}
Status Tptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation,
             DenseBlasPackedMatrixView<const float> a,
             DenseBlasMatrixView<float> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, triangle, diagonal, operation, a, b, plan, workspace,
               report);
}

Result<LapackWorkspacePlan> QueryTptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasPackedMatrixView<double> a) {
  return QueryInverse(provider, triangle, diagonal, a);
}
Status Tptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasPackedMatrixView<double> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryTptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasPackedMatrixView<const double> a, DenseBlasMatrixView<double> b) {
  return QuerySolve(provider, triangle, diagonal, operation, a, b);
}
Status Tptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation,
             DenseBlasPackedMatrixView<const double> a,
             DenseBlasMatrixView<double> b, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, triangle, diagonal, operation, a, b, plan, workspace,
               report);
}

Result<LapackWorkspacePlan> QueryTptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal,
    DenseBlasPackedMatrixView<std::complex<float>> a) {
  return QueryInverse(provider, triangle, diagonal, a);
}
Status Tptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasPackedMatrixView<std::complex<float>> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryTptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasPackedMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b) {
  return QuerySolve(provider, triangle, diagonal, operation, a, b);
}
Status Tptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation,
             DenseBlasPackedMatrixView<const std::complex<float>> a,
             DenseBlasMatrixView<std::complex<float>> b,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, triangle, diagonal, operation, a, b, plan, workspace,
               report);
}

Result<LapackWorkspacePlan> QueryTptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal,
    DenseBlasPackedMatrixView<std::complex<double>> a) {
  return QueryInverse(provider, triangle, diagonal, a);
}
Status Tptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasPackedMatrixView<std::complex<double>> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, diagonal, a, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryTptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasPackedMatrixView<const std::complex<double>> a,
    DenseBlasMatrixView<std::complex<double>> b) {
  return QuerySolve(provider, triangle, diagonal, operation, a, b);
}
Status Tptrs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation,
             DenseBlasPackedMatrixView<const std::complex<double>> a,
             DenseBlasMatrixView<std::complex<double>> b,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, triangle, diagonal, operation, a, b, plan, workspace,
               report);
}

}  // namespace asc
