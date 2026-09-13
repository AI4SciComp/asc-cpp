#ifndef ASC_DENSE_LAPACK_INTERNAL_CHOLESKY_EXPERT_H_
#define ASC_DENSE_LAPACK_INTERNAL_CHOLESKY_EXPERT_H_

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
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "internal_cholesky_expert_counts.h"
#include "lapack_build_config.h"

// Exact pinned declarations, private to the audited optional GNU facet.
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#elif ASC_LAPACK_INTEGER_BITS != 32
#error "Reference LAPACK requires an explicitly selected 32/64 integer ABI"
#endif
#include <lapack.h>          // IWYU pragma: export
#include <lapacke_config.h>  // IWYU pragma: export

#if !defined(__GLIBCXX__) || __GLIBCXX__ != 20230528
#error "Reference LAPACK complex ABI requires the audited libstdc++ build"
#endif

namespace asc::internal_cholesky_expert {

static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);
constexpr extent_t kIntegerLimit = std::numeric_limits<lapack_int>::max();
constexpr std::size_t kScalar =
    static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
constexpr std::size_t kReal =
    static_cast<std::size_t>(LapackWorkspaceKind::kReal);
constexpr std::size_t kInteger =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
constexpr std::size_t kLayout =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);

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
  Status valid = Disjoint(metadata);
  if (!valid.ok()) {
    return valid;
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
  Status status =
      ValidateLapackWorkspace(supplied, expected.identity, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  for (const auto& region : workspace.regions) {
    if (region.size() != 0 && !provider.context().CanAccess(region.space())) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  return Status::Ok();
}

inline Status Triangle(DenseBlasTriangle triangle) {
  return triangle == DenseBlasTriangle::kUpper ||
                 triangle == DenseBlasTriangle::kLower
             ? Status::Ok()
             : Status(ErrorCode::kInvalidArgument);
}

inline char TriangleCharacter(DenseBlasTriangle triangle) {
  return triangle == DenseBlasTriangle::kUpper ? 'U' : 'L';
}

template <typename T>
bool Packed(DenseBlasMatrixView<T> matrix, bool force = false) {
  return force || matrix.layout() == DenseBlasLayout::kRowMajor;
}

template <typename T>
extent_t Leading(DenseBlasMatrixView<T> matrix, bool force = false) {
  return Packed(matrix, force) ? std::max<extent_t>(1, matrix.rows())
                               : matrix.leading_dimension();
}

template <typename T>
Status Matrix(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<T> matrix, bool force = false) {
  if (matrix.memory_space() != MemorySpace::kHost &&
      matrix.memory_space() != MemorySpace::kPinnedHost) {
    return Status(ErrorCode::kMemoryAccess);
  }
  if (!provider.context().CanAccess(matrix.memory_space())) {
    return Status(ErrorCode::kMemoryAccess);
  }
  for (const auto count :
       {matrix.rows(), matrix.columns(), Leading(matrix, force)}) {
    if (count > kIntegerLimit) {
      return Status(ErrorCode::kOverflow);
    }
  }
  return Status::Ok();
}

template <typename T>
Status Vector(const ReferenceLapackProvider& provider,
              DenseBlasVectorView<T> vector, extent_t size) {
  if (vector.size() != size) {
    return Status(ErrorCode::kShape);
  }
  if (vector.memory_space() != MemorySpace::kHost &&
      vector.memory_space() != MemorySpace::kPinnedHost) {
    return Status(ErrorCode::kMemoryAccess);
  }
  if (!provider.context().CanAccess(vector.memory_space())) {
    return Status(ErrorCode::kMemoryAccess);
  }
  if (vector.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}

template <typename T>
T& Entry(DenseBlasMatrixView<T> matrix, extent_t row, extent_t column) {
  return matrix.layout() == DenseBlasLayout::kColumnMajor
             ? matrix.data()[column * matrix.leading_dimension() + row]
             : matrix.data()[row * matrix.leading_dimension() + column];
}

template <typename T>
std::remove_const_t<T> RealDiagonal(const T& value) {
  if constexpr (DenseBlasComplex<std::remove_const_t<T>>) {
    return {value.real(), 0};
  } else {
    return value;
  }
}

inline Status TotalBytes(const LapackWorkspacePlan& plan) {
  std::size_t total = 0;
  for (const auto& region : plan.regions) {
    const auto count = static_cast<std::uint64_t>(region.preferred_entries);
    if (count > (std::numeric_limits<std::size_t>::max() - total) /
                    region.entry_bytes) {
      return Status(ErrorCode::kOverflow);
    }
    total += static_cast<std::size_t>(count) * region.entry_bytes;
  }
  return Status::Ok();
}

template <typename T>
Status AddPacking(DenseBlasMatrixView<T> matrix, LapackWorkspacePlan& plan,
                  bool force = false) {
  if (!Packed(matrix, force)) {
    return TotalBytes(plan);
  }
  const auto previous = plan.regions[kLayout].preferred_entries;
  const auto limit = std::numeric_limits<extent_t>::max() - previous;
  if (matrix.rows() != 0 && matrix.columns() > limit / matrix.rows()) {
    return Status(ErrorCode::kOverflow);
  }
  const auto count = previous + matrix.rows() * matrix.columns();
  plan.regions[kLayout] = {count, count, sizeof(T), alignof(T)};
  return TotalBytes(plan);
}

// Counts follow fixed WORK/RWORK/IWORK arguments, not a fabricated LWORK=-1.
// Both real and complex LACN2 evaluate provider INTEGER 3*N.
template <typename T>
Status AddEstimatorWork(extent_t order, LapackWorkspacePlan& plan) {
  Status counts =
      internal_cholesky_expert_counts::Estimator(order, kIntegerLimit);
  if (!counts.ok()) {
    return counts;
  }
  if constexpr (DenseBlasComplex<T>) {
    using Real = DenseBlasRealType<T>;
    plan.regions[kScalar] = {2 * order, 2 * order, sizeof(T), alignof(T)};
    plan.regions[kReal] = {order, order, sizeof(Real), alignof(Real)};
  } else {
    plan.regions[kScalar] = {3 * order, 3 * order, sizeof(T), alignof(T)};
    plan.regions[kInteger] = {order, order, sizeof(lapack_int),
                              alignof(lapack_int)};
  }
  return TotalBytes(plan);
}

template <typename T>
lapack_int* IntegerWork(extent_t order, const LapackWorkspace& workspace) {
  if constexpr (DenseBlasComplex<T>) {
    return nullptr;
  } else {
    return ::new (workspace.regions[kInteger].data())
        lapack_int[static_cast<std::size_t>(order)];
  }
}

// Only call after capacity/lifetime checks. Output-only packing reserves live
// caller objects without reading or initializing the ignored previous values.
template <typename T>
std::remove_const_t<T>* Reserve(DenseBlasMatrixView<T> matrix,
                                std::remove_const_t<T>*& cursor) {
  auto* result = cursor;
  if (matrix.rows() != 0 && matrix.columns() != 0) {
    cursor += matrix.rows() * matrix.columns();
  }
  return result;
}

template <typename T>
T* PackTriangle(DenseBlasMatrixView<T> matrix, DenseBlasTriangle triangle,
                bool hermitian, std::remove_const_t<T>*& cursor,
                bool force = false) {
  if (!Packed(matrix, force) || matrix.rows() == 0) {
    return matrix.data();
  }
  auto* result = Reserve(matrix, cursor);
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    const auto begin = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const auto end =
        triangle == DenseBlasTriangle::kUpper ? j + 1 : matrix.rows();
    for (extent_t i = begin; i < end; ++i) {
      result[j * matrix.rows() + i] = i == j && hermitian
                                          ? RealDiagonal(Entry(matrix, i, j))
                                          : Entry(matrix, i, j);
    }
  }
  return result;
}

template <typename T>
T* PackFull(DenseBlasMatrixView<T> matrix, std::remove_const_t<T>*& cursor) {
  if (!Packed(matrix) || matrix.rows() == 0 || matrix.columns() == 0) {
    return matrix.data();
  }
  auto* result = Reserve(matrix, cursor);
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    for (extent_t i = 0; i < matrix.rows(); ++i) {
      result[j * matrix.rows() + i] = Entry(matrix, i, j);
    }
  }
  return result;
}

template <typename T>
T* PackOutput(DenseBlasMatrixView<T> matrix, T*& cursor, bool force = false) {
  return Packed(matrix, force) && matrix.rows() != 0 && matrix.columns() != 0
             ? Reserve(matrix, cursor)
             : matrix.data();
}

template <typename T>
void PublishFull(const T* values, DenseBlasMatrixView<T> matrix) {
  if (!Packed(matrix)) {
    return;
  }
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    for (extent_t i = 0; i < matrix.rows(); ++i) {
      Entry(matrix, i, j) = values[j * matrix.rows() + i];
    }
  }
}

template <typename T>
void PublishTriangle(const T* values, DenseBlasMatrixView<T> matrix,
                     DenseBlasTriangle triangle, bool partial,
                     bool force = false) {
  if (!Packed(matrix, force)) {
    return;
  }
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    const auto begin = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const auto end =
        triangle == DenseBlasTriangle::kUpper ? j + 1 : matrix.rows();
    for (extent_t i = begin; i < end; ++i) {
      if constexpr (DenseBlasComplex<T>) {
        if (partial && i == j) {
          Entry(matrix, i, j).real(values[j * matrix.rows() + i].real());
          continue;
        }
      }
      Entry(matrix, i, j) = values[j * matrix.rows() + i];
    }
  }
}

template <typename T>
Status ZeroFactor(DenseBlasMatrixView<const T> matrix, LapackReport& report) {
  for (extent_t i = 0; i < matrix.rows(); ++i) {
    if (Entry(matrix, i, i) == T{}) {
      report.outcome = LapackOutcome::kSingular;
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
  }
  return Status::Ok();
}

}  // namespace asc::internal_cholesky_expert

#endif  // ASC_DENSE_LAPACK_INTERNAL_CHOLESKY_EXPERT_H_
