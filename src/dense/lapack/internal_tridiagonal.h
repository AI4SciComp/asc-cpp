#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIDIAGONAL_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIDIAGONAL_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.
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
#include "asc/dense/providers/lapack_tridiagonal.h"
#include "internal_layout.h"
#include "internal_tridiagonal_counts.h"
#include "internal_workspace_context.h"
#include "lapack_build_config.h"

// Exact installed pinned prototypes; complex objects are the audited GNU
// libstdc++ specializations, never reinterpreted provider structs.
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#elif ASC_LAPACK_INTEGER_BITS != 32
#error "Reference LAPACK requires the explicit attested integer width"
#endif
#include <lapack.h>          // IWYU pragma: export
#include <lapacke_config.h>  // IWYU pragma: export

#if !defined(__GLIBCXX__) || __GLIBCXX__ != 20230528
#error "Reference LAPACK complex ABI requires the audited libstdc++ build"
#endif

namespace asc::internal_tridiagonal {

static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);
constexpr extent_t kLimit = std::numeric_limits<lapack_int>::max();
constexpr std::size_t kScalar =
    static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
constexpr std::size_t kReal =
    static_cast<std::size_t>(LapackWorkspaceKind::kReal);
constexpr std::size_t kInteger =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
constexpr std::size_t kLayout =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);
constexpr std::size_t kScratch =
    static_cast<std::size_t>(LapackWorkspaceKind::kScratch);

template <typename T>
constexpr LapackScalarKind Kind() {
  if constexpr (std::is_same_v<T, float>) {
    return LapackScalarKind::kF32;
  } else if constexpr (std::is_same_v<T, double>) {
    return LapackScalarKind::kF64;
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return LapackScalarKind::kC64;
  } else {
    return LapackScalarKind::kC128;
  }
}

template <typename T>
ConstMemoryView ObjectStorage(const T& object) {
  return {&object, sizeof(T), MemorySpace::kHost};
}

inline bool Overlap(ConstMemoryView a, ConstMemoryView b) {
  if (a.size() == 0 || b.size() == 0) {
    return false;
  }
  const auto x = reinterpret_cast<std::uintptr_t>(a.data());
  const auto y = reinterpret_cast<std::uintptr_t>(b.data());
  return x <= y ? y - x < a.size() : x - y < b.size();
}

