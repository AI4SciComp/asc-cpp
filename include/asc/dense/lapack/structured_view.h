#ifndef ASC_DENSE_LAPACK_STRUCTURED_VIEW_H_
#define ASC_DENSE_LAPACK_STRUCTURED_VIEW_H_

/** @file
 * @brief Distinct LAPACK structured-storage contracts without implicit packing.
 *
 * All descriptors borrow validated CPU storage. Owners and element lifetimes
 * must cover every use; concurrent mutation of shared storage is forbidden.
 * Construction reads no values, allocates nothing on success, and performs no
 * transfer or densification. These descriptors do not imply routine coverage.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc {
namespace internal_dense_lapack {

inline bool IsHost(MemorySpace space) {
  return space == MemorySpace::kHost || space == MemorySpace::kPinnedHost;
}

inline bool IsTriangle(DenseBlasTriangle triangle) {
  return triangle == DenseBlasTriangle::kUpper ||
         triangle == DenseBlasTriangle::kLower;
}

inline Status Disjoint(std::span<const ConstMemoryView> spans) {
  for (std::size_t index = 0; index < spans.size(); ++index) {
    if (!IsHost(spans[index].space()) || !spans[index].valid()) {
      return Status(ErrorCode::kMemoryAccess);
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(spans[index].data());
    if (spans[index].size() >
        std::numeric_limits<std::uintptr_t>::max() - begin) {
      return Status(ErrorCode::kOverflow);
    }
    for (std::size_t previous = 0; previous < index; ++previous) {
      const auto other =
          reinterpret_cast<std::uintptr_t>(spans[previous].data());
      if (spans[index].size() != 0 && spans[previous].size() != 0 &&
          begin < other + spans[previous].size() &&
          other < begin + spans[index].size()) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

template <typename Element>
bool Contiguous(DenseBlasVectorView<Element> vector) {
  return vector.increment() == 1 && IsHost(vector.memory_space());
}

}  // namespace internal_dense_lapack

/** @brief Underlying real element retaining the input element's constness. */
template <DenseBlasScalar Element>
using LapackRealElement = std::conditional_t<std::is_const_v<Element>,
                                             const DenseBlasRealType<Element>,
                                             DenseBlasRealType<Element>>;

/** @brief Column-major GBTRF factor-band storage including its fill-in rows.
 *
 * Zero-based A(i,j) is stored at AB(lower+upper+i-j,j) on input. The first
 * lower rows are reserved for U fill-in and are not input matrix coefficients.
 * This cannot be constructed by reinterpreting ordinary BLAS band metadata.
 */
template <DenseBlasScalar Element>
class LapackLuBandView {
 public:
  /** @brief Checks dimensions, 2*lower+upper+1 rows, alignment and full
   * capacity.
   * @param data Column-major factor-band storage, including fill-in rows.
   * @param rows Logical matrix rows, nonnegative.
   * @param columns Logical matrix columns, nonnegative.
   * @param lower Number of subdiagonals, nonnegative.
   * @param upper Number of superdiagonals, nonnegative.
   * @param leading_dimension Physical column stride, at least 2*lower+upper+1.
   * @param backing Borrowed CPU span covering leading_dimension*columns
   * entries.
   * @return Borrowed descriptor or shape/overflow/storage failure. Null storage
   * is accepted for zero columns; no coefficients are accessed or modified.
   */
  static Result<LapackLuBandView> Create(Element* data, extent_t rows,
                                         extent_t columns, extent_t lower,
                                         extent_t upper,
                                         stride_t leading_dimension,
                                         ConstMemoryView backing) {
    if (rows < 0 || columns < 0 || lower < 0 || upper < 0 ||
        !internal_dense_lapack::IsHost(backing.space())) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const auto doubled = CheckedMultiply<extent_t>(lower, 2);
    if (!doubled.ok()) {
      return doubled.status();
    }
    const auto bands = CheckedAdd<extent_t>(*doubled, upper);
    if (!bands.ok()) {
      return bands.status();
    }
    const auto stored_rows = CheckedAdd<extent_t>(*bands, 1);
    if (!stored_rows.ok()) {
      return stored_rows.status();
    }
    if (leading_dimension < *stored_rows) {
      return Status(ErrorCode::kShape);
    }
    auto storage = DenseBlasMatrixView<Element>::Create(
        data, leading_dimension, columns, DenseBlasLayout::kColumnMajor,
        leading_dimension, backing);
    if (!storage.ok()) {
      return storage.status();
    }
    return LapackLuBandView(*storage, rows, lower, upper);
  }
  /** @brief Returns logical matrix rows, not physical band-storage rows.
   * @return Logical matrix rows, not physical band-storage rows.
   */
  [[nodiscard]] extent_t rows() const noexcept { return rows_; }
  /** @brief Returns logical columns.
   * @return Logical columns.
   */
  [[nodiscard]] extent_t columns() const noexcept { return storage_.columns(); }
  /** @brief Returns the lower input bandwidth.
   * @return The lower input bandwidth.
   */
  [[nodiscard]] extent_t lower_bandwidth() const noexcept { return lower_; }
  /** @brief Returns the upper input bandwidth before factor fill-in.
   * @return The upper input bandwidth before factor fill-in.
   */
  [[nodiscard]] extent_t upper_bandwidth() const noexcept { return upper_; }
  /** @brief Returns the zero-based physical row containing the input diagonal.
   * @return The zero-based physical row containing the input diagonal.
   */
  [[nodiscard]] extent_t diagonal_row() const noexcept {
    return lower_ + upper_;
  }
  /** @brief Returns borrowed physical storage including padding/fill-in rows.
   * @return Borrowed physical storage including padding/fill-in rows.
   */
  [[nodiscard]] DenseBlasMatrixView<Element> storage() const noexcept {
    return storage_;
  }

