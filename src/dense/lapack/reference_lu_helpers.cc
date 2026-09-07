#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "asc/dense/providers/lapack_lu_helpers.h"
#include "internal_lu_helpers.h"
#include "lapack_build_config.h"

#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#elif ASC_LAPACK_INTEGER_BITS != 32
#error "Reference LAPACK requires an explicitly selected 32/64 integer ABI"
#endif
#include <lapack.h>
#include <lapacke_config.h>

#if !defined(__GLIBCXX__) || __GLIBCXX__ != 20230528
#error "Reference LAPACK complex ABI requires the audited libstdc++ build"
#endif

// LAQGE is absent from pinned lapack.h. These private declarations match the
// exact GNU Fortran 11.4 compiler's -fc-prototypes-external output for all four
// pinned sources, with -fdefault-integer-8 for the true ILP64 build. The output
// explicitly uses std::complex in C++ and a trailing size_t character length.
// Source/compiler identities and direct-call evidence are in the helper review.
extern "C" {
void slaqge_(lapack_int* m, lapack_int* n, float* a, lapack_int* lda, float* r,
             float* c, float* rowcnd, float* colcnd, float* amax, char* equed,
             std::size_t equed_length);
void dlaqge_(lapack_int* m, lapack_int* n, double* a, lapack_int* lda,
             double* r, double* c, double* rowcnd, double* colcnd, double* amax,
             char* equed, std::size_t equed_length);
void claqge_(lapack_int* m, lapack_int* n, std::complex<float>* a,
             lapack_int* lda, float* r, float* c, float* rowcnd, float* colcnd,
             float* amax, char* equed, std::size_t equed_length);
void zlaqge_(lapack_int* m, lapack_int* n, std::complex<double>* a,
             lapack_int* lda, double* r, double* c, double* rowcnd,
             double* colcnd, double* amax, char* equed,
             std::size_t equed_length);
}

namespace asc {
namespace {
constexpr auto kInteger =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
constexpr auto kLayout =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);
constexpr extent_t kIntegerMax = std::numeric_limits<lapack_int>::max();
static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kSwap = "slaswp";
  static constexpr std::string_view kScale = "slaqge";
  static constexpr auto kSwapCall = LAPACK_slaswp;
  static constexpr auto kScaleCall = slaqge_;
};
template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kSwap = "dlaswp";
  static constexpr std::string_view kScale = "dlaqge";
  static constexpr auto kSwapCall = LAPACK_dlaswp;
  static constexpr auto kScaleCall = dlaqge_;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kSwap = "claswp";
  static constexpr std::string_view kScale = "claqge";
  static constexpr auto kSwapCall = LAPACK_claswp;
  static constexpr auto kScaleCall = claqge_;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kSwap = "zlaswp";
  static constexpr std::string_view kScale = "zlaqge";
  static constexpr auto kSwapCall = LAPACK_zlaswp;
  static constexpr auto kScaleCall = zlaqge_;
};

