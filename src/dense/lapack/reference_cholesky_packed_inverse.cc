#include <array>
#include <complex>
#include <cstdint>
#include <limits>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_inverse.h"
#include "internal_indefinite.h"
#include "internal_packed_cholesky_inverse_counts.h"
#include "internal_packed_triangular.h"
#include "internal_triangular.h"

namespace asc {
namespace {
namespace common = internal_indefinite;
namespace packed = internal_packed_triangular;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kRoutine = "spptri";
  static lapack_int Inverse(char uplo, lapack_int n, float* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_spptri(&uplo, &n, a, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kRoutine = "dpptri";
  static lapack_int Inverse(char uplo, lapack_int n, double* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dpptri(&uplo, &n, a, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kRoutine = "cpptri";
  static lapack_int Inverse(char uplo, lapack_int n, std::complex<float>* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cpptri(&uplo, &n, a, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kRoutine = "zpptri";
  static lapack_int Inverse(char uplo, lapack_int n, std::complex<double>* a) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zpptri(&uplo, &n, a, &info);
    return info;
  }
};

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasPackedMatrixView<T> a) {
  if (!common::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  Status status = common::Accessible(provider, a.reachable_storage());
  if (!status.ok()) {
    return status;
  }
  status = internal_packed_cholesky_inverse_counts::Inverse(
      a.order(), triangle, common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      Native<T>::kRoutine, Native<T>::kKind, std::array{a.order()},
      std::array<std::int64_t, 2>{static_cast<std::int64_t>(triangle),
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
               DenseBlasTriangle triangle, DenseBlasPackedMatrixView<T> a,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{a.reachable_storage()};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kRoutine, report);
  auto expected = Query(provider, triangle, a);
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
  auto* values = packed::Pack(a, triangle, DenseBlasDiagonal::kNonUnit, cursor);
  report.called_provider = true;
  const auto info = Native<T>::Inverse(
      common::Uplo(triangle), static_cast<lapack_int>(a.order()), values);
  status = internal_triangular::Info(info, a.order(), true, report);
  if (status.ok()) {
    packed::Publish(values, a, triangle, DenseBlasDiagonal::kNonUnit);
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryPptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> a) {
  return Query(provider, triangle, a);
}
Status Pptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasPackedMatrixView<float> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, a, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> a) {
  return Query(provider, triangle, a);
}
Status Pptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasPackedMatrixView<double> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, a, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> a) {
  return Query(provider, triangle, a);
}
Status Pptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<float>> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, a, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> a) {
  return Query(provider, triangle, a);
}
Status Pptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<double>> a,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Inverse(provider, triangle, a, plan, workspace, report);
}

}  // namespace asc
