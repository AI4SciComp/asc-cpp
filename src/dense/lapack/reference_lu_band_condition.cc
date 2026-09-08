#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_band_condition.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "internal_indefinite.h"
#include "internal_lu_band_expert.h"
#include "internal_lu_band_expert_counts.h"
namespace asc {
namespace {
namespace checked = internal_lu_band_expert;
namespace common = internal_indefinite;
namespace counts = internal_lu_band_expert_counts;
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "sgbcon";
  static constexpr auto kExecute = LAPACK_sgbcon_base;
};
template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dgbcon";
  static constexpr auto kExecute = LAPACK_dgbcon_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cgbcon";
  static constexpr auto kExecute = LAPACK_cgbcon_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zgbcon";
  static constexpr auto kExecute = LAPACK_zgbcon_base;
};

template <typename T>
auto Operands(LapackLuBandView<const T> factors,
              ReferenceLuBandPivotView pivots,
              const DenseBlasRealType<T>& rcond) {
  return std::array{factors.storage().reachable_storage(),
                    pivots.reachable_storage(), common::Object(rcond)};
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  LapackConditionNorm norm,
                                  LapackLuBandView<const T> factors,
                                  ReferenceLuBandPivotView pivots,
                                  DenseBlasRealType<T> anorm,
                                  const DenseBlasRealType<T>& rcond) {
  if ((norm != LapackConditionNorm::kOne &&
       norm != LapackConditionNorm::kInfinity) ||
      !std::isfinite(anorm) || anorm < DenseBlasRealType<T>{0}) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.rows() != factors.columns()) {
    return Status(ErrorCode::kShape);
  }
  const auto operands = Operands(factors, pivots, rcond);
  for (const auto& status :
       {common::Accessible(provider, operands[0]),
        checked::Vector(provider, pivots.storage(), factors.rows()),
        common::Accessible(provider, operands[2]),
        common::Disjoint(std::array{operands[0], operands[1], operands[2],
                                    common::Object(provider)}),
        counts::Condition(factors.rows(), factors.lower_bandwidth(),
                          factors.upper_bandwidth(),
                          factors.storage().leading_dimension(),
                          common::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalar,
      std::array{factors.rows(), factors.lower_bandwidth(),
                 factors.upper_bandwidth(),
                 factors.storage().leading_dimension(), pivots.storage().size(),
                 pivots.storage().increment()},
      std::array<std::int64_t, 1>{static_cast<std::int64_t>(norm)},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  const auto status = checked::EstimateWorkspace<T>(factors.rows(), plan);
  if (!status.ok()) {
    return status;
  }
  return plan;
}
template <typename T>
lapack_int Call(LapackConditionNorm norm, LapackLuBandView<const T> factors,
                DenseBlasRealType<T> anorm, DenseBlasRealType<T>& rcond,
                lapack_int* integers, const LapackWorkspace& workspace) {
  using Real = DenseBlasRealType<T>;
  const char selected = norm == LapackConditionNorm::kOne ? '1' : 'I';
  const auto n = static_cast<lapack_int>(factors.rows());
  const auto kl = static_cast<lapack_int>(factors.lower_bandwidth());
  const auto ku = static_cast<lapack_int>(factors.upper_bandwidth());
  const auto ld =
      static_cast<lapack_int>(factors.storage().leading_dimension());
  T scalar_dummy{};
  const T matrix_dummy{};
  auto* work = checked::Nonnull(
      static_cast<T*>(workspace.regions[common::kScalar].data()), scalar_dummy);
  lapack_int info = std::numeric_limits<lapack_int>::min();
  if constexpr (DenseBlasComplex<T>) {
    Real real_dummy{};
    auto* real = checked::Nonnull(
        static_cast<Real*>(workspace.regions[checked::kReal].data()),
        real_dummy);
    Native<T>::kExecute(
        &selected, &n, &kl, &ku,
        checked::Nonnull(factors.storage().data(), matrix_dummy), &ld, integers,
        &anorm, &rcond, work, real, &info, std::size_t{1});
  } else {
    Native<T>::kExecute(
        &selected, &n, &kl, &ku,
        checked::Nonnull(factors.storage().data(), matrix_dummy), &ld, integers,
        &anorm, &rcond, work, integers + n, &info, std::size_t{1});
  }
  return info;
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               LapackConditionNorm norm, LapackLuBandView<const T> factors,
               ReferenceLuBandPivotView pivots, DenseBlasRealType<T> anorm,
               DenseBlasRealType<T>& rcond, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const auto operands = Operands(factors, pivots, rcond);
  auto status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kName, report);
  const auto expected = Query(provider, norm, factors, pivots, anorm, rcond);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  status = checked::Pivots(pivots.values(), factors.rows(),
                           factors.lower_bandwidth());
  if (!status.ok()) {
    return status;
  }
  lapack_int dummy = std::numeric_limits<lapack_int>::min();
  auto* integers = checked::Integers(
      workspace, plan.regions[common::kPivot].minimum_entries, dummy);
  std::copy(pivots.values().begin(), pivots.values().end(), integers);
  report.called_provider = true;
  const auto info = Call(norm, factors, anorm, rcond, integers, workspace);
  report.native_info = info;
  return info == 0 ? common::Complete(report) : common::Defect(info, report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryGbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackLuBandView<const float> factors, ReferenceLuBandPivotView pivots,
    float original_norm, const float& reciprocal_condition) {
  return Query(provider, norm, factors, pivots, original_norm,
               reciprocal_condition);
}
Status Gbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             LapackLuBandView<const float> factors,
             ReferenceLuBandPivotView pivots, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, norm, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackLuBandView<const double> factors, ReferenceLuBandPivotView pivots,
    double original_norm, const double& reciprocal_condition) {
  return Query(provider, norm, factors, pivots, original_norm,
               reciprocal_condition);
}
Status Gbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             LapackLuBandView<const double> factors,
             ReferenceLuBandPivotView pivots, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, norm, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackLuBandView<const std::complex<float>> factors,
    ReferenceLuBandPivotView pivots, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, norm, factors, pivots, original_norm,
               reciprocal_condition);
}
Status Gbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             LapackLuBandView<const std::complex<float>> factors,
             ReferenceLuBandPivotView pivots, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, norm, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackLuBandView<const std::complex<double>> factors,
    ReferenceLuBandPivotView pivots, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, norm, factors, pivots, original_norm,
               reciprocal_condition);
}
Status Gbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             LapackLuBandView<const std::complex<double>> factors,
             ReferenceLuBandPivotView pivots, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, norm, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report);
}
}  // namespace asc
