#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_support.h"
#include "matrix_support_numerical.h"
#include "normal_return_guard.h"
namespace {
namespace numerical = asc_matrix_support_numerical;
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
constexpr auto kHost = asc::MemorySpace::kHost;
template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}
template <bool Rows, typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasMatrixView<const T> matrix) {
  if constexpr (Rows) {
    return asc::QueryIlalrWorkspace(provider, matrix);
  } else {
    return asc::QueryIlalcWorkspace(provider, matrix);
  }
}
template <bool Rows, typename T>
auto Execute(const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasMatrixView<const T> matrix, asc::index_t& extent,
             const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  if constexpr (Rows) {
    return asc::Ilalr(provider, matrix, extent, plan, workspace, report);
  } else {
    return asc::Ilalc(provider, matrix, extent, plan, workspace, report);
  }
}
template <typename T>
struct Matrix {
  std::array<T, 96> data{};
  int rows;
  int columns;
  int leading;
  bool row_major;
  Matrix(int m, int n, bool row, int padding)
      : rows(m),
        columns(n),
        leading(std::max(1, row ? n : m) + padding),
        row_major(row) {
    data.fill(T{71});
  }
  [[nodiscard]] auto View() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        data.data() + 2, rows, columns,
        row_major ? asc::DenseBlasLayout::kRowMajor
                  : asc::DenseBlasLayout::kColumnMajor,
        leading, {data.data(), sizeof(data), kHost}));
  }
};
template <typename T, bool Rows>
bool Case(const asc::ReferenceLapackProvider& provider, Matrix<T>& matrix,
          int pattern, T marker) {
  numerical::Fill(matrix.data.data() + 2, matrix.rows, matrix.columns,
                  matrix.leading, matrix.row_major, pattern, marker);
  const auto before = numerical::Bits(matrix.data);
  const auto view = matrix.View();
  auto plan = Take(Query<Rows>(provider, view));
  std::array<T, 128> scratch{};
  scratch.fill(T{-17});
  asc::LapackWorkspace workspace;
  const auto count = plan.regions[kLayout].minimum_entries;
  if (count != (matrix.row_major ? matrix.rows * matrix.columns : 0)) {
    return false;
  }
  if (count > 0) {
    workspace.regions[kLayout] = {
        scratch.data() + 1, static_cast<std::size_t>(count) * sizeof(T), kHost};
  }
  std::array<asc::index_t, 3> extent{71, -19, 73};
  asc::LapackReport report;
  for (int repeat = 0; repeat < 2; ++repeat) {
    const auto status =
        Execute<Rows>(provider, view, extent[1], plan, workspace, report);
    if (!status.ok() ||
        report.called_provider != (matrix.rows != 0 && matrix.columns != 0) ||
        report.native_info.has_value() || report.native_argument.has_value() ||
        report.diagnostic_index.has_value() ||
        report.factor_family.has_value() ||
        report.output_validity != asc::LapackOutputValidity::kComplete ||
        report.outcome != asc::LapackOutcome::kSuccess ||
        extent[1] != numerical::Expected<Rows>(
                         matrix.data.data() + 2, matrix.rows, matrix.columns,
                         matrix.leading, matrix.row_major) ||
        extent[0] != 71 || extent[2] != 73 ||
        numerical::Bits(matrix.data) != before) {
      return false;
    }
    for (std::size_t i = 0; i < scratch.size(); ++i) {
      if ((i == 0 || i > static_cast<std::size_t>(count)) &&
          scratch[i] != T{-17}) {
        return false;
      }
    }
  }
  const auto original_extent = extent;
  ++plan.regions[kLayout].minimum_entries;
  const auto rejected =
      Execute<Rows>(provider, view, extent[1], plan, workspace, report);
  return rejected.code() == asc::ErrorCode::kInvalidState &&
         !report.called_provider && !report.native_info.has_value() &&
         report.output_validity == asc::LapackOutputValidity::kUnchanged &&
         extent == original_extent && numerical::Bits(matrix.data) == before;
}
template <typename T, bool Rows>
bool Run(const asc::ReferenceLapackProvider& provider, std::size_t& count) {
  bool pass = true;
  for (bool row : {false, true}) {
    numerical::ForEach<T>(
        [&](int m, int n, int padding, int pattern, T marker) {
          Matrix<T> matrix(m, n, row, padding);
          pass = Case<T, Rows>(provider, matrix, pattern, marker) && pass;
          ++count;
        });
  }
  return pass;
}
template <typename T>
bool Both(const asc::ReferenceLapackProvider& provider, std::size_t& count) {
  const bool columns = Run<T, false>(provider, count);
  const bool rows = Run<T, true>(provider, count);
  return columns && rows;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  std::size_t count = 0;
  const std::array results{Both<float>(provider, count),
                           Both<double>(provider, count),
                           Both<std::complex<float>>(provider, count),
                           Both<std::complex<double>>(provider, count)};
  const bool pass = std::all_of(results.begin(), results.end(),
                                [](bool value) { return value; });
  std::printf(
      "%zu public matrix support fixtures, both layouts and reused outputs: "
      "%s\n",
      count, pass ? "passed" : "failed");
  return pass ? 0 : 1;
}
