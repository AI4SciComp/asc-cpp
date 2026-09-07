#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; nonallocating placement array construction.
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
#include "internal_workspace_context.h"
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
constexpr auto kScalar = static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
constexpr auto kReal = static_cast<std::size_t>(LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
constexpr auto kLayout =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);
static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);
enum class Mode : char { kNew = 'N', kEquilibrate = 'E', kSupplied = 'F' };

bool Rows(LapackEquilibration equed) {
  return equed == LapackEquilibration::kRows ||
         equed == LapackEquilibration::kBoth;
}
bool Columns(LapackEquilibration equed) {
  return equed == LapackEquilibration::kColumns ||
         equed == LapackEquilibration::kBoth;
}
char EquedChar(LapackEquilibration equed) {
  switch (equed) {
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
char TransChar(DenseBlasTranspose transpose) {
  switch (transpose) {
    case DenseBlasTranspose::kNone:
      return 'N';
    case DenseBlasTranspose::kTranspose:
      return 'T';
    case DenseBlasTranspose::kConjugateTranspose:
      return 'C';
  }
  return '?';
}

template <typename T, Mode M>
struct Operands {
  using Real = DenseBlasRealType<T>;
  using A = std::conditional_t<M == Mode::kEquilibrate, T, const T>;
  using Af = std::conditional_t<M == Mode::kSupplied, const T, T>;
  using B = std::conditional_t<M == Mode::kNew, const T, T>;
  using Scale = std::conditional_t<M == Mode::kEquilibrate, Real, const Real>;
  using Pivot = std::conditional_t<M == Mode::kSupplied, RawLapackPivotView,
                                   DenseBlasVectorView<index_t>>;
  DenseBlasMatrixView<A> a;
  DenseBlasMatrixView<Af> af;
  Pivot pivots;
  DenseBlasVectorView<Scale> r;
  DenseBlasVectorView<Scale> c;
  DenseBlasMatrixView<B> b;
  DenseBlasMatrixView<T> x;
  DenseBlasVectorView<Real> ferr;
  DenseBlasVectorView<Real> berr;
  const LapackSolveStatistics<Real>& statistics;
  const LapackEquilibration* equed_address;
  LapackEquilibration equed;

  [[nodiscard]] extent_t PivotSize() const {
    if constexpr (M == Mode::kSupplied) {
      return static_cast<extent_t>(pivots.values().size());
    } else {
      return pivots.size();
    }
  }
  [[nodiscard]] stride_t PivotStride() const {
    if constexpr (M == Mode::kSupplied) {
      return 1;
    } else {
      return pivots.increment();
    }
  }
  [[nodiscard]] std::array<ConstMemoryView, 11> Spans() const {
    return {
        a.reachable_storage(),
        af.reachable_storage(),
        pivots.reachable_storage(),
        r.reachable_storage(),
        c.reachable_storage(),
        b.reachable_storage(),
        x.reachable_storage(),
        ferr.reachable_storage(),
        berr.reachable_storage(),
        ConstMemoryView(&statistics, sizeof(statistics), MemorySpace::kHost),
        ConstMemoryView(equed_address,
                        equed_address == nullptr ? 0 : sizeof(*equed_address),
                        MemorySpace::kHost)};
  }
};

template <typename T>
struct Packed {
  T* a;
  T* af;
  T* b;
  T* x;
  std::array<lapack_int, 4> ld;
};

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr std::string_view kName = "sgesvx";
  static constexpr auto kScalarKind = LapackScalarKind::kF32;
  static lapack_int Execute(char fact, char trans, lapack_int n,
                            lapack_int nrhs, const Packed<float>& data,
                            lapack_int* pivots, char& equed, float* r, float* c,
                            float& rcond, float* ferr, float* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    LAPACK_sgesvx(&fact, &trans, &n, &nrhs, data.a, data.ld.data(), data.af,
                  &data.ld[1], pivots, &equed, r, c, data.b, &data.ld[2],
                  data.x, &data.ld[3], &rcond, ferr, berr,
                  static_cast<float*>(workspace.regions[kScalar].data()),
                  pivots + n, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr std::string_view kName = "dgesvx";
  static constexpr auto kScalarKind = LapackScalarKind::kF64;
  static lapack_int Execute(char fact, char trans, lapack_int n,
                            lapack_int nrhs, const Packed<double>& data,
                            lapack_int* pivots, char& equed, double* r,
                            double* c, double& rcond, double* ferr,
                            double* berr, const LapackWorkspace& workspace) {
    lapack_int info = 0;
    LAPACK_dgesvx(&fact, &trans, &n, &nrhs, data.a, data.ld.data(), data.af,
                  &data.ld[1], pivots, &equed, r, c, data.b, &data.ld[2],
                  data.x, &data.ld[3], &rcond, ferr, berr,
                  static_cast<double*>(workspace.regions[kScalar].data()),
                  pivots + n, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr std::string_view kName = "cgesvx";
  static constexpr auto kScalarKind = LapackScalarKind::kC64;
  static lapack_int Execute(char fact, char trans, lapack_int n,
                            lapack_int nrhs,
                            const Packed<std::complex<float>>& data,
                            lapack_int* pivots, char& equed, float* r, float* c,
                            float& rcond, float* ferr, float* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    LAPACK_cgesvx(
        &fact, &trans, &n, &nrhs, data.a, data.ld.data(), data.af, &data.ld[1],
        pivots, &equed, r, c, data.b, &data.ld[2], data.x, &data.ld[3], &rcond,
        ferr, berr,
        static_cast<std::complex<float>*>(workspace.regions[kScalar].data()),
        static_cast<float*>(workspace.regions[kReal].data()), &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr std::string_view kName = "zgesvx";
  static constexpr auto kScalarKind = LapackScalarKind::kC128;
  static lapack_int Execute(char fact, char trans, lapack_int n,
                            lapack_int nrhs,
                            const Packed<std::complex<double>>& data,
                            lapack_int* pivots, char& equed, double* r,
                            double* c, double& rcond, double* ferr,
                            double* berr, const LapackWorkspace& workspace) {
    lapack_int info = 0;
    LAPACK_zgesvx(
        &fact, &trans, &n, &nrhs, data.a, data.ld.data(), data.af, &data.ld[1],
        pivots, &equed, r, c, data.b, &data.ld[2], data.x, &data.ld[3], &rcond,
        ferr, berr,
        static_cast<std::complex<double>*>(workspace.regions[kScalar].data()),
        static_cast<double*>(workspace.regions[kReal].data()), &info);
    return info;
  }
};

template <typename T>
DenseBlasVectorView<const T> EmptyVector() {
  return *DenseBlasVectorView<const T>::Create(
      nullptr, 0, 1, {nullptr, 0, MemorySpace::kHost});
}

bool Overlap(ConstMemoryView first, ConstMemoryView second) {
  if (first.size() == 0 || second.size() == 0) {
    return false;
  }
  const auto a = reinterpret_cast<std::uintptr_t>(first.data());
  const auto b = reinterpret_cast<std::uintptr_t>(second.data());
  return a < b + second.size() && b < a + first.size();
}

template <typename T>
extent_t ForeignLd(DenseBlasMatrixView<T> matrix) {
  return std::max<extent_t>(1, matrix.layout() == DenseBlasLayout::kColumnMajor
                                   ? matrix.leading_dimension()
                                   : matrix.rows());
}

template <typename Scale>
Status ValidateScale(DenseBlasVectorView<Scale> scales, extent_t n,
                     bool required, bool inspect) {
  if (scales.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (scales.size() != n && (required || scales.size() != 0)) {
    return Status(ErrorCode::kShape);
  }
  if (inspect) {
    for (extent_t i = 0; i < n; ++i) {
      if (!std::isfinite(scales.data()[i]) || scales.data()[i] <= 0) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

template <typename T, Mode M>
Status Validate(const ReferenceLapackProvider& provider,
                DenseBlasTranspose transpose, const Operands<T, M>& values) {
  if (TransChar(transpose) == '?' || EquedChar(values.equed) == '?') {
    return Status(ErrorCode::kInvalidArgument);
  }
  const extent_t n = values.a.rows();
  const extent_t nrhs = values.b.columns();
  if (values.a.columns() != n || values.af.rows() != n ||
      values.af.columns() != n || values.b.rows() != n ||
      values.x.rows() != n || values.x.columns() != nrhs ||
      values.PivotSize() != n || values.ferr.size() != nrhs ||
      values.berr.size() != nrhs) {
    return Status(ErrorCode::kShape);
  }
  if (values.PivotStride() != 1 || values.ferr.increment() != 1 ||
      values.berr.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  // Real GECON WORK=4*N; complex LACN2 computes 3*N; GERFS uses N+1.
  if (n > std::numeric_limits<lapack_int>::max() /
              (DenseBlasComplex<T> ? 3 : 4) ||
      nrhs >= std::numeric_limits<lapack_int>::max()) {
    return Status(ErrorCode::kOverflow);
  }
  for (extent_t ld : {ForeignLd(values.a), ForeignLd(values.af),
                      ForeignLd(values.b), ForeignLd(values.x)}) {
    if (ld > std::numeric_limits<lapack_int>::max()) {
      return Status(ErrorCode::kOverflow);
    }
  }
  const auto spans = values.Spans();
  for (std::size_t i = 0; i < spans.size(); ++i) {
    if ((spans[i].space() != MemorySpace::kHost &&
         spans[i].space() != MemorySpace::kPinnedHost) ||
        !provider.context().CanAccess(spans[i].space())) {
      return Status(ErrorCode::kMemoryAccess);
    }
    for (std::size_t j = 0; j < i; ++j) {
      if (Overlap(spans[i], spans[j])) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  auto status =
      ValidateScale(values.r, n, M == Mode::kEquilibrate || Rows(values.equed),
                    M == Mode::kSupplied && Rows(values.equed));
  if (!status.ok()) {
    return status;
  }
  status = ValidateScale(values.c, n,
                         M == Mode::kEquilibrate || Columns(values.equed),
                         M == Mode::kSupplied && Columns(values.equed));
  if (!status.ok()) {
    return status;
  }
  if constexpr (M == Mode::kSupplied) {
    return ValidateLuPivots(values.pivots, n);
  }
  return Status::Ok();
}

template <typename T>
Status AddPacking(DenseBlasMatrixView<T> matrix, extent_t& count) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor || matrix.rows() == 0) {
    return Status::Ok();
  }
  if (matrix.columns() > std::numeric_limits<extent_t>::max() / matrix.rows()) {
    return Status(ErrorCode::kOverflow);
  }
  const auto entries = matrix.rows() * matrix.columns();
  if (entries > std::numeric_limits<extent_t>::max() - count) {
    return Status(ErrorCode::kOverflow);
  }
  count += entries;
  return Status::Ok();
}

bool TotalFits(const LapackWorkspacePlan& plan) {
  std::size_t remaining = std::numeric_limits<std::size_t>::max();
  for (const auto& region : plan.regions) {
    const auto count = static_cast<std::uint64_t>(region.minimum_entries);
    if (count > remaining / region.entry_bytes) {
      return false;
    }
    remaining -= static_cast<std::size_t>(count) * region.entry_bytes;
  }
  return true;
}

template <typename T, Mode M>
Result<LapackWorkspacePlan> MakePlan(const ReferenceLapackProvider& provider,
                                     DenseBlasTranspose transpose,
                                     const Operands<T, M>& v,
                                     extent_t packing) {
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalarKind,
      std::array{v.a.rows(), v.b.columns(), ForeignLd(v.a), ForeignLd(v.af),
                 ForeignLd(v.b), ForeignLd(v.x), v.PivotSize(), v.r.size(),
                 v.c.size(), v.ferr.size(), v.berr.size()},
      std::array<std::int64_t, 16>{
          static_cast<std::int64_t>(M), static_cast<std::int64_t>(transpose),
          static_cast<std::int64_t>(v.equed),
          static_cast<std::int64_t>(v.a.layout()),
          static_cast<std::int64_t>(v.af.layout()),
          static_cast<std::int64_t>(v.b.layout()),
          static_cast<std::int64_t>(v.x.layout()), v.a.leading_dimension(),
          v.af.leading_dimension(), v.b.leading_dimension(),
          v.x.leading_dimension(), v.PivotStride(), v.r.increment(),
          v.c.increment(), v.ferr.increment(), v.berr.increment()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  const extent_t n = v.a.rows();
  const extent_t scalar =
      DenseBlasComplex<T> ? 2 * n : std::max<extent_t>(1, 4 * n);
  const extent_t integers = (DenseBlasComplex<T> ? 1 : 2) * n;
  plan.regions[kScalar] = {scalar, scalar, sizeof(T), alignof(T)};
  plan.regions[kInteger] = {integers, integers, sizeof(lapack_int),
                            alignof(lapack_int)};
  if constexpr (DenseBlasComplex<T>) {
    using Real = DenseBlasRealType<T>;
    const auto real = std::max<extent_t>(1, 2 * n);
    plan.regions[kReal] = {real, real, sizeof(Real), alignof(Real)};
  }
  plan.regions[kLayout] = {packing, packing, sizeof(T), alignof(T)};
  if (!TotalFits(plan)) {
    return Status(ErrorCode::kOverflow);
  }
  return plan;
}

template <typename T, Mode M>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTranspose transpose,
                                  const Operands<T, M>& values) {
  auto status = Validate(provider, transpose, values);
  if (!status.ok()) {
    return status;
  }
  extent_t packing = 0;
  for (const auto& matrix :
       {static_cast<DenseBlasMatrixView<const T>>(values.a),
        static_cast<DenseBlasMatrixView<const T>>(values.af),
        static_cast<DenseBlasMatrixView<const T>>(values.b),
        static_cast<DenseBlasMatrixView<const T>>(values.x)}) {
    status = AddPacking(matrix, packing);
    if (!status.ok()) {
      return status;
    }
  }
  return MakePlan(provider, transpose, values, packing);
}

Status ValidatePlan(const ReferenceLapackProvider& provider,
                    const LapackWorkspacePlan& expected,
                    const LapackWorkspacePlan& supplied,
                    const LapackWorkspace& workspace,
                    std::span<const ConstMemoryView> spans) {
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
      provider, supplied, expected.identity, workspace, spans);
}

template <typename T, typename E>
T* Pack(DenseBlasMatrixView<E> matrix, T*& cursor, bool read) {
  // Fortran lacks mode-dependent constness. Only source-audited read-only
  // branches receive const objects; output branches have mutable descriptors.
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    return const_cast<T*>(matrix.data());
  }
  T* destination = cursor;
  if (read) {
    for (extent_t j = 0; j < matrix.columns(); ++j) {
      for (extent_t i = 0; i < matrix.rows(); ++i) {
        destination[j * matrix.rows() + i] =
            matrix.data()[i * matrix.leading_dimension() + j];
      }
    }
  }
  const extent_t count = matrix.rows() * matrix.columns();
  if (count != 0) {
    cursor += count;
  }
  return destination;
}

template <typename T, Mode M>
Packed<T> PackAll(const Operands<T, M>& values,
                  const LapackWorkspace& workspace) {
  auto* cursor = static_cast<T*>(workspace.regions[kLayout].data());
  return {Pack(values.a, cursor, true),
          Pack(values.af, cursor, M == Mode::kSupplied),
          Pack(values.b, cursor, true),
          Pack(values.x, cursor, false),
          {static_cast<lapack_int>(ForeignLd(values.a)),
           static_cast<lapack_int>(ForeignLd(values.af)),
           static_cast<lapack_int>(ForeignLd(values.b)),
           static_cast<lapack_int>(ForeignLd(values.x))}};
}

template <typename T>
void Unpack(const T* packed, DenseBlasMatrixView<T> matrix) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    return;
  }
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    for (extent_t i = 0; i < matrix.rows(); ++i) {
      matrix.data()[i * matrix.leading_dimension() + j] =
          packed[j * matrix.rows() + i];
    }
  }
}

template <typename Real>
Real Growth(const LapackWorkspace& workspace, bool complex) {
  return *static_cast<const Real*>(
      workspace.regions[complex ? kReal : kScalar].data());
}

Status InterpretInfo(lapack_int info, extent_t n, LapackReport& report) {
  report.native_info = info;
  if (info < 0) {
    report.outcome = LapackOutcome::kProviderArgument;
    report.output_validity = LapackOutputValidity::kUnusable;
    if (info != std::numeric_limits<lapack_int>::min()) {
      report.native_argument = -static_cast<std::int64_t>(info);
    }
    return Status(ErrorCode::kProvider);
  }
  if (info > n + 1) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  if (info > 0) {
    report.outcome =
        info <= n ? LapackOutcome::kSingular : LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    if (info <= n) {
      report.diagnostic_index = info - 1;
    }
    return Status(ErrorCode::kNumerical);
  }
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

template <typename T, Mode M>
Status CheckOutputs(const Operands<T, M>& values, LapackReport& report) {
  bool nonfinite = false;
  bool negative = false;
  for (extent_t j = 0; j < values.ferr.size(); ++j) {
    const auto ferr = values.ferr.data()[j];
    const auto berr = values.berr.data()[j];
    negative |= ferr < 0 || berr < 0;
    nonfinite |= !std::isfinite(ferr) || !std::isfinite(berr);
  }
  for (const auto value : {values.statistics.reciprocal_condition,
                           values.statistics.reciprocal_pivot_growth}) {
    negative |= value < 0;
    nonfinite |= !std::isfinite(value);
  }
  if (negative || nonfinite) {
    report.outcome = negative ? LapackOutcome::kPartialResult
                              : LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(negative ? ErrorCode::kProvider : ErrorCode::kNumerical);
  }
  return Status::Ok();
}

template <typename T, Mode M>
Status Complete(const Operands<T, M>& v, const Packed<T>& packed,
                const lapack_int* converted, char equed,
                LapackEquilibration* output_equed,
                const LapackWorkspace& workspace,
                LapackSolveStatistics<DenseBlasRealType<T>>& statistics,
                lapack_int info, LapackReport& report) {
  Status status = InterpretInfo(info, v.a.rows(), report);
  if (status.code() == ErrorCode::kProvider) {
    return status;
  }
  LapackEquilibration applied = LapackEquilibration::kNone;
  switch (equed) {
    case 'N':
      break;
    case 'R':
      applied = LapackEquilibration::kRows;
      break;
    case 'C':
      applied = LapackEquilibration::kColumns;
      break;
    case 'B':
      applied = LapackEquilibration::kBoth;
      break;
    default:
      report.outcome = LapackOutcome::kPartialResult;
      report.output_validity = LapackOutputValidity::kUnusable;
      return Status(ErrorCode::kProvider);
  }
  if constexpr (M != Mode::kEquilibrate) {
    if (applied != v.equed) {
      report.outcome = LapackOutcome::kPartialResult;
      report.output_validity = LapackOutputValidity::kUnusable;
      return Status(ErrorCode::kProvider);
    }
  }
  if constexpr (M != Mode::kSupplied) {
    for (extent_t i = 0; i < v.a.rows(); ++i) {
      if (converted[i] < i + 1 || converted[i] > v.a.rows()) {
        report.outcome = LapackOutcome::kPartialResult;
        report.output_validity = LapackOutputValidity::kUnusable;
        return Status(ErrorCode::kProvider);
      }
    }
    for (extent_t i = 0; i < v.a.rows(); ++i) {
      v.pivots.data()[i] = converted[i];
    }
    Unpack(packed.af, v.af);
  }
  if constexpr (M == Mode::kEquilibrate) {
    Unpack(packed.a, v.a);
    *output_equed = applied;
  }
  if constexpr (M != Mode::kNew) {
    Unpack(packed.b, v.b);
  }
  statistics.reciprocal_pivot_growth =
      Growth<DenseBlasRealType<T>>(workspace, DenseBlasComplex<T>);
  if (info > 0 && info <= v.a.rows()) {
    return status;
  }
  Unpack(packed.x, v.x);
  const auto quality = CheckOutputs(v, report);
  return quality.ok() ? status : quality;
}

template <typename T, Mode M>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTranspose transpose, const Operands<T, M>& values,
               LapackEquilibration* output_equed,
               LapackSolveStatistics<DenseBlasRealType<T>>& statistics,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  if constexpr (M != Mode::kSupplied) {
    report.factor_family = LapackFactorFamily::kLuPartialPivot;
  }
  const auto name = Native<T>::kName;
  std::copy(name.begin(), name.end(), report.routine.begin());
  const auto query = Query(provider, transpose, values);
  if (!query.ok()) {
    return query.status();
  }
  auto status = ValidatePlan(provider, *query, plan, workspace, values.Spans());
  if (!status.ok()) {
    return status;
  }
  const extent_t n = values.a.rows();
  if constexpr (M == Mode::kSupplied) {
    for (extent_t i = 0; i < n; ++i) {
      if (values.af.data()[i * values.af.leading_dimension() + i] == T{}) {
        report.outcome = LapackOutcome::kSingular;
        report.diagnostic_index = i;
        return Status(ErrorCode::kNumerical);
      }
    }
  }
  if (n == 0) {
    statistics = {1, 1};
    if constexpr (M == Mode::kEquilibrate) {
      *output_equed = LapackEquilibration::kNone;
    }
    for (extent_t j = 0; j < values.ferr.size(); ++j) {
      values.ferr.data()[j] = 0;
      values.berr.data()[j] = 0;
    }
    using Real = DenseBlasRealType<T>;
    *static_cast<Real*>(
        workspace.regions[DenseBlasComplex<T> ? kReal : kScalar].data()) = 1;
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  auto* converted = ::new (workspace.regions[kInteger].data())
      lapack_int[static_cast<std::size_t>((DenseBlasComplex<T> ? 1 : 2) * n)];
  if constexpr (M == Mode::kSupplied) {
    for (extent_t i = 0; i < n; ++i) {
      converted[i] = static_cast<lapack_int>(values.pivots.values()[i]);
    }
  }
  const auto packed = PackAll(values, workspace);
  char equed = EquedChar(values.equed);
  report.called_provider = true;
  const auto info = Native<T>::Execute(
      static_cast<char>(M), TransChar(transpose), static_cast<lapack_int>(n),
      static_cast<lapack_int>(values.b.columns()), packed, converted, equed,
      const_cast<DenseBlasRealType<T>*>(values.r.data()),
      const_cast<DenseBlasRealType<T>*>(values.c.data()),
      statistics.reciprocal_condition, values.ferr.data(), values.berr.data(),
      workspace);
  return Complete(values, packed, converted, equed, output_equed, workspace,
                  statistics, info, report);
}

}  // namespace

Result<LapackWorkspacePlan> QueryGesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<float> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<float, Mode::kNew> values{original,
                                           factors,
                                           pivots,
                                           EmptyVector<float>(),
                                           EmptyVector<float>(),
                                           rhs,
                                           solution,
                                           forward_error,
                                           backward_error,
                                           statistics,
                                           nullptr,
                                           LapackEquilibration::kNone};
  return Query(provider, transpose, values);
}

Status Gesvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<float> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<float, Mode::kNew> values{original,
                                           factors,
                                           pivots,
                                           EmptyVector<float>(),
                                           EmptyVector<float>(),
                                           rhs,
                                           solution,
                                           forward_error,
                                           backward_error,
                                           statistics,
                                           nullptr,
                                           LapackEquilibration::kNone};
  return Execute(provider, transpose, values, nullptr, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGesvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<float> original, DenseBlasMatrixView<float> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<float, Mode::kEquilibrate> values{
      original,       factors,    pivots,         row_scales,
      column_scales,  rhs,        solution,       forward_error,
      backward_error, statistics, &equilibration, LapackEquilibration::kNone};
  return Query(provider, transpose, values);
}

Status GesvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<float> original, DenseBlasMatrixView<float> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<float, Mode::kEquilibrate> values{
      original,       factors,    pivots,         row_scales,
      column_scales,  rhs,        solution,       forward_error,
      backward_error, statistics, &equilibration, LapackEquilibration::kNone};
  return Execute(provider, transpose, values, &equilibration, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<float, Mode::kSupplied> values{
      original,       factors,    pivots,   row_scales,
      column_scales,  rhs,        solution, forward_error,
      backward_error, statistics, nullptr,  equilibration};
  return Query(provider, transpose, values);
}

Status GesvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<float, Mode::kSupplied> values{
      original,       factors,    pivots,   row_scales,
      column_scales,  rhs,        solution, forward_error,
      backward_error, statistics, nullptr,  equilibration};
  return Execute(provider, transpose, values, nullptr, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<double, Mode::kNew> values{original,
                                            factors,
                                            pivots,
                                            EmptyVector<double>(),
                                            EmptyVector<double>(),
                                            rhs,
                                            solution,
                                            forward_error,
                                            backward_error,
                                            statistics,
                                            nullptr,
                                            LapackEquilibration::kNone};
  return Query(provider, transpose, values);
}

Status Gesvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<double, Mode::kNew> values{original,
                                            factors,
                                            pivots,
                                            EmptyVector<double>(),
                                            EmptyVector<double>(),
                                            rhs,
                                            solution,
                                            forward_error,
                                            backward_error,
                                            statistics,
                                            nullptr,
                                            LapackEquilibration::kNone};
  return Execute(provider, transpose, values, nullptr, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGesvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<double> original, DenseBlasMatrixView<double> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<double, Mode::kEquilibrate> values{
      original,       factors,    pivots,         row_scales,
      column_scales,  rhs,        solution,       forward_error,
      backward_error, statistics, &equilibration, LapackEquilibration::kNone};
  return Query(provider, transpose, values);
}

Status GesvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<double> original, DenseBlasMatrixView<double> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<double, Mode::kEquilibrate> values{
      original,       factors,    pivots,         row_scales,
      column_scales,  rhs,        solution,       forward_error,
      backward_error, statistics, &equilibration, LapackEquilibration::kNone};
  return Execute(provider, transpose, values, &equilibration, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<double, Mode::kSupplied> values{
      original,       factors,    pivots,   row_scales,
      column_scales,  rhs,        solution, forward_error,
      backward_error, statistics, nullptr,  equilibration};
  return Query(provider, transpose, values);
}

Status GesvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<double, Mode::kSupplied> values{
      original,       factors,    pivots,   row_scales,
      column_scales,  rhs,        solution, forward_error,
      backward_error, statistics, nullptr,  equilibration};
  return Execute(provider, transpose, values, nullptr, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<std::complex<float>, Mode::kNew> values{
      original,
      factors,
      pivots,
      EmptyVector<float>(),
      EmptyVector<float>(),
      rhs,
      solution,
      forward_error,
      backward_error,
      statistics,
      nullptr,
      LapackEquilibration::kNone};
  return Query(provider, transpose, values);
}

Status Gesvx(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<const std::complex<float>> original,
             DenseBlasMatrixView<std::complex<float>> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             LapackSolveStatistics<float>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Operands<std::complex<float>, Mode::kNew> values{
      original,
      factors,
      pivots,
      EmptyVector<float>(),
      EmptyVector<float>(),
      rhs,
      solution,
      forward_error,
      backward_error,
      statistics,
      nullptr,
      LapackEquilibration::kNone};
  return Execute(provider, transpose, values, nullptr, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGesvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<std::complex<float>, Mode::kEquilibrate> values{
      original,       factors,    pivots,         row_scales,
      column_scales,  rhs,        solution,       forward_error,
      backward_error, statistics, &equilibration, LapackEquilibration::kNone};
  return Query(provider, transpose, values);
}

Status GesvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<std::complex<float>, Mode::kEquilibrate> values{
      original,       factors,    pivots,         row_scales,
      column_scales,  rhs,        solution,       forward_error,
      backward_error, statistics, &equilibration, LapackEquilibration::kNone};
  return Execute(provider, transpose, values, &equilibration, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics) {
  const Operands<std::complex<float>, Mode::kSupplied> values{
      original,       factors,    pivots,   row_scales,
      column_scales,  rhs,        solution, forward_error,
      backward_error, statistics, nullptr,  equilibration};
  return Query(provider, transpose, values);
}

Status GesvxFactored(const ReferenceLapackProvider& provider,
                     DenseBlasTranspose transpose,
                     DenseBlasMatrixView<const std::complex<float>> original,
                     DenseBlasMatrixView<const std::complex<float>> factors,
                     RawLapackPivotView pivots,
                     LapackEquilibration equilibration,
                     DenseBlasVectorView<const float> row_scales,
                     DenseBlasVectorView<const float> column_scales,
                     DenseBlasMatrixView<std::complex<float>> rhs,
                     DenseBlasMatrixView<std::complex<float>> solution,
                     DenseBlasVectorView<float> forward_error,
                     DenseBlasVectorView<float> backward_error,
                     LapackSolveStatistics<float>& statistics,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<std::complex<float>, Mode::kSupplied> values{
      original,       factors,    pivots,   row_scales,
      column_scales,  rhs,        solution, forward_error,
      backward_error, statistics, nullptr,  equilibration};
  return Execute(provider, transpose, values, nullptr, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<std::complex<double>, Mode::kNew> values{
      original,
      factors,
      pivots,
      EmptyVector<double>(),
      EmptyVector<double>(),
      rhs,
      solution,
      forward_error,
      backward_error,
      statistics,
      nullptr,
      LapackEquilibration::kNone};
  return Query(provider, transpose, values);
}

Status Gesvx(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<const std::complex<double>> original,
             DenseBlasMatrixView<std::complex<double>> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             LapackSolveStatistics<double>& statistics,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const Operands<std::complex<double>, Mode::kNew> values{
      original,
      factors,
      pivots,
      EmptyVector<double>(),
      EmptyVector<double>(),
      rhs,
      solution,
      forward_error,
      backward_error,
      statistics,
      nullptr,
      LapackEquilibration::kNone};
  return Execute(provider, transpose, values, nullptr, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGesvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<std::complex<double>, Mode::kEquilibrate> values{
      original,       factors,    pivots,         row_scales,
      column_scales,  rhs,        solution,       forward_error,
      backward_error, statistics, &equilibration, LapackEquilibration::kNone};
  return Query(provider, transpose, values);
}

Status GesvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<std::complex<double>, Mode::kEquilibrate> values{
      original,       factors,    pivots,         row_scales,
      column_scales,  rhs,        solution,       forward_error,
      backward_error, statistics, &equilibration, LapackEquilibration::kNone};
  return Execute(provider, transpose, values, &equilibration, statistics, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics) {
  const Operands<std::complex<double>, Mode::kSupplied> values{
      original,       factors,    pivots,   row_scales,
      column_scales,  rhs,        solution, forward_error,
      backward_error, statistics, nullptr,  equilibration};
  return Query(provider, transpose, values);
}

Status GesvxFactored(const ReferenceLapackProvider& provider,
                     DenseBlasTranspose transpose,
                     DenseBlasMatrixView<const std::complex<double>> original,
                     DenseBlasMatrixView<const std::complex<double>> factors,
                     RawLapackPivotView pivots,
                     LapackEquilibration equilibration,
                     DenseBlasVectorView<const double> row_scales,
                     DenseBlasVectorView<const double> column_scales,
                     DenseBlasMatrixView<std::complex<double>> rhs,
                     DenseBlasMatrixView<std::complex<double>> solution,
                     DenseBlasVectorView<double> forward_error,
                     DenseBlasVectorView<double> backward_error,
                     LapackSolveStatistics<double>& statistics,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  const Operands<std::complex<double>, Mode::kSupplied> values{
      original,       factors,    pivots,   row_scales,
      column_scales,  rhs,        solution, forward_error,
      backward_error, statistics, nullptr,  equilibration};
  return Execute(provider, transpose, values, nullptr, statistics, plan,
                 workspace, report);
}
}  // namespace asc
