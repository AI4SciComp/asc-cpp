#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; caller-owned native integer lifetimes.
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_triangular_condition.h"
#include "asc/dense/providers/lapack_triangular_error_bounds.h"
#include "internal_indefinite.h"
#include "internal_triangular.h"
#include "internal_triangular_expert_counts.h"

namespace asc {
namespace {
namespace common = internal_indefinite;
namespace triangular = internal_triangular;
namespace counts = internal_triangular_expert_counts;
constexpr std::size_t kReal =
    static_cast<std::size_t>(LapackWorkspaceKind::kReal);

template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kTrcon = "strcon";
  static constexpr std::string_view kTrrfs = "strrfs";
  static lapack_int Condition(char norm, char uplo, char diag, lapack_int n,
                              const float* a, lapack_int lda, float& rcond,
                              const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_strcon(
        &norm, &uplo, &diag, &n, a, &lda, &rcond,
        static_cast<float*>(workspace.regions[common::kScalar].data()),
        static_cast<lapack_int*>(workspace.regions[common::kPivot].data()),
        &info);
    return info;
  }
  static lapack_int Errors(char uplo, char trans, char diag, lapack_int n,
                           lapack_int nrhs, const float* a, lapack_int lda,
                           const float* b, lapack_int ldb, const float* x,
                           lapack_int ldx, float* ferr, float* berr,
                           const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_strrfs(
        &uplo, &trans, &diag, &n, &nrhs, a, &lda, b, &ldb, x, &ldx, ferr, berr,
        static_cast<float*>(workspace.regions[common::kScalar].data()),
        static_cast<lapack_int*>(workspace.regions[common::kPivot].data()),
        &info);
    return info;
  }
};
template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kTrcon = "dtrcon";
  static constexpr std::string_view kTrrfs = "dtrrfs";
  static lapack_int Condition(char norm, char uplo, char diag, lapack_int n,
                              const double* a, lapack_int lda, double& rcond,
                              const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dtrcon(
        &norm, &uplo, &diag, &n, a, &lda, &rcond,
        static_cast<double*>(workspace.regions[common::kScalar].data()),
        static_cast<lapack_int*>(workspace.regions[common::kPivot].data()),
        &info);
    return info;
  }
  static lapack_int Errors(char uplo, char trans, char diag, lapack_int n,
                           lapack_int nrhs, const double* a, lapack_int lda,
                           const double* b, lapack_int ldb, const double* x,
                           lapack_int ldx, double* ferr, double* berr,
                           const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dtrrfs(
        &uplo, &trans, &diag, &n, &nrhs, a, &lda, b, &ldb, x, &ldx, ferr, berr,
        static_cast<double*>(workspace.regions[common::kScalar].data()),
        static_cast<lapack_int*>(workspace.regions[common::kPivot].data()),
        &info);
    return info;
  }
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kTrcon = "ctrcon";
  static constexpr std::string_view kTrrfs = "ctrrfs";
  static lapack_int Condition(char norm, char uplo, char diag, lapack_int n,
                              const std::complex<float>* a, lapack_int lda,
                              float& rcond, const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ctrcon(&norm, &uplo, &diag, &n, a, &lda, &rcond,
                  static_cast<std::complex<float>*>(
                      workspace.regions[common::kScalar].data()),
                  static_cast<float*>(workspace.regions[kReal].data()), &info);
    return info;
  }
  static lapack_int Errors(char uplo, char trans, char diag, lapack_int n,
                           lapack_int nrhs, const std::complex<float>* a,
                           lapack_int lda, const std::complex<float>* b,
                           lapack_int ldb, const std::complex<float>* x,
                           lapack_int ldx, float* ferr, float* berr,
                           const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ctrrfs(&uplo, &trans, &diag, &n, &nrhs, a, &lda, b, &ldb, x, &ldx,
                  ferr, berr,
                  static_cast<std::complex<float>*>(
                      workspace.regions[common::kScalar].data()),
                  static_cast<float*>(workspace.regions[kReal].data()), &info);
    return info;
  }
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kTrcon = "ztrcon";
  static constexpr std::string_view kTrrfs = "ztrrfs";
  static lapack_int Condition(char norm, char uplo, char diag, lapack_int n,
                              const std::complex<double>* a, lapack_int lda,
                              double& rcond, const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ztrcon(&norm, &uplo, &diag, &n, a, &lda, &rcond,
                  static_cast<std::complex<double>*>(
                      workspace.regions[common::kScalar].data()),
                  static_cast<double*>(workspace.regions[kReal].data()), &info);
    return info;
  }
  static lapack_int Errors(char uplo, char trans, char diag, lapack_int n,
                           lapack_int nrhs, const std::complex<double>* a,
                           lapack_int lda, const std::complex<double>* b,
                           lapack_int ldb, const std::complex<double>* x,
                           lapack_int ldx, double* ferr, double* berr,
                           const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_ztrrfs(&uplo, &trans, &diag, &n, &nrhs, a, &lda, b, &ldb, x, &ldx,
                  ferr, berr,
                  static_cast<std::complex<double>*>(
                      workspace.regions[common::kScalar].data()),
                  static_cast<double*>(workspace.regions[kReal].data()), &info);
    return info;
  }
};
template <typename T>
Status Work(extent_t n, LapackWorkspacePlan& plan) {
  const extent_t scalar = (DenseBlasComplex<T> ? 2 : 3) * n;
  plan.regions[common::kScalar] = {scalar, scalar, sizeof(T), alignof(T)};
  if constexpr (DenseBlasComplex<T>) {
    using Real = DenseBlasRealType<T>;
    plan.regions[kReal] = {n, n, sizeof(Real), alignof(Real)};
  } else {
    plan.regions[common::kPivot] = {n, n, sizeof(lapack_int),
                                    alignof(lapack_int)};
  }
  return common::Total(plan);
}

