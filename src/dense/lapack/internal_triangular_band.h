#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_BAND_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_BAND_H_

#include <algorithm>
#include <limits>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/triangular_band_view.h"
#include "asc/dense/lapack/workspace.h"
#include "internal_indefinite.h"

namespace asc::internal_triangular_band {
namespace common = internal_indefinite;

template <typename T>
extent_t Leading(LapackTriangularBandView<T> a) {
  return a.layout() == DenseBlasLayout::kColumnMajor
             ? a.storage().leading_dimension()
             : a.bandwidth() + 1;
}

// The descriptor has checked the complete n*ld byte range, with ld>=kd+1.
// Subtract the in-band distance before adding the physical major offset.
inline extent_t Offset(extent_t kd, extent_t ld, DenseBlasTriangle triangle,
                       DenseBlasLayout layout, extent_t i, extent_t j) {
  if (layout == DenseBlasLayout::kColumnMajor) {
    return j * ld +
           (triangle == DenseBlasTriangle::kUpper ? kd - (j - i) : i - j);
  }
  return i * ld +
         (triangle == DenseBlasTriangle::kLower ? kd - (i - j) : j - i);
}

template <typename T>
Status Packing(LapackTriangularBandView<T> a, LapackWorkspacePlan& plan) {
  if (a.layout() == DenseBlasLayout::kColumnMajor || a.order() == 0) {
    return common::Total(plan);
  }
  const extent_t extra = a.order() * (a.bandwidth() + 1);
  auto& region = plan.regions[common::kLayout];
  if (extra > std::numeric_limits<extent_t>::max() - region.preferred_entries) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t count = region.preferred_entries + extra;
  region = {count, count, sizeof(T), alignof(T)};
  return common::Total(plan);
}

template <typename T>
T* Pack(LapackTriangularBandView<T> a, DenseBlasDiagonal diagonal,
        std::remove_const_t<T>*& cursor, bool diagonal_only) {
  if (a.layout() == DenseBlasLayout::kColumnMajor || a.order() == 0) {
    return a.storage().data();
  }
  auto* packed = cursor;
  const extent_t ld = a.bandwidth() + 1;
  cursor += a.order() * ld;
  for (extent_t j = 0; j < a.order(); ++j) {
    if (diagonal_only) {
      packed[Offset(a.bandwidth(), ld, a.triangle(),
                    DenseBlasLayout::kColumnMajor, j, j)] =
          a.storage()
              .data()[Offset(a.bandwidth(), a.storage().leading_dimension(),
                             a.triangle(), a.layout(), j, j)];
      continue;
    }
    const bool upper = a.triangle() == DenseBlasTriangle::kUpper;
    const extent_t first = upper ? std::max<extent_t>(0, j - a.bandwidth()) : j;
    const extent_t last =
        upper ? j + 1 : j + std::min(a.bandwidth(), a.order() - 1 - j) + 1;
    for (extent_t i = first; i < last; ++i) {
      if (i != j || diagonal == DenseBlasDiagonal::kNonUnit) {
        packed[Offset(a.bandwidth(), ld, a.triangle(),
                      DenseBlasLayout::kColumnMajor, i, j)] =
            a.storage()
                .data()[Offset(a.bandwidth(), a.storage().leading_dimension(),
                               a.triangle(), a.layout(), i, j)];
      }
    }
  }
  return packed;
}

}  // namespace asc::internal_triangular_band

#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_BAND_H_
