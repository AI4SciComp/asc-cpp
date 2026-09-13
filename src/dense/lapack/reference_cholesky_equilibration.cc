#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_equilibration.h"
#include "internal_cholesky_expert.h"
#include "internal_cholesky_expert_counts.h"

namespace asc {
namespace {
namespace checked = internal_cholesky_expert;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kOrdinary = "spoequ";
  static constexpr std::string_view kRadix = "spoequb";
  static lapack_int Execute(bool radix, lapack_int n, const float* a,
                            lapack_int lda, float* scales, float* condition,
                            float* maximum) {
    lapack_int info = 0;
    if (radix) {
      LAPACK_spoequb(&n, a, &lda, scales, condition, maximum, &info);
    } else {
      LAPACK_spoequ(&n, a, &lda, scales, condition, maximum, &info);
    }
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kOrdinary = "dpoequ";
  static constexpr std::string_view kRadix = "dpoequb";
  static lapack_int Execute(bool radix, lapack_int n, const double* a,
                            lapack_int lda, double* scales, double* condition,
                            double* maximum) {
    lapack_int info = 0;
    if (radix) {
      LAPACK_dpoequb(&n, a, &lda, scales, condition, maximum, &info);
    } else {
      LAPACK_dpoequ(&n, a, &lda, scales, condition, maximum, &info);
    }
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kOrdinary = "cpoequ";
  static constexpr std::string_view kRadix = "cpoequb";
  static lapack_int Execute(bool radix, lapack_int n,
                            const std::complex<float>* a, lapack_int lda,
                            float* scales, float* condition, float* maximum) {
    lapack_int info = 0;
    if (radix) {
      LAPACK_cpoequb(&n, a, &lda, scales, condition, maximum, &info);
    } else {
      LAPACK_cpoequ(&n, a, &lda, scales, condition, maximum, &info);
    }
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kOrdinary = "zpoequ";
  static constexpr std::string_view kRadix = "zpoequb";
  static lapack_int Execute(bool radix, lapack_int n,
                            const std::complex<double>* a, lapack_int lda,
                            double* scales, double* condition,
                            double* maximum) {
    lapack_int info = 0;
    if (radix) {
      LAPACK_zpoequb(&n, a, &lda, scales, condition, maximum, &info);
    } else {
      LAPACK_zpoequ(&n, a, &lda, scales, condition, maximum, &info);
    }
    return info;
  }
};

template <typename T>
auto Spans(DenseBlasMatrixView<const T> matrix,
           DenseBlasVectorView<DenseBlasRealType<T>> scales,
           const DenseBlasRealType<T>& condition,
           const DenseBlasRealType<T>& maximum) {
  return std::array{matrix.reachable_storage(), scales.reachable_storage(),
                    checked::ObjectStorage(condition),
                    checked::ObjectStorage(maximum)};
}

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider, bool radix,
    DenseBlasMatrixView<const T> matrix,
    DenseBlasVectorView<DenseBlasRealType<T>> scales,
    const DenseBlasRealType<T>& condition,
    const DenseBlasRealType<T>& maximum) {
  Status status = checked::Matrix(provider, matrix);
  if (!status.ok()) {
    return status;
  }
  if (matrix.rows() != matrix.columns()) {
    return Status(ErrorCode::kShape);
  }
  status = internal_cholesky_expert_counts::Equilibration(
      matrix.rows(), checked::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  status = checked::Vector(provider, scales, matrix.rows());
  if (!status.ok()) {
    return status;
  }
  status = checked::Disjoint(Spans(matrix, scales, condition, maximum));
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      radix ? Native<T>::kRadix : Native<T>::kOrdinary, Native<T>::kKind,
      std::array{matrix.rows(), matrix.columns(), checked::Leading(matrix),
                 scales.size()},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(matrix.layout()),
                                  matrix.leading_dimension(),
                                  scales.increment()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  status = checked::AddPacking(matrix, plan);
  if (!status.ok()) {
    return status;
  }
  return plan;
}

template <typename T>
const T* PackDiagonal(DenseBlasMatrixView<const T> matrix,
                      const LapackWorkspace& workspace) {
  if (!checked::Packed(matrix)) {
    return matrix.data();
  }
  auto* packed = static_cast<T*>(workspace.regions[checked::kLayout].data());
  for (extent_t i = 0; i < matrix.rows(); ++i) {
    packed[i * matrix.rows() + i] =
        checked::RealDiagonal(checked::Entry(matrix, i, i));
  }
  return packed;
}

template <typename T>
Status FiniteDiagonal(DenseBlasMatrixView<const T> matrix) {
  for (extent_t i = 0; i < matrix.rows(); ++i) {
    if (!std::isfinite(std::real(checked::Entry(matrix, i, i)))) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  return Status::Ok();
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
  return checked::Complete(report);
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider, bool radix,
               DenseBlasMatrixView<const T> matrix,
               DenseBlasVectorView<DenseBlasRealType<T>> scales,
               DenseBlasRealType<T>& condition, DenseBlasRealType<T>& maximum,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const auto operands = Spans(matrix, scales, condition, maximum);
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(
      provider, radix ? Native<T>::kRadix : Native<T>::kOrdinary, report);
  const auto expected =
      Query(provider, radix, matrix, scales, condition, maximum);
  if (!expected.ok()) {
    return expected.status();
  }
  status =
      checked::ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (radix) {
    status = FiniteDiagonal(matrix);
    if (!status.ok()) {
      return status;
    }
  }
  if (matrix.rows() == 0) {
    condition = 1;
    maximum = 0;
    return checked::Complete(report);
  }
  const auto* packed = PackDiagonal(matrix, workspace);
  report.called_provider = true;
  const auto info =
      Native<T>::Execute(radix, static_cast<lapack_int>(matrix.rows()), packed,
                         static_cast<lapack_int>(checked::Leading(matrix)),
                         scales.data(), &condition, &maximum);
  report.native_info = info;
  if (info < 0 || info > matrix.rows()) {
    return checked::ProviderDefect(info, report);
  }
  if (info > 0) {
    report.outcome = LapackOutcome::kNotPositiveDefinite;
    report.diagnostic_index = info - 1;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return ValidateOutputs(scales, condition, maximum, report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryPoequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix, DenseBlasVectorView<float> scales,
    const float& scale_condition, const float& absolute_maximum) {
  return Query(provider, false, matrix, scales, scale_condition,
               absolute_maximum);
}
Status Poequ(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const float> matrix,
             DenseBlasVectorView<float> scales, float& scale_condition,
             float& absolute_maximum, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, false, matrix, scales, scale_condition,
                 absolute_maximum, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPoequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix, DenseBlasVectorView<float> scales,
    const float& scale_condition, const float& absolute_maximum) {
  return Query(provider, true, matrix, scales, scale_condition,
               absolute_maximum);
}
Status Poequb(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<const float> matrix,
              DenseBlasVectorView<float> scales, float& scale_condition,
              float& absolute_maximum, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, true, matrix, scales, scale_condition,
                 absolute_maximum, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPoequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum) {
  return Query(provider, false, matrix, scales, scale_condition,
               absolute_maximum);
}
Status Poequ(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const double> matrix,
             DenseBlasVectorView<double> scales, double& scale_condition,
             double& absolute_maximum, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, false, matrix, scales, scale_condition,
                 absolute_maximum, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPoequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum) {
  return Query(provider, true, matrix, scales, scale_condition,
               absolute_maximum);
}
Status Poequb(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<const double> matrix,
              DenseBlasVectorView<double> scales, double& scale_condition,
              double& absolute_maximum, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, true, matrix, scales, scale_condition,
                 absolute_maximum, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPoequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> scales, const float& scale_condition,
    const float& absolute_maximum) {
  return Query(provider, false, matrix, scales, scale_condition,
               absolute_maximum);
}
Status Poequ(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const std::complex<float>> matrix,
             DenseBlasVectorView<float> scales, float& scale_condition,
             float& absolute_maximum, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, false, matrix, scales, scale_condition,
                 absolute_maximum, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPoequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> scales, const float& scale_condition,
    const float& absolute_maximum) {
  return Query(provider, true, matrix, scales, scale_condition,
               absolute_maximum);
}
Status Poequb(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<const std::complex<float>> matrix,
              DenseBlasVectorView<float> scales, float& scale_condition,
              float& absolute_maximum, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, true, matrix, scales, scale_condition,
                 absolute_maximum, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPoequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum) {
  return Query(provider, false, matrix, scales, scale_condition,
               absolute_maximum);
}
Status Poequ(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const std::complex<double>> matrix,
             DenseBlasVectorView<double> scales, double& scale_condition,
             double& absolute_maximum, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, false, matrix, scales, scale_condition,
                 absolute_maximum, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPoequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum) {
  return Query(provider, true, matrix, scales, scale_condition,
               absolute_maximum);
}
Status Poequb(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<const std::complex<double>> matrix,
              DenseBlasVectorView<double> scales, double& scale_condition,
              double& absolute_maximum, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, true, matrix, scales, scale_condition,
                 absolute_maximum, plan, workspace, report);
}

}  // namespace asc
