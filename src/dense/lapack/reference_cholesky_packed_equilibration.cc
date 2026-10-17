#include <array>
#include <cmath>
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
#include "asc/dense/providers/lapack_cholesky_packed_equilibration.h"
#include "internal_cholesky_expert.h"
#include "internal_indefinite.h"
#include "internal_packed_cholesky_equilibration_counts.h"

namespace asc {
namespace {
namespace common = internal_indefinite;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kRoutine = "sppequ";
  static lapack_int Execute(char uplo, lapack_int n, const float* a,
                            float* scales, float* condition, float* maximum) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sppequ(&uplo, &n, a, scales, condition, maximum, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kRoutine = "dppequ";
  static lapack_int Execute(char uplo, lapack_int n, const double* a,
                            double* scales, double* condition,
                            double* maximum) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dppequ(&uplo, &n, a, scales, condition, maximum, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kRoutine = "cppequ";
  static lapack_int Execute(char uplo, lapack_int n,
                            const std::complex<float>* a, float* scales,
                            float* condition, float* maximum) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cppequ(&uplo, &n, a, scales, condition, maximum, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kRoutine = "zppequ";
  static lapack_int Execute(char uplo, lapack_int n,
                            const std::complex<double>* a, double* scales,
                            double* condition, double* maximum) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zppequ(&uplo, &n, a, scales, condition, maximum, &info);
    return info;
  }
};

DenseBlasTriangle NativeTriangle(DenseBlasTriangle triangle,
                                 DenseBlasLayout layout) {
  if (layout == DenseBlasLayout::kRowMajor) {
    return triangle == DenseBlasTriangle::kUpper ? DenseBlasTriangle::kLower
                                                 : DenseBlasTriangle::kUpper;
  }
  return triangle;
}

template <typename T>
auto Spans(DenseBlasPackedMatrixView<const T> a,
           DenseBlasVectorView<DenseBlasRealType<T>> scales,
           const DenseBlasRealType<T>& condition,
           const DenseBlasRealType<T>& maximum) {
  return std::array{a.reachable_storage(), scales.reachable_storage(),
                    common::Object(condition), common::Object(maximum)};
}

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const T> a,
    DenseBlasVectorView<DenseBlasRealType<T>> scales,
    const DenseBlasRealType<T>& condition,
    const DenseBlasRealType<T>& maximum) {
  Status status = internal_packed_cholesky_equilibration_counts::Equilibrate(
      a.order(), triangle, a.layout(), common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  status = common::Accessible(provider, a.reachable_storage());
  if (!status.ok()) {
    return status;
  }
  status = internal_cholesky_expert::Vector(provider, scales, a.order());
  if (!status.ok()) {
    return status;
  }
  status = common::Disjoint(Spans(a, scales, condition, maximum));
  if (!status.ok()) {
    return status;
  }
  auto identity = LapackPlanIdentity::Create(
      Native<T>::kRoutine, Native<T>::kKind,
      std::array{a.order(), scales.size()},
      std::array<std::int64_t, 4>{
          static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(a.layout()),
          static_cast<std::int64_t>(NativeTriangle(triangle, a.layout())),
          scales.increment()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  return LapackWorkspacePlan{*identity};
}

template <typename Real>
Status ValidateOutputs(DenseBlasVectorView<Real> scales, Real condition,
                       Real maximum, LapackReport& report) {
  bool finite = std::isfinite(condition) && condition >= 0 &&
                std::isfinite(maximum) && maximum > 0;
  for (extent_t i = 0; i < scales.size(); ++i) {
    finite = finite && std::isfinite(scales.data()[i]) && scales.data()[i] > 0;
  }
  if (!finite) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return common::Complete(report);
}

template <typename T>
Status Equilibrate(const ReferenceLapackProvider& provider,
                   DenseBlasTriangle triangle,
                   DenseBlasPackedMatrixView<const T> a,
                   DenseBlasVectorView<DenseBlasRealType<T>> scales,
                   DenseBlasRealType<T>& condition,
                   DenseBlasRealType<T>& maximum,
                   const LapackWorkspacePlan& plan,
                   const LapackWorkspace& workspace, LapackReport& report) {
  const auto operands = Spans(a, scales, condition, maximum);
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kRoutine, report);
  auto expected = Query(provider, triangle, a, scales, condition, maximum);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (a.order() == 0) {
    condition = 1;
    maximum = 0;
    return common::Complete(report);
  }
  report.called_provider = true;
  const auto info =
      Native<T>::Execute(common::Uplo(NativeTriangle(triangle, a.layout())),
                         static_cast<lapack_int>(a.order()), a.data(),
                         scales.data(), &condition, &maximum);
  report.native_info = info;
  if (info < 0 || info > a.order()) {
    return common::Defect(info, report);
  }
  if (info > 0) {
    report.outcome = LapackOutcome::kNotPositiveDefinite;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = static_cast<index_t>(info) - 1;
    return Status(ErrorCode::kNumerical);
  }
  return ValidateOutputs(scales, condition, maximum, report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryPpequWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> a, DenseBlasVectorView<float> scales,
    const float& scale_condition, const float& absolute_maximum) {
  return Query(provider, triangle, a, scales, scale_condition,
               absolute_maximum);
}
Status Ppequ(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const float> a,
             DenseBlasVectorView<float> scales, float& scale_condition,
             float& absolute_maximum, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Equilibrate(provider, triangle, a, scales, scale_condition,
                     absolute_maximum, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPpequWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> a,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum) {
  return Query(provider, triangle, a, scales, scale_condition,
               absolute_maximum);
}
Status Ppequ(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const double> a,
             DenseBlasVectorView<double> scales, double& scale_condition,
             double& absolute_maximum, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Equilibrate(provider, triangle, a, scales, scale_condition,
                     absolute_maximum, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPpequWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> a,
    DenseBlasVectorView<float> scales, const float& scale_condition,
    const float& absolute_maximum) {
  return Query(provider, triangle, a, scales, scale_condition,
               absolute_maximum);
}
Status Ppequ(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<float>> a,
             DenseBlasVectorView<float> scales, float& scale_condition,
             float& absolute_maximum, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Equilibrate(provider, triangle, a, scales, scale_condition,
                     absolute_maximum, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPpequWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> a,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum) {
  return Query(provider, triangle, a, scales, scale_condition,
               absolute_maximum);
}
Status Ppequ(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<double>> a,
             DenseBlasVectorView<double> scales, double& scale_condition,
             double& absolute_maximum, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Equilibrate(provider, triangle, a, scales, scale_condition,
                     absolute_maximum, plan, workspace, report);
}

}  // namespace asc