 private:
  LapackLuBandView(DenseBlasMatrixView<Element> storage, extent_t rows,
                   extent_t lower, extent_t upper)
      : storage_(storage), rows_(rows), lower_(lower), upper_(upper) {}
  DenseBlasMatrixView<Element> storage_;
  extent_t rows_;
  extent_t lower_;
  extent_t upper_;
};

/** @brief Selected-triangle positive-definite/Hermitian band storage.
 *
 * With zero-based indices, column-major upper A(i,j) is stored at
 * data[bandwidth+i-j+j*ld], and lower A(i,j) at data[i-j+j*ld]. Row-major
 * upper A(i,j) is stored at data[i*ld+j-i], and lower A(i,j) at
 * data[i*ld+bandwidth+j-i]. Only selected entries with abs(i-j)<=bandwidth
 * and 0<=i,j<order are mathematical input. Full ld*order backing, including
 * padding and unused corner entries, is required but is not implicitly read.
 * Bandwidth may exceed order; there is no dense expansion. Complex
 * coefficients denote Hermitian storage, never complex symmetry. Descriptors
 * borrow live scalar objects and do not extend storage lifetimes.
 */
template <DenseBlasScalar Element>
class LapackPositiveDefiniteBandView {
 public:
  /** @brief Validates bandwidth+1 storage rows and exact selected-triangle tag.
   * @param data Column-major band table, borrowed for the descriptor lifetime.
   * @param order Nonnegative matrix order.
   * @param bandwidth Nonnegative number of stored off-diagonals.
   * @param triangle Stored upper or lower triangle.
   * @param leading_dimension Column stride, at least bandwidth+1.
   * @param backing CPU backing span covering the complete physical table.
   * @return Descriptor or validation failure without coefficient access.
   */
  static Result<LapackPositiveDefiniteBandView> Create(
      Element* data, extent_t order, extent_t bandwidth,
      DenseBlasTriangle triangle, stride_t leading_dimension,
      ConstMemoryView backing) {
    return Create(data, order, bandwidth, triangle,
                  DenseBlasLayout::kColumnMajor, leading_dimension, backing);
  }
  /** @brief Validates an explicitly laid-out selected-triangle band table.
   * @param data Borrowed live scalar objects; null is allowed for order zero.
   * @param order Nonnegative logical matrix order, independent of layout.
   * @param bandwidth Nonnegative stored off-diagonal count, including >=order.
   * @param triangle Upper or lower Hermitian/symmetric interpretation.
   * @param layout Column-major LAPACK or row-major band encoding above.
   * @param leading_dimension Physical column/row stride, at least bandwidth+1.
   * @param backing CPU span covering every element of the full ld*order table.
   * @return Descriptor or shape/overflow/storage failure without accessing
   * coefficients, allocating on success, transferring or densifying.
   * @note O(1) validation. Concurrent descriptors may share immutable storage;
   * callers must exclude overlapping mutation and retain borrowed lifetimes.
   */
  static Result<LapackPositiveDefiniteBandView> Create(
      Element* data, extent_t order, extent_t bandwidth,
      DenseBlasTriangle triangle, DenseBlasLayout layout,
      stride_t leading_dimension, ConstMemoryView backing) {
    if (order < 0 || bandwidth < 0 ||
        !internal_dense_lapack::IsTriangle(triangle) ||
        !internal_dense_lapack::IsHost(backing.space()) ||
        (layout != DenseBlasLayout::kColumnMajor &&
         layout != DenseBlasLayout::kRowMajor)) {
      return Status(ErrorCode::kInvalidArgument);
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
    return LapackPositiveDefiniteBandView(*storage, bandwidth, triangle);
  }
  /** @brief Returns the nonnegative logical matrix order.
   * @return The nonnegative logical matrix order.
   */
  [[nodiscard]] extent_t order() const noexcept {
    return layout() == DenseBlasLayout::kColumnMajor ? storage_.columns()
                                                     : storage_.rows();
  }
  /** @brief Returns the explicitly selected physical band layout.
   * @return Column-major LAPACK or row-major encoding from the class contract.
   */
  [[nodiscard]] DenseBlasLayout layout() const noexcept {
    return storage_.layout();
  }
  /** @brief Returns the number of stored off-diagonals.
   * @return The number of stored off-diagonals.
   */
  [[nodiscard]] extent_t bandwidth() const noexcept { return bandwidth_; }
  /** @brief Returns the stored triangle.
   * @return The stored triangle.
   */
  [[nodiscard]] DenseBlasTriangle triangle() const noexcept {
    return triangle_;
  }
  /** @brief Returns the diagonal offset within each physical column or row.
   * @return Column-major: bandwidth for upper and zero for lower. Row-major:
   * zero for upper and bandwidth for lower; this is then a column offset.
   */
  [[nodiscard]] extent_t diagonal_row() const noexcept {
    const bool upper = triangle_ == DenseBlasTriangle::kUpper;
    const bool column_major = layout() == DenseBlasLayout::kColumnMajor;
    return upper == column_major ? bandwidth_ : 0;
  }
  /** @brief Returns the borrowed physical table, without interpreting padding.
   * @return Column-major ld-by-order or row-major order-by-ld physical table.
   * This is band storage, not an order-by-order dense mathematical matrix.
   */
  [[nodiscard]] DenseBlasMatrixView<Element> storage() const noexcept {
    return storage_;
  }

 private:
  LapackPositiveDefiniteBandView(DenseBlasMatrixView<Element> storage,
                                 extent_t bandwidth, DenseBlasTriangle triangle)
      : storage_(storage), bandwidth_(bandwidth), triangle_(triangle) {}
  DenseBlasMatrixView<Element> storage_;
  extent_t bandwidth_;
  DenseBlasTriangle triangle_;
};

/** @brief General tridiagonal DL/D/DU with separate disjoint contiguous
 * buffers. */
template <DenseBlasScalar Element>
class LapackTridiagonalView {
 public:
  /** @brief Checks n diagonal and max(n-1,0) off-diagonal entries and no
   * overlap.
   * @param lower Borrowed subdiagonal, increment exactly one.
   * @param diagonal Borrowed diagonal, determining the matrix order.
   * @param upper Borrowed superdiagonal, increment exactly one.
   * @return Descriptor or shape/placement/alias failure; no values are read.
   */
  static Result<LapackTridiagonalView> Create(
      DenseBlasVectorView<Element> lower, DenseBlasVectorView<Element> diagonal,
      DenseBlasVectorView<Element> upper) {
    const extent_t off_diagonal = std::max<extent_t>(0, diagonal.size() - 1);
    if (lower.size() != off_diagonal || upper.size() != off_diagonal ||
        !internal_dense_lapack::Contiguous(lower) ||
        !internal_dense_lapack::Contiguous(diagonal) ||
        !internal_dense_lapack::Contiguous(upper)) {
      return Status(ErrorCode::kShape);
    }
    const std::array<ConstMemoryView, 3> spans{lower.reachable_storage(),
                                               diagonal.reachable_storage(),
                                               upper.reachable_storage()};
    Status status = internal_dense_lapack::Disjoint(spans);
    if (!status.ok()) {
      return status;
    }
    return LapackTridiagonalView(lower, diagonal, upper);
  }
  /** @brief Returns borrowed subdiagonal storage.
   * @return Borrowed subdiagonal storage.
   */
  [[nodiscard]] DenseBlasVectorView<Element> lower() const noexcept {
    return lower_;
  }
  /** @brief Returns borrowed diagonal storage.
   * @return Borrowed diagonal storage.
   */
  [[nodiscard]] DenseBlasVectorView<Element> diagonal() const noexcept {
    return diagonal_;
  }
  /** @brief Returns borrowed superdiagonal storage.
   * @return Borrowed superdiagonal storage.
   */
  [[nodiscard]] DenseBlasVectorView<Element> upper() const noexcept {
    return upper_;
  }
  /** @brief Returns the matrix order.
   * @return The matrix order.
   */
  [[nodiscard]] extent_t order() const noexcept { return diagonal_.size(); }

