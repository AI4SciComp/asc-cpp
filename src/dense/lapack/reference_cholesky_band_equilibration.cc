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
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_equilibration.h"
#include "internal_band_abi.h"
#include "internal_band_expert.h"
#include "internal_band_expert_limits.h"
#include "internal_layout.h"

namespace asc {
namespace {
namespace band_internal = internal_band_expert;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "spbequ";
  static constexpr auto kCall = LAPACK_spbequ_base;
};

template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dpbequ";
  static constexpr auto kCall = LAPACK_dpbequ_base;
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cpbequ";
  static constexpr auto kCall = LAPACK_cpbequ_base;
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zpbequ";
  static constexpr auto kCall = LAPACK_zpbequ_base;
};

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const T> matrix,
    DenseBlasVectorView<DenseBlasRealType<T>> scales,
    const LapackBandEquilibrationStatistics<DenseBlasRealType<T>>& statistics) {
  const std::array operands{matrix.storage().reachable_storage(),
                            scales.reachable_storage(),
                            band_internal::ObjectStorage(statistics)};
  auto status = band_internal::CheckOperands(provider, operands);
  if (!status.ok()) {
    return status;
  }
  if (scales.size() != matrix.order()) {
    return Status(ErrorCode::kShape);
  }
  if (scales.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  constexpr auto kMaximum = std::numeric_limits<lapack_int>::max();
  status = internal_band_expert_limits::CheckEquilibration(
      matrix.order(), matrix.bandwidth(),
      band_internal::LeadingDimension(matrix), kMaximum);
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalar,
      std::array{matrix.order(), matrix.bandwidth(),
                 band_internal::LeadingDimension(matrix)},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(matrix.triangle()),
                                  static_cast<std::int64_t>(matrix.layout()),
                                  matrix.storage().leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  status = band_internal::AddBandPacking(matrix, plan);
  if (!status.ok()) {
    return status;
  }
  return plan;
}

// The source reads only REAL/DBLE(AB(diagonal,j)); never copy off-diagonal
// or ignored imaginary values merely to satisfy a column-major representation.
template <typename T>
const T* PackDiagonal(LapackPositiveDefiniteBandView<const T> matrix,
                      T* cursor) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    return matrix.storage().data();
  }
  const auto width = matrix.bandwidth() + 1;
  const auto diagonal =
      matrix.triangle() == DenseBlasTriangle::kUpper ? matrix.bandwidth() : 0;
  for (extent_t i = 0; i < matrix.order(); ++i) {
    cursor[i * width + diagonal] = T(std::real(
        matrix.storage().data()[band_internal::Offset(matrix, i, i)]));
  }
  return cursor;
}

template <typename T>
Status Execute(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const T> matrix,
    DenseBlasVectorView<DenseBlasRealType<T>> scales,
    LapackBandEquilibrationStatistics<DenseBlasRealType<T>>& statistics,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  const std::array operands{matrix.storage().reachable_storage(),
                            scales.reachable_storage(),
                            band_internal::ObjectStorage(statistics)};
  auto status =
      band_internal::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  band_internal::StartReport(provider, Native<T>::kName, report);
  const auto expected = Query(provider, matrix, scales, statistics);
  if (!expected.ok()) {
    return expected.status();
  }
  status = band_internal::ValidatePlan(provider, *expected, plan, workspace,
                                       operands);
  if (!status.ok()) {
    return status;
  }
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  T dummy_matrix{};
  DenseBlasRealType<T> dummy_scale{};
  const auto* packed =
      matrix.order() == 0 ? &dummy_matrix : PackDiagonal(matrix, cursor);
  auto* packed_scales = matrix.order() == 0 ? &dummy_scale : scales.data();
  const char triangle =
      matrix.triangle() == DenseBlasTriangle::kUpper ? 'U' : 'L';
  const auto n = static_cast<lapack_int>(matrix.order());
  const auto kd = static_cast<lapack_int>(matrix.bandwidth());
  const auto ldab =
      static_cast<lapack_int>(band_internal::LeadingDimension(matrix));
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native<T>::kCall(&triangle, &n, &kd, packed, &ldab, packed_scales,
                   &statistics.scale_condition, &statistics.absolute_maximum,
                   &info, 1);
  return band_internal::InterpretInfo(info, matrix.order(), true, report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryPbequWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> matrix,
    DenseBlasVectorView<float> scales,
    const LapackBandEquilibrationStatistics<float>& statistics) {
  return Query(provider, matrix, scales, statistics);
}
Status Pbequ(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const float> matrix,
             DenseBlasVectorView<float> scales,
             LapackBandEquilibrationStatistics<float>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, matrix, scales, statistics, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbequWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> matrix,
    DenseBlasVectorView<double> scales,
    const LapackBandEquilibrationStatistics<double>& statistics) {
  return Query(provider, matrix, scales, statistics);
}
Status Pbequ(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const double> matrix,
             DenseBlasVectorView<double> scales,
             LapackBandEquilibrationStatistics<double>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, matrix, scales, statistics, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbequWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> scales,
    const LapackBandEquilibrationStatistics<float>& statistics) {
  return Query(provider, matrix, scales, statistics);
}
Status Pbequ(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const std::complex<float>> matrix,
             DenseBlasVectorView<float> scales,
             LapackBandEquilibrationStatistics<float>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, matrix, scales, statistics, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbequWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> scales,
    const LapackBandEquilibrationStatistics<double>& statistics) {
  return Query(provider, matrix, scales, statistics);
}
Status Pbequ(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const std::complex<double>> matrix,
             DenseBlasVectorView<double> scales,
             LapackBandEquilibrationStatistics<double>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, matrix, scales, statistics, plan, workspace, report);
}

}  // namespace asc
