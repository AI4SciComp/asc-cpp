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
#include "asc/dense/providers/lapack_lu_refinement.h"
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

template <typename T>
struct Operands {
  DenseBlasMatrixView<const T> a;
  DenseBlasMatrixView<const T> af;
  RawLapackPivotView pivots;
  DenseBlasMatrixView<const T> b;
  DenseBlasMatrixView<T> x;
  DenseBlasVectorView<DenseBlasRealType<T>> ferr;
  DenseBlasVectorView<DenseBlasRealType<T>> berr;

  [[nodiscard]] std::array<ConstMemoryView, 7> Spans() const {
    return {a.reachable_storage(),      af.reachable_storage(),
            pivots.reachable_storage(), b.reachable_storage(),
            x.reachable_storage(),      ferr.reachable_storage(),
            berr.reachable_storage()};
  }
};

template <typename T>
struct Packed {
  const T* a;
  const T* af;
  const T* b;
  T* x;
  std::array<lapack_int, 4> ld;
};

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kScalarKind = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "sgerfs";
  static lapack_int Execute(char trans, lapack_int n, lapack_int nrhs,
                            const Packed<float>& data, const lapack_int* pivots,
                            float* ferr, float* berr,
                            const LapackWorkspace& workspace) {
    // The provider must write the complete native INFO destination.
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sgerfs(
        &trans, &n, &nrhs, data.a, data.ld.data(), data.af, &data.ld[1], pivots,
        data.b, &data.ld[2], data.x, &data.ld[3], ferr, berr,
        static_cast<float*>(workspace.regions[kScalar].data()),
        static_cast<lapack_int*>(workspace.regions[kInteger].data()) + n,
        &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kScalarKind = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dgerfs";
  static lapack_int Execute(char trans, lapack_int n, lapack_int nrhs,
                            const Packed<double>& data,
                            const lapack_int* pivots, double* ferr,
                            double* berr, const LapackWorkspace& workspace) {
    // The provider must write the complete native INFO destination.
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dgerfs(
        &trans, &n, &nrhs, data.a, data.ld.data(), data.af, &data.ld[1], pivots,
        data.b, &data.ld[2], data.x, &data.ld[3], ferr, berr,
        static_cast<double*>(workspace.regions[kScalar].data()),
        static_cast<lapack_int*>(workspace.regions[kInteger].data()) + n,
        &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kScalarKind = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cgerfs";
  static lapack_int Execute(char trans, lapack_int n, lapack_int nrhs,
                            const Packed<std::complex<float>>& data,
                            const lapack_int* pivots, float* ferr, float* berr,
                            const LapackWorkspace& workspace) {
    // The provider must write the complete native INFO destination.
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cgerfs(
        &trans, &n, &nrhs, data.a, data.ld.data(), data.af, &data.ld[1], pivots,
        data.b, &data.ld[2], data.x, &data.ld[3], ferr, berr,
        static_cast<std::complex<float>*>(workspace.regions[kScalar].data()),
        static_cast<float*>(workspace.regions[kReal].data()), &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kScalarKind = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zgerfs";
  static lapack_int Execute(char trans, lapack_int n, lapack_int nrhs,
                            const Packed<std::complex<double>>& data,
                            const lapack_int* pivots, double* ferr,
                            double* berr, const LapackWorkspace& workspace) {
    // The provider must write the complete native INFO destination.
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zgerfs(
        &trans, &n, &nrhs, data.a, data.ld.data(), data.af, &data.ld[1], pivots,
        data.b, &data.ld[2], data.x, &data.ld[3], ferr, berr,
        static_cast<std::complex<double>*>(workspace.regions[kScalar].data()),
        static_cast<double*>(workspace.regions[kReal].data()), &info);
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
extent_t ForeignLd(DenseBlasMatrixView<T> matrix) {
  return matrix.layout() == DenseBlasLayout::kColumnMajor
             ? matrix.leading_dimension()
             : std::max<extent_t>(1, matrix.rows());
}

template <typename T>
Status Validate(const ReferenceLapackProvider& provider,
                DenseBlasTranspose transpose, const Operands<T>& values) {
  if (transpose != DenseBlasTranspose::kNone &&
      transpose != DenseBlasTranspose::kTranspose &&
      transpose != DenseBlasTranspose::kConjugateTranspose) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const extent_t n = values.a.rows();
  const extent_t nrhs = values.b.columns();
  if (values.a.columns() != n || values.af.rows() != n ||
      values.af.columns() != n || values.b.rows() != n ||
      values.x.rows() != n || values.x.columns() != nrhs ||
      values.pivots.values().size() != static_cast<std::size_t>(n) ||
      values.ferr.size() != nrhs || values.berr.size() != nrhs) {
    return Status(ErrorCode::kShape);
  }
  if (values.ferr.increment() != 1 || values.berr.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  // LACN2 uses 3*N, GERFS uses N+1, and the RHS DO-loop increments NRHS.
  if (n > std::numeric_limits<lapack_int>::max() / 3 ||
      nrhs >= std::numeric_limits<lapack_int>::max()) {
    return Status(ErrorCode::kOverflow);
  }
  for (const extent_t ld : {ForeignLd(values.a), ForeignLd(values.af),
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
  return ValidateLuPivots(values.pivots, n);
}

template <typename T>
Status AddPacking(DenseBlasMatrixView<T> matrix, extent_t& entries) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor || matrix.rows() == 0) {
    return Status::Ok();
  }
  if (matrix.columns() > std::numeric_limits<extent_t>::max() / matrix.rows()) {
    return Status(ErrorCode::kOverflow);
  }
  const auto count = matrix.rows() * matrix.columns();
  if (count > std::numeric_limits<extent_t>::max() - entries) {
    return Status(ErrorCode::kOverflow);
  }
  entries += count;
  return Status::Ok();
}

bool TotalFits(const LapackWorkspacePlan& plan) {
  std::size_t remaining = std::numeric_limits<std::size_t>::max();
  for (const auto& region : plan.regions) {
    const auto entries = static_cast<std::uint64_t>(region.minimum_entries);
    if (entries > remaining / region.entry_bytes) {
      return false;
    }
    remaining -= static_cast<std::size_t>(entries) * region.entry_bytes;
  }
  return true;
}

template <typename T>
Result<LapackWorkspacePlan> MakePlan(const ReferenceLapackProvider& provider,
                                     DenseBlasTranspose transpose,
                                     const Operands<T>& values,
                                     extent_t packing) {
  using Real = DenseBlasRealType<T>;
  const extent_t n = values.a.rows();
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kScalarKind,
      std::array{n, values.b.columns(), ForeignLd(values.a),
                 ForeignLd(values.af), ForeignLd(values.b), ForeignLd(values.x),
                 values.ferr.size(), values.berr.size(),
                 static_cast<extent_t>(values.pivots.values().size())},
      std::array<std::int64_t, 11>{
          static_cast<std::int64_t>(transpose),
          static_cast<std::int64_t>(values.a.layout()),
          static_cast<std::int64_t>(values.af.layout()),
          static_cast<std::int64_t>(values.b.layout()),
          static_cast<std::int64_t>(values.x.layout()), values.ferr.increment(),
          values.berr.increment(), values.a.leading_dimension(),
          values.af.leading_dimension(), values.b.leading_dimension(),
          values.x.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  const extent_t scalar = (DenseBlasComplex<T> ? 2 : 3) * n;
  plan.regions[kScalar] = {scalar, scalar, sizeof(T), alignof(T)};
  const extent_t integers = (DenseBlasComplex<T> ? 1 : 2) * n;
  plan.regions[kInteger] = {integers, integers, sizeof(lapack_int),
                            alignof(lapack_int)};
  if constexpr (DenseBlasComplex<T>) {
    plan.regions[kReal] = {n, n, sizeof(Real), alignof(Real)};
  }
  plan.regions[kLayout] = {packing, packing, sizeof(T), alignof(T)};
  if (!TotalFits(plan)) {
    return Status(ErrorCode::kOverflow);
  }
  return plan;
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTranspose transpose,
                                  const Operands<T>& values) {
  Status status = Validate(provider, transpose, values);
  if (!status.ok()) {
    return status;
  }
  extent_t packing = 0;
  for (const auto& matrix :
       {values.a, values.af, values.b,
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

template <typename T>
T* Pack(DenseBlasMatrixView<T> matrix, std::remove_const_t<T>*& cursor) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    return matrix.data();
  }
  auto* packed = cursor;
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    for (extent_t i = 0; i < matrix.rows(); ++i) {
      packed[j * matrix.rows() + i] =
          matrix.data()[i * matrix.leading_dimension() + j];
    }
  }
  cursor += matrix.rows() * matrix.columns();
  return packed;
}

template <typename T>
Packed<T> PackAll(const Operands<T>& values, const LapackWorkspace& workspace) {
  auto* cursor = static_cast<T*>(workspace.regions[kLayout].data());
  Packed<T> packed{Pack(values.a, cursor),
                   Pack(values.af, cursor),
                   Pack(values.b, cursor),
                   Pack(values.x, cursor),
                   {}};
  std::size_t i = 0;
  for (const auto& matrix :
       {values.a, values.af, values.b,
        static_cast<DenseBlasMatrixView<const T>>(values.x)}) {
    packed.ld[i++] =
        static_cast<lapack_int>(matrix.layout() == DenseBlasLayout::kColumnMajor
                                    ? matrix.leading_dimension()
                                    : matrix.rows());
  }
  return packed;
}

template <typename T>
void Unpack(const T* packed, DenseBlasMatrixView<T> destination) {
  if (destination.layout() == DenseBlasLayout::kColumnMajor) {
    return;
  }
  for (extent_t j = 0; j < destination.columns(); ++j) {
    for (extent_t i = 0; i < destination.rows(); ++i) {
      destination.data()[i * destination.leading_dimension() + j] =
          packed[j * destination.rows() + i];
    }
  }
}

Status InterpretInfo(lapack_int info, LapackReport& report) {
  report.native_info = info;
  if (info < 0) {
    report.outcome = LapackOutcome::kProviderArgument;
    report.output_validity = LapackOutputValidity::kUnusable;
    if (info != std::numeric_limits<lapack_int>::min()) {
      report.native_argument = -static_cast<std::int64_t>(info);
    }
    return Status(ErrorCode::kProvider);
  }
  if (info > 0) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

template <typename T>
Status CheckEstimates(const Operands<T>& values, LapackReport& report) {
  bool nonfinite = false;
  for (extent_t j = 0; j < values.ferr.size(); ++j) {
    const auto ferr = values.ferr.data()[j];
    const auto berr = values.berr.data()[j];
    if (ferr < 0 || berr < 0) {
      report.outcome = LapackOutcome::kPartialResult;
      report.output_validity = LapackOutputValidity::kDocumentedPartial;
      report.diagnostic_index = j;
      return Status(ErrorCode::kProvider);
    }
    if (!std::isfinite(ferr) || !std::isfinite(berr)) {
      if (!nonfinite) {
        report.diagnostic_index = j;
      }
      nonfinite = true;
    }
  }
  if (nonfinite) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return Status::Ok();
}

template <typename T>
Status Refine(const ReferenceLapackProvider& provider,
              DenseBlasTranspose transpose, const Operands<T>& values,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  const auto name = Native<T>::kName;
  std::copy(name.begin(), name.end(), report.routine.begin());
  const auto expected = Query(provider, transpose, values);
  if (!expected.ok()) {
    return expected.status();
  }
  Status status =
      ValidatePlan(provider, *expected, plan, workspace, values.Spans());
  if (!status.ok()) {
    return status;
  }
  const extent_t n = values.a.rows();
  if (n == 0 || values.b.columns() == 0) {
    for (extent_t j = 0; j < values.ferr.size(); ++j) {
      values.ferr.data()[j] = 0;
      values.berr.data()[j] = 0;
    }
    report.outcome = LapackOutcome::kSuccess;
    report.output_validity = LapackOutputValidity::kComplete;
    return Status::Ok();
  }
  for (extent_t i = 0; i < n; ++i) {
    if (values.af.data()[i * values.af.leading_dimension() + i] == T{}) {
      report.outcome = LapackOutcome::kSingular;
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
  }
  auto* converted = ::new (workspace.regions[kInteger].data())
      lapack_int[static_cast<std::size_t>((DenseBlasComplex<T> ? 1 : 2) * n)];
  for (extent_t i = 0; i < n; ++i) {
    converted[i] = static_cast<lapack_int>(values.pivots.values()[i]);
  }
  const Packed<T> packed = PackAll(values, workspace);
  char trans = 'N';
  if (transpose == DenseBlasTranspose::kTranspose) {
    trans = 'T';
  }
  if (transpose == DenseBlasTranspose::kConjugateTranspose) {
    trans = 'C';
  }
  report.called_provider = true;
  status = InterpretInfo(
      Native<T>::Execute(trans, static_cast<lapack_int>(n),
                         static_cast<lapack_int>(values.b.columns()), packed,
                         converted, values.ferr.data(), values.berr.data(),
                         workspace),
      report);
  if (status.ok()) {
    Unpack(packed.x, values.x);
    return CheckEstimates(values, report);
  }
  return status;
}
}  // namespace

Result<LapackWorkspacePlan> QueryGerfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(provider, transpose,
               Operands<float>{original, factors, pivots, rhs, solution,
                               forward_error, backward_error});
}

Status Gerfs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<const float> original,
             DenseBlasMatrixView<const float> factors,
             RawLapackPivotView pivots, DenseBlasMatrixView<const float> rhs,
             DenseBlasMatrixView<float> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Refine(provider, transpose,
                Operands<float>{original, factors, pivots, rhs, solution,
                                forward_error, backward_error},
                plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGerfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(provider, transpose,
               Operands<double>{original, factors, pivots, rhs, solution,
                                forward_error, backward_error});
}

Status Gerfs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<const double> original,
             DenseBlasMatrixView<const double> factors,
             RawLapackPivotView pivots, DenseBlasMatrixView<const double> rhs,
             DenseBlasMatrixView<double> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Refine(provider, transpose,
                Operands<double>{original, factors, pivots, rhs, solution,
                                 forward_error, backward_error},
                plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGerfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(
      provider, transpose,
      Operands<std::complex<float>>{original, factors, pivots, rhs, solution,
                                    forward_error, backward_error});
}

Status Gerfs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<const std::complex<float>> original,
             DenseBlasMatrixView<const std::complex<float>> factors,
             RawLapackPivotView pivots,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Refine(
      provider, transpose,
      Operands<std::complex<float>>{original, factors, pivots, rhs, solution,
                                    forward_error, backward_error},
      plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGerfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(
      provider, transpose,
      Operands<std::complex<double>>{original, factors, pivots, rhs, solution,
                                     forward_error, backward_error});
}

Status Gerfs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             DenseBlasMatrixView<const std::complex<double>> original,
             DenseBlasMatrixView<const std::complex<double>> factors,
             RawLapackPivotView pivots,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Refine(
      provider, transpose,
      Operands<std::complex<double>>{original, factors, pivots, rhs, solution,
                                     forward_error, backward_error},
      plan, workspace, report);
}

}  // namespace asc