 private:
  LapackTridiagonalView(DenseBlasVectorView<Element> lower,
                        DenseBlasVectorView<Element> diagonal,
                        DenseBlasVectorView<Element> upper)
      : lower_(lower), diagonal_(diagonal), upper_(upper) {}
  DenseBlasVectorView<Element> lower_;
  DenseBlasVectorView<Element> diagonal_;
  DenseBlasVectorView<Element> upper_;
};

/** @brief GTTRF storage adds a separate second superdiagonal for LU fill-in.
 *
 * This is raw factor storage, not a promise of successful factorization.
 * Reusable solve factors additionally require checked GTTRF pivots and report.
 */
template <DenseBlasScalar Element>
class LapackTridiagonalLuStorage {
 public:
  /** @brief Checks max(n-2,0) second-superdiagonal entries and all four spans.
   * @param primary Checked raw DL/D/DU storage, interpreted as LU factors.
   * @param second_upper Contiguous caller-owned DU2 buffer, separate from
   * DL/D/DU.
   * @return Raw storage descriptor or shape/alias failure; no values are read.
   */
  static Result<LapackTridiagonalLuStorage> Create(
      LapackTridiagonalView<Element> primary,
      DenseBlasVectorView<Element> second_upper) {
    if (second_upper.size() != std::max<extent_t>(0, primary.order() - 2) ||
        !internal_dense_lapack::Contiguous(second_upper)) {
      return Status(ErrorCode::kShape);
    }
    const std::array<ConstMemoryView, 4> spans{
        primary.lower().reachable_storage(),
        primary.diagonal().reachable_storage(),
        primary.upper().reachable_storage(), second_upper.reachable_storage()};
    Status status = internal_dense_lapack::Disjoint(spans);
    if (!status.ok()) {
      return status;
    }
    return LapackTridiagonalLuStorage(primary, second_upper);
  }
  /** @brief Returns raw DL/D/DU factor storage.
   * @return Raw DL/D/DU factor storage.
   */
  [[nodiscard]] LapackTridiagonalView<Element> primary() const noexcept {
    return primary_;
  }
  /** @brief Returns explicit second-superdiagonal fill-in storage.
   * @return Explicit second-superdiagonal fill-in storage.
   */
  [[nodiscard]] DenseBlasVectorView<Element> second_upper() const noexcept {
    return second_upper_;
  }

