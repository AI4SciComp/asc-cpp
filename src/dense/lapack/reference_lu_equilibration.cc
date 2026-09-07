#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
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

namespace asc {
namespace {
constexpr std::size_t kLayout =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);
static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kScalar = LapackScalarKind::kF32;
  static constexpr std::string_view kOrdinary = "sgeequ";
  static constexpr std::string_view kRadix = "sgeequb";
  static lapack_int Execute(bool radix, lapack_int m, lapack_int n,
                            const float* a, lapack_int lda, float* rows,
                            float* columns,
                            LapackEquilibrationStatistics<float>& statistics) {
    lapack_int info = 0;
    if (radix) {
      LAPACK_sgeequb(&m, &n, a, &lda, rows, columns, &statistics.row_condition,
                     &statistics.column_condition, &statistics.absolute_maximum,
                     &info);
    } else {
      LAPACK_sgeequ(&m, &n, a, &lda, rows, columns, &statistics.row_condition,
                    &statistics.column_condition, &statistics.absolute_maximum,
                    &info);
    }
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kScalar = LapackScalarKind::kF64;
  static constexpr std::string_view kOrdinary = "dgeequ";
  static constexpr std::string_view kRadix = "dgeequb";
  static lapack_int Execute(bool radix, lapack_int m, lapack_int n,
                            const double* a, lapack_int lda, double* rows,
                            double* columns,
                            LapackEquilibrationStatistics<double>& statistics) {
    lapack_int info = 0;
    if (radix) {
      LAPACK_dgeequb(&m, &n, a, &lda, rows, columns, &statistics.row_condition,
                     &statistics.column_condition, &statistics.absolute_maximum,
                     &info);
    } else {
      LAPACK_dgeequ(&m, &n, a, &lda, rows, columns, &statistics.row_condition,
                    &statistics.column_condition, &statistics.absolute_maximum,
                    &info);
    }
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalar = LapackScalarKind::kC64;
  static constexpr std::string_view kOrdinary = "cgeequ";
  static constexpr std::string_view kRadix = "cgeequb";
  static lapack_int Execute(bool radix, lapack_int m, lapack_int n,
                            const std::complex<float>* a, lapack_int lda,
                            float* rows, float* columns,
                            LapackEquilibrationStatistics<float>& statistics) {
    lapack_int info = 0;
    if (radix) {
      LAPACK_cgeequb(&m, &n, a, &lda, rows, columns, &statistics.row_condition,
                     &statistics.column_condition, &statistics.absolute_maximum,
                     &info);
    } else {
      LAPACK_cgeequ(&m, &n, a, &lda, rows, columns, &statistics.row_condition,
                    &statistics.column_condition, &statistics.absolute_maximum,
                    &info);
    }
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalar = LapackScalarKind::kC128;
  static constexpr std::string_view kOrdinary = "zgeequ";
  static constexpr std::string_view kRadix = "zgeequb";
  static lapack_int Execute(bool radix, lapack_int m, lapack_int n,
                            const std::complex<double>* a, lapack_int lda,
                            double* rows, double* columns,
                            LapackEquilibrationStatistics<double>& statistics) {
    lapack_int info = 0;
    if (radix) {
      LAPACK_zgeequb(&m, &n, a, &lda, rows, columns, &statistics.row_condition,
                     &statistics.column_condition, &statistics.absolute_maximum,
                     &info);
    } else {
      LAPACK_zgeequ(&m, &n, a, &lda, rows, columns, &statistics.row_condition,
                    &statistics.column_condition, &statistics.absolute_maximum,
                    &info);
    }
    return info;
  }
};

bool Overlap(ConstMemoryView first, ConstMemoryView second) {
  const auto a = reinterpret_cast<std::uintptr_t>(first.data());
  const auto b = reinterpret_cast<std::uintptr_t>(second.data());
  return first.size() != 0 && second.size() != 0 && a < b + second.size() &&
         b < a + first.size();
}

template <typename T>
Status Validate(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const T> matrix,
    DenseBlasVectorView<DenseBlasRealType<T>> rows,
    DenseBlasVectorView<DenseBlasRealType<T>> columns,
    const LapackEquilibrationStatistics<DenseBlasRealType<T>>& stats) {
  for (const auto space :
       {matrix.memory_space(), rows.memory_space(), columns.memory_space()}) {
    if ((space != MemorySpace::kHost && space != MemorySpace::kPinnedHost) ||
        !provider.context().CanAccess(space)) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  for (extent_t value :
       {matrix.rows(), matrix.columns(), matrix.leading_dimension()}) {
    if (value > std::numeric_limits<lapack_int>::max()) {
      return Status(ErrorCode::kOverflow);
    }
  }
  // The provider reports a zero column as INFO=M+J, in its integer ABI.
  if (matrix.rows() >
      std::numeric_limits<lapack_int>::max() - matrix.columns()) {
    return Status(ErrorCode::kOverflow);
  }
  if (rows.size() != matrix.rows() || columns.size() != matrix.columns()) {
    return Status(ErrorCode::kShape);
  }
  if (rows.increment() != 1 || columns.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const std::array operands{
      matrix.reachable_storage(), rows.reachable_storage(),
      columns.reachable_storage(),
      ConstMemoryView(&stats, sizeof(stats), MemorySpace::kHost)};
  for (std::size_t i = 0; i < operands.size(); ++i) {
    for (std::size_t j = 0; j < i; ++j) {
      if (Overlap(operands[i], operands[j])) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const T> matrix,
    DenseBlasVectorView<DenseBlasRealType<T>> rows,
    DenseBlasVectorView<DenseBlasRealType<T>> columns,
    const LapackEquilibrationStatistics<DenseBlasRealType<T>>& stats,
    bool radix) {
  Status status = Validate(provider, matrix, rows, columns, stats);
  if (!status.ok()) {
    return status;
  }
  extent_t entries = 0;
  if (matrix.layout() == DenseBlasLayout::kRowMajor && matrix.rows() != 0) {
    if (matrix.columns() >
        std::numeric_limits<extent_t>::max() / matrix.rows()) {
      return Status(ErrorCode::kOverflow);
    }
    entries = matrix.rows() * matrix.columns();
    if (static_cast<std::uint64_t>(entries) >
        std::numeric_limits<std::size_t>::max() / sizeof(T)) {
      return Status(ErrorCode::kOverflow);
    }
  }
  const auto key = LapackPlanIdentity::Create(
      radix ? Native<T>::kRadix : Native<T>::kOrdinary, Native<T>::kScalar,
      std::array{matrix.rows(), matrix.columns(), matrix.leading_dimension(),
                 rows.size(), columns.size()},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(matrix.layout()),
                                  rows.increment(), columns.increment()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  if (matrix.layout() == DenseBlasLayout::kRowMajor) {
    plan.regions[kLayout] = {entries, entries, sizeof(T), alignof(T)};
  }
  return plan;
}

Status ValidatePlan(const LapackWorkspacePlan& expected,
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
  return ValidateLapackWorkspace(supplied, expected.identity, workspace,
                                 operands);
}

template <typename T>
const T* PackedMatrix(DenseBlasMatrixView<const T> matrix,
                      const LapackWorkspace& workspace) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    return matrix.data();
  }
  auto* packed = static_cast<T*>(workspace.regions[kLayout].data());
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    for (extent_t i = 0; i < matrix.rows(); ++i) {
      packed[j * matrix.rows() + i] =
          matrix.data()[i * matrix.leading_dimension() + j];
    }
  }
  return packed;
}

Status InterpretInfo(lapack_int info, extent_t rows, extent_t columns,
                     bool radix, LapackReport& report) {
  report.native_info = info;
  if (info < 0) {
    report.outcome = LapackOutcome::kProviderArgument;
    report.output_validity = LapackOutputValidity::kUnusable;
    if (info != std::numeric_limits<lapack_int>::min()) {
      report.native_argument = -static_cast<std::int64_t>(info);
    }
    return Status(ErrorCode::kProvider);
  }
  if (info > rows + columns) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  if (info > 0) {
    // Radix exponentiation can underflow to a zero scale for a nonzero row.
    // Preserve the routine failure without falsely certifying singular A.
    report.outcome =
        radix ? LapackOutcome::kPartialResult : LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info <= rows ? info - 1 : info - rows - 1;
    return Status(ErrorCode::kNumerical);
  }
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

template <typename T>
Status Equilibrate(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const T> matrix,
    DenseBlasVectorView<DenseBlasRealType<T>> rows,
    DenseBlasVectorView<DenseBlasRealType<T>> columns,
    LapackEquilibrationStatistics<DenseBlasRealType<T>>& statistics, bool radix,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  const auto name = radix ? Native<T>::kRadix : Native<T>::kOrdinary;
  std::copy(name.begin(), name.end(), report.routine.begin());
  const auto expected =
      Query(provider, matrix, rows, columns, statistics, radix);
  if (!expected.ok()) {
    return expected.status();
  }
  Status status = ValidatePlan(
      *expected, plan, workspace,
      std::array{matrix.reachable_storage(), rows.reachable_storage(),
                 columns.reachable_storage(),
                 ConstMemoryView(&statistics, sizeof(statistics),
                                 MemorySpace::kHost)});
  if (!status.ok()) {
    return status;
  }
  if (matrix.rows() == 0 || matrix.columns() == 0) {
    statistics = {1, 1, 0};
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  const auto* packed = PackedMatrix(matrix, workspace);
  const auto ld = matrix.layout() == DenseBlasLayout::kColumnMajor
                      ? matrix.leading_dimension()
                      : matrix.rows();
  report.called_provider = true;
  const lapack_int info = Native<T>::Execute(
      radix, static_cast<lapack_int>(matrix.rows()),
      static_cast<lapack_int>(matrix.columns()), packed,
      static_cast<lapack_int>(ld), rows.data(), columns.data(), statistics);
  return InterpretInfo(info, matrix.rows(), matrix.columns(), radix, report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryGeequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics, false);
}
Status Geequ(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const float> matrix,
             DenseBlasVectorView<float> row_scales,
             DenseBlasVectorView<float> column_scales,
             LapackEquilibrationStatistics<float>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Equilibrate(provider, matrix, row_scales, column_scales, statistics,
                     false, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics, true);
}
Status Geequb(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<const float> matrix,
              DenseBlasVectorView<float> row_scales,
              DenseBlasVectorView<float> column_scales,
              LapackEquilibrationStatistics<float>& statistics,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Equilibrate(provider, matrix, row_scales, column_scales, statistics,
                     true, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics, false);
}
Status Geequ(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const double> matrix,
             DenseBlasVectorView<double> row_scales,
             DenseBlasVectorView<double> column_scales,
             LapackEquilibrationStatistics<double>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Equilibrate(provider, matrix, row_scales, column_scales, statistics,
                     false, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics, true);
}
Status Geequb(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<const double> matrix,
              DenseBlasVectorView<double> row_scales,
              DenseBlasVectorView<double> column_scales,
              LapackEquilibrationStatistics<double>& statistics,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Equilibrate(provider, matrix, row_scales, column_scales, statistics,
                     true, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics, false);
}
Status Geequ(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const std::complex<float>> matrix,
             DenseBlasVectorView<float> row_scales,
             DenseBlasVectorView<float> column_scales,
             LapackEquilibrationStatistics<float>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Equilibrate(provider, matrix, row_scales, column_scales, statistics,
                     false, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics, true);
}
Status Geequb(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<const std::complex<float>> matrix,
              DenseBlasVectorView<float> row_scales,
              DenseBlasVectorView<float> column_scales,
              LapackEquilibrationStatistics<float>& statistics,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Equilibrate(provider, matrix, row_scales, column_scales, statistics,
                     true, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics, false);
}
Status Geequ(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const std::complex<double>> matrix,
             DenseBlasVectorView<double> row_scales,
             DenseBlasVectorView<double> column_scales,
             LapackEquilibrationStatistics<double>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Equilibrate(provider, matrix, row_scales, column_scales, statistics,
                     false, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGeequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics) {
  return Query(provider, matrix, row_scales, column_scales, statistics, true);
}
Status Geequb(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<const std::complex<double>> matrix,
              DenseBlasVectorView<double> row_scales,
              DenseBlasVectorView<double> column_scales,
              LapackEquilibrationStatistics<double>& statistics,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Equilibrate(provider, matrix, row_scales, column_scales, statistics,
                     true, plan, workspace, report);
}

}  // namespace asc
