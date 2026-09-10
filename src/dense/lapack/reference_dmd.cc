#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
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
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_dmd.h"
#include "internal_dmd_execution_counts.h"
#include "internal_dmd_query_counts.h"
#include "internal_layout.h"
#include "internal_rank_revealing_counts.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;
namespace counts = internal_dmd_counts;
template <typename T>
using Real = DenseBlasRealType<T>;
template <typename T>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return "sgedmd";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dgedmd";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return "cgedmd";
  } else {
    return "zgedmd";
  }
}
char Code(auto option) { return static_cast<char>(option); }
bool Vectors(LapackDmdOptions options) {
  return options.vectors != LapackDmdVectors::kNone ||
         options.extra == LapackDmdExtra::kExact;
}
Status Options(LapackDmdOptions options, extent_t n) {
  const auto scaling = options.scaling;
  const auto vectors = options.vectors;
  const auto extra = options.extra;
  if ((scaling != LapackDmdScaling::kNone &&
       scaling != LapackDmdScaling::kSnapshots &&
       scaling != LapackDmdScaling::kConsistentSnapshots &&
       scaling != LapackDmdScaling::kSuccessors) ||
      (vectors != LapackDmdVectors::kNone &&
       vectors != LapackDmdVectors::kExplicit &&
       vectors != LapackDmdVectors::kFactored) ||
      (extra != LapackDmdExtra::kNone && extra != LapackDmdExtra::kRefinement &&
       extra != LapackDmdExtra::kExact) ||
      (options.residuals && vectors != LapackDmdVectors::kExplicit) ||
      static_cast<int>(options.svd) < 1 || static_cast<int>(options.svd) > 4 ||
      (options.rank_selection != -1 && options.rank_selection != -2 &&
       (options.rank_selection < 1 || options.rank_selection > n))) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}