 private:
  LapackTridiagonalLuStorage(LapackTridiagonalView<Element> primary,
                             DenseBlasVectorView<Element> second_upper)
      : primary_(primary), second_upper_(second_upper) {}
  LapackTridiagonalView<Element> primary_;
  DenseBlasVectorView<Element> second_upper_;
};

/** @brief SPD/Hermitian tridiagonal has a real diagonal even for complex E.
 *
 * The off-diagonal convention is explicitly lower; its conjugate determines
 * the upper off-diagonal. No complex buffer is reinterpreted as real storage.
 */
template <DenseBlasScalar Element>
class LapackPositiveDefiniteTridiagonalView {
 public:
  /** @brief Validates contiguous n real and max(n-1,0) scalar disjoint entries.
   * @param diagonal Borrowed real diagonal with matching component precision.
   * @param lower Borrowed real/complex lower off-diagonal.
   * @return Descriptor or shape/alias/placement failure, without coefficient
   * reads.
   */
  static Result<LapackPositiveDefiniteTridiagonalView> Create(
      DenseBlasVectorView<LapackRealElement<Element>> diagonal,
      DenseBlasVectorView<Element> lower) {
    if (lower.size() != std::max<extent_t>(0, diagonal.size() - 1) ||
        !internal_dense_lapack::Contiguous(diagonal) ||
        !internal_dense_lapack::Contiguous(lower)) {
      return Status(ErrorCode::kShape);
    }
    const std::array<ConstMemoryView, 2> spans{diagonal.reachable_storage(),
                                               lower.reachable_storage()};
    Status status = internal_dense_lapack::Disjoint(spans);
    if (!status.ok()) {
      return status;
    }
    return LapackPositiveDefiniteTridiagonalView(diagonal, lower);
  }
  /** @brief Returns the borrowed real diagonal, retaining constness.
   * @return The borrowed real diagonal, retaining constness.
   */
  [[nodiscard]] DenseBlasVectorView<LapackRealElement<Element>> diagonal()
      const noexcept {
    return diagonal_;
  }
  /** @brief Returns the borrowed lower off-diagonal.
   * @return The borrowed lower off-diagonal.
   */
  [[nodiscard]] DenseBlasVectorView<Element> lower() const noexcept {
    return lower_;
  }
  /** @brief Returns the logical matrix order.
   * @return The logical matrix order.
   */
  [[nodiscard]] extent_t order() const noexcept { return diagonal_.size(); }

