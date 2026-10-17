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
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_band_equilibration_radix.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
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
  static constexpr std::string_view kName = "sgbequb";
  static constexpr auto kExecute = LAPACK_sgbequb;
};
template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dgbequb";
  static constexpr auto kExecute = LAPACK_dgbequb;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cgbequb";
  static constexpr auto kExecute = LAPACK_cgbequb;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zgbequb";
  static constexpr auto kExecute = LAPACK_zgbequb;
};

template <typename T>
auto Operands(
    ReferenceGeneralBandView<const T> matrix,
    DenseBlasVectorView<DenseBlasRealType<T>> rows,
    DenseBlasVectorView<DenseBlasRealType<T>> columns,
    const LapackEquilibrationStatistics<DenseBlasRealType<T>>& stats) {
  return std::array{matrix.storage().reachable_storage(),
                    rows.reachable_storage(), columns.reachable_storage(),
                    common::Object(stats)};
}

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const T> matrix,
    DenseBlasVectorView<DenseBlasRealType<T>> rows,
    DenseBlasVectorView<DenseBlasRealType<T>> columns,
    const LapackEquilibrationStatistics<DenseBlasRealType<T>>& stats) {
  const auto operands = Operands(matrix, rows, columns, stats);
  for (const auto& status :
       {checked::Compact(provider, matrix),
        checked::Vector(provider, rows, matrix.rows()),
        checked::Vector(provider, columns, matrix.columns()),
        common::Accessible(provider, common::Object(stats)),
        common::Disjoint(std::array{operands[0], operands[1], operands[2],
                                    operands[3], common::Object(provider)}),
        counts::Equilibrate(matrix.rows(), matrix.columns(),
                            matrix.lower_bandwidth(), matrix.upper_bandwidth(),
                            matrix.storage().leading_dimension(),
                            common::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalar,
      std::array{matrix.rows(), matrix.columns(), matrix.lower_bandwidth(),
                 matrix.upper_bandwidth(), matrix.storage().leading_dimension(),
                 rows.size(), rows.increment(), columns.size(),
                 columns.increment()},
      std::array<std::int64_t, 0>{}, provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  return LapackWorkspacePlan{*identity};
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               ReferenceGeneralBandView<const T> matrix,
               DenseBlasVectorView<DenseBlasRealType<T>> rows,
               DenseBlasVectorView<DenseBlasRealType<T>> columns,
               LapackEquilibrationStatistics<DenseBlasRealType<T>>& stats,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const auto operands = Operands(matrix, rows, columns, stats);
  auto status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kName, report);
  const auto expected = Query(provider, matrix, rows, columns, stats);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const auto m = static_cast<lapack_int>(matrix.rows());
  const auto n = static_cast<lapack_int>(matrix.columns());
  const auto kl = static_cast<lapack_int>(matrix.lower_bandwidth());
  const auto ku = static_cast<lapack_int>(matrix.upper_bandwidth());
  const auto ld = static_cast<lapack_int>(matrix.storage().leading_dimension());
  const T matrix_dummy{};
  DenseBlasRealType<T> scale_dummy{};
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native<T>::kExecute(
      &m, &n, &kl, &ku, checked::Nonnull(matrix.storage().data(), matrix_dummy),
      &ld, checked::Nonnull(rows.data(), scale_dummy),
      checked::Nonnull(columns.data(), scale_dummy), &stats.row_condition,
      &stats.column_condition, &stats.absolute_maximum, &info);
  report.native_info = info;
  if (info < 0 || info > matrix.rows() + matrix.columns() ||
      ((m == 0 || n == 0) && info != 0)) {
    return common::Defect(info, report);
  }
  if (info > 0) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info <= m ? info - 1 : info - m - 1;
    return Status(ErrorCode::kNumerical);
  }
  return common::Complete(report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryGbequbWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const float> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics);
}
Status Gbequb(const ReferenceLapackProvider& provider,
              ReferenceGeneralBandView<const float> matrix,
              DenseBlasVectorView<float> row_scales,
              DenseBlasVectorView<float> column_scales,
              LapackEquilibrationStatistics<float>& statistics,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, matrix, row_scales, column_scales, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGbequbWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const double> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics);
}
Status Gbequb(const ReferenceLapackProvider& provider,
              ReferenceGeneralBandView<const double> matrix,
              DenseBlasVectorView<double> row_scales,
              DenseBlasVectorView<double> column_scales,
              LapackEquilibrationStatistics<double>& statistics,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, matrix, row_scales, column_scales, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGbequbWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics);
}
Status Gbequb(const ReferenceLapackProvider& provider,
              ReferenceGeneralBandView<const std::complex<float>> matrix,
              DenseBlasVectorView<float> row_scales,
              DenseBlasVectorView<float> column_scales,
              LapackEquilibrationStatistics<float>& statistics,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, matrix, row_scales, column_scales, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGbequbWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics);
}
Status Gbequb(const ReferenceLapackProvider& provider,
              ReferenceGeneralBandView<const std::complex<double>> matrix,
              DenseBlasVectorView<double> row_scales,
              DenseBlasVectorView<double> column_scales,
              LapackEquilibrationStatistics<double>& statistics,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, matrix, row_scales, column_scales, statistics, plan,
                 workspace, report);
}

}  // namespace asc