template <typename T>
auto Matrices(LapackDmdBuffers<T> b) {
  return std::array{b.snapshots, b.successors,    b.modes,
                    b.extra,     b.reduced_modes, b.reduced_workspace};
}
template <typename T>
auto Spans(LapackDmdBuffers<T> b) {
  return std::array<ConstMemoryView, 9>{b.snapshots.reachable_storage(),
                                        b.successors.reachable_storage(),
                                        b.modes.reachable_storage(),
                                        b.extra.reachable_storage(),
                                        b.reduced_modes.reachable_storage(),
                                        b.reduced_workspace.reachable_storage(),
                                        b.eigenvalues.reachable_storage(),
                                        b.singular_values.reachable_storage(),
                                        b.residual_norms.reachable_storage()};
}
template <typename R>
std::int64_t ToleranceBits(R value) {
  if constexpr (std::is_same_v<R, float>) {
    return std::bit_cast<std::uint32_t>(value);
  } else {
    return std::bit_cast<std::int64_t>(value);
  }
}
template <typename T>
Result<LapackPlanIdentity> Identity(const ReferenceLapackProvider& provider,
                                    LapackDmdOptions options,
                                    LapackDmdBuffers<T> b, Real<T> tolerance) {
  std::array<extent_t, 8> dimensions{b.snapshots.rows(), b.snapshots.columns()};
  std::array<std::int64_t, 13> flags{
      Code(options.scaling),   Code(options.vectors),
      Code(options.extra),     static_cast<std::int64_t>(options.svd),
      options.residuals,       options.rank_selection,
      ToleranceBits(tolerance)};
  const auto matrices = Matrices(b);
  for (std::size_t i = 0; i < matrices.size(); ++i) {
    dimensions[i + 2] = matrices[i].leading_dimension();
    flags[i + 7] = static_cast<std::int64_t>(matrices[i].layout());
  }
  return LapackPlanIdentity::Create(Routine<T>(), checked::Kind<T>(),
                                    dimensions, flags, provider.identity());
}
template <typename T>
Status Metadata(const ReferenceLapackProvider& provider,
                LapackDmdOptions options, LapackDmdBuffers<T> b,
                Real<T> tolerance) {
  const auto m = b.snapshots.rows();
  const auto n = b.snapshots.columns();
  Status status = Options(options, n);
  if (!status.ok()) {
    return status;
  }
  if (!std::isfinite(tolerance) || tolerance < 0 || tolerance >= 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n > m) {
    return Status(ErrorCode::kShape);
  }
  const auto matrices = Matrices(b);
  for (std::size_t i = 0; i < matrices.size(); ++i) {
    if (matrices[i].rows() != (i < 4 ? m : n) || matrices[i].columns() != n) {
      return Status(ErrorCode::kShape);
    }
  }
  if (b.eigenvalues.size() != n || b.eigenvalues.increment() != 1 ||
      b.singular_values.size() != n || b.singular_values.increment() != 1 ||
      b.residual_norms.size() != n || b.residual_norms.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  const auto spans = Spans(b);
  status = checked::Disjoint(spans);
  for (const auto span : spans) {
    if (status.ok()) {
      status = checked::Access(provider, span);
    }
    if (status.ok() &&
        checked::Overlap(span, checked::ObjectStorage(provider))) {
      status = Status(ErrorCode::kInvalidArgument);
    }
  }
  return status;
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  LapackDmdOptions options,
                                  LapackDmdBuffers<T> b, Real<T> tolerance) {
  const Status metadata = Metadata(provider, options, b, tolerance);
  if (!metadata.ok()) {
    return metadata;
  }
  const auto identity = Identity(provider, options, b, tolerance);
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  const auto m = b.snapshots.rows();
  const auto n = b.snapshots.columns();
  if (n == 0) {
    return plan;
  }
  const Status bound = counts::ExecutionBounds<Real<T>>(
      m, n, static_cast<int>(options.svd), Vectors(options),
      DenseBlasComplex<T>, checked::kLimit);
  if (!bound.ok()) {
    return bound;
  }
  const auto resource = counts::Query<Real<T>>(
      m, n, static_cast<int>(options.svd), Vectors(options),
      DenseBlasComplex<T>, checked::kLimit);
  if (!resource.ok()) {
    return resource.status();
  }
  plan.regions[checked::kScalar] = {resource->minimum.scalar,
                                    resource->capacity_preferred, sizeof(T),
                                    alignof(T)};
  plan.regions[checked::kReal] = {resource->minimum.real,
                                  resource->minimum.real, sizeof(Real<T>),
                                  alignof(Real<T>)};
  plan.regions[checked::kInteger] = {resource->minimum.integer,
                                     resource->minimum.integer,
                                     sizeof(lapack_int), alignof(lapack_int)};
  // Real eigenparts/residuals are distinct live real objects, including in
  // complex calls. All full matrix/eigenvalue staging contains live T objects.
  plan.regions[checked::kScratch] = {3 * n, 3 * n, sizeof(Real<T>),
                                     alignof(Real<T>)};
  extent_t packed = 0;
  for (const auto& matrix : Matrices(b)) {
    const auto next = internal_lapack_rank_revealing::AppendPacking(
        packed, matrix.rows(), matrix.columns(), sizeof(T));
    if (!next.ok()) {
      return next.status();
    }
    packed = *next;
  }
  if constexpr (DenseBlasComplex<T>) {
    const auto next =
        internal_lapack_rank_revealing::AppendPacking(packed, n, 1, sizeof(T));
    if (!next.ok()) {
      return next.status();
    }
    packed = *next;
  }
  plan.regions[checked::kLayout] = {packed, packed, sizeof(T), alignof(T)};
  const Status total = internal_lapack_layout::CheckTotal(plan);
  return total.ok() ? Result<LapackWorkspacePlan>(plan)
                    : Result<LapackWorkspacePlan>(total);
}
template <typename T>
T& Entry(DenseBlasMatrixView<T> matrix, extent_t row, extent_t column) {
  return matrix.data()[matrix.layout() == DenseBlasLayout::kColumnMajor
                           ? column * matrix.leading_dimension() + row
                           : row * matrix.leading_dimension() + column];
}
template <typename T>
Status Input(LapackDmdBuffers<T> b) {
  bool nonzero = false;
  for (extent_t j = 0; j < b.snapshots.columns(); ++j) {
    for (extent_t i = 0; i < b.snapshots.rows(); ++i) {
      const auto x = Entry(b.snapshots, i, j);
      const auto y = Entry(b.successors, i, j);
      if (!std::isfinite(std::real(x)) || !std::isfinite(std::imag(x)) ||
          !std::isfinite(std::real(y)) || !std::isfinite(std::imag(y))) {
        return Status(ErrorCode::kNumerical);
      }
      nonzero = nonzero || x != T{};
    }
  }
  return nonzero ? Status::Ok() : Status(ErrorCode::kNumerical);
}
template <typename T>
struct Staged {
  std::array<T*, 6> matrix;
  T* eigenvalues;
  Real<T>* eigenreal;
  Real<T>* eigenimag;
  Real<T>* residual;
};
template <typename T>
Staged<T> Stage(LapackDmdBuffers<T> b, const LapackWorkspacePlan& plan,
                const LapackWorkspace& workspace) {
  Staged<T> result{};
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  // Output scratch initialization reads no previous caller numeric output.
  std::fill_n(
      cursor,
      static_cast<std::size_t>(plan.regions[checked::kLayout].minimum_entries),
      T{});
  const auto matrices = Matrices(b);
  for (std::size_t k = 0; k < matrices.size(); ++k) {
    result.matrix[k] = cursor;
    const auto matrix = matrices[k];
    if (k < 2) {
      for (extent_t j = 0; j < matrix.columns(); ++j) {
        for (extent_t i = 0; i < matrix.rows(); ++i) {
          cursor[j * matrix.rows() + i] = Entry(matrix, i, j);
        }
      }
    }
    cursor += matrix.rows() * matrix.columns();
  }
  if constexpr (DenseBlasComplex<T>) {
    result.eigenvalues = cursor;
  }
  auto* scratch =
      static_cast<Real<T>*>(workspace.regions[checked::kScratch].data());
  std::fill_n(scratch, static_cast<std::size_t>(3 * b.snapshots.columns()),
              Real<T>{});
  result.eigenreal = scratch;
  result.eigenimag = scratch + b.snapshots.columns();
  result.residual = scratch + 2 * b.snapshots.columns();
  return result;
}
template <typename T>
void Native(LapackDmdOptions options, lapack_int m, lapack_int n,
            Real<T> tolerance, Staged<T> b, T* work, lapack_int lwork,
            Real<T>* real_work, lapack_int real_count, lapack_int* integers,
            lapack_int integer_count, lapack_int& rank, lapack_int& info) {
  const auto scaling = Code(options.scaling);
  const auto vectors = Code(options.vectors);
  const char residual = options.residuals ? 'R' : 'N';
  const auto extra = Code(options.extra);
  const auto svd = static_cast<lapack_int>(options.svd);
  const auto rank_selection = static_cast<lapack_int>(options.rank_selection);
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sgedmd(&scaling, &vectors, &residual, &extra, &svd, &m, &n,
                  b.matrix[0], &m, b.matrix[1], &m, &rank_selection, &tolerance,
                  &rank, b.eigenreal, b.eigenimag, b.matrix[2], &m, b.residual,
                  b.matrix[3], &m, b.matrix[4], &n, b.matrix[5], &n, work,
                  &lwork, integers, &integer_count, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dgedmd(&scaling, &vectors, &residual, &extra, &svd, &m, &n,
                  b.matrix[0], &m, b.matrix[1], &m, &rank_selection, &tolerance,
                  &rank, b.eigenreal, b.eigenimag, b.matrix[2], &m, b.residual,
                  b.matrix[3], &m, b.matrix[4], &n, b.matrix[5], &n, work,
                  &lwork, integers, &integer_count, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cgedmd(&scaling, &vectors, &residual, &extra, &svd, &m, &n,
                  b.matrix[0], &m, b.matrix[1], &m, &rank_selection, &tolerance,
                  &rank, b.eigenvalues, b.matrix[2], &m, b.residual,
                  b.matrix[3], &m, b.matrix[4], &n, b.matrix[5], &n, work,
                  &lwork, real_work, &real_count, integers, &integer_count,
                  &info);
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    LAPACK_zgedmd(&scaling, &vectors, &residual, &extra, &svd, &m, &n,
                  b.matrix[0], &m, b.matrix[1], &m, &rank_selection, &tolerance,
                  &rank, b.eigenvalues, b.matrix[2], &m, b.residual,
                  b.matrix[3], &m, b.matrix[4], &n, b.matrix[5], &n, work,
                  &lwork, real_work, &real_count, integers, &integer_count,
                  &info);
  }
}
template <typename T>
void PublishMatrix(const T* source, DenseBlasMatrixView<T> destination,
                   extent_t rows, extent_t columns) {
  for (extent_t j = 0; j < columns; ++j) {
    for (extent_t i = 0; i < rows; ++i) {
      Entry(destination, i, j) = source[j * destination.rows() + i];
    }
  }
}
template <typename T>
void Publish(LapackDmdOptions options, LapackDmdBuffers<T> b, Staged<T> staged,
             const T* work, const Real<T>* real_work, lapack_int rank) {
  const auto m = b.snapshots.rows();
  const auto n = b.snapshots.columns();
  PublishMatrix(staged.matrix[0], b.snapshots, m, rank);
  PublishMatrix(staged.matrix[1], b.successors, m,
                options.residuals ? rank : n);
  if (options.vectors == LapackDmdVectors::kExplicit) {
    PublishMatrix(staged.matrix[2], b.modes, m, rank);
  }
  if (options.extra != LapackDmdExtra::kNone) {
    PublishMatrix(staged.matrix[3], b.extra, m, rank);
  }
  if (Vectors(options)) {
    PublishMatrix(staged.matrix[4], b.reduced_modes, rank, rank);
  }
  PublishMatrix(staged.matrix[5], b.reduced_workspace, rank, rank);
  for (extent_t i = 0; i < rank; ++i) {
    if constexpr (DenseBlasComplex<T>) {
      b.eigenvalues.data()[i] = staged.eigenvalues[i];
    } else {
      b.eigenvalues.data()[i] = {staged.eigenreal[i], staged.eigenimag[i]};
    }
    if (options.residuals) {
      b.residual_norms.data()[i] = staged.residual[i];
    }
  }
  for (extent_t i = 0; i < n; ++i) {
    if constexpr (DenseBlasComplex<T>) {
      b.singular_values.data()[i] = real_work[i];
    } else {
      b.singular_values.data()[i] = work[i];
    }
  }
}
template <typename T>
Status Run(LapackDmdOptions options, LapackDmdBuffers<T> b, Real<T> tolerance,
           index_t& rank, const LapackWorkspacePlan& plan,
           const LapackWorkspace& workspace, LapackReport& report) {
  const auto staged = Stage(b, plan, workspace);
  auto* work = static_cast<T*>(workspace.regions[checked::kScalar].data());
  auto* real_work =
      static_cast<Real<T>*>(workspace.regions[checked::kReal].data());
  // Like the existing SVD least-squares adapters, expose at most the
  // checked preferred WORK count; surplus caller bytes remain spare capacity.
  const auto lwork = static_cast<lapack_int>(
      std::min(workspace.regions[checked::kScalar].size() / sizeof(T),
               static_cast<std::size_t>(
                   plan.regions[checked::kScalar].preferred_entries)));
  const auto real_count =
      static_cast<lapack_int>(plan.regions[checked::kReal].minimum_entries);
  const auto integer_count =
      static_cast<lapack_int>(plan.regions[checked::kInteger].minimum_entries);
  auto* integers =
      checked::IntegerObjects(checked::kInteger, integer_count, workspace);
  // INFO and rank must overwrite every byte before they can authorize
  // publication.
  lapack_int native_rank = std::numeric_limits<lapack_int>::min();
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native(options, static_cast<lapack_int>(b.snapshots.rows()),
         static_cast<lapack_int>(b.snapshots.columns()), tolerance, staged,
         work, lwork, real_work, real_count, integers, integer_count,
         native_rank, info);
  if (info == std::numeric_limits<lapack_int>::min()) {
    report.outcome = LapackOutcome::kPartialResult;
    return Status(ErrorCode::kProvider);
  }
  report.native_info = info;
  if (info == 2 || info == 3) {
    report.outcome = LapackOutcome::kNonconvergence;
    return Status(ErrorCode::kNumerical);
  }
  if ((info != 0 && info != 4) || native_rank < 1 ||
      native_rank > b.snapshots.columns()) {
    const auto status = checked::ProviderDefect(info, report);
    report.output_validity = LapackOutputValidity::kUnchanged;
    return status;
  }
  Publish(options, b, staged, work, real_work, native_rank);
  rank = native_rank;
  report.output_validity = LapackOutputValidity::kComplete;
  report.outcome =
      info == 4 ? LapackOutcome::kAccuracyWarning : LapackOutcome::kSuccess;
  return info == 4 ? Status(ErrorCode::kNumerical) : Status::Ok();
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               LapackDmdOptions options, LapackDmdBuffers<T> b,
               Real<T> tolerance, index_t& rank,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const auto numerical = Spans(b);
  const std::array spans{numerical[0], numerical[1],
                         numerical[2], numerical[3],
                         numerical[4], numerical[5],
                         numerical[6], numerical[7],
                         numerical[8], checked::ObjectStorage(rank)};
  Status status = checked::Disjoint(spans);
  if (status.ok()) {
    status = checked::CheckMetadata(provider, plan, workspace, report, spans);
  }
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<T>(), report);
  const auto expected = Query(provider, options, b, tolerance);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  if (!status.ok()) {
    return status;
  }
  if (b.snapshots.columns() == 0) {
    rank = 0;
    return checked::Complete(report);
  }
  status = Input(b);
  if (!status.ok()) {
    return status;
  }
  return Run(options, b, tolerance, rank, plan, workspace, report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryGedmdWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdOptions options,
    LapackDmdBuffers<float> buffers, float tolerance) {
  return Query(provider, options, buffers, tolerance);
}
Status Gedmd(const ReferenceLapackProvider& provider, LapackDmdOptions options,
             LapackDmdBuffers<float> buffers, float tolerance, index_t& rank,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, options, buffers, tolerance, rank, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryGedmdWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdOptions options,
    LapackDmdBuffers<double> buffers, double tolerance) {
  return Query(provider, options, buffers, tolerance);
}
Status Gedmd(const ReferenceLapackProvider& provider, LapackDmdOptions options,
             LapackDmdBuffers<double> buffers, double tolerance, index_t& rank,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, options, buffers, tolerance, rank, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryGedmdWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdOptions options,
    LapackDmdBuffers<std::complex<float>> buffers, float tolerance) {
  return Query(provider, options, buffers, tolerance);
}
Status Gedmd(const ReferenceLapackProvider& provider, LapackDmdOptions options,
             LapackDmdBuffers<std::complex<float>> buffers, float tolerance,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, options, buffers, tolerance, rank, plan, workspace,
                 report);
}
Result<LapackWorkspacePlan> QueryGedmdWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdOptions options,
    LapackDmdBuffers<std::complex<double>> buffers, double tolerance) {
  return Query(provider, options, buffers, tolerance);
}
Status Gedmd(const ReferenceLapackProvider& provider, LapackDmdOptions options,
             LapackDmdBuffers<std::complex<double>> buffers, double tolerance,
             index_t& rank, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, options, buffers, tolerance, rank, plan, workspace,
                 report);
}
}  // namespace asc