 private:
  LapackPositiveDefiniteTridiagonalView(
      DenseBlasVectorView<LapackRealElement<Element>> diagonal,
      DenseBlasVectorView<Element> lower)
      : diagonal_(diagonal), lower_(lower) {}
  DenseBlasVectorView<LapackRealElement<Element>> diagonal_;
  DenseBlasVectorView<Element> lower_;
};

/** @brief Real square bidiagonal represented by its diagonal and one
 * off-diagonal. */
template <DenseBlasReal Real>
class LapackBidiagonalView {
 public:
  /** @brief Checks real contiguous disjoint buffers and upper/lower
   * orientation.
   * @param diagonal Borrowed n-entry real diagonal.
   * @param off_diagonal Borrowed max(n-1,0)-entry real off-diagonal.
   * @param triangle Whether the off-diagonal is above or below the diagonal.
   * @return Descriptor or invalid tag/shape/alias failure; no value access.
   */
  static Result<LapackBidiagonalView> Create(
      DenseBlasVectorView<Real> diagonal,
      DenseBlasVectorView<Real> off_diagonal, DenseBlasTriangle triangle) {
    if (!internal_dense_lapack::IsTriangle(triangle)) {
      return Status(ErrorCode::kInvalidArgument);
    }
    auto storage = LapackPositiveDefiniteTridiagonalView<Real>::Create(
        diagonal, off_diagonal);
    if (!storage.ok()) {
      return storage.status();
    }
    return LapackBidiagonalView(*storage, triangle);
  }
  /** @brief Returns the real diagonal.
   * @return The real diagonal.
   */
  [[nodiscard]] DenseBlasVectorView<Real> diagonal() const noexcept {
    return storage_.diagonal();
  }
  /** @brief Returns the real off-diagonal.
   * @return The real off-diagonal.
   */
  [[nodiscard]] DenseBlasVectorView<Real> off_diagonal() const noexcept {
    return storage_.lower();
  }
  /** @brief Returns whether the single off-diagonal is upper or lower.
   * @return Whether the single off-diagonal is upper or lower.
   */
  [[nodiscard]] DenseBlasTriangle triangle() const noexcept {
    return triangle_;
  }

