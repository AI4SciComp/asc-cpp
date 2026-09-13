#ifndef ASC_DENSE_LAPACK_TRIANGULAR_BAND_VIEW_H_
#define ASC_DENSE_LAPACK_TRIANGULAR_BAND_VIEW_H_

/** @file
 * @brief Borrowed triangular band storage with the full LAPACK bandwidth
 * domain.
 */

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc {

/** @brief Selected triangular band coefficients without implicit conversion.
 *
 * For zero-based indices, column-major upper A(i,j) occupies
 * data[bandwidth+i-j+j*ld], and lower A(i,j) occupies data[i-j+j*ld].
 * Row-major upper A(i,j) occupies data[i*ld+j-i], and lower A(i,j)
 * occupies data[i*ld+bandwidth+j-i]. Only selected entries with
 * abs(i-j)<=bandwidth and 0<=i,j<order are matrix coefficients. The other
 * triangle is mathematically zero; complex coefficients do not imply a
 * Hermitian or symmetric completion. A routine's separate unit-diagonal
 * option makes the stored diagonal unused.
 *
 * Bandwidth may equal or exceed order, unlike the existing BLAS triangular
 * band descriptor's nonempty domain. Full ld*order backing is required,
 * including padding and unused corners. Construction inspects metadata only,
 * takes O(1) work, allocates nothing on success, and performs no transfer or
 * densification. No routine or foreign integer capacity is implied.
 *
 * The descriptor borrows live scalar objects in host or pinned-host storage;
 * their lifetime must cover every use. Immutable storage may be shared;
 * callers must exclude overlapping mutation for the duration of each use.
 */
template <DenseBlasScalar Element>
class LapackTriangularBandView {
 public:
  /** @brief Validates a column-major LAPACK triangular band table.
   * @param data Borrowed live scalar objects; null is allowed for order zero.
   * @param order Nonnegative logical square matrix order.
   * @param bandwidth Nonnegative stored off-diagonal count, including >=order.
   * @param triangle Upper or lower triangular interpretation.
   * @param leading_dimension Physical column stride, at least bandwidth+1.
   * @param backing Host or pinned-host span covering the complete ld*order
   * table.
   * @return Descriptor or tag, shape, overflow, alignment or storage failure.
   * No coefficients are read or modified.
   */
  static Result<LapackTriangularBandView> Create(Element* data, extent_t order,
                                                 extent_t bandwidth,
                                                 DenseBlasTriangle triangle,
                                                 stride_t leading_dimension,
                                                 ConstMemoryView backing) {
    return Create(data, order, bandwidth, triangle,
                  DenseBlasLayout::kColumnMajor, leading_dimension, backing);
  }

  /** @brief Validates the explicitly selected band encoding and full capacity.
   * @param data Borrowed live scalar objects; null is allowed for order zero.
   * @param order Nonnegative logical square matrix order.
   * @param bandwidth Nonnegative stored off-diagonal count, including >=order.
   * @param triangle Upper or lower triangular interpretation.
   * @param layout Column-major LAPACK or row-major encoding from the class
   * contract.
   * @param leading_dimension Physical column/row stride, at least bandwidth+1.
   * @param backing Host or pinned-host span covering all ld*order scalar
   * entries.
   * @return Descriptor or invalid-tag/dimension, shape, overflow, alignment or
   * storage failure. Unsupported placement returns kMemoryAccess before any
   * coefficient access. Empty tables still validate bandwidth+1 and stride.
   */
  static Result<LapackTriangularBandView> Create(Element* data, extent_t order,
                                                 extent_t bandwidth,
                                                 DenseBlasTriangle triangle,
                                                 DenseBlasLayout layout,
                                                 stride_t leading_dimension,
                                                 ConstMemoryView backing) {
    if (order < 0 || bandwidth < 0 ||
        (triangle != DenseBlasTriangle::kUpper &&
         triangle != DenseBlasTriangle::kLower) ||
        (layout != DenseBlasLayout::kColumnMajor &&
         layout != DenseBlasLayout::kRowMajor)) {
      return Status(ErrorCode::kInvalidArgument);
    }
    if (backing.space() != MemorySpace::kHost &&
        backing.space() != MemorySpace::kPinnedHost) {
      return Status(ErrorCode::kMemoryAccess);
    }
    const auto stored_rows = CheckedAdd<extent_t>(bandwidth, 1);
    if (!stored_rows.ok()) {
      return stored_rows.status();
    }
    if (leading_dimension < *stored_rows) {
      return Status(ErrorCode::kShape);
    }
    const bool column_major = layout == DenseBlasLayout::kColumnMajor;
    auto storage = DenseBlasMatrixView<Element>::Create(
        data, column_major ? leading_dimension : order,
        column_major ? order : leading_dimension, layout, leading_dimension,
        backing);
    if (!storage.ok()) {
      return storage.status();
    }
    return LapackTriangularBandView(*storage, bandwidth, triangle);
  }

  /** @brief Returns the logical square matrix order.
   * @return Nonnegative order, independent of physical layout.
   */
  [[nodiscard]] extent_t order() const noexcept {
    return layout() == DenseBlasLayout::kColumnMajor ? storage_.columns()
                                                     : storage_.rows();
  }
  /** @brief Returns the selected physical band encoding.
   * @return Column-major or row-major layout from the class contract.
   */
  [[nodiscard]] DenseBlasLayout layout() const noexcept {
    return storage_.layout();
  }
  /** @brief Returns the stored off-diagonal count.
   * @return Nonnegative bandwidth, which can equal or exceed order.
   */
  [[nodiscard]] extent_t bandwidth() const noexcept { return bandwidth_; }
  /** @brief Returns which triangle contains matrix coefficients.
   * @return Upper or lower triangular interpretation, without conjugate
   * completion.
   */
  [[nodiscard]] DenseBlasTriangle triangle() const noexcept {
    return triangle_;
  }
  /** @brief Returns the diagonal offset within each physical column or row.
   * @return Column-major: bandwidth for upper, zero for lower. Row-major:
   * zero for upper, bandwidth for lower; the result is then a column offset.
   * A routine's unit-diagonal option makes these stored entries unused.
   */
  [[nodiscard]] extent_t diagonal_row() const noexcept {
    const bool upper = triangle_ == DenseBlasTriangle::kUpper;
    const bool column_major = layout() == DenseBlasLayout::kColumnMajor;
    return upper == column_major ? bandwidth_ : 0;
  }
  /** @brief Returns the physical table, including unused corners and padding.
   * @return Column-major ld-by-order or row-major order-by-ld borrowed storage.
   * This table is a band encoding, not an order-by-order dense matrix.
   */
  [[nodiscard]] DenseBlasMatrixView<Element> storage() const noexcept {
    return storage_;
  }

 private:
  LapackTriangularBandView(DenseBlasMatrixView<Element> storage,
                           extent_t bandwidth, DenseBlasTriangle triangle)
      : storage_(storage), bandwidth_(bandwidth), triangle_(triangle) {}

  DenseBlasMatrixView<Element> storage_;
  extent_t bandwidth_;
  DenseBlasTriangle triangle_;
};

}  // namespace asc

#endif  // ASC_DENSE_LAPACK_TRIANGULAR_BAND_VIEW_H_
