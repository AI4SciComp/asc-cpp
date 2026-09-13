#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_condition.h"
#include "internal_positive_tridiagonal_condition_counts.h"
#include "internal_tridiagonal.h"
namespace asc {
namespace {
namespace checked = internal_tridiagonal;
template <typename T>
using Factor = ReferencePositiveDefiniteTridiagonalFactorView<T>;
template <typename T>
using Real = DenseBlasRealType<T>;
template <typename T>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return "sptcon";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dptcon";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return "cptcon";
  } else {
    return "zptcon";
  }
}
template <typename T>
auto Operands(Factor<T> factor, const Real<T>& output) {
  return std::array{factor.diagonal().reachable_storage(),
                    factor.off_diagonal().reachable_storage(),
                    checked::ObjectStorage(output)};
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  Factor<T> factor, Real<T> norm,
                                  const Real<T>& output) {
  if (factor.provider() != provider.identity()) {
    return Status(ErrorCode::kInvalidState);
  }
  if (!std::isfinite(norm) || norm < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto valid = Factor<T>::FromRaw(
      provider, factor.triangle(), factor.diagonal(), factor.off_diagonal());
  if (!valid.ok()) {
    return valid.status();
  }
  const auto status = checked::Disjoint(Operands(factor, output));
  if (!status.ok()) {
    return status;
  }
  const auto n = factor.diagonal().size();
  const bool active = n > 0 && norm > 0;
  const auto counts = internal_positive_tridiagonal_condition_counts::Condition(
      n, norm > 0, checked::kLimit);
  if (!counts.ok()) {
    return counts;
  }
  const auto identity = LapackPlanIdentity::Create(
      Routine<T>(), checked::Kind<T>(), std::array{n},
      std::array<std::int64_t, 2>{static_cast<std::int64_t>(factor.triangle()),
                                  static_cast<std::int64_t>(active)},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (active) {
    plan.regions[checked::kReal] = {n, n, sizeof(Real<T>), alignof(Real<T>)};
  }
  return plan;
}
template <typename T>
Status FactorValues(Factor<T> factor, LapackReport& report) {
  const auto diagonal = factor.diagonal();
  for (extent_t i = 0; i < diagonal.size(); ++i) {
    if (!std::isfinite(diagonal.data()[i]) || diagonal.data()[i] <= 0) {
      report.outcome = LapackOutcome::kNotPositiveDefinite;
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
  }
  const auto off = factor.off_diagonal();
  for (extent_t i = 0; i < off.size(); ++i) {
    bool finite;
    if constexpr (DenseBlasComplex<T>) {
      finite = std::isfinite(off.data()[i].real()) &&
               std::isfinite(off.data()[i].imag());
    } else {
      finite = std::isfinite(off.data()[i]);
    }
    if (!finite) {
      report.outcome = LapackOutcome::kAccuracyWarning;
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
  }
  return Status::Ok();
}
template <typename T>
lapack_int Native(lapack_int n, const Real<T>* diagonal, const T* off,
                  Real<T> norm, Real<T>* output, Real<T>* work) {
  lapack_int info = std::numeric_limits<lapack_int>::min();
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sptcon(&n, diagonal, off, &norm, output, work, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dptcon(&n, diagonal, off, &norm, output, work, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cptcon(&n, diagonal, off, &norm, output, work, &info);
  } else {
    LAPACK_zptcon(&n, diagonal, off, &norm, output, work, &info);
  }
  return info;
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider, Factor<T> factor,
               Real<T> norm, Real<T>& output, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const auto operands = Operands(factor, output);
  auto status =
      checked::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<T>(), report);
  const auto expected = Query(provider, factor, norm, output);
  if (!expected.ok()) {
    return expected.status();
  }
  status =
      checked::ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const auto n = factor.diagonal().size();
  const bool active = n > 0 && norm > 0;
  if (active) {
    status = FactorValues(factor, report);
    if (!status.ok()) {
      return status;
    }
  }
  Real<T> dummy_diagonal{1};
  T dummy_off{};
  Real<T> dummy_work{};
  Real<T> staged{-1};  // Negative estimates are impossible; no old output read.
  const auto* diagonal = n == 0 ? &dummy_diagonal : factor.diagonal().data();
  const auto* off = n < 2 ? &dummy_off : factor.off_diagonal().data();
  auto* work =
      active ? static_cast<Real<T>*>(workspace.regions[checked::kReal].data())
             : &dummy_work;
  report.called_provider = true;
  const auto info =
      Native<T>(static_cast<lapack_int>(n), diagonal, off, norm, &staged, work);
  report.native_info = info;
  if (info != 0 || staged < 0) {
    status = checked::ProviderDefect(info, report);
    report.output_validity = LapackOutputValidity::kUnchanged;
    return status;
  }
  output = staged;
  if (!std::isfinite(staged)) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return checked::Complete(report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryPtconWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
    float original_norm, const float& reciprocal_condition) {
  return Query(provider, factor, original_norm, reciprocal_condition);
}
Status Ptcon(const ReferenceLapackProvider& provider,
             ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
             float original_norm, float& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, factor, original_norm, reciprocal_condition, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryPtconWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
    double original_norm, const double& reciprocal_condition) {
  return Query(provider, factor, original_norm, reciprocal_condition);
}
Status Ptcon(const ReferenceLapackProvider& provider,
             ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
             double original_norm, double& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, factor, original_norm, reciprocal_condition, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryPtconWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    float original_norm, const float& reciprocal_condition) {
  return Query(provider, factor, original_norm, reciprocal_condition);
}
Status Ptcon(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    float original_norm, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  return Execute(provider, factor, original_norm, reciprocal_condition, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryPtconWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    double original_norm, const double& reciprocal_condition) {
  return Query(provider, factor, original_norm, reciprocal_condition);
}
Status Ptcon(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    double original_norm, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  return Execute(provider, factor, original_norm, reciprocal_condition, plan,
                 workspace, report);
}
}  // namespace asc