 private:
  LapackBidiagonalView(LapackPositiveDefiniteTridiagonalView<Real> storage,
                       DenseBlasTriangle triangle)
      : storage_(storage), triangle_(triangle) {}
  LapackPositiveDefiniteTridiagonalView<Real> storage_;
  DenseBlasTriangle triangle_;
};

/** @brief Scalar-dependent RFP orientation, separate from ordinary packed
 * layout. */
enum class LapackRfpOrientation : std::uint8_t {
  kNormal,             ///< Upstream TRANSR=N.
  kTranspose,          ///< Real RFP TRANSR=T; invalid for complex elements.
  kConjugateTranspose  ///< Complex RFP TRANSR=C; invalid for real elements.
};

/** @brief Rectangular-full-packed coefficients with
 * parity/triangle/orientation.
 *
 * Exactly n*(n+1)/2 contiguous entries are borrowed. Normal physical shape is
 * (n+1,n/2) for even n and (n,(n+1)/2) for odd n; transposed orientations
 * exchange those dimensions. Triangle controls the upstream RFP coefficient
 * arrangement. This descriptor never interprets its buffer as ordinary packed
 * storage.
 */
template <DenseBlasScalar Element>
class LapackRfpView {
 public:
  /** @brief Validates order, checked packed capacity and scalar-specific
   * TRANSR.
   * @param storage Contiguous borrowed RFP entries, exact packed entry count.
   * @param order Nonnegative logical square matrix order.
   * @param triangle Selected original matrix triangle.
   * @param orientation Normal, real-transpose or complex-conjugate-transpose.
   * @return Descriptor or shape/overflow/tag failure; no values are accessed.
   */
  static Result<LapackRfpView> Create(DenseBlasVectorView<Element> storage,
                                      extent_t order,
                                      DenseBlasTriangle triangle,
                                      LapackRfpOrientation orientation) {
    if (order < 0 || !internal_dense_lapack::IsTriangle(triangle) ||
        !internal_dense_lapack::Contiguous(storage) ||
        orientation > LapackRfpOrientation::kConjugateTranspose ||
        (DenseBlasReal<Element> &&
         orientation == LapackRfpOrientation::kConjugateTranspose) ||
        (DenseBlasComplex<Element> &&
         orientation == LapackRfpOrientation::kTranspose)) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const auto successor = CheckedAdd<extent_t>(order, 1);
    if (!successor.ok()) {
      return successor.status();
    }
    const bool even = order % 2 == 0;
    const auto entries = CheckedMultiply<extent_t>(
        even ? order / 2 : order, even ? *successor : *successor / 2);
    if (!entries.ok()) {
      return entries.status();
    }
    if (storage.size() != *entries) {
      return Status(ErrorCode::kShape);
    }
    return LapackRfpView(storage, order, triangle, orientation,
                         even ? *successor : order,
                         even ? order / 2 : *successor / 2);
  }
  /** @brief Returns the contiguous borrowed RFP payload.
   * @return The contiguous borrowed RFP payload.
   */
  [[nodiscard]] DenseBlasVectorView<Element> storage() const noexcept {
    return storage_;
  }
  /** @brief Returns logical square order, independent of RFP physical shape.
   * @return Logical square order, independent of RFP physical shape.
   */
  [[nodiscard]] extent_t order() const noexcept { return order_; }
  /** @brief Returns the original matrix's selected triangle.
   * @return The original matrix's selected triangle.
   */
  [[nodiscard]] DenseBlasTriangle triangle() const noexcept {
    return triangle_;
  }
  /** @brief Returns the exact normal/transpose/conjugate-transpose storage tag.
   * @return The exact normal/transpose/conjugate-transpose storage tag.
   */
  [[nodiscard]] LapackRfpOrientation orientation() const noexcept {
    return orientation_;
  }
  /** @brief Returns the number of physical rows in the RFP rectangle.
   * @return The number of physical rows in the RFP rectangle.
   */
  [[nodiscard]] extent_t physical_rows() const noexcept {
    return orientation_ == LapackRfpOrientation::kNormal ? normal_rows_
                                                         : normal_columns_;
  }
  /** @brief Returns the number of physical columns in the RFP rectangle.
   * @return The number of physical columns in the RFP rectangle.
   */
  [[nodiscard]] extent_t physical_columns() const noexcept {
    return orientation_ == LapackRfpOrientation::kNormal ? normal_columns_
                                                         : normal_rows_;
  }

 private:
  LapackRfpView(DenseBlasVectorView<Element> storage, extent_t order,
                DenseBlasTriangle triangle, LapackRfpOrientation orientation,
                extent_t normal_rows, extent_t normal_columns)
      : storage_(storage),
        order_(order),
        triangle_(triangle),
        orientation_(orientation),
        normal_rows_(normal_rows),
        normal_columns_(normal_columns) {}
  DenseBlasVectorView<Element> storage_;
  extent_t order_;
  DenseBlasTriangle triangle_;
  LapackRfpOrientation orientation_;
  extent_t normal_rows_;
  extent_t normal_columns_;
};

}  // namespace asc

#endif  // ASC_DENSE_LAPACK_STRUCTURED_VIEW_H_
