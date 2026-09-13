#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <limits>
#include <span>
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
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_band_expert.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "internal_indefinite.h"
#include "internal_layout.h"
#include "internal_lu_band_driver.h"
#include "internal_lu_band_expert.h"
namespace asc {
namespace {
namespace checked = internal_lu_band_expert;
namespace driver = internal_lu_band_driver;
namespace common = internal_indefinite;
namespace layout = internal_lapack_layout;
using driver::Mode;
using driver::Operands;
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "sgbsvx";
  static constexpr auto kExecute = LAPACK_sgbsvx_base;
};
template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dgbsvx";
  static constexpr auto kExecute = LAPACK_dgbsvx_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cgbsvx";
  static constexpr auto kExecute = LAPACK_cgbsvx_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zgbsvx";
  static constexpr auto kExecute = LAPACK_zgbsvx_base;
};

template <typename T>
struct Buffers {
  using Real = DenseBlasRealType<T>;
  T* a;
  T* af;
  T* b;
  T* x;
  Real* rows;
  Real* columns;
  Real* ferr;
  Real* berr;
  std::array<lapack_int, 4> ld;
};
struct NativeResult {
  lapack_int info = std::numeric_limits<lapack_int>::min();
  char equed = '?';
};
template <typename T, Mode Selected>
NativeResult Call(DenseBlasTranspose operation,
                  const Operands<T, Selected>& data, const Buffers<T>& buffers,
                  lapack_int* pivots,
                  LapackSolveStatistics<DenseBlasRealType<T>>& stats,
                  const LapackWorkspace& workspace) {
  using Real = DenseBlasRealType<T>;
  constexpr std::array kFacts{'N', 'E', 'F'};
  const char fact = kFacts[static_cast<std::size_t>(Selected)];
  const char trans = checked::Transpose(operation);
  const auto n = static_cast<lapack_int>(data.a.rows());
  const auto kl = static_cast<lapack_int>(data.a.lower_bandwidth());
  const auto ku = static_cast<lapack_int>(data.a.upper_bandwidth());
  const auto nrhs = static_cast<lapack_int>(data.b.columns());
  NativeResult result;
  if constexpr (Selected == Mode::kFactored) {
    result.equed = driver::Equed(data.selected);
  }
  T scalar_dummy{};
  auto* work = checked::Nonnull(
      static_cast<T*>(workspace.regions[common::kScalar].data()), scalar_dummy);
  if constexpr (DenseBlasComplex<T>) {
    auto* real = static_cast<Real*>(workspace.regions[checked::kReal].data());
    Native<T>::kExecute(
        &fact, &trans, &n, &kl, &ku, &nrhs, buffers.a, &buffers.ld[0],
        buffers.af, &buffers.ld[1], pivots, &result.equed, buffers.rows,
        buffers.columns, buffers.b, &buffers.ld[2], buffers.x, &buffers.ld[3],
        &stats.reciprocal_condition, buffers.ferr, buffers.berr, work, real,
        &result.info, std::size_t{1}, std::size_t{1}, std::size_t{1});
  } else {
    Native<T>::kExecute(&fact, &trans, &n, &kl, &ku, &nrhs, buffers.a,
                        &buffers.ld[0], buffers.af, &buffers.ld[1], pivots,
                        &result.equed, buffers.rows, buffers.columns, buffers.b,
                        &buffers.ld[2], buffers.x, &buffers.ld[3],
                        &stats.reciprocal_condition, buffers.ferr, buffers.berr,
                        work, pivots + n, &result.info, std::size_t{1},
                        std::size_t{1}, std::size_t{1});
  }
  return result;
}
template <typename T, Mode Selected>
Status ResultStatus(const Operands<T, Selected>& data,
                    const NativeResult& result, const lapack_int* native,
                    LapackReport& report) {
  const auto n = data.a.rows();
  if (result.info < 0 || result.info > n + 1 ||
      (result.info > 0 && result.info <= n && Selected == Mode::kFactored) ||
      (n == 0 && result.info != 0) ||
      (result.equed != 'N' && result.equed != 'R' && result.equed != 'C' &&
       result.equed != 'B') ||
      (Selected == Mode::kNew && result.equed != 'N') ||
      (Selected == Mode::kFactored &&
       result.equed != driver::Equed(data.selected))) {
    return common::Defect(result.info, report);
  }
  if constexpr (Selected != Mode::kFactored) {
    if (!checked::Pivots(
             std::span<const lapack_int>(native, static_cast<std::size_t>(n)),
             n, data.a.lower_bandwidth())
             .ok()) {
      return common::Defect(result.info, report);
    }
  }
  return Status::Ok();
}
template <typename T, Mode Selected>
Status Publish(const Operands<T, Selected>& data, const Buffers<T>& buffers,
               const lapack_int* native, const NativeResult& result,
               LapackEquilibration* equilibration,
               LapackSolveStatistics<DenseBlasRealType<T>>& stats,
               const LapackWorkspace& workspace, LapackReport& report) {
  using Real = DenseBlasRealType<T>;
  if constexpr (Selected != Mode::kFactored) {
    std::copy_n(native, data.a.rows(), data.pivots.data());
  }
  if constexpr (Selected == Mode::kEquilibrated) {
    *equilibration = driver::Scaling(result.equed);
  }
  if constexpr (Selected != Mode::kNew) {
    layout::Unpack(buffers.b, data.b);
  }
  if constexpr (DenseBlasComplex<T>) {
    stats.reciprocal_pivot_growth =
        *static_cast<Real*>(workspace.regions[checked::kReal].data());
  } else {
    stats.reciprocal_pivot_growth =
        *static_cast<T*>(workspace.regions[common::kScalar].data());
  }
  if (result.info > 0 && result.info <= data.a.rows()) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = result.info - 1;
    return Status(ErrorCode::kNumerical);
  }
  layout::Unpack(buffers.x, data.x);
  return driver::Diagnostics(data.ferr, data.berr, stats, data.a.rows(),
                             report);
}
template <typename T, Mode Selected>
Status Invoke(DenseBlasTranspose trans, const Operands<T, Selected>& data,
              LapackEquilibration* equilibration,
              LapackSolveStatistics<DenseBlasRealType<T>>& stats,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  using Real = DenseBlasRealType<T>;
  lapack_int dummy = std::numeric_limits<lapack_int>::min();
  auto* native = checked::Integers(
      workspace, plan.regions[common::kPivot].minimum_entries, dummy);
  if constexpr (Selected == Mode::kFactored) {
    std::copy_n(data.pivots.data(), data.a.rows(), native);
  } else {
    std::fill_n(native, data.a.rows(), std::numeric_limits<lapack_int>::min());
  }
  auto* cursor = static_cast<T*>(workspace.regions[common::kLayout].data());
  auto* b = layout::Pack(data.b, cursor);
  auto* x = driver::Output(data.x, cursor);
  T scalar_dummy{};
  Real real_dummy{};
  // The exact driver declaration admits E's mutations for every FACT value.
  // Source N preserves A/B, F preserves A/AF/R/C; casts expose only those
  // source-readonly operands without copying or extending their lifetimes.
  Buffers<T> buffers{
      checked::Nonnull(const_cast<T*>(data.a.storage().data()), scalar_dummy),
      checked::Nonnull(const_cast<T*>(data.af.storage().data()), scalar_dummy),
      checked::Nonnull(const_cast<T*>(b), scalar_dummy),
      checked::Nonnull(x, scalar_dummy),
      checked::Nonnull(const_cast<Real*>(data.rows.data), real_dummy),
      checked::Nonnull(const_cast<Real*>(data.columns.data), real_dummy),
      checked::Nonnull(data.ferr.data(), real_dummy),
      checked::Nonnull(data.berr.data(), real_dummy),
      {static_cast<lapack_int>(data.a.storage().leading_dimension()),
       static_cast<lapack_int>(data.af.storage().leading_dimension()),
       static_cast<lapack_int>(checked::RhsLeading(data.b)),
       static_cast<lapack_int>(checked::RhsLeading(data.x))}};
  report.called_provider = true;
  const auto result = Call(trans, data, buffers, native, stats, workspace);
  report.native_info = result.info;
  auto status = ResultStatus(data, result, native, report);
  if (!status.ok()) {
    return status;
  }
  return Publish(data, buffers, native, result, equilibration, stats, workspace,
                 report);
}
template <typename T, Mode Selected>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTranspose trans, const Operands<T, Selected>& data,
               LapackEquilibration* equilibration,
               LapackSolveStatistics<DenseBlasRealType<T>>& stats,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  auto status =
      common::Metadata(provider, plan, workspace, report, data.Spans(stats));
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kName, report);
  const auto expected = driver::Query(provider, Native<T>::kName,
                                      Native<T>::kScalar, trans, data, stats);
  if (!expected.ok()) {
    return expected.status();
  }
  status =
      common::Plan(provider, *expected, plan, workspace, data.Spans(stats));
  if (!status.ok()) {
    return status;
  }
  status = driver::Values(data, report);
  if (!status.ok()) {
    return status;
  }
  return Invoke(trans, data, equilibration, stats, plan, workspace, report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryGbsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const float> original,
    LapackLuBandView<float> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<float, Mode::kNew> data{
      original, factors,       pivots,        {}, {}, rhs,
      solution, forward_error, backward_error};
  return driver::Query(provider, Native<float>::kName, Native<float>::kScalar,
                       transpose, data, statistics);
}
Status Gbsvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const float> original,
    LapackLuBandView<float> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<float, Mode::kNew> data{
      original, factors,       pivots,        {}, {}, rhs,
      solution, forward_error, backward_error};
  return Execute(provider, transpose, data, nullptr, statistics, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<float> original, LapackLuBandView<float> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<float, Mode::kEquilibrated> data{
      original,
      factors,
      pivots,
      driver::Scale<float>(row_scales),
      driver::Scale<float>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      LapackEquilibration::kNone,
      common::Object(equilibration)};
  return driver::Query(provider, Native<float>::kName, Native<float>::kScalar,
                       transpose, data, statistics);
}
Status GbsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<float> original, LapackLuBandView<float> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<float, Mode::kEquilibrated> data{
      original,
      factors,
      pivots,
      driver::Scale<float>(row_scales),
      driver::Scale<float>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      LapackEquilibration::kNone,
      common::Object(equilibration)};
  return Execute(provider, transpose, data, &equilibration, statistics, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const float> original,
    LapackLuBandView<const float> factors, ReferenceLuBandPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<float, Mode::kFactored> data{
      original,
      factors,
      pivots.storage(),
      driver::Scale<const float>(row_scales),
      driver::Scale<const float>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      equilibration};
  return driver::Query(provider, Native<float>::kName, Native<float>::kScalar,
                       transpose, data, statistics);
}
Status GbsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const float> original,
    LapackLuBandView<const float> factors, ReferenceLuBandPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<float, Mode::kFactored> data{
      original,
      factors,
      pivots.storage(),
      driver::Scale<const float>(row_scales),
      driver::Scale<const float>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      equilibration};
  return Execute(provider, transpose, data, nullptr, statistics, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const double> original,
    LapackLuBandView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<double, Mode::kNew> data{
      original, factors,       pivots,        {}, {}, rhs,
      solution, forward_error, backward_error};
  return driver::Query(provider, Native<double>::kName, Native<double>::kScalar,
                       transpose, data, statistics);
}
Status Gbsvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const double> original,
    LapackLuBandView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<double, Mode::kNew> data{
      original, factors,       pivots,        {}, {}, rhs,
      solution, forward_error, backward_error};
  return Execute(provider, transpose, data, nullptr, statistics, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<double> original, LapackLuBandView<double> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<double, Mode::kEquilibrated> data{
      original,
      factors,
      pivots,
      driver::Scale<double>(row_scales),
      driver::Scale<double>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      LapackEquilibration::kNone,
      common::Object(equilibration)};
  return driver::Query(provider, Native<double>::kName, Native<double>::kScalar,
                       transpose, data, statistics);
}
Status GbsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<double> original, LapackLuBandView<double> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<double, Mode::kEquilibrated> data{
      original,
      factors,
      pivots,
      driver::Scale<double>(row_scales),
      driver::Scale<double>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      LapackEquilibration::kNone,
      common::Object(equilibration)};
  return Execute(provider, transpose, data, &equilibration, statistics, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const double> original,
    LapackLuBandView<const double> factors, ReferenceLuBandPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<double, Mode::kFactored> data{
      original,
      factors,
      pivots.storage(),
      driver::Scale<const double>(row_scales),
      driver::Scale<const double>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      equilibration};
  return driver::Query(provider, Native<double>::kName, Native<double>::kScalar,
                       transpose, data, statistics);
}
Status GbsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const double> original,
    LapackLuBandView<const double> factors, ReferenceLuBandPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<double, Mode::kFactored> data{
      original,
      factors,
      pivots.storage(),
      driver::Scale<const double>(row_scales),
      driver::Scale<const double>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      equilibration};
  return Execute(provider, transpose, data, nullptr, statistics, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<float>> original,
    LapackLuBandView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<std::complex<float>, Mode::kNew> data{
      original, factors,       pivots,        {}, {}, rhs,
      solution, forward_error, backward_error};
  return driver::Query(provider, Native<std::complex<float>>::kName,
                       Native<std::complex<float>>::kScalar, transpose, data,
                       statistics);
}
Status Gbsvx(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             ReferenceGeneralBandView<const std::complex<float>> original,
             LapackLuBandView<std::complex<float>> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             LapackSolveStatistics<float>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Operands<std::complex<float>, Mode::kNew> data{
      original, factors,       pivots,        {}, {}, rhs,
      solution, forward_error, backward_error};
  return Execute(provider, transpose, data, nullptr, statistics, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<std::complex<float>> original,
    LapackLuBandView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<std::complex<float>, Mode::kEquilibrated> data{
      original,
      factors,
      pivots,
      driver::Scale<float>(row_scales),
      driver::Scale<float>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      LapackEquilibration::kNone,
      common::Object(equilibration)};
  return driver::Query(provider, Native<std::complex<float>>::kName,
                       Native<std::complex<float>>::kScalar, transpose, data,
                       statistics);
}
Status GbsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<std::complex<float>> original,
    LapackLuBandView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<std::complex<float>, Mode::kEquilibrated> data{
      original,
      factors,
      pivots,
      driver::Scale<float>(row_scales),
      driver::Scale<float>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      LapackEquilibration::kNone,
      common::Object(equilibration)};
  return Execute(provider, transpose, data, &equilibration, statistics, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<float>> original,
    LapackLuBandView<const std::complex<float>> factors,
    ReferenceLuBandPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<std::complex<float>, Mode::kFactored> data{
      original,
      factors,
      pivots.storage(),
      driver::Scale<const float>(row_scales),
      driver::Scale<const float>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      equilibration};
  return driver::Query(provider, Native<std::complex<float>>::kName,
                       Native<std::complex<float>>::kScalar, transpose, data,
                       statistics);
}
Status GbsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<float>> original,
    LapackLuBandView<const std::complex<float>> factors,
    ReferenceLuBandPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<std::complex<float>, Mode::kFactored> data{
      original,
      factors,
      pivots.storage(),
      driver::Scale<const float>(row_scales),
      driver::Scale<const float>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      equilibration};
  return Execute(provider, transpose, data, nullptr, statistics, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<double>> original,
    LapackLuBandView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<std::complex<double>, Mode::kNew> data{
      original, factors,       pivots,        {}, {}, rhs,
      solution, forward_error, backward_error};
  return driver::Query(provider, Native<std::complex<double>>::kName,
                       Native<std::complex<double>>::kScalar, transpose, data,
                       statistics);
}
Status Gbsvx(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             ReferenceGeneralBandView<const std::complex<double>> original,
             LapackLuBandView<std::complex<double>> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             LapackSolveStatistics<double>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Operands<std::complex<double>, Mode::kNew> data{
      original, factors,       pivots,        {}, {}, rhs,
      solution, forward_error, backward_error};
  return Execute(provider, transpose, data, nullptr, statistics, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<std::complex<double>> original,
    LapackLuBandView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<std::complex<double>, Mode::kEquilibrated> data{
      original,
      factors,
      pivots,
      driver::Scale<double>(row_scales),
      driver::Scale<double>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      LapackEquilibration::kNone,
      common::Object(equilibration)};
  return driver::Query(provider, Native<std::complex<double>>::kName,
                       Native<std::complex<double>>::kScalar, transpose, data,
                       statistics);
}
Status GbsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<std::complex<double>> original,
    LapackLuBandView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<std::complex<double>, Mode::kEquilibrated> data{
      original,
      factors,
      pivots,
      driver::Scale<double>(row_scales),
      driver::Scale<double>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      LapackEquilibration::kNone,
      common::Object(equilibration)};
  return Execute(provider, transpose, data, &equilibration, statistics, plan,
                 workspace, report);
}
Result<LapackWorkspacePlan> QueryGbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<double>> original,
    LapackLuBandView<const std::complex<double>> factors,
    ReferenceLuBandPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<std::complex<double>, Mode::kFactored> data{
      original,
      factors,
      pivots.storage(),
      driver::Scale<const double>(row_scales),
      driver::Scale<const double>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      equilibration};
  return driver::Query(provider, Native<std::complex<double>>::kName,
                       Native<std::complex<double>>::kScalar, transpose, data,
                       statistics);
}
Status GbsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<double>> original,
    LapackLuBandView<const std::complex<double>> factors,
    ReferenceLuBandPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<std::complex<double>, Mode::kFactored> data{
      original,
      factors,
      pivots.storage(),
      driver::Scale<const double>(row_scales),
      driver::Scale<const double>(column_scales),
      rhs,
      solution,
      forward_error,
      backward_error,
      equilibration};
  return Execute(provider, transpose, data, nullptr, statistics, plan,
                 workspace, report);
}
}  // namespace asc