inline Status Disjoint(std::span<const ConstMemoryView> spans) {
  for (std::size_t i = 0; i < spans.size(); ++i) {
    for (std::size_t j = i + 1; j < spans.size(); ++j) {
      if (Overlap(spans[i], spans[j])) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

inline Status CheckMetadata(const ReferenceLapackProvider& provider,
                            const LapackWorkspacePlan& plan,
                            const LapackWorkspace& workspace,
                            const LapackReport& report,
                            std::span<const ConstMemoryView> operands) {
  const std::array metadata{ObjectStorage(provider), ObjectStorage(plan),
                            ObjectStorage(workspace), ObjectStorage(report)};
  Status status = Disjoint(metadata);
  if (!status.ok()) {
    return status;
  }
  for (const auto memory : metadata) {
    for (const auto operand : operands) {
      if (Overlap(memory, operand)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    for (const auto& region : workspace.regions) {
      if (Overlap(memory, region)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

inline void StartReport(const ReferenceLapackProvider& provider,
                        std::string_view routine, LapackReport& report) {
  report = {};
  report.provider = provider.identity();
  std::copy(routine.begin(), routine.end(), report.routine.begin());
}

inline Status Complete(LapackReport& report) {
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

inline Status ProviderDefect(lapack_int info, LapackReport& report) {
  report.native_info = info;
  report.outcome = info < 0 ? LapackOutcome::kProviderArgument
                            : LapackOutcome::kPartialResult;
  report.output_validity = LapackOutputValidity::kUnusable;
  if (info < 0 && info != std::numeric_limits<lapack_int>::min()) {
    report.native_argument = -static_cast<std::int64_t>(info);
  }
  return Status(ErrorCode::kProvider);
}

inline Status ValidatePlan(const ReferenceLapackProvider& provider,
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

inline Status Access(const ReferenceLapackProvider& provider,
                     ConstMemoryView storage) {
  if ((storage.space() != MemorySpace::kHost &&
       storage.space() != MemorySpace::kPinnedHost) ||
      !provider.context().CanAccess(storage.space())) {
    return Status(ErrorCode::kMemoryAccess);
  }
  return Status::Ok();
}

template <typename T>
Status Vector(const ReferenceLapackProvider& provider,
              DenseBlasVectorView<T> vector, extent_t size) {
  if (vector.size() != size || vector.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  return Access(provider, vector.reachable_storage());
}

template <typename T>
std::array<ConstMemoryView, 3> Spans(LapackTridiagonalView<T> matrix) {
  return {matrix.lower().reachable_storage(),
          matrix.diagonal().reachable_storage(),
          matrix.upper().reachable_storage()};
}

template <typename T>
std::array<ConstMemoryView, 4> Spans(LapackTridiagonalLuStorage<T> factors) {
  const auto primary = Spans(factors.primary());
  return {primary[0], primary[1], primary[2],
          factors.second_upper().reachable_storage()};
}

template <typename T>
Status Storage(const ReferenceLapackProvider& provider,
               LapackTridiagonalView<T> matrix) {
  if (matrix.order() > kLimit) {
    return Status(ErrorCode::kOverflow);
  }
  for (const auto span : Spans(matrix)) {
    const Status status = Access(provider, span);
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}

template <typename T>
Status Storage(const ReferenceLapackProvider& provider,
               LapackTridiagonalLuStorage<T> factors) {
  const Status status = Storage(provider, factors.primary());
  return status.ok()
             ? Access(provider, factors.second_upper().reachable_storage())
             : status;
}

inline Status PivotMetadata(const ReferenceLapackProvider& provider,
                            ReferenceTridiagonalPivotView pivots,
                            extent_t order) {
  if (pivots.values().size() != static_cast<std::size_t>(order)) {
    return Status(ErrorCode::kShape);
  }
  return Access(provider, pivots.reachable_storage());
}

inline Status PivotValues(ReferenceTridiagonalPivotView pivots) {
  const auto values = pivots.values();
  for (std::size_t i = 0; i < values.size(); ++i) {
    const auto own = static_cast<index_t>(i + 1);
    const auto upper =
        own == static_cast<index_t>(values.size()) ? own : own + 1;
    if (values[i] < own || values[i] > upper) {
      return Status(ErrorCode::kIndex);
    }
  }
  return Status::Ok();
}

inline Status NativePivotValues(const lapack_int* values, extent_t order) {
  for (extent_t i = 0; i < order; ++i) {
    const extent_t upper = i + 1 == order ? order : i + 2;
    if (values[i] < i + 1 || values[i] > upper) {
      return Status(ErrorCode::kProvider);
    }
  }
  return Status::Ok();
}

inline Status Transpose(DenseBlasTranspose transpose) {
  return transpose == DenseBlasTranspose::kNone ||
                 transpose == DenseBlasTranspose::kTranspose ||
                 transpose == DenseBlasTranspose::kConjugateTranspose
             ? Status::Ok()
             : Status(ErrorCode::kInvalidArgument);
}

inline char TransposeCharacter(DenseBlasTranspose transpose) {
  if (transpose == DenseBlasTranspose::kNone) {
    return 'N';
  }
  return transpose == DenseBlasTranspose::kTranspose ? 'T' : 'C';
}

// With <=1 logical column no second-column address uses the original stride.
// max(1,n) also satisfies upstream validation for nonempty factor/estimate
// operations that retain NRHS=0 and valid surrogate/dummy RHS objects.
template <typename T>
extent_t Leading(DenseBlasMatrixView<T> matrix) {
  return matrix.layout() == DenseBlasLayout::kRowMajor ||
                 matrix.columns() <= 1 || matrix.rows() == 0
             ? std::max<extent_t>(1, matrix.rows())
             : matrix.leading_dimension();
}

template <typename T>
Status Matrix(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<T> matrix, extent_t order) {
  if (matrix.rows() != order) {
    return Status(ErrorCode::kShape);
  }
  if (matrix.rows() > kLimit || matrix.columns() > kLimit ||
      Leading(matrix) > kLimit) {
    return Status(ErrorCode::kOverflow);
  }
  return Access(provider, matrix.reachable_storage());
}

inline Status AddPivots(extent_t order, LapackWorkspacePlan& plan) {
  plan.regions[kInteger] = {order, order, sizeof(lapack_int),
                            alignof(lapack_int)};
  return internal_lapack_layout::CheckTotal(plan);
}

template <typename T>
Status AddEstimator(extent_t order, bool refinement,
                    LapackWorkspacePlan& plan) {
  Status status = internal_tridiagonal_counts::Estimator(order, kLimit);
  if (!status.ok()) {
    return status;
  }
  const extent_t count = DenseBlasReal<T> && refinement ? 3 * order : 2 * order;
  plan.regions[kScalar] = {count, count, sizeof(T), alignof(T)};
  // Native pivot conversion precedes the separate, simultaneously live real
  // estimator IWORK array. The frozen kPivotConversion role is ASC index_t
  // storage and is deliberately not reinterpreted as foreign INTEGER.
  const extent_t integer_count = DenseBlasReal<T> ? 2 * order : order;
  plan.regions[kInteger] = {integer_count, integer_count, sizeof(lapack_int),
                            alignof(lapack_int)};
  if constexpr (DenseBlasComplex<T>) {
    if (refinement) {
      using Real = DenseBlasRealType<T>;
      plan.regions[kReal] = {order, order, sizeof(Real), alignof(Real)};
    }
  }
  return internal_lapack_layout::CheckTotal(plan);
}

inline lapack_int* IntegerObjects(std::size_t region, extent_t order,
                                  const LapackWorkspace& workspace) {
  return order == 0 ? nullptr
                    : ::new (workspace.regions[region].data())
                          lapack_int[static_cast<std::size_t>(order)];
}

// Native output pivots must overwrite every byte before value validation.
// Initialize only after the caller's full structural preflight has succeeded.
inline lapack_int* OutputPivotObjects(extent_t order,
                                      const LapackWorkspace& workspace) {
  auto* result = IntegerObjects(kInteger, order, workspace);
  if (order > 0) {
    std::fill_n(result, static_cast<std::size_t>(order),
                std::numeric_limits<lapack_int>::min());
  }
  return result;
}

inline lapack_int* ConvertPivots(ReferenceTridiagonalPivotView pivots,
                                 const LapackWorkspace& workspace) {
  auto* result = IntegerObjects(
      kInteger, static_cast<extent_t>(pivots.values().size()), workspace);
  for (std::size_t i = 0; i < pivots.values().size(); ++i) {
    result[i] = static_cast<lapack_int>(pivots.values()[i]);
  }
  return result;
}

// Two independent arrays occupy disjoint checked subregions. Starting the
// estimator array does not end the first n pivot objects' lifetimes.
inline lapack_int* EstimatorIntegerObjects(extent_t order,
                                           const LapackWorkspace& workspace) {
  auto* bytes = static_cast<std::byte*>(workspace.regions[kInteger].data());
  return order == 0 ? nullptr
                    : ::new (bytes + static_cast<std::size_t>(order) *
                                         sizeof(lapack_int))
                          lapack_int[static_cast<std::size_t>(order)];
}

template <typename T>
Status ZeroDiagonal(LapackTridiagonalLuStorage<const T> factors,
                    LapackReport& report) {
  const auto diagonal = factors.primary().diagonal();
  for (extent_t i = 0; i < diagonal.size(); ++i) {
    if (diagonal.data()[i] == T{}) {
      report.outcome = LapackOutcome::kSingular;
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
  }
  return Status::Ok();
}

template <typename Real>
Status Diagnostic(Real value, LapackReport& report) {
  if (value < 0) {
    return ProviderDefect(
        static_cast<lapack_int>(report.native_info.value_or(0)), report);
  }
  if (!std::isfinite(value)) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return Status::Ok();
}

template <typename T>
T* Nonnull(T* pointer, T& dummy) {
  return pointer == nullptr ? &dummy : pointer;
}

}  // namespace asc::internal_tridiagonal

#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIDIAGONAL_H_
