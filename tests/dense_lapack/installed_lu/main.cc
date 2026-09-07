#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"

namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;

template <typename T>
T Take(asc::Result<T> value) {
  if (!value.ok()) {
    std::abort();
  }
  return std::move(*value);
}

template <typename T, std::size_t N>
auto Matrix(std::array<T, N>& values, asc::DenseBlasLayout layout = kColumn) {
  return Take(asc::DenseBlasMatrixView<T>::Create(
      values.data(), 2, 2, layout, 2, {values.data(), sizeof(values), kHost}));
}

template <typename T, std::size_t N>
auto Vector(std::array<T, N>& values) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data(), N, 1, {values.data(), sizeof(values), kHost}));
}

template <typename T>
bool Near(T value, double expected) {
  using Real = asc::DenseBlasRealType<T>;
  return std::abs(value - T{static_cast<Real>(expected)}) < Real{0.00001};
}

bool CalledSuccessfully(const asc::LapackReport& report) {
  return report.called_provider && report.native_info == 0 &&
         report.outcome == asc::LapackOutcome::kSuccess;
}

template <typename T>
bool ExerciseLu(const asc::ReferenceLapackProvider& provider) {
  std::array<T, 4> values{T{0}, T{4}, T{2}, T{6}};
  std::array<asc::index_t, 2> pivot_values{};
  auto matrix = Matrix(values);
  auto pivots = Vector(pivot_values);
  alignas(std::max_align_t) std::array<std::byte, 32> integers{};
  std::array<T, 128> scalar_work{};
  asc::LapackWorkspace workspace;
  workspace
      .regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger)] = {
      integers.data(), integers.size(), kHost};
  asc::LapackReport report;
  for (int algorithm = 0; algorithm < 2; ++algorithm) {
    values = {T{0}, T{4}, T{2}, T{6}};
    const auto plan = algorithm == 0
                          ? asc::QueryGetrf2Workspace(provider, matrix, pivots)
                          : asc::QueryGetf2Workspace(provider, matrix, pivots);
    if (!plan.ok()) {
      return false;
    }
    const auto status =
        algorithm == 0
            ? asc::Getrf2(provider, matrix, pivots, *plan, workspace, report)
            : asc::Getf2(provider, matrix, pivots, *plan, workspace, report);
    if (!status.ok() || !CalledSuccessfully(report) || pivot_values[0] != 2 ||
        pivot_values[1] != 2 || !Near(values[0], 4) || !Near(values[1], 0) ||
        !Near(values[2], 6) || !Near(values[3], 2)) {
      return false;
    }
  }
  const auto raw = Take(asc::RawLapackPivotView::Create(
      pivot_values.data(), 2, asc::LapackFactorFamily::kLuPartialPivot,
      {pivot_values.data(), sizeof(pivot_values), kHost}));
  const auto inverse_plan =
      asc::QueryGetriWorkspace(provider, matrix, raw, workspace, report);
  if (!inverse_plan.ok() || !CalledSuccessfully(report)) {
    return false;
  }
  workspace
      .regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)] = {
      scalar_work.data(), sizeof(scalar_work), kHost};
  if (!asc::Getri(provider, matrix, raw, *inverse_plan, workspace, report)
           .ok() ||
      !CalledSuccessfully(report) || !Near(values[0], -0.75) ||
      !Near(values[1], 0.5) || !Near(values[2], 0.25) || !Near(values[3], 0)) {
    return false;
  }
  workspace
      .regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)] = {
      nullptr, 0, kHost};
  values = {T{0}, T{4}, T{2}, T{6}};
  std::array<T, 4> rhs_values{T{6}, T{22}, T{8}, T{32}};
  auto rhs = Matrix(rhs_values);
  const auto driver_plan =
      asc::QueryGesvWorkspace(provider, matrix, pivots, rhs);
  return driver_plan.ok() &&
         asc::Gesv(provider, matrix, pivots, rhs, *driver_plan, workspace,
                   report)
             .ok() &&
         CalledSuccessfully(report) && Near(rhs_values[0], 1) &&
         Near(rhs_values[1], 3) && Near(rhs_values[2], 2) &&
         Near(rhs_values[3], 4);
}

template <typename T>
bool ExerciseEquilibration(const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 4> values{};
  std::array<T, 128> scalar_work{};
  asc::LapackReport report;
  for (auto layout : {kColumn, asc::DenseBlasLayout::kRowMajor}) {
    // A diagonal power-of-two matrix has exact scales for both algorithms.
    values = {T{2}, T{0}, T{0}, T{8}};
    const auto immutable = Take(asc::DenseBlasMatrixView<const T>::Create(
        values.data(), 2, 2, layout, 2,
        {values.data(), sizeof(values), kHost}));
    std::array<Real, 2> row_values{};
    std::array<Real, 2> column_values{};
    auto rows = Vector(row_values);
    auto columns = Vector(column_values);
    asc::LapackEquilibrationStatistics<Real> statistics;
    asc::LapackWorkspace packing;
    if (layout == asc::DenseBlasLayout::kRowMajor) {
      packing.regions[static_cast<std::size_t>(
          asc::LapackWorkspaceKind::kLayoutConversion)] = {
          scalar_work.data(), sizeof(scalar_work), kHost};
    }
    for (int algorithm = 0; algorithm < 2; ++algorithm) {
      const auto plan =
          algorithm == 0 ? asc::QueryGeequWorkspace(provider, immutable, rows,
                                                    columns, statistics)
                         : asc::QueryGeequbWorkspace(provider, immutable, rows,
                                                     columns, statistics);
      if (!plan.ok()) {
        return false;
      }
      const auto status = algorithm == 0
                              ? asc::Geequ(provider, immutable, rows, columns,
                                           statistics, *plan, packing, report)
                              : asc::Geequb(provider, immutable, rows, columns,
                                            statistics, *plan, packing, report);
      if (!status.ok() || !CalledSuccessfully(report) ||
          !Near(row_values[0], 0.5) || !Near(row_values[1], 0.125) ||
          !Near(column_values[0], 1) || !Near(column_values[1], 1) ||
          !Near(statistics.absolute_maximum, 8) ||
          !Near(statistics.row_condition, 0.25) ||
          !Near(statistics.column_condition, 1)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool Exercise(const asc::ReferenceLapackProvider& provider) {
  return ExerciseLu<T>(provider) && ExerciseEquilibration<T>(provider);
}
}  // namespace

int main() {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  return Exercise<float>(provider) && Exercise<double>(provider) &&
                 Exercise<std::complex<float>>(provider) &&
                 Exercise<std::complex<double>>(provider)
             ? 0
             : 1;
}
