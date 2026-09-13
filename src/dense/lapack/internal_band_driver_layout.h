#ifndef ASC_DENSE_LAPACK_INTERNAL_BAND_DRIVER_LAYOUT_H_
#define ASC_DENSE_LAPACK_INTERNAL_BAND_DRIVER_LAYOUT_H_

#include <cstddef>
#include <limits>
#include <type_traits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "internal_band_expert.h"
#include "internal_layout.h"

namespace asc::internal_band_driver_layout {

template <typename T>
extent_t LeadingDimension(LapackPositiveDefiniteBandView<T> band,
                          bool force_pack) {
  return force_pack ? band.bandwidth() + 1
                    : internal_band_expert::LeadingDimension(band);
}

template <typename T>
Status AddBandPacking(LapackPositiveDefiniteBandView<T> band, bool force_pack,
                      LapackWorkspacePlan& plan) {
  if (!force_pack || band.layout() == DenseBlasLayout::kRowMajor) {
    return internal_band_expert::AddBandPacking(band, plan);
  }
  const extent_t width = band.bandwidth() + 1;
  if (band.order() > std::numeric_limits<extent_t>::max() / width) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t count = band.order() * width;
  auto& region = plan.regions[internal_lapack_layout::kRegion];
  if (count > std::numeric_limits<extent_t>::max() - region.minimum_entries) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t total = count + region.minimum_entries;
  region = {total, total, sizeof(T), alignof(T)};
  return internal_lapack_layout::CheckTotal(plan);
}

template <typename T>
T* PackOriginal(LapackPositiveDefiniteBandView<T> band,
                std::remove_const_t<T>*& cursor, bool force_pack) {
  if (!force_pack || band.layout() == DenseBlasLayout::kRowMajor ||
      band.order() == 0) {
    return internal_band_expert::PackBand(band, cursor, true);
  }
  auto* result = cursor;
  const extent_t width = band.bandwidth() + 1;
  internal_band_expert::Visit(band, [&](extent_t i, extent_t j) {
    const extent_t physical = band.triangle() == DenseBlasTriangle::kUpper
                                  ? band.bandwidth() - (j - i)
                                  : i - j;
    const auto offset = internal_band_expert::Offset(band, i, j);
    if constexpr (DenseBlasComplex<std::remove_const_t<T>>) {
      if (i == j) {
        result[j * width + physical] =
            std::remove_const_t<T>(band.storage().data()[offset].real(), 0);
        return;
      }
    }
    result[j * width + physical] = band.storage().data()[offset];
  });
  cursor += band.order() * width;
  return result;
}

// Output-only AF/X reservation does not load any old output scalar.
template <typename T>
T* ReserveBand(LapackPositiveDefiniteBandView<T> band, T*& cursor) {
  if (band.layout() == DenseBlasLayout::kColumnMajor || band.order() == 0) {
    return band.storage().data();
  }
  auto* result = cursor;
  cursor += band.order() * (band.bandwidth() + 1);
  return result;
}
template <typename T>
T* ReserveSolution(DenseBlasMatrixView<T> matrix, T*& cursor) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor || matrix.rows() == 0 ||
      matrix.columns() == 0) {
    return matrix.data();
  }
  auto* result = cursor;
  cursor += matrix.rows() * matrix.columns();
  return result;
}

// Only called when the actual driver applied Hermitian equilibration. That
// operation writes every selected diagonal with zero imaginary component.
template <typename T>
void PublishOriginal(const T* packed, LapackPositiveDefiniteBandView<T> band,
                     bool forced_pack) {
  if (!forced_pack || band.layout() == DenseBlasLayout::kRowMajor) {
    internal_band_expert::UnpackBand(packed, band, band.order());
    return;
  }
  const extent_t width = band.bandwidth() + 1;
  internal_band_expert::Visit(band, [&](extent_t i, extent_t j) {
    const extent_t physical = band.triangle() == DenseBlasTriangle::kUpper
                                  ? band.bandwidth() - (j - i)
                                  : i - j;
    band.storage().data()[internal_band_expert::Offset(band, i, j)] =
        packed[j * width + physical];
  });
}

}  // namespace asc::internal_band_driver_layout
#endif  // ASC_DENSE_LAPACK_INTERNAL_BAND_DRIVER_LAYOUT_H_
