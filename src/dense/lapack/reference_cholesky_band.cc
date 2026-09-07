#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "internal_band_abi.h"
#include "internal_band_limits.h"
#include "internal_layout.h"
#include "internal_workspace_context.h"

namespace asc {
namespace {
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::array<std::string_view, 3> kNames{"spbtrf", "spbtf2",
                                                          "spbtrs"};
  static constexpr auto kFactor = LAPACK_spbtrf_base;
  static constexpr auto kUnblocked = LAPACK_GLOBAL_SUFFIX(spbtf2, SPBTF2);
  static constexpr auto kSolve = LAPACK_spbtrs_base;
};
template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::array<std::string_view, 3> kNames{"dpbtrf", "dpbtf2",
                                                          "dpbtrs"};
  static constexpr auto kFactor = LAPACK_dpbtrf_base;
  static constexpr auto kUnblocked = LAPACK_GLOBAL_SUFFIX(dpbtf2, DPBTF2);
  static constexpr auto kSolve = LAPACK_dpbtrs_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::array<std::string_view, 3> kNames{"cpbtrf", "cpbtf2",
                                                          "cpbtrs"};
  static constexpr auto kFactor = LAPACK_cpbtrf_base;
  static constexpr auto kUnblocked = LAPACK_GLOBAL_SUFFIX(cpbtf2, CPBTF2);
  static constexpr auto kSolve = LAPACK_cpbtrs_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::array<std::string_view, 3> kNames{"zpbtrf", "zpbtf2",
                                                          "zpbtrs"};
  static constexpr auto kFactor = LAPACK_zpbtrf_base;
  static constexpr auto kUnblocked = LAPACK_GLOBAL_SUFFIX(zpbtf2, ZPBTF2);
  static constexpr auto kSolve = LAPACK_zpbtrs_base;
};

