#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; nonallocating foreign INTEGER lifetimes.
#include <string_view>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_condition.h"
#include "internal_band_abi.h"
#include "internal_band_estimation_limits.h"
#include "internal_band_expert.h"
#include "internal_layout.h"

namespace asc {
namespace {
namespace band_internal = internal_band_expert;
constexpr auto kScalar = static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
constexpr auto kReal = static_cast<std::size_t>(LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "spbcon";
  static constexpr auto kCall = LAPACK_spbcon_base;
};
template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dpbcon";
  static constexpr auto kCall = LAPACK_dpbcon_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cpbcon";
  static constexpr auto kCall = LAPACK_cpbcon_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zpbcon";
  static constexpr auto kCall = LAPACK_zpbcon_base;
};

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const T> factors,
    DenseBlasRealType<T> original_norm,
    const DenseBlasRealType<T>& reciprocal_condition) {
  using Real = DenseBlasRealType<T>;
  const std::array operands{factors.storage().reachable_storage(),
                            band_internal::ObjectStorage(reciprocal_condition)};
  auto status = band_internal::CheckOperands(provider, operands);
  if (!status.ok()) {
    return status;
  }
  if (!std::isfinite(original_norm) || original_norm < Real{0}) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const bool active = factors.order() != 0 && original_norm != Real{0};
  status = internal_band_estimation_limits::CheckCondition(
      factors.order(), factors.bandwidth(),
      band_internal::LeadingDimension(factors), factors.triangle(), active,
      std::numeric_limits<lapack_int>::max());
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalar,
      std::array{factors.order(), factors.bandwidth(),
                 band_internal::LeadingDimension(factors)},
      std::array<std::int64_t, 4>{static_cast<std::int64_t>(factors.triangle()),
                                  static_cast<std::int64_t>(factors.layout()),
                                  factors.storage().leading_dimension(),
                                  active},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  if (active) {
    const auto n = factors.order();
    const auto scalar_count = (DenseBlasComplex<T> ? 2 : 3) * n;
    plan.regions[kScalar] = {scalar_count, scalar_count, sizeof(T), alignof(T)};
    if constexpr (DenseBlasComplex<T>) {
      plan.regions[kReal] = {n, n, sizeof(Real), alignof(Real)};
    } else {
      plan.regions[kInteger] = {n, n, sizeof(lapack_int), alignof(lapack_int)};
    }
    status = band_internal::AddBandPacking(factors, plan);
    if (!status.ok()) {
      return status;
    }
  }
  status = internal_lapack_layout::CheckTotal(plan);
  if (!status.ok()) {
    return status;
  }
  return plan;
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               LapackPositiveDefiniteBandView<const T> factors,
               DenseBlasRealType<T> original_norm,
               DenseBlasRealType<T>& reciprocal_condition,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  using Real = DenseBlasRealType<T>;
  using Auxiliary = std::conditional_t<DenseBlasComplex<T>, Real, lapack_int>;
  constexpr auto kAuxiliary = DenseBlasComplex<T> ? kReal : kInteger;
  const std::array operands{factors.storage().reachable_storage(),
                            band_internal::ObjectStorage(reciprocal_condition)};
  auto status =
      band_internal::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  band_internal::StartReport(provider, Native<T>::kName, report);
  const auto expected =
      Query(provider, factors, original_norm, reciprocal_condition);
  if (!expected.ok()) {
    return expected.status();
  }
  status = band_internal::ValidatePlan(provider, *expected, plan, workspace,
                                       operands);
  if (!status.ok()) {
    return status;
  }
  const bool active = factors.order() != 0 && original_norm != Real{0};
  T dummy_factor{};
  T dummy_work{};
  Auxiliary dummy_auxiliary{};
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  const T* factor =
      active ? band_internal::PackBand(factors, cursor, false) : &dummy_factor;
  auto* work =
      active ? static_cast<T*>(workspace.regions[kScalar].data()) : &dummy_work;
  auto* auxiliary =
      active ? static_cast<Auxiliary*>(workspace.regions[kAuxiliary].data())
             : &dummy_auxiliary;
  if constexpr (!DenseBlasComplex<T>) {
    if (active) {
      for (extent_t i = 0; i < factors.order(); ++i) {
        ::new (static_cast<void*>(auxiliary + i)) lapack_int;
      }
    }
  }
  const char triangle =
      factors.triangle() == DenseBlasTriangle::kUpper ? 'U' : 'L';
  const auto n = static_cast<lapack_int>(factors.order());
  const auto kd = static_cast<lapack_int>(factors.bandwidth());
  const auto ld =
      static_cast<lapack_int>(band_internal::LeadingDimension(factors));
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native<T>::kCall(&triangle, &n, &kd, factor, &ld, &original_norm,
                   &reciprocal_condition, work, auxiliary, &info, 1);
  status = band_internal::InterpretInfo(info, factors.order(), false, report);
  if (info == 0 && (!std::isfinite(reciprocal_condition) ||
                    reciprocal_condition < Real{0})) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryPbconWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> factors, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, factors, original_norm, reciprocal_condition);
}
Status Pbcon(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const float> factors,
             float original_norm, float& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, factors, original_norm, reciprocal_condition, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryPbconWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> factors, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, factors, original_norm, reciprocal_condition);
}
Status Pbcon(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const double> factors,
             double original_norm, double& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, factors, original_norm, reciprocal_condition, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryPbconWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> factors,
    float original_norm, const float& reciprocal_condition) {
  return Query(provider, factors, original_norm, reciprocal_condition);
}
Status Pbcon(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const std::complex<float>> factors,
             float original_norm, float& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, factors, original_norm, reciprocal_condition, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryPbconWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> factors,
    double original_norm, const double& reciprocal_condition) {
  return Query(provider, factors, original_norm, reciprocal_condition);
}
Status Pbcon(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const std::complex<double>> factors,
             double original_norm, double& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, factors, original_norm, reciprocal_condition, plan,
                 workspace, report);
}

}  // namespace asc
