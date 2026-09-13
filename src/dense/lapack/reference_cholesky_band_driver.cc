#include <array>
#include <complex>
#include <cstdint>
#include <limits>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_driver.h"
#include "internal_band_abi.h"
#include "internal_band_expert.h"
#include "internal_band_expert_limits.h"
#include "internal_band_limits.h"
#include "internal_layout.h"

namespace asc {
namespace {
namespace band_internal = internal_band_expert;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "spbsv";
  static constexpr auto kCall = LAPACK_spbsv_base;
};

template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dpbsv";
  static constexpr auto kCall = LAPACK_dpbsv_base;
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cpbsv";
  static constexpr auto kCall = LAPACK_cpbsv_base;
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zpbsv";
  static constexpr auto kCall = LAPACK_zpbsv_base;
};

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  LapackPositiveDefiniteBandView<T> matrix,
                                  DenseBlasMatrixView<T> rhs) {
  const std::array operands{matrix.storage().reachable_storage(),
                            rhs.reachable_storage()};
  auto status = band_internal::CheckOperands(provider, operands);
  if (!status.ok()) {
    return status;
  }
  if (rhs.rows() != matrix.order()) {
    return Status(ErrorCode::kShape);
  }
  constexpr auto kMaximum = std::numeric_limits<lapack_int>::max();
  status = internal_band_expert_limits::CheckSimpleDriver(
      matrix.order(), matrix.bandwidth(),
      band_internal::LeadingDimension(matrix), rhs.columns(),
      internal_lapack_layout::LeadingDimension(rhs), matrix.triangle(),
      kMaximum);
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalar,
      std::array{matrix.order(), matrix.bandwidth(), rhs.columns(),
                 band_internal::LeadingDimension(matrix),
                 internal_lapack_layout::LeadingDimension(rhs)},
      std::array<std::int64_t, 5>{static_cast<std::int64_t>(matrix.triangle()),
                                  static_cast<std::int64_t>(matrix.layout()),
                                  static_cast<std::int64_t>(rhs.layout()),
                                  matrix.storage().leading_dimension(),
                                  rhs.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  // PBSV always factors, including NRHS=0. Do not inherit PBTRS's empty
  // factor-packing suppression for this actual driver call.
  status = band_internal::AddBandPacking(matrix, plan);
  if (!status.ok()) {
    return status;
  }
  status = internal_lapack_layout::AddPacking(rhs, plan);
  if (!status.ok()) {
    return status;
  }
  return plan;
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               LapackPositiveDefiniteBandView<T> matrix,
               DenseBlasMatrixView<T> rhs, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{matrix.storage().reachable_storage(),
                            rhs.reachable_storage()};
  auto status =
      band_internal::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  band_internal::StartReport(provider, Native<T>::kName, report);
  const auto expected = Query(provider, matrix, rhs);
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
  T dummy_rhs{};
  auto* packed = matrix.order() == 0
                     ? &dummy_matrix
                     : band_internal::PackBand(matrix, cursor, true);
  const bool empty_rhs = matrix.order() == 0 || rhs.columns() == 0;
  auto* packed_rhs =
      empty_rhs ? &dummy_rhs : internal_lapack_layout::Pack(rhs, cursor);
  const char triangle =
      matrix.triangle() == DenseBlasTriangle::kUpper ? 'U' : 'L';
  const auto n = static_cast<lapack_int>(matrix.order());
  const auto kd = static_cast<lapack_int>(matrix.bandwidth());
  const auto nrhs = static_cast<lapack_int>(rhs.columns());
  const auto ldab =
      static_cast<lapack_int>(band_internal::LeadingDimension(matrix));
  const auto ldb =
      static_cast<lapack_int>(internal_lapack_layout::LeadingDimension(rhs));
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.factor_family = LapackFactorFamily::kCholesky;
  report.called_provider = true;
  Native<T>::kCall(&triangle, &n, &kd, &nrhs, packed, &ldab, packed_rhs, &ldb,
                   &info, 1);
  status = band_internal::InterpretInfo(info, matrix.order(), true, report);
  if (info >= 0 && info <= matrix.order()) {
    band_internal::UnpackBand(
        packed, matrix,
        internal_band_limits::NormalizedDiagonalPrefix(
            matrix.order(), matrix.bandwidth(), true, info));
  }
  if (info == 0 && !empty_rhs) {
    internal_lapack_layout::Unpack(packed_rhs, rhs);
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryPbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<float> matrix,
    DenseBlasMatrixView<float> rhs) {
  return Query(provider, matrix, rhs);
}
Status Pbsv(const ReferenceLapackProvider& provider,
            LapackPositiveDefiniteBandView<float> matrix,
            DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, matrix, rhs, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<double> matrix,
    DenseBlasMatrixView<double> rhs) {
  return Query(provider, matrix, rhs);
}
Status Pbsv(const ReferenceLapackProvider& provider,
            LapackPositiveDefiniteBandView<double> matrix,
            DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, matrix, rhs, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, matrix, rhs);
}
Status Pbsv(const ReferenceLapackProvider& provider,
            LapackPositiveDefiniteBandView<std::complex<float>> matrix,
            DenseBlasMatrixView<std::complex<float>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Execute(provider, matrix, rhs, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, matrix, rhs);
}
Status Pbsv(const ReferenceLapackProvider& provider,
            LapackPositiveDefiniteBandView<std::complex<double>> matrix,
            DenseBlasMatrixView<std::complex<double>> rhs,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  return Execute(provider, matrix, rhs, plan, workspace, report);
}

}  // namespace asc
