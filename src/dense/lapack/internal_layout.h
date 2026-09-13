#ifndef ASC_DENSE_LAPACK_INTERNAL_LAYOUT_H_
#define ASC_DENSE_LAPACK_INTERNAL_LAYOUT_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/workspace.h"

namespace asc::internal_lapack_layout {

inline constexpr std::size_t kRegion =
    static_cast<std::size_t>(LapackWorkspaceKind::kLayoutConversion);

// Even individually representable region products can overflow in aggregate.
// These are generated plans; each role has a positive entry size and valid
// nonnegative capacities before this final simultaneous-storage check.
inline Status CheckTotal(const LapackWorkspacePlan& plan) {
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

// Append only ASC-owned conversion storage, never a foreign LWORK count.
// All provider dimensions/intermediate formulas are checked by the caller.
template <typename T>
Status AddPacking(DenseBlasMatrixView<T> matrix, LapackWorkspacePlan& plan) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    return CheckTotal(plan);
  }
  extent_t count = 0;
  if (matrix.rows() != 0) {
    if (matrix.columns() >
        std::numeric_limits<extent_t>::max() / matrix.rows()) {
      return Status(ErrorCode::kOverflow);
    }
    count = matrix.rows() * matrix.columns();
  }
  auto& region = plan.regions[kRegion];
  if (count > std::numeric_limits<extent_t>::max() - region.minimum_entries) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t total = region.minimum_entries + count;
  if (static_cast<std::uint64_t>(total) >
      std::numeric_limits<std::size_t>::max() / sizeof(T)) {
    return Status(ErrorCode::kOverflow);
  }
  region = {total, total, sizeof(T), alignof(T)};
  return CheckTotal(plan);
}

template <typename T>
extent_t LeadingDimension(DenseBlasMatrixView<T> matrix) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    return matrix.leading_dimension();
  }
  return matrix.rows() == 0 ? 1 : matrix.rows();
}

// The validated caller region contains live scalar objects. Advance only for
// nonempty row-major operands, avoiding arithmetic on an empty null pointer.
template <typename T>
T* Pack(DenseBlasMatrixView<T> matrix, std::remove_const_t<T>*& cursor) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor || matrix.rows() == 0 ||
      matrix.columns() == 0) {
    return matrix.data();
  }
  auto* result = cursor;
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    for (extent_t i = 0; i < matrix.rows(); ++i) {
      result[j * matrix.rows() + i] =
          matrix.data()[i * matrix.leading_dimension() + j];
    }
  }
  cursor += matrix.rows() * matrix.columns();
  return result;
}

// Publish only defined numerical outputs. Padding and all inputs stay intact.
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

}  // namespace asc::internal_lapack_layout

#endif  // ASC_DENSE_LAPACK_INTERNAL_LAYOUT_H_