Status CheckOperands(const ReferenceLapackProvider& provider,
                     std::span<const ConstMemoryView> operands) {
  for (const auto operand : operands) {
    const auto space = operand.space();
    if ((space != MemorySpace::kHost && space != MemorySpace::kPinnedHost) ||
        !provider.context().CanAccess(space)) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  for (std::size_t i = 0; i < operands.size(); ++i) {
    const auto a = reinterpret_cast<std::uintptr_t>(operands[i].data());
    for (std::size_t j = 0; j < i; ++j) {
      const auto b = reinterpret_cast<std::uintptr_t>(operands[j].data());
      if (operands[i].size() != 0 && operands[j].size() != 0 &&
          a < b + operands[j].size() && b < a + operands[i].size()) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

template <typename T>
extent_t LeadingDimension(DenseBlasMatrixView<T> matrix) {
  return std::max<extent_t>(1, matrix.layout() == DenseBlasLayout::kColumnMajor
                                   ? matrix.leading_dimension()
                                   : matrix.rows());
}

template <typename T>
Status SetPacking(DenseBlasMatrixView<T> matrix, bool active,
                  LapackWorkspacePlan& plan) {
  if (active && matrix.layout() == DenseBlasLayout::kRowMajor) {
    if (matrix.rows() != 0 &&
        matrix.columns() >
            std::numeric_limits<extent_t>::max() / matrix.rows()) {
      return Status(ErrorCode::kOverflow);
    }
    const auto entries = matrix.rows() * matrix.columns();
    if (static_cast<std::uint64_t>(entries) >
        std::numeric_limits<std::size_t>::max() / sizeof(T)) {
      return Status(ErrorCode::kOverflow);
    }
    plan.regions[kLayout] = {entries, entries, sizeof(T), alignof(T)};
  }
  std::size_t remaining = plan.total_byte_limit;
  for (const auto& region : plan.regions) {
    const auto entries = static_cast<std::uint64_t>(region.preferred_entries);
    if (entries > remaining / region.entry_bytes) {
      return Status(ErrorCode::kOverflow);
    }
    remaining -= static_cast<std::size_t>(entries) * region.entry_bytes;
  }
  return Status::Ok();
}

Status CheckPlan(const ReferenceLapackProvider& provider,
                 const LapackWorkspacePlan& expected,
                 const LapackWorkspacePlan& supplied,
                 const LapackWorkspace& workspace,
                 std::span<const ConstMemoryView> operands) {
  for (const auto& region : workspace.regions) {
    if (!provider.context().CanAccess(region.space())) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
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
  return ValidateLapackWorkspace(supplied, expected.identity, workspace,
                                 operands);
}

template <typename T>
T* Pack(DenseBlasMatrixView<T> matrix, const LapackWorkspace& workspace,
        bool reads_matrix) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    return matrix.data();
  }
  auto* packed = static_cast<T*>(workspace.regions[kLayout].data());
  if (reads_matrix) {
    for (extent_t j = 0; j < matrix.columns(); ++j) {
      for (extent_t i = 0; i < matrix.rows(); ++i) {
        packed[j * matrix.rows() + i] =
            matrix.data()[i * matrix.leading_dimension() + j];
      }
    }
  }
  return packed;
}

template <typename T>
void Unpack(const T* packed, DenseBlasMatrixView<T> matrix) {
  if (matrix.layout() == DenseBlasLayout::kRowMajor) {
    for (extent_t j = 0; j < matrix.columns(); ++j) {
      for (extent_t i = 0; i < matrix.rows(); ++i) {
        matrix.data()[i * matrix.leading_dimension() + j] =
            packed[j * matrix.rows() + i];
      }
    }
  }
}

void BeginReport(const ReferenceLapackProvider& provider, std::string_view name,
                 LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  std::copy(name.begin(), name.end(), report.routine.begin());
}

Status Complete(LapackReport& report) {
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

// Called only after SwapCapacity proved every selected position representable.
extent_t PivotSlot(index_t first_row, extent_t step, index_t increment) {
  const auto magnitude =
      increment < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(increment)
                    : static_cast<std::uint64_t>(increment);
  return first_row +
         static_cast<extent_t>(static_cast<std::uint64_t>(step) * magnitude);
}

template <typename T>
Result<LapackWorkspacePlan> SwapQuery(const ReferenceLapackProvider& provider,
                                      DenseBlasMatrixView<T> matrix,
                                      index_t first_row, extent_t row_count,
                                      RawLapackPivotView pivots,
                                      index_t increment) {
  Status status = CheckOperands(
      provider,
      std::array{matrix.reachable_storage(), pivots.reachable_storage()});
  if (!status.ok()) {
    return status;
  }
  if (pivots.family() != LapackFactorFamily::kLuPartialPivot) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto capacity = internal_lu_helpers::SwapCapacity(
      matrix.rows(), matrix.columns(), LeadingDimension(matrix), first_row,
      row_count, increment, kIntegerMax);
  if (!capacity.ok()) {
    return capacity.status();
  }
  if (static_cast<std::uint64_t>(*capacity) > pivots.values().size()) {
    return Status(ErrorCode::kShape);
  }
  if (*capacity != 0) {
    for (extent_t i = 0; i < row_count; ++i) {
      const auto value = pivots.values()[PivotSlot(first_row, i, increment)];
      if (value < 1 || value > matrix.rows()) {
        return Status(ErrorCode::kIndex);
      }
    }
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kSwap, Native<T>::kScalar,
      std::array{matrix.rows(), matrix.columns(), LeadingDimension(matrix),
                 row_count},
      std::array<std::int64_t, 6>{
          first_row, increment, static_cast<std::int64_t>(matrix.layout()),
          matrix.leading_dimension(),
          static_cast<std::int64_t>(pivots.values().size()),
          static_cast<std::int64_t>(pivots.family())},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  if (*capacity != 0) {
    plan.regions[kInteger] = {*capacity, *capacity, sizeof(lapack_int),
                              alignof(lapack_int)};
  }
  status = SetPacking(matrix, *capacity != 0, plan);
  if (!status.ok()) {
    return status;
  }
  return plan;
}

template <typename T>
Status Swap(const ReferenceLapackProvider& provider,
            DenseBlasMatrixView<T> matrix, index_t first_row,
            extent_t row_count, RawLapackPivotView pivots, index_t increment,
            const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
            LapackReport& report) {
  BeginReport(provider, Native<T>::kSwap, report);
  const auto expected =
      SwapQuery(provider, matrix, first_row, row_count, pivots, increment);
  if (!expected.ok()) {
    return expected.status();
  }
  Status status = CheckPlan(
      provider, *expected, plan, workspace,
      std::array{matrix.reachable_storage(), pivots.reachable_storage()});
  if (!status.ok()) {
    return status;
  }
  if (expected->regions[kInteger].minimum_entries == 0) {
    return Complete(report);
  }
  auto* converted =
      static_cast<lapack_int*>(workspace.regions[kInteger].data());
  for (extent_t i = 0; i < row_count; ++i) {
    const auto slot = PivotSlot(first_row, i, increment);
    std::construct_at(converted + slot,
                      static_cast<lapack_int>(pivots.values()[slot]));
  }
  auto* packed = Pack(matrix, workspace, true);
  const auto n = static_cast<lapack_int>(matrix.columns());
  const auto lda = static_cast<lapack_int>(LeadingDimension(matrix));
  const auto k1 = static_cast<lapack_int>(first_row + 1);
  const auto k2 = static_cast<lapack_int>(first_row + row_count);
  const auto inc = static_cast<lapack_int>(increment);
  report.called_provider = true;
  Native<T>::kSwapCall(&n, packed, &lda, &k1, &k2, converted, &inc);
  Unpack(packed, matrix);
  return Complete(report);
}

template <typename Real>
std::int64_t Bits(Real value) {
  if constexpr (std::is_same_v<Real, float>) {
    return std::bit_cast<std::uint32_t>(value);
  } else {
    return std::bit_cast<std::int64_t>(value);
  }
}

template <typename Real>
Result<LapackEquilibration> ScaleMode(
    bool empty, const LapackEquilibrationStatistics<Real>& statistics) {
  if (empty) {
    return LapackEquilibration::kNone;
  }
  for (Real value : {statistics.row_condition, statistics.column_condition}) {
    if (!std::isfinite(value) || value < 0 || value > 1) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  if (!std::isfinite(statistics.absolute_maximum) ||
      statistics.absolute_maximum < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  // Exact pinned IEEE binary32/binary64 LAMCH(S)/LAMCH(P) values, independently
  // checked against direct calls. No foreign query or approximate threshold.
  constexpr Real kSmall =
      std::numeric_limits<Real>::min() / std::numeric_limits<Real>::epsilon();
  constexpr Real kLarge = Real{1} / kSmall;
  const bool rows = statistics.row_condition < Real{0.1} ||
                    statistics.absolute_maximum < kSmall ||
                    statistics.absolute_maximum > kLarge;
  const bool columns = statistics.column_condition < Real{0.1};
  if (rows) {
    return columns ? LapackEquilibration::kBoth : LapackEquilibration::kRows;
  }
  return columns ? LapackEquilibration::kColumns : LapackEquilibration::kNone;
}

bool UsesRows(LapackEquilibration mode) {
  return mode == LapackEquilibration::kRows ||
         mode == LapackEquilibration::kBoth;
}
bool UsesColumns(LapackEquilibration mode) {
  return mode == LapackEquilibration::kColumns ||
         mode == LapackEquilibration::kBoth;
}
char Equed(LapackEquilibration mode) {
  switch (mode) {
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

template <typename Real>
Status CheckScales(DenseBlasVectorView<const Real> scales, extent_t expected,
                   bool used) {
  if (scales.size() != expected && (used || scales.size() != 0)) {
    return Status(ErrorCode::kShape);
  }
  if (scales.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (used) {
    for (extent_t i = 0; i < scales.size(); ++i) {
      if (!std::isfinite(scales.data()[i]) || scales.data()[i] <= 0) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

template <typename T>
auto ScaleOperands(
    DenseBlasMatrixView<T> matrix,
    DenseBlasVectorView<const DenseBlasRealType<T>> rows,
    DenseBlasVectorView<const DenseBlasRealType<T>> columns,
    const LapackEquilibrationStatistics<DenseBlasRealType<T>>& statistics,
    const LapackEquilibration& applied) {
  return std::array{
      matrix.reachable_storage(), rows.reachable_storage(),
      columns.reachable_storage(),
      ConstMemoryView(&statistics, sizeof(statistics), MemorySpace::kHost),
      ConstMemoryView(&applied, sizeof(applied), MemorySpace::kHost)};
}

template <typename T>
Result<LapackWorkspacePlan> ScaleQuery(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<T> matrix,
    DenseBlasVectorView<const DenseBlasRealType<T>> rows,
    DenseBlasVectorView<const DenseBlasRealType<T>> columns,
    const LapackEquilibrationStatistics<DenseBlasRealType<T>>& statistics,
    const LapackEquilibration& applied) {
  Status status = CheckOperands(
      provider, ScaleOperands(matrix, rows, columns, statistics, applied));
  if (!status.ok()) {
    return status;
  }
  const bool empty = matrix.rows() == 0 || matrix.columns() == 0;
  const auto mode = ScaleMode(empty, statistics);
  if (!mode.ok()) {
    return mode.status();
  }
  status = internal_lu_helpers::ScaleDimensions(
      matrix.rows(), matrix.columns(), LeadingDimension(matrix),
      *mode != LapackEquilibration::kNone, kIntegerMax);
  if (!status.ok()) {
    return status;
  }
  status = CheckScales(rows, matrix.rows(), UsesRows(*mode));
  if (!status.ok()) {
    return status;
  }
  status = CheckScales(columns, matrix.columns(), UsesColumns(*mode));
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kScale, Native<T>::kScalar,
      std::array{matrix.rows(), matrix.columns(), LeadingDimension(matrix),
                 rows.size(), columns.size()},
      std::array<std::int64_t, 7>{
          static_cast<std::int64_t>(matrix.layout()),
          matrix.leading_dimension(), rows.increment(), columns.increment(),
          empty ? 0 : Bits(statistics.row_condition),
          empty ? 0 : Bits(statistics.column_condition),
          empty ? 0 : Bits(statistics.absolute_maximum)},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  status = SetPacking(matrix, !empty, plan);
  if (!status.ok()) {
    return status;
  }
  return plan;
}

template <typename T>
char NativeScale(DenseBlasMatrixView<T> matrix, T* packed,
                 DenseBlasVectorView<const DenseBlasRealType<T>> rows,
                 DenseBlasVectorView<const DenseBlasRealType<T>> columns,
                 LapackEquilibrationStatistics<DenseBlasRealType<T>> statistics,
                 LapackEquilibration mode) {
  using Real = DenseBlasRealType<T>;
  auto m = static_cast<lapack_int>(matrix.rows());
  auto n = static_cast<lapack_int>(matrix.columns());
  auto lda = static_cast<lapack_int>(LeadingDimension(matrix));
  Real dummy = 1;
  // Pinned Fortran has no INTENT annotations, but never writes R/C. Unselected
  // arrays are not accessed; a live dummy satisfies the borrowed argument ABI.
  auto* r = UsesRows(mode) ? const_cast<Real*>(rows.data()) : &dummy;
  auto* c = UsesColumns(mode) ? const_cast<Real*>(columns.data()) : &dummy;
  char applied = '?';
  Native<T>::kScaleCall(&m, &n, packed, &lda, r, c, &statistics.row_condition,
                        &statistics.column_condition,
                        &statistics.absolute_maximum, &applied, std::size_t{1});
  return applied;
}

template <typename T>
Status CheckScaledOutput(DenseBlasMatrixView<T> matrix, LapackReport& report) {
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    for (extent_t i = 0; i < matrix.rows(); ++i) {
      const auto offset = matrix.layout() == DenseBlasLayout::kColumnMajor
                              ? j * matrix.leading_dimension() + i
                              : i * matrix.leading_dimension() + j;
      const T value = matrix.data()[offset];
      if (!std::isfinite(std::real(value)) ||
          !std::isfinite(std::imag(value))) {
        report.outcome = LapackOutcome::kAccuracyWarning;
        report.output_validity = LapackOutputValidity::kDocumentedPartial;
        return Status(ErrorCode::kNumerical);
      }
    }
  }
  return Complete(report);
}

template <typename T>
Status Scale(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<T> matrix,
    DenseBlasVectorView<const DenseBlasRealType<T>> rows,
    DenseBlasVectorView<const DenseBlasRealType<T>> columns,
    const LapackEquilibrationStatistics<DenseBlasRealType<T>>& statistics,
    LapackEquilibration& applied, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  BeginReport(provider, Native<T>::kScale, report);
  const auto expected =
      ScaleQuery(provider, matrix, rows, columns, statistics, applied);
  if (!expected.ok()) {
    return expected.status();
  }
  Status status =
      CheckPlan(provider, *expected, plan, workspace,
                ScaleOperands(matrix, rows, columns, statistics, applied));
  if (!status.ok()) {
    return status;
  }
  const bool empty = matrix.rows() == 0 || matrix.columns() == 0;
  if (empty) {
    applied = LapackEquilibration::kNone;
    return Complete(report);
  }
  const auto mode = *ScaleMode(false, statistics);
  auto* packed = Pack(matrix, workspace, mode != LapackEquilibration::kNone);
  report.called_provider = true;
  const char equed =
      NativeScale(matrix, packed, rows, columns, statistics, mode);
  if (equed != Equed(mode)) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  if (mode != LapackEquilibration::kNone) {
    Unpack(packed, matrix);
  }
  applied = mode;
  return mode == LapackEquilibration::kNone ? Complete(report)
                                            : CheckScaledOutput(matrix, report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryLaswpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    index_t first_row, extent_t row_count, RawLapackPivotView pivots,
    index_t pivot_increment) {
  return SwapQuery(provider, matrix, first_row, row_count, pivots,
                   pivot_increment);
}
Status Laswp(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix, index_t first_row,
             extent_t row_count, RawLapackPivotView pivots,
             index_t pivot_increment, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Swap(provider, matrix, first_row, row_count, pivots, pivot_increment,
              plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLaswpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    index_t first_row, extent_t row_count, RawLapackPivotView pivots,
    index_t pivot_increment) {
  return SwapQuery(provider, matrix, first_row, row_count, pivots,
                   pivot_increment);
}
Status Laswp(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix, index_t first_row,
             extent_t row_count, RawLapackPivotView pivots,
             index_t pivot_increment, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Swap(provider, matrix, first_row, row_count, pivots, pivot_increment,
              plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLaswpWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix, index_t first_row,
    extent_t row_count, RawLapackPivotView pivots, index_t pivot_increment) {
  return SwapQuery(provider, matrix, first_row, row_count, pivots,
                   pivot_increment);
}
Status Laswp(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix, index_t first_row,
             extent_t row_count, RawLapackPivotView pivots,
             index_t pivot_increment, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Swap(provider, matrix, first_row, row_count, pivots, pivot_increment,
              plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLaswpWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix, index_t first_row,
    extent_t row_count, RawLapackPivotView pivots, index_t pivot_increment) {
  return SwapQuery(provider, matrix, first_row, row_count, pivots,
                   pivot_increment);
}
Status Laswp(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             index_t first_row, extent_t row_count, RawLapackPivotView pivots,
             index_t pivot_increment, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Swap(provider, matrix, first_row, row_count, pivots, pivot_increment,
              plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryLaqgeWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics,
    const LapackEquilibration& applied) {
  return ScaleQuery(provider, matrix, row_scales, column_scales, statistics,
                    applied);
}
Status Laqge(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<const float> row_scales,
             DenseBlasVectorView<const float> column_scales,
             const LapackEquilibrationStatistics<float>& statistics,
             LapackEquilibration& applied, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Scale(provider, matrix, row_scales, column_scales, statistics, applied,
               plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLaqgeWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics,
    const LapackEquilibration& applied) {
  return ScaleQuery(provider, matrix, row_scales, column_scales, statistics,
                    applied);
}
Status Laqge(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<const double> row_scales,
             DenseBlasVectorView<const double> column_scales,
             const LapackEquilibrationStatistics<double>& statistics,
             LapackEquilibration& applied, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Scale(provider, matrix, row_scales, column_scales, statistics, applied,
               plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLaqgeWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics,
    const LapackEquilibration& applied) {
  return ScaleQuery(provider, matrix, row_scales, column_scales, statistics,
                    applied);
}
Status Laqge(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<const float> row_scales,
             DenseBlasVectorView<const float> column_scales,
             const LapackEquilibrationStatistics<float>& statistics,
             LapackEquilibration& applied, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Scale(provider, matrix, row_scales, column_scales, statistics, applied,
               plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLaqgeWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics,
    const LapackEquilibration& applied) {
  return ScaleQuery(provider, matrix, row_scales, column_scales, statistics,
                    applied);
}
Status Laqge(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<const double> row_scales,
             DenseBlasVectorView<const double> column_scales,
             const LapackEquilibrationStatistics<double>& statistics,
             LapackEquilibration& applied, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Scale(provider, matrix, row_scales, column_scales, statistics, applied,
               plan, workspace, report);
}

}  // namespace asc