template <typename T>
void StartIntegers(extent_t n, const LapackWorkspace& workspace) {
  if constexpr (!DenseBlasComplex<T>) {
    ::new (workspace.regions[common::kPivot].data())
        lapack_int[static_cast<std::size_t>(n)];
  }
}

template <typename T>
Result<LapackWorkspacePlan> QueryCondition(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
    DenseBlasMatrixView<const T> a, const DenseBlasRealType<T>& rcond) {
  if ((norm != LapackConditionNorm::kOne &&
       norm != LapackConditionNorm::kInfinity) ||
      !common::Triangle(triangle) || !triangular::Diagonal(diagonal)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (a.rows() != a.columns()) {
    return Status(ErrorCode::kShape);
  }
  const std::array operands{a.reachable_storage(), common::Object(rcond)};
  for (const auto operand : operands) {
    Status access = common::Accessible(provider, operand);
    if (!access.ok()) {
      return access;
    }
  }
  Status status = common::Disjoint(operands);
  if (!status.ok()) {
    return status;
  }
  const extent_t n = a.rows();
  const extent_t lda = common::Leading(a);
  status = counts::Active(n, 1, lda, std::max<extent_t>(1, n),
                          std::max<extent_t>(1, n), common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      Native<T>::kTrcon, Native<T>::kKind, std::array{n, lda},
      std::array<std::int64_t, 5>{
          static_cast<std::int64_t>(norm), static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(diagonal),
          static_cast<std::int64_t>(a.layout()), a.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  status = Work<T>(n, plan);
  if (!status.ok()) {
    return status;
  }
  status = common::Packing(a, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan) : status;
}

template <typename T>
Status Condition(const ReferenceLapackProvider& provider,
                 LapackConditionNorm norm, DenseBlasTriangle triangle,
                 DenseBlasDiagonal diagonal, DenseBlasMatrixView<const T> a,
                 DenseBlasRealType<T>& rcond, const LapackWorkspacePlan& plan,
                 const LapackWorkspace& workspace, LapackReport& report) {
  using Real = DenseBlasRealType<T>;
  const std::array operands{a.reachable_storage(), common::Object(rcond)};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kTrcon, report);
  auto expected = QueryCondition(provider, norm, triangle, diagonal, a, rcond);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (a.rows() == 0) {
    rcond = Real{1};
    return common::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  const T* packed = triangular::Pack(a, triangle, diagonal, cursor);
  StartIntegers<T>(a.rows(), workspace);
  rcond = std::numeric_limits<Real>::quiet_NaN();
  report.called_provider = true;
  const lapack_int info = Native<T>::Condition(
      norm == LapackConditionNorm::kOne ? '1' : 'I', common::Uplo(triangle),
      triangular::Diag(diagonal), static_cast<lapack_int>(a.rows()), packed,
      static_cast<lapack_int>(common::Leading(a)), rcond, workspace);
  report.native_info = info;
  if (info != 0 || rcond < Real{0}) {
    return common::Defect(info, report);
  }
  if (!std::isfinite(rcond)) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return common::Complete(report);
}

template <typename Real>
Status ErrorVector(const ReferenceLapackProvider& provider,
                   DenseBlasVectorView<Real> values, extent_t size) {
  if (values.size() != size) {
    return Status(ErrorCode::kShape);
  }
  if (values.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return common::Accessible(provider, values.reachable_storage());
}

template <typename T>
Result<LapackWorkspacePlan> QueryErrors(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const T> a, DenseBlasMatrixView<const T> b,
    DenseBlasMatrixView<const T> x,
    DenseBlasVectorView<DenseBlasRealType<T>> ferr,
    DenseBlasVectorView<DenseBlasRealType<T>> berr) {
  if (!common::Triangle(triangle) || !triangular::Diagonal(diagonal) ||
      !triangular::Operation(operation)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (a.rows() != a.columns() || b.rows() != a.rows() || x.rows() != a.rows() ||
      x.columns() != b.columns()) {
    return Status(ErrorCode::kShape);
  }
  const std::array operands{a.reachable_storage(), b.reachable_storage(),
                            x.reachable_storage(), ferr.reachable_storage(),
                            berr.reachable_storage()};
  for (const auto operand : operands) {
    Status access = common::Accessible(provider, operand);
    if (!access.ok()) {
      return access;
    }
  }
  for (const auto vector : {ferr, berr}) {
    Status valid = ErrorVector(provider, vector, b.columns());
    if (!valid.ok()) {
      return valid;
    }
  }
  Status status = common::Disjoint(operands);
  if (!status.ok()) {
    return status;
  }
  const bool active = a.rows() != 0 && b.columns() != 0;
  const extent_t lda = active ? common::Leading(a) : 1;
  const extent_t ldb = active ? common::Leading(b) : 1;
  const extent_t ldx = active ? common::Leading(x) : 1;
  status = counts::Active(a.rows(), b.columns(), lda, ldb, ldx,
                          common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      Native<T>::kTrrfs, Native<T>::kKind,
      std::array{a.rows(), b.columns(), lda, ldb, ldx},
      std::array<std::int64_t, 9>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(diagonal),
                                  static_cast<std::int64_t>(operation),
                                  static_cast<std::int64_t>(a.layout()),
                                  static_cast<std::int64_t>(b.layout()),
                                  static_cast<std::int64_t>(x.layout()),
                                  a.leading_dimension(), b.leading_dimension(),
                                  x.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (!active) {
    return plan;
  }
  status = Work<T>(a.rows(), plan);
  if (!status.ok()) {
    return status;
  }
  for (const auto& matrix : {a, b, x}) {
    status = common::Packing(matrix, plan);
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}

template <typename T>
const T* PackInput(DenseBlasMatrixView<const T> matrix, T*& cursor) {
  if (!common::Packed(matrix)) {
    return matrix.data();
  }
  T* packed = cursor;
  cursor += matrix.rows() * matrix.columns();
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    for (extent_t i = 0; i < matrix.rows(); ++i) {
      packed[j * matrix.rows() + i] = common::Entry(matrix, i, j);
    }
  }
  return packed;
}

template <typename T>
Status Errors(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
              DenseBlasTranspose operation, DenseBlasMatrixView<const T> a,
              DenseBlasMatrixView<const T> b, DenseBlasMatrixView<const T> x,
              DenseBlasVectorView<DenseBlasRealType<T>> ferr,
              DenseBlasVectorView<DenseBlasRealType<T>> berr,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  using Real = DenseBlasRealType<T>;
  const std::array operands{a.reachable_storage(), b.reachable_storage(),
                            x.reachable_storage(), ferr.reachable_storage(),
                            berr.reachable_storage()};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kTrrfs, report);
  auto expected =
      QueryErrors(provider, triangle, diagonal, operation, a, b, x, ferr, berr);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (a.rows() == 0 || b.columns() == 0) {
    for (extent_t j = 0; j < b.columns(); ++j) {
      ferr.data()[j] = Real{0};
      berr.data()[j] = Real{0};
    }
    return common::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  const T* packed_a = triangular::Pack(a, triangle, diagonal, cursor);
  const T* packed_b = PackInput(b, cursor);
  const T* packed_x = PackInput(x, cursor);
  StartIntegers<T>(a.rows(), workspace);
  for (extent_t j = 0; j < b.columns(); ++j) {
    ferr.data()[j] = std::numeric_limits<Real>::quiet_NaN();
    berr.data()[j] = std::numeric_limits<Real>::quiet_NaN();
  }
  report.called_provider = true;
  const lapack_int info = Native<T>::Errors(
      common::Uplo(triangle), triangular::Trans(operation),
      triangular::Diag(diagonal), static_cast<lapack_int>(a.rows()),
      static_cast<lapack_int>(b.columns()), packed_a,
      static_cast<lapack_int>(common::Leading(a)), packed_b,
      static_cast<lapack_int>(common::Leading(b)), packed_x,
      static_cast<lapack_int>(common::Leading(x)), ferr.data(), berr.data(),
      workspace);
  report.native_info = info;
  if (info != 0) {
    return common::Defect(info, report);
  }
  bool nonfinite = false;
  for (extent_t j = 0; j < b.columns(); ++j) {
    if (ferr.data()[j] < Real{0} || berr.data()[j] < Real{0}) {
      report.diagnostic_index = j;
      return common::Defect(info, report);
    }
    if (!std::isfinite(ferr.data()[j]) || !std::isfinite(berr.data()[j])) {
      if (!nonfinite) {
        report.diagnostic_index = j;
      }
      nonfinite = true;
    }
  }
  if (nonfinite) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return common::Complete(report);
}

}  // namespace

Result<LapackWorkspacePlan> QueryTrconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
    DenseBlasMatrixView<const float> a, const float& rcond) {
  return QueryCondition(provider, norm, triangle, diagonal, a, rcond);
}
Status Trcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<const float> a, float& rcond,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Condition(provider, norm, triangle, diagonal, a, rcond, plan,
                   workspace, report);
}
Result<LapackWorkspacePlan> QueryTrrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const float> a, DenseBlasMatrixView<const float> b,
    DenseBlasMatrixView<const float> x, DenseBlasVectorView<float> ferr,
    DenseBlasVectorView<float> berr) {
  return QueryErrors(provider, triangle, diagonal, operation, a, b, x, ferr,
                     berr);
}
Status Trrfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation, DenseBlasMatrixView<const float> a,
             DenseBlasMatrixView<const float> b,
             DenseBlasMatrixView<const float> x,
             DenseBlasVectorView<float> ferr, DenseBlasVectorView<float> berr,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Errors(provider, triangle, diagonal, operation, a, b, x, ferr, berr,
                plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryTrconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
    DenseBlasMatrixView<const double> a, const double& rcond) {
  return QueryCondition(provider, norm, triangle, diagonal, a, rcond);
}
Status Trcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<const double> a, double& rcond,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Condition(provider, norm, triangle, diagonal, a, rcond, plan,
                   workspace, report);
}
Result<LapackWorkspacePlan> QueryTrrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const double> a, DenseBlasMatrixView<const double> b,
    DenseBlasMatrixView<const double> x, DenseBlasVectorView<double> ferr,
    DenseBlasVectorView<double> berr) {
  return QueryErrors(provider, triangle, diagonal, operation, a, b, x, ferr,
                     berr);
}
Status Trrfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation, DenseBlasMatrixView<const double> a,
             DenseBlasMatrixView<const double> b,
             DenseBlasMatrixView<const double> x,
             DenseBlasVectorView<double> ferr, DenseBlasVectorView<double> berr,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Errors(provider, triangle, diagonal, operation, a, b, x, ferr, berr,
                plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryTrconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
    DenseBlasMatrixView<const std::complex<float>> a, const float& rcond) {
  return QueryCondition(provider, norm, triangle, diagonal, a, rcond);
}
Status Trcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<const std::complex<float>> a, float& rcond,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Condition(provider, norm, triangle, diagonal, a, rcond, plan,
                   workspace, report);
}
Result<LapackWorkspacePlan> QueryTrrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<const std::complex<float>> b,
    DenseBlasMatrixView<const std::complex<float>> x,
    DenseBlasVectorView<float> ferr, DenseBlasVectorView<float> berr) {
  return QueryErrors(provider, triangle, diagonal, operation, a, b, x, ferr,
                     berr);
}
Status Trrfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation,
             DenseBlasMatrixView<const std::complex<float>> a,
             DenseBlasMatrixView<const std::complex<float>> b,
             DenseBlasMatrixView<const std::complex<float>> x,
             DenseBlasVectorView<float> ferr, DenseBlasVectorView<float> berr,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Errors(provider, triangle, diagonal, operation, a, b, x, ferr, berr,
                plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryTrconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
    DenseBlasMatrixView<const std::complex<double>> a, const double& rcond) {
  return QueryCondition(provider, norm, triangle, diagonal, a, rcond);
}
Status Trcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasMatrixView<const std::complex<double>> a, double& rcond,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Condition(provider, norm, triangle, diagonal, a, rcond, plan,
                   workspace, report);
}
Result<LapackWorkspacePlan> QueryTrrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const std::complex<double>> a,
    DenseBlasMatrixView<const std::complex<double>> b,
    DenseBlasMatrixView<const std::complex<double>> x,
    DenseBlasVectorView<double> ferr, DenseBlasVectorView<double> berr) {
  return QueryErrors(provider, triangle, diagonal, operation, a, b, x, ferr,
                     berr);
}
Status Trrfs(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
             DenseBlasTranspose operation,
             DenseBlasMatrixView<const std::complex<double>> a,
             DenseBlasMatrixView<const std::complex<double>> b,
             DenseBlasMatrixView<const std::complex<double>> x,
             DenseBlasVectorView<double> ferr, DenseBlasVectorView<double> berr,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Errors(provider, triangle, diagonal, operation, a, b, x, ferr, berr,
                plan, workspace, report);
}

}  // namespace asc
