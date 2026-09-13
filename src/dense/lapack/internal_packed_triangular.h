#ifndef ASC_DENSE_LAPACK_INTERNAL_PACKED_TRIANGULAR_H_
#define ASC_DENSE_LAPACK_INTERNAL_PACKED_TRIANGULAR_H_

#include <limits>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/workspace.h"
#include "internal_indefinite.h"

namespace asc::internal_packed_triangular {
namespace common = internal_indefinite;

// DenseBlasPackedMatrixView::Create has already checked n*(n+1).
inline extent_t Entries(extent_t n) { return n * (n + 1) / 2; }

inline extent_t Offset(extent_t n, DenseBlasTriangle triangle,
                       DenseBlasLayout layout, extent_t i, extent_t j) {
  if (layout == DenseBlasLayout::kColumnMajor) {
    return triangle == DenseBlasTriangle::kUpper
               ? j * (j + 1) / 2 + i
               : j * n - j * (j - 1) / 2 + i - j;
  }
  return triangle == DenseBlasTriangle::kLower
             ? i * (i + 1) / 2 + j
             : i * n - i * (i - 1) / 2 + j - i;
}

template <typename T>
Status Packing(DenseBlasPackedMatrixView<T> a, LapackWorkspacePlan& plan) {
  if (a.layout() == DenseBlasLayout::kColumnMajor || a.order() == 0) {
    return common::Total(plan);
  }
  auto& region = plan.regions[common::kLayout];
  const extent_t extra = Entries(a.order());
  if (extra > std::numeric_limits<extent_t>::max() - region.preferred_entries) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t count = region.preferred_entries + extra;
  region = {count, count, sizeof(T), alignof(T)};
  return common::Total(plan);
}

template <typename T>
T* Pack(DenseBlasPackedMatrixView<T> a, DenseBlasTriangle triangle,
        DenseBlasDiagonal diagonal, std::remove_const_t<T>*& cursor,
        bool diagonal_only = false) {
  if (a.layout() == DenseBlasLayout::kColumnMajor || a.order() == 0) {
    return a.data();
  }
  auto* packed = cursor;
  cursor += Entries(a.order());
  for (extent_t j = 0; j < a.order(); ++j) {
    const extent_t first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const extent_t last =
        triangle == DenseBlasTriangle::kUpper ? j + 1 : a.order();
    for (extent_t i = first; i < last; ++i) {
      if ((i != j || diagonal == DenseBlasDiagonal::kNonUnit) &&
          (!diagonal_only || i == j)) {
        packed[Offset(a.order(), triangle, DenseBlasLayout::kColumnMajor, i,
                      j)] =
            a.data()[Offset(a.order(), triangle, a.layout(), i, j)];
      }
    }
  }
  return packed;
}

template <typename T>
void Publish(const T* packed, DenseBlasPackedMatrixView<T> a,
             DenseBlasTriangle triangle, DenseBlasDiagonal diagonal) {
  if (a.layout() == DenseBlasLayout::kColumnMajor) {
    return;
  }
  for (extent_t j = 0; j < a.order(); ++j) {
    const extent_t first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const extent_t last =
        triangle == DenseBlasTriangle::kUpper ? j + 1 : a.order();
    for (extent_t i = first; i < last; ++i) {
      if (i != j || diagonal == DenseBlasDiagonal::kNonUnit) {
        a.data()[Offset(a.order(), triangle, a.layout(), i, j)] = packed[Offset(
            a.order(), triangle, DenseBlasLayout::kColumnMajor, i, j)];
      }
    }
  }
}

}  // namespace asc::internal_packed_triangular

#endif  // ASC_DENSE_LAPACK_INTERNAL_PACKED_TRIANGULAR_H_
