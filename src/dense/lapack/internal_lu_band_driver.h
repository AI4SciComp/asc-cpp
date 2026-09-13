#ifndef ASC_DENSE_LAPACK_INTERNAL_LU_BAND_DRIVER_H_
#define ASC_DENSE_LAPACK_INTERNAL_LU_BAND_DRIVER_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

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
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "internal_layout.h"
#include "internal_lu_band_expert.h"
#include "internal_lu_band_expert_counts.h"
namespace asc::internal_lu_band_driver {
namespace checked = internal_lu_band_expert;
namespace common = internal_indefinite;
namespace counts = internal_lu_band_expert_counts;
namespace layout = internal_lapack_layout;
enum class Mode : std::uint8_t { kNew = 0, kEquilibrated = 1, kFactored = 2 };
inline bool RowScaled(LapackEquilibration selected) {
  return selected == LapackEquilibration::kRows ||
         selected == LapackEquilibration::kBoth;
}
inline bool ColumnScaled(LapackEquilibration selected) {
  return selected == LapackEquilibration::kColumns ||
         selected == LapackEquilibration::kBoth;
}
inline char Equed(LapackEquilibration selected) {
  switch (selected) {
    case LapackEquilibration::kNone:
      return 'N';
    case LapackEquilibration::kRows:
      return 'R';
    case LapackEquilibration::kColumns:
      return 'C';
    case LapackEquilibration::kBoth:
      return 'B';
  }
  return '?';
}
inline LapackEquilibration Scaling(char equed) {
  switch (equed) {
    case 'R':
      return LapackEquilibration::kRows;
    case 'C':
      return LapackEquilibration::kColumns;
    case 'B':
      return LapackEquilibration::kBoth;
    default:
      return LapackEquilibration::kNone;
  }
}
template <typename T>
struct Scale {
  T* data = nullptr;
  extent_t size = 0;
  stride_t increment = 1;
  ConstMemoryView span{nullptr, 0, MemorySpace::kHost};
  Scale() = default;
  explicit Scale(DenseBlasVectorView<T> view)
      : data(view.data()),
        size(view.size()),
        increment(view.increment()),
        span(view.reachable_storage()) {}
};
template <typename T, Mode Selected>
struct Operands {
  using AElement =
      std::conditional_t<Selected == Mode::kEquilibrated, T, const T>;
  using AfElement = std::conditional_t<Selected == Mode::kFactored, const T, T>;
  using BElement = std::conditional_t<Selected == Mode::kNew, const T, T>;
  using PivotElement =
      std::conditional_t<Selected == Mode::kFactored, const index_t, index_t>;
  using Real = DenseBlasRealType<T>;
  using ScaleElement =
      std::conditional_t<Selected == Mode::kFactored, const Real, Real>;
  ReferenceGeneralBandView<AElement> a;
  LapackLuBandView<AfElement> af;
  DenseBlasVectorView<PivotElement> pivots;
  Scale<ScaleElement> rows;
  Scale<ScaleElement> columns;
  DenseBlasMatrixView<BElement> b;
  DenseBlasMatrixView<T> x;
  DenseBlasVectorView<Real> ferr;
  DenseBlasVectorView<Real> berr;
  LapackEquilibration selected = LapackEquilibration::kNone;
  ConstMemoryView equilibration_output{nullptr, 0, MemorySpace::kHost};
  [[nodiscard]] auto Spans(const LapackSolveStatistics<Real>& stats) const {
    return std::array{a.storage().reachable_storage(),
                      af.storage().reachable_storage(),
                      pivots.reachable_storage(),
                      rows.span,
                      columns.span,
                      b.reachable_storage(),
                      x.reachable_storage(),
                      ferr.reachable_storage(),
                      berr.reachable_storage(),
                      equilibration_output,
                      common::Object(stats)};
  }
};
template <typename T, Mode Selected>
Status Scales(const Operands<T, Selected>& data) {
  const auto n = data.a.rows();
  if constexpr (Selected == Mode::kFactored) {
    if (Equed(data.selected) == '?') {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  const bool rows = Selected == Mode::kEquilibrated || RowScaled(data.selected);
  const bool columns =
      Selected == Mode::kEquilibrated || ColumnScaled(data.selected);
  if (data.rows.increment != 1 || data.columns.increment != 1 ||
      (rows ? data.rows.size != n
            : data.rows.size != 0 && data.rows.size != n) ||
      (columns ? data.columns.size != n
               : data.columns.size != 0 && data.columns.size != n)) {
    return Status(ErrorCode::kShape);
  }
  return Status::Ok();
}
template <typename T, Mode Selected>
Status Structure(const ReferenceLapackProvider& provider,
                 DenseBlasTranspose trans, const Operands<T, Selected>& data,
                 const LapackSolveStatistics<DenseBlasRealType<T>>& stats) {
  if (!checked::Operation(trans)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto n = data.a.rows();
  if (data.a.columns() != n || data.af.rows() != n || data.af.columns() != n ||
      data.a.lower_bandwidth() != data.af.lower_bandwidth() ||
      data.a.upper_bandwidth() != data.af.upper_bandwidth() ||
      data.b.rows() != n || data.x.rows() != n ||
      data.b.columns() != data.x.columns()) {
    return Status(ErrorCode::kShape);
  }
  for (const auto span : data.Spans(stats)) {
    auto status = common::Accessible(provider, span);
    if (!status.ok()) {
      return status;
    }
    if (common::Overlap(span, common::Object(provider))) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  for (const auto& status :
       {Scales(data), checked::Vector(provider, data.pivots, n),
        checked::Vector(provider, data.ferr, data.b.columns()),
        checked::Vector(provider, data.berr, data.b.columns()),
        common::Disjoint(data.Spans(stats)),
        counts::Driver(n, data.a.lower_bandwidth(), data.a.upper_bandwidth(),
                       data.a.storage().leading_dimension(),
                       data.af.storage().leading_dimension(), data.b.columns(),
                       checked::RhsLeading(data.b), checked::RhsLeading(data.x),
                       static_cast<int>(Selected), common::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}
template <typename T, Mode Selected>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider, std::string_view name,
    LapackScalarKind scalar, DenseBlasTranspose trans,
    const Operands<T, Selected>& data,
    const LapackSolveStatistics<DenseBlasRealType<T>>& stats) {
  const auto status = Structure(provider, trans, data, stats);
  if (!status.ok()) {
    return status;
  }
  const auto identity = LapackPlanIdentity::Create(
      name, scalar,
      std::array{data.a.rows(), data.a.lower_bandwidth(),
                 data.a.upper_bandwidth(), data.a.storage().leading_dimension(),
                 data.af.storage().leading_dimension(), data.b.columns(),
                 checked::RhsLeading(data.b), checked::RhsLeading(data.x),
                 data.rows.size, data.rows.increment, data.columns.size,
                 data.columns.increment},
      std::array<std::int64_t, 7>{
          static_cast<std::int64_t>(Selected), static_cast<std::int64_t>(trans),
          static_cast<std::int64_t>(data.b.layout()),
          data.b.leading_dimension(),
          static_cast<std::int64_t>(data.x.layout()),
          data.x.leading_dimension(), static_cast<std::int64_t>(data.selected)},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  for (const auto& result :
       {checked::EstimateWorkspace<T>(data.a.rows(), plan, true),
        layout::AddPacking(data.b, plan), layout::AddPacking(data.x, plan)}) {
    if (!result.ok()) {
      return result;
    }
  }
  return plan;
}
template <typename T, Mode Selected>
Status Values(const Operands<T, Selected>& data, LapackReport& report) {
  if constexpr (Selected == Mode::kFactored) {
    const auto pivots = std::span<const index_t>(
        data.pivots.data(), static_cast<std::size_t>(data.pivots.size()));
    auto status =
        checked::Pivots(pivots, data.a.rows(), data.a.lower_bandwidth());
    if (!status.ok()) {
      return status;
    }
    for (extent_t i = 0; i < data.a.rows(); ++i) {
      if ((RowScaled(data.selected) &&
           (!std::isfinite(data.rows.data[i]) || data.rows.data[i] <= 0)) ||
          (ColumnScaled(data.selected) &&
           (!std::isfinite(data.columns.data[i]) ||
            data.columns.data[i] <= 0))) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    status = checked::ZeroDiagonal(data.af, report);
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}
template <typename T>
T* Output(DenseBlasMatrixView<T> matrix, T*& cursor) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor || matrix.rows() == 0 ||
      matrix.columns() == 0) {
    return matrix.data();
  }
  auto* result = cursor;
  cursor += matrix.rows() * matrix.columns();
  return result;
}
template <typename T>
Status Diagnostics(DenseBlasVectorView<T> ferr, DenseBlasVectorView<T> berr,
                   const LapackSolveStatistics<T>& stats, extent_t n,
                   LapackReport& report) {
  if (stats.reciprocal_condition < 0 || stats.reciprocal_pivot_growth < 0) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kProvider);
  }
  auto status = checked::Errors(ferr, berr, report);
  if (!status.ok()) {
    return status;
  }
  if (!std::isfinite(stats.reciprocal_condition) ||
      !std::isfinite(stats.reciprocal_pivot_growth) ||
      report.native_info == n + 1) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return Status::Ok();
}
}  // namespace asc::internal_lu_band_driver
#endif  // ASC_DENSE_LAPACK_INTERNAL_LU_BAND_DRIVER_H_
