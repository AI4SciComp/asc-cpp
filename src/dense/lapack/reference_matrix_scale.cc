#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
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
#include "asc/dense/providers/lapack_matrix_scale.h"
#include "internal_layout.h"
#include "internal_matrix_scale_counts.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;
namespace counts = internal_matrix_scale_counts;

char PartCode(LapackMatrixScalePart part) {
  switch (part) {
    case LapackMatrixScalePart::kAll:
      return 'G';
    case LapackMatrixScalePart::kLower:
      return 'L';
    case LapackMatrixScalePart::kUpper:
      return 'U';
    case LapackMatrixScalePart::kUpperHessenberg:
      return 'H';
  }
  return '?';
}

template <typename T>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return "slascl";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dlascl";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return "clascl";
  } else {
    return "zlascl";
  }
}

template <typename T>
struct Operand {
  DenseBlasMatrixView<T> storage;
  extent_t rows;
  extent_t columns;
  extent_t lower;
  extent_t upper;
  char type;

  [[nodiscard]] bool IsRow() const {
    return storage.layout() == DenseBlasLayout::kRowMajor;
  }

  [[nodiscard]] extent_t NativeRow(extent_t i, extent_t j) const {
    if (type == 'B') {
      return i - j;
    }
    if (type == 'Q') {
      return upper - (j - i);
    }
    if (type == 'Z') {
      return i >= j ? lower + upper + (i - j) : lower + upper - (j - i);
    }
    return i;
  }

  [[nodiscard]] T& Entry(extent_t i, extent_t j) const {
    const auto leading = storage.leading_dimension();
    if (!IsRow()) {
      return storage.data()[j * leading + NativeRow(i, j)];
    }
    if (type == 'B') {
      return storage.data()[i * leading + (lower - (i - j))];
    }
    if (type == 'Q') {
      return storage.data()[i * leading + j - i];
    }
    return storage.data()[i * leading + j];
  }

  template <typename Function>
  void ForEachSelected(Function function) const {
    for (extent_t j = 0; j < columns; ++j) {
      extent_t begin = 0;
      extent_t end = rows;
      if (type == 'L') {
        begin = std::min(j, rows);
      } else if (type == 'U') {
        end = std::min(rows, j + 1);
      } else if (type == 'H') {
        end = std::min(rows, j + 2);
      } else if (type == 'B' || type == 'Q' || type == 'Z') {
        begin = type == 'B' ? j : std::max<extent_t>(0, j - upper);
        if (type == 'Q') {
          end = j + 1;
        } else if (j < rows) {
          // Bound before adding; rows and columns may have independent widths.
          end = j + std::min(lower, rows - 1 - j) + 1;
        }
      }
      for (extent_t i = begin; i < end; ++i) {
        function(i, j);
      }
    }
  }
};

template <typename T>
Operand<T> Make(LapackMatrixScalePart part, DenseBlasMatrixView<T> matrix) {
  return {matrix, matrix.rows(), matrix.columns(), 0, 0, PartCode(part)};
}

template <typename T>
Operand<T> Make(LapackPositiveDefiniteBandView<T> matrix) {
  return {matrix.storage(),
          matrix.order(),
          matrix.order(),
          matrix.bandwidth(),
          matrix.bandwidth(),
          matrix.triangle() == DenseBlasTriangle::kLower ? 'B' : 'Q'};
}

template <typename T>
Operand<T> Make(LapackLuBandView<T> matrix) {
  return {matrix.storage(),         matrix.rows(),
          matrix.columns(),         matrix.lower_bandwidth(),
          matrix.upper_bandwidth(), 'Z'};
}

