#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_H_

#include <algorithm>
#include <array>
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
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite.h"
#include "internal_indefinite_counts.h"
#include "lapack_build_config.h"

// The installed pinned declarations/mangling and verified GNU complex ABI are
// private. No struct reinterpretation or guessed hidden CHARACTER length.
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#elif ASC_LAPACK_INTEGER_BITS != 32
#error "The reference facet requires an explicitly selected integer ABI"
#endif
#include <lapack.h>          // IWYU pragma: export
#include <lapacke_config.h>  // IWYU pragma: export

#if !defined(__GLIBCXX__) || __GLIBCXX__ != 20230528
#error "The reference complex ABI requires the audited libstdc++ build"
#endif

namespace asc::internal_indefinite {

static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);
constexpr extent_t kIntegerLimit = std::numeric_limits<lapack_int>::max();
constexpr std::size_t kScalar =
    static_cast<std::size_t>(LapackWorkspaceKind::kScalar);
constexpr std::size_t kPivot =
    static_cast<std::size_t>(LapackWorkspaceKind::kInteger);
constexpr std::size_t kLayout =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);

template <typename T>
ConstMemoryView Object(const T& object) {
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

inline Status Accessible(const ReferenceLapackProvider& provider,
                         ConstMemoryView memory) {
  return (memory.space() == MemorySpace::kHost ||
          memory.space() == MemorySpace::kPinnedHost) &&
                 provider.context().CanAccess(memory.space())
             ? Status::Ok()
             : Status(ErrorCode::kMemoryAccess);
}

inline Status Metadata(const ReferenceLapackProvider& provider,
                       const LapackWorkspacePlan& plan,
                       const LapackWorkspace& workspace,
                       const LapackReport& report,
                       std::span<const ConstMemoryView> operands) {
  const std::array objects{Object(provider), Object(plan), Object(workspace),
                           Object(report)};
  Status disjoint = Disjoint(objects);
  if (!disjoint.ok()) {
    return disjoint;
  }
  for (const auto object : objects) {
    for (const auto operand : operands) {
      if (Overlap(object, operand)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    for (const auto& region : workspace.regions) {
      if (Overlap(object, region)) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

inline Status Plan(const ReferenceLapackProvider& provider,
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
  Status valid =
      ValidateLapackWorkspace(supplied, expected.identity, workspace, operands);
  if (!valid.ok()) {
    return valid;
  }
  for (const auto& region : workspace.regions) {
    if (region.size() != 0 && !provider.context().CanAccess(region.space())) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  return Status::Ok();
}

inline bool Triangle(DenseBlasTriangle triangle) {
  return triangle == DenseBlasTriangle::kUpper ||
         triangle == DenseBlasTriangle::kLower;
}

inline char Uplo(DenseBlasTriangle triangle) {
  return triangle == DenseBlasTriangle::kUpper ? 'U' : 'L';
}

template <typename T>
bool Packed(DenseBlasMatrixView<T> matrix, bool force = false) {
  return force || matrix.layout() == DenseBlasLayout::kRowMajor;
}

template <typename T>
extent_t Leading(DenseBlasMatrixView<T> matrix, bool force = false) {
  if (matrix.rows() == 0 || matrix.columns() == 0) {
    return 1;
  }
  // A single column never advances by its original ASC column stride. The
  // foreign routines may nevertheless compute BLAS terminal vector cursors;
  // use the equivalent compact stride and validate that actual value below.
  return Packed(matrix, force) || matrix.columns() == 1
             ? matrix.rows()
             : matrix.leading_dimension();
}

template <typename T>
Status Matrix(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<T> matrix, bool force = false) {
  Status access = Accessible(provider, matrix.reachable_storage());
  if (!access.ok()) {
    return access;
  }
  return matrix.rows() > kIntegerLimit || matrix.columns() > kIntegerLimit ||
                 Leading(matrix, force) > kIntegerLimit
             ? Status(ErrorCode::kOverflow)
             : Status::Ok();
}

template <typename T>
T& Entry(DenseBlasMatrixView<T> matrix, extent_t i, extent_t j) {
  return matrix.layout() == DenseBlasLayout::kColumnMajor
             ? matrix.data()[j * matrix.leading_dimension() + i]
             : matrix.data()[i * matrix.leading_dimension() + j];
}

template <typename T>
DenseBlasRealType<std::remove_const_t<T>> Real(const T& value) {
  if constexpr (DenseBlasComplex<std::remove_const_t<T>>) {
    return value.real();
  } else {
    return value;
  }
}

inline Status Total(const LapackWorkspacePlan& plan) {
  std::size_t remaining = plan.total_byte_limit;
  for (const auto& region : plan.regions) {
    const auto count = static_cast<std::uint64_t>(region.preferred_entries);
    if (count > remaining / region.entry_bytes) {
      return Status(ErrorCode::kOverflow);
    }
    remaining -= static_cast<std::size_t>(count) * region.entry_bytes;
  }
  return Status::Ok();
}

template <typename T>
Status Packing(DenseBlasMatrixView<T> matrix, LapackWorkspacePlan& plan,
               bool force = false) {
  if (!Packed(matrix, force)) {
    return Total(plan);
  }
  auto& region = plan.regions[kLayout];
  const extent_t remaining =
      std::numeric_limits<extent_t>::max() - region.preferred_entries;
  if (matrix.rows() != 0 && matrix.columns() > remaining / matrix.rows()) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t count =
      region.preferred_entries + matrix.rows() * matrix.columns();
  region = {count, count, sizeof(T), alignof(T)};
  return Total(plan);
}

template <typename Integer>
Status Paired(std::span<const Integer> values, extent_t order,
              DenseBlasTriangle triangle) {
  if (!Triangle(triangle) || order < 0 ||
      values.size() != static_cast<std::size_t>(order)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  for (extent_t i = 0; i < order;) {
    const index_t p = values[static_cast<std::size_t>(i)];
    if (p == 0 || p < -order || p > order) {
      return Status(ErrorCode::kInvalidArgument);
    }
    if (p > 0) {
      if ((triangle == DenseBlasTriangle::kUpper && p > i + 1) ||
          (triangle == DenseBlasTriangle::kLower && p < i + 1)) {
        return Status(ErrorCode::kInvalidArgument);
      }
      ++i;
    } else {
      if (i + 1 >= order || values[static_cast<std::size_t>(i + 1)] != p ||
          (triangle == DenseBlasTriangle::kUpper && -p > i + 1) ||
          (triangle == DenseBlasTriangle::kLower && -p < i + 2)) {
        return Status(ErrorCode::kInvalidArgument);
      }
      i += 2;
    }
  }
  return Status::Ok();
}

template <typename T>
T* PackTriangle(DenseBlasMatrixView<T> matrix, DenseBlasTriangle triangle,
                bool original_hermitian, std::remove_const_t<T>*& cursor,
                bool force = false) {
  if (!Packed(matrix, force) || matrix.rows() == 0) {
    return matrix.data();
  }
  auto* packed = cursor;
  cursor += matrix.rows() * matrix.columns();
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    const auto first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const auto last =
        triangle == DenseBlasTriangle::kUpper ? j + 1 : matrix.rows();
    for (extent_t i = first; i < last; ++i) {
      packed[j * matrix.rows() + i] =
          original_hermitian && i == j
              ? std::remove_const_t<T>{Real(Entry(matrix, i, j))}
              : Entry(matrix, i, j);
    }
  }
  return packed;
}

template <typename T>
T* PackRhs(DenseBlasMatrixView<T> matrix, T*& cursor) {
  if (!Packed(matrix) || matrix.rows() == 0 || matrix.columns() == 0) {
    return matrix.data();
  }
  auto* packed = cursor;
  cursor += matrix.rows() * matrix.columns();
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    for (extent_t i = 0; i < matrix.rows(); ++i) {
      packed[j * matrix.rows() + i] = Entry(matrix, i, j);
    }
  }
  return packed;
}

template <typename T>
void PublishTriangle(const T* packed, DenseBlasMatrixView<T> matrix,
                     DenseBlasTriangle triangle, bool force) {
  if (!Packed(matrix, force)) {
    return;
  }
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    const auto first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const auto last =
        triangle == DenseBlasTriangle::kUpper ? j + 1 : matrix.rows();
    for (extent_t i = first; i < last; ++i) {
      Entry(matrix, i, j) = packed[j * matrix.rows() + i];
    }
  }
}

template <typename T>
void PublishRhs(const T* packed, DenseBlasMatrixView<T> matrix) {
  if (!Packed(matrix)) {
    return;
  }
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    for (extent_t i = 0; i < matrix.rows(); ++i) {
      Entry(matrix, i, j) = packed[j * matrix.rows() + i];
    }
  }
}

inline void Start(const ReferenceLapackProvider& provider,
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

inline Status Defect(lapack_int info, LapackReport& report) {
  report.native_info = info;
  report.outcome = info < 0 ? LapackOutcome::kProviderArgument
                            : LapackOutcome::kPartialResult;
  report.output_validity = LapackOutputValidity::kUnusable;
  if (info < 0 && info != std::numeric_limits<lapack_int>::min()) {
    report.native_argument = -static_cast<index_t>(info);
  }
  return Status(ErrorCode::kProvider);
}

}  // namespace asc::internal_indefinite

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_H_