bool Overlap(ConstMemoryView a, ConstMemoryView b) {
  if (a.size() == 0 || b.size() == 0) {
    return false;
  }
  const auto first = reinterpret_cast<std::uintptr_t>(a.data());
  const auto second = reinterpret_cast<std::uintptr_t>(b.data());
  return first <= second ? second - first < a.size()
                         : first - second < b.size();
}
template <typename T>
ConstMemoryView ObjectStorage(const T& value) {
  return {&value, sizeof(value), MemorySpace::kHost};
}
Status CheckMetadata(const ReferenceLapackProvider& provider,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report,
                     std::span<const ConstMemoryView> operands) {
  const std::array metadata{ObjectStorage(provider), ObjectStorage(plan),
                            ObjectStorage(workspace), ObjectStorage(report)};
  for (std::size_t i = 0; i < metadata.size(); ++i) {
    for (std::size_t j = i + 1; j < metadata.size(); ++j) {
      if (Overlap(metadata[i], metadata[j])) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    for (const auto operand : operands) {
      if (Overlap(metadata[i], operand)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    for (const auto& region : workspace.regions) {
      if (Overlap(metadata[i], region)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}
Status Admit(const ReferenceLapackProvider& provider, MemorySpace space) {
  if ((space != MemorySpace::kHost && space != MemorySpace::kPinnedHost) ||
      !provider.context().CanAccess(space)) {
    return Status(ErrorCode::kMemoryAccess);
  }
  return Status::Ok();
}
template <typename T>
extent_t LeadingDimension(LapackPositiveDefiniteBandView<T> band) {
  return band.layout() == DenseBlasLayout::kColumnMajor
             ? band.storage().leading_dimension()
             : band.bandwidth() + 1;
}
template <typename T>
Status AddBandPacking(LapackPositiveDefiniteBandView<T> band,
                      LapackWorkspacePlan& plan) {
  if (band.layout() == DenseBlasLayout::kColumnMajor) {
    return Status::Ok();
  }
  const extent_t width = band.bandwidth() + 1;
  if (band.order() > std::numeric_limits<extent_t>::max() / width) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t count = band.order() * width;
  plan.regions[internal_lapack_layout::kRegion] = {count, count, sizeof(T),
                                                   alignof(T)};
  return internal_lapack_layout::CheckTotal(plan);
}
template <typename T>
Result<LapackWorkspacePlan> QueryFactor(const ReferenceLapackProvider& provider,
                                        LapackPositiveDefiniteBandView<T> band,
                                        bool blocked) {
  const auto admitted = Admit(provider, band.storage().memory_space());
  if (!admitted.ok()) {
    return admitted;
  }
  const auto checked = internal_band_limits::CheckFactor(
      band.order(), band.bandwidth(), LeadingDimension(band), band.triangle(),
      blocked, std::numeric_limits<lapack_int>::max());
  if (!checked.ok()) {
    return checked;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kNames[blocked ? 0 : 1], Native<T>::kScalar,
      std::array{band.order(), band.bandwidth(), LeadingDimension(band)},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(band.triangle()),
                                  static_cast<std::int64_t>(band.layout()),
                                  band.storage().leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  const auto packed = AddBandPacking(band, plan);
  if (!packed.ok()) {
    return packed;
  }
  return plan;
}
template <typename T>
Result<LapackWorkspacePlan> QuerySolve(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const T> band, DenseBlasMatrixView<T> rhs) {
  for (const auto space : {band.storage().memory_space(), rhs.memory_space()}) {
    const auto admitted = Admit(provider, space);
    if (!admitted.ok()) {
      return admitted;
    }
  }
  if (rhs.rows() != band.order()) {
    return Status(ErrorCode::kShape);
  }
  const auto checked = internal_band_limits::CheckSolve(
      band.order(), band.bandwidth(), LeadingDimension(band), rhs.columns(),
      internal_lapack_layout::LeadingDimension(rhs), band.triangle(),
      std::numeric_limits<lapack_int>::max());
  if (!checked.ok()) {
    return checked;
  }
  if (Overlap(band.storage().reachable_storage(), rhs.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kNames[2], Native<T>::kScalar,
      std::array{band.order(), band.bandwidth(), rhs.columns(),
                 LeadingDimension(band),
                 internal_lapack_layout::LeadingDimension(rhs)},
      std::array<std::int64_t, 5>{static_cast<std::int64_t>(band.triangle()),
                                  static_cast<std::int64_t>(band.layout()),
                                  static_cast<std::int64_t>(rhs.layout()),
                                  band.storage().leading_dimension(),
                                  rhs.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  if (band.order() != 0 && rhs.columns() != 0) {
    const auto packed = AddBandPacking(band, plan);
    if (!packed.ok()) {
      return packed;
    }
    const auto rhs_packed = internal_lapack_layout::AddPacking(rhs, plan);
    if (!rhs_packed.ok()) {
      return rhs_packed;
    }
  }
  return plan;
}
Status ValidatePlan(const ReferenceLapackProvider& provider,
                    const LapackWorkspacePlan& expected,
                    const LapackWorkspacePlan& supplied,
                    const LapackWorkspace& workspace,
                    std::span<const ConstMemoryView> operands) {
  if (expected.total_byte_limit != supplied.total_byte_limit) {
    return Status(ErrorCode::kInvalidState);
  }
  for (std::size_t i = 0; i < expected.regions.size(); ++i) {
    const auto& a = expected.regions[i];
    const auto& b = supplied.regions[i];
    if (a.minimum_entries != b.minimum_entries ||
        a.preferred_entries != b.preferred_entries ||
        a.entry_bytes != b.entry_bytes || a.alignment != b.alignment) {
      return Status(ErrorCode::kInvalidState);
    }
  }
  return internal_lapack_workspace::Validate(
      provider, supplied, expected.identity, workspace, operands);
}
// Visit only selected coordinates, never the corner/padding slots.
template <typename T, typename Function>
void Visit(LapackPositiveDefiniteBandView<T> band, Function function) {
  const bool upper = band.triangle() == DenseBlasTriangle::kUpper;
  for (extent_t j = 0; j < band.order(); ++j) {
    const extent_t first = upper ? j - std::min(band.bandwidth(), j) : j;
    const extent_t last =
        upper ? j : j + std::min(band.bandwidth(), band.order() - 1 - j);
    for (extent_t i = first; i <= last; ++i) {
      function(i, j);
    }
  }
}
template <typename T>
extent_t Offset(LapackPositiveDefiniteBandView<T> band, extent_t i,
                extent_t j) {
  const bool upper = band.triangle() == DenseBlasTriangle::kUpper;
  const auto ld = band.storage().leading_dimension();
  if (band.layout() == DenseBlasLayout::kColumnMajor) {
    return j * ld + (upper ? band.bandwidth() - (j - i) : i - j);
  }
  return i * ld + (upper ? j - i : band.bandwidth() - (i - j));
}
template <typename T>
T* PackBand(LapackPositiveDefiniteBandView<T> band,
            std::remove_const_t<T>*& cursor, bool hermitian_input) {
  if (band.layout() == DenseBlasLayout::kColumnMajor || band.order() == 0) {
    return band.storage().data();
  }
  auto* result = cursor;
  const auto width = band.bandwidth() + 1;
  const bool upper = band.triangle() == DenseBlasTriangle::kUpper;
  Visit(band, [&](extent_t i, extent_t j) {
    const auto physical = upper ? band.bandwidth() - (j - i) : i - j;
    if constexpr (DenseBlasComplex<std::remove_const_t<T>>) {
      if (hermitian_input && i == j) {
        result[j * width + physical] = std::remove_const_t<T>(
            band.storage().data()[Offset(band, i, j)].real(), 0);
        return;
      }
    }
    // Raw factors (PBTRS) retain every selected complex component.
    result[j * width + physical] = band.storage().data()[Offset(band, i, j)];
  });
  cursor += band.order() * width;
  return result;
}
template <typename T>
void UnpackBand(const T* packed, LapackPositiveDefiniteBandView<T> band,
                extent_t normalized_diagonal_prefix) {
  if (band.layout() == DenseBlasLayout::kColumnMajor) {
    return;
  }
  const auto width = band.bandwidth() + 1;
  const bool upper = band.triangle() == DenseBlasTriangle::kUpper;
  Visit(band, [&](extent_t i, extent_t j) {
    const auto physical = upper ? band.bandwidth() - (j - i) : i - j;
    if constexpr (DenseBlasComplex<T>) {
      if (i == j) {
        auto& destination = band.storage().data()[Offset(band, i, j)];
        const auto& source = packed[j * width + physical];
        destination.real(source.real());
        if (i < normalized_diagonal_prefix) {
          destination.imag(source.imag());
        }
        return;
      }
    }
    band.storage().data()[Offset(band, i, j)] = packed[j * width + physical];
  });
}
void StartReport(const ReferenceLapackProvider& provider, std::string_view name,
                 LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  std::copy(name.begin(), name.end(), report.routine.begin());
}
Status InterpretInfo(lapack_int info, extent_t order, bool factor,
                     LapackReport& report) {
  report.native_info = info;
  if (info == 0) {
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  if (info < 0) {
    report.outcome = LapackOutcome::kProviderArgument;
    report.output_validity = LapackOutputValidity::kUnusable;
    if (static_cast<std::int64_t>(info) !=
        std::numeric_limits<std::int64_t>::min()) {
      report.native_argument = -static_cast<std::int64_t>(info);
    }
    return Status(ErrorCode::kProvider);
  }
  if (factor && info <= order) {
    report.outcome = LapackOutcome::kNotPositiveDefinite;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = static_cast<index_t>(info) - 1;
    return Status(ErrorCode::kNumerical);
  }
  report.outcome = LapackOutcome::kPartialResult;
  report.output_validity = LapackOutputValidity::kUnusable;
  return Status(ErrorCode::kProvider);
}
template <typename T>
Status Factor(const ReferenceLapackProvider& provider,
              LapackPositiveDefiniteBandView<T> band, bool blocked,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  const std::array operands{band.storage().reachable_storage()};
  auto status = CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  StartReport(provider, Native<T>::kNames[blocked ? 0 : 1], report);
  const auto expected = QueryFactor(provider, band, blocked);
  if (!expected.ok()) {
    return expected.status();
  }
  status = ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  auto* packed = PackBand(band, cursor, true);
  T dummy{};
  char triangle = band.triangle() == DenseBlasTriangle::kUpper ? 'U' : 'L';
  auto n = static_cast<lapack_int>(band.order());
  auto kd = static_cast<lapack_int>(band.bandwidth());
  auto ldab = static_cast<lapack_int>(LeadingDimension(band));
  lapack_int info = 0;
  report.factor_family = LapackFactorFamily::kCholesky;
  report.called_provider = true;
  if (blocked) {
    Native<T>::kFactor(&triangle, &n, &kd, n == 0 ? &dummy : packed, &ldab,
                       &info, 1);
  } else {
    Native<T>::kUnblocked(&triangle, &n, &kd, n == 0 ? &dummy : packed, &ldab,
                          &info, 1);
  }
  status = InterpretInfo(info, band.order(), true, report);
  if (info >= 0 && info <= band.order()) {
    UnpackBand(packed, band,
               internal_band_limits::NormalizedDiagonalPrefix(
                   band.order(), band.bandwidth(), blocked, info));
  }
  return status;
}
template <typename T>
Status Solve(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const T> band,
             DenseBlasMatrixView<T> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{band.storage().reachable_storage(),
                            rhs.reachable_storage()};
  auto status = CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  StartReport(provider, Native<T>::kNames[2], report);
  const auto expected = QuerySolve(provider, band, rhs);
  if (!expected.ok()) {
    return expected.status();
  }
  status = ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const bool empty = band.order() == 0 || rhs.columns() == 0;
  auto* cursor = static_cast<T*>(
      workspace.regions[internal_lapack_layout::kRegion].data());
  T dummy_factor{};
  T dummy_rhs{};
  const auto* packed = empty ? &dummy_factor : PackBand(band, cursor, false);
  auto* packed_rhs =
      empty ? &dummy_rhs : internal_lapack_layout::Pack(rhs, cursor);
  const char triangle =
      band.triangle() == DenseBlasTriangle::kUpper ? 'U' : 'L';
  const auto n = static_cast<lapack_int>(band.order());
  const auto kd = static_cast<lapack_int>(band.bandwidth());
  const auto nrhs = static_cast<lapack_int>(rhs.columns());
  const auto ldab = static_cast<lapack_int>(LeadingDimension(band));
  const auto ldb =
      static_cast<lapack_int>(internal_lapack_layout::LeadingDimension(rhs));
  lapack_int info = 0;
  report.called_provider = true;
  Native<T>::kSolve(&triangle, &n, &kd, &nrhs, packed, &ldab, packed_rhs, &ldb,
                    &info, 1);
  status = InterpretInfo(info, band.order(), false, report);
  if (info == 0 && !empty) {
    internal_lapack_layout::Unpack(packed_rhs, rhs);
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryPbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<float> matrix) {
  return QueryFactor(provider, matrix, true);
}
Result<LapackWorkspacePlan> QueryPbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<double> matrix) {
  return QueryFactor(provider, matrix, true);
}
Result<LapackWorkspacePlan> QueryPbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<float>> matrix) {
  return QueryFactor(provider, matrix, true);
}
Result<LapackWorkspacePlan> QueryPbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<double>> matrix) {
  return QueryFactor(provider, matrix, true);
}
Result<LapackWorkspacePlan> QueryPbtf2Workspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<float> matrix) {
  return QueryFactor(provider, matrix, false);
}
Result<LapackWorkspacePlan> QueryPbtf2Workspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<double> matrix) {
  return QueryFactor(provider, matrix, false);
}
Result<LapackWorkspacePlan> QueryPbtf2Workspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<float>> matrix) {
  return QueryFactor(provider, matrix, false);
}
Result<LapackWorkspacePlan> QueryPbtf2Workspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<double>> matrix) {
  return QueryFactor(provider, matrix, false);
}
Result<LapackWorkspacePlan> QueryPbtrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> factor,
    DenseBlasMatrixView<float> rhs) {
  return QuerySolve(provider, factor, rhs);
}
Result<LapackWorkspacePlan> QueryPbtrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> factor,
    DenseBlasMatrixView<double> rhs) {
  return QuerySolve(provider, factor, rhs);
}
Result<LapackWorkspacePlan> QueryPbtrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return QuerySolve(provider, factor, rhs);
}
Result<LapackWorkspacePlan> QueryPbtrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return QuerySolve(provider, factor, rhs);
}

Status Pbtrf(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<float> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, true, plan, workspace, report);
}
Status Pbtrf(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<double> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, true, plan, workspace, report);
}
Status Pbtrf(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<std::complex<float>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, true, plan, workspace, report);
}
Status Pbtrf(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<std::complex<double>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, true, plan, workspace, report);
}
Status Pbtf2(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<float> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, false, plan, workspace, report);
}
Status Pbtf2(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<double> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, false, plan, workspace, report);
}
Status Pbtf2(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<std::complex<float>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, false, plan, workspace, report);
}
Status Pbtf2(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<std::complex<double>> matrix,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, false, plan, workspace, report);
}
Status Pbtrs(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const float> factor,
             DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, factor, rhs, plan, workspace, report);
}
Status Pbtrs(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const double> factor,
             DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, factor, rhs, plan, workspace, report);
}
Status Pbtrs(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const std::complex<float>> factor,
             DenseBlasMatrixView<std::complex<float>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, factor, rhs, plan, workspace, report);
}
Status Pbtrs(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteBandView<const std::complex<double>> factor,
             DenseBlasMatrixView<std::complex<double>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, factor, rhs, plan, workspace, report);
}
}  // namespace asc