template <typename T>
Result<counts::Counts> Counts(Operand<T> matrix) {
  return counts::Count(matrix.type, matrix.rows, matrix.columns, matrix.lower,
                       matrix.upper, matrix.storage.leading_dimension(),
                       matrix.IsRow(), checked::kLimit);
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  Operand<T> matrix) {
  const auto dimensions = Counts(matrix);
  if (!dimensions.ok()) {
    return dimensions.status();
  }
  Status status = checked::Access(provider, matrix.storage.reachable_storage());
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Routine<T>(), checked::Kind<T>(),
      std::array{matrix.rows, matrix.columns, matrix.lower, matrix.upper,
                 matrix.storage.leading_dimension()},
      std::array<std::int64_t, 2>{
          matrix.type, static_cast<std::int64_t>(matrix.storage.layout())},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  if (dimensions->packing_entries != 0) {
    plan.regions[checked::kLayout] = {dimensions->packing_entries,
                                      dimensions->packing_entries, sizeof(T),
                                      alignof(T)};
  }
  status = internal_lapack_layout::CheckTotal(plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

template <typename T>
void Native(char type, lapack_int lower, lapack_int upper,
            DenseBlasRealType<T> cfrom, DenseBlasRealType<T> cto, lapack_int m,
            lapack_int n, T* matrix, lapack_int leading, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_slascl(&type, &lower, &upper, &cfrom, &cto, &m, &n, matrix, &leading,
                  &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dlascl(&type, &lower, &upper, &cfrom, &cto, &m, &n, matrix, &leading,
                  &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_clascl(&type, &lower, &upper, &cfrom, &cto, &m, &n, matrix, &leading,
                  &info);
  } else {
    LAPACK_zlascl(&type, &lower, &upper, &cfrom, &cto, &m, &n, matrix, &leading,
                  &info);
  }
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider, Operand<T> matrix,
               DenseBlasRealType<T> cfrom, DenseBlasRealType<T> cto,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  checked::StartReport(provider, Routine<T>(), report);
  const auto expected = Query(provider, matrix);
  if (!expected.ok()) {
    return expected.status();
  }
  if (cfrom == 0 || std::isnan(cfrom) || std::isnan(cto)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const std::array spans{ConstMemoryView(matrix.storage.reachable_storage())};
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (status.ok()) {
    status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  }
  if (!status.ok()) {
    return status;
  }
  if (matrix.rows == 0 || matrix.columns == 0 ||
      (std::isfinite(cfrom) && cfrom == cto)) {
    return checked::Complete(report);
  }
  const auto dimensions = Counts(matrix);
  T* values = matrix.storage.data();
  const auto leading = dimensions->leading;
  if (matrix.IsRow()) {
    values = static_cast<T*>(workspace.regions[checked::kLayout].data());
    matrix.ForEachSelected([&](extent_t i, extent_t j) {
      values[j * leading + matrix.NativeRow(i, j)] = matrix.Entry(i, j);
    });
  }
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native(matrix.type, static_cast<lapack_int>(matrix.lower),
         static_cast<lapack_int>(matrix.upper), cfrom, cto,
         static_cast<lapack_int>(matrix.rows),
         static_cast<lapack_int>(matrix.columns), values,
         static_cast<lapack_int>(leading), info);
  report.native_info = info;
  if (info != 0) {
    status = checked::ProviderDefect(info, report);
    if (matrix.IsRow()) {
      report.output_validity = LapackOutputValidity::kUnchanged;
    }
    return status;
  }
  if (matrix.IsRow()) {
    matrix.ForEachSelected([&](extent_t i, extent_t j) {
      matrix.Entry(i, j) = values[j * leading + matrix.NativeRow(i, j)];
    });
  }
  return checked::Complete(report);
}
}  // namespace

#define ASC_MATRIX_SCALE(SCALAR, REAL)                                        \
  Result<LapackWorkspacePlan> QueryLasclWorkspace(                            \
      const ReferenceLapackProvider& provider, LapackMatrixScalePart part,    \
      DenseBlasMatrixView<SCALAR> matrix) {                                   \
    return Query(provider, Make(part, matrix));                               \
  }                                                                           \
  Status Lascl(const ReferenceLapackProvider& provider,                       \
               LapackMatrixScalePart part, REAL cfrom, REAL cto,              \
               DenseBlasMatrixView<SCALAR> matrix,                            \
               const LapackWorkspacePlan& plan,                               \
               const LapackWorkspace& workspace, LapackReport& report) {      \
    return Execute(provider, Make(part, matrix), cfrom, cto, plan, workspace, \
                   report);                                                   \
  }                                                                           \
  Result<LapackWorkspacePlan> QueryLasclWorkspace(                            \
      const ReferenceLapackProvider& provider,                                \
      LapackPositiveDefiniteBandView<SCALAR> matrix) {                        \
    return Query(provider, Make(matrix));                                     \
  }                                                                           \
  Status Lascl(const ReferenceLapackProvider& provider, REAL cfrom, REAL cto, \
               LapackPositiveDefiniteBandView<SCALAR> matrix,                 \
               const LapackWorkspacePlan& plan,                               \
               const LapackWorkspace& workspace, LapackReport& report) {      \
    return Execute(provider, Make(matrix), cfrom, cto, plan, workspace,       \
                   report);                                                   \
  }                                                                           \
  Result<LapackWorkspacePlan> QueryLasclWorkspace(                            \
      const ReferenceLapackProvider& provider,                                \
      LapackLuBandView<SCALAR> matrix) {                                      \
    return Query(provider, Make(matrix));                                     \
  }                                                                           \
  Status Lascl(const ReferenceLapackProvider& provider, REAL cfrom, REAL cto, \
               LapackLuBandView<SCALAR> matrix,                               \
               const LapackWorkspacePlan& plan,                               \
               const LapackWorkspace& workspace, LapackReport& report) {      \
    return Execute(provider, Make(matrix), cfrom, cto, plan, workspace,       \
                   report);                                                   \
  }
ASC_MATRIX_SCALE(float, float)
ASC_MATRIX_SCALE(double, double)
ASC_MATRIX_SCALE(std::complex<float>, float)
ASC_MATRIX_SCALE(std::complex<double>, double)
#undef ASC_MATRIX_SCALE
}  // namespace asc
