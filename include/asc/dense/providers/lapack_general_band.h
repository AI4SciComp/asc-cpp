#ifndef ASC_DENSE_PROVIDERS_LAPACK_GENERAL_BAND_H_
#define ASC_DENSE_PROVIDERS_LAPACK_GENERAL_BAND_H_

/** @file
 * @brief Compact general-band inputs and nominal reference band-LU pivots.
 *
 * Compact AB and expanded LapackLuBandView factor storage are distinct.
 * Compact A(i,j) occupies zero-based AB(ku+i-j,j); expanded factor storage
 * reserves kl extra leading rows and places the original diagonal at kl+ku.
 * Neither descriptor densifies, packs or reinterprets the other convention.
 * All nonnegative LAPACK bandwidths are admitted, including widths exceeding
 * matrix extents. Coefficients outside the logical matrix and physical padding
 * are not numerical input. Backing includes every ld*n physical entry even
 * when m=0. Null storage is permitted when n=0; factories read no coefficients.
 *
 * Factories check metadata, acquire no ownership, allocate no storage and
 * perform no transfer or provider call. All scalar objects and backing storage
 * must remain live for the lifetime of each borrowed view. Operations validate
 * selected-provider accessibility and source-integer arithmetic separately.
 * Raw factor and pivot inputs must originate from the same source-defined
 * general-band LU factorization. These metadata checks cannot authenticate
 * buffer contents, their origin or successful numerical factorization.
 */

#include <cstddef>
#include <span>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc {

/** @brief Borrows canonical column-major compact general-band coefficients.
 * @tparam Element float/double/standard complex, with optional constness.
 */
template <DenseBlasScalar Element>
class ReferenceGeneralBandView {
 public:
  /** @brief Checks complete compact-band CPU backing without reading values.
   * @param data First compact AB element, including source-ignored corners.
   * @param rows Nonnegative logical row count m.
   * @param columns Nonnegative logical column count n.
   * @param lower Nonnegative subdiagonal count kl; may exceed matrix extents.
   * @param upper Nonnegative superdiagonal count ku; may exceed matrix extents.
   * @param leading_dimension Column stride ld, at least kl+ku+1.
   * @param backing Live host/pinned-host span covering all ld*n scalar objects.
   * @return Borrowed descriptor or checked metadata/capacity/overflow failure.
   */
  static Result<ReferenceGeneralBandView> Create(Element* data, extent_t rows,
                                                 extent_t columns,
                                                 extent_t lower, extent_t upper,
                                                 stride_t leading_dimension,
                                                 ConstMemoryView backing) {
    if (rows < 0 || columns < 0 || lower < 0 || upper < 0 ||
        (backing.space() != MemorySpace::kHost &&
         backing.space() != MemorySpace::kPinnedHost)) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const auto bands = CheckedAdd<extent_t>(lower, upper);
    if (!bands.ok()) {
      return bands.status();
    }
    const auto stored = CheckedAdd<extent_t>(*bands, 1);
    if (!stored.ok()) {
      return stored.status();
    }
    if (leading_dimension < *stored) {
      return Status(ErrorCode::kShape);
    }
    auto storage = DenseBlasMatrixView<Element>::Create(
        data, leading_dimension, columns, DenseBlasLayout::kColumnMajor,
        leading_dimension, backing);
    if (!storage.ok()) {
      return storage.status();
    }
    return ReferenceGeneralBandView(*storage, rows, lower, upper);
  }
  /** @brief Returns logical matrix rows m.
   * @return Logical row count; physical storage has ld rows.
   */
  [[nodiscard]] extent_t rows() const noexcept { return rows_; }
  /** @brief Returns logical matrix columns n.
   * @return Number of physical band columns.
   */
  [[nodiscard]] extent_t columns() const noexcept { return storage_.columns(); }
  /** @brief Returns declared lower bandwidth kl.
   * @return Nonnegative subdiagonal count, without clipping to matrix extents.
   */
  [[nodiscard]] extent_t lower_bandwidth() const noexcept { return lower_; }
  /** @brief Returns declared upper bandwidth ku.
   * @return Nonnegative superdiagonal count, without clipping to matrix
   * extents.
   */
  [[nodiscard]] extent_t upper_bandwidth() const noexcept { return upper_; }
  /** @brief Returns the zero-based compact diagonal row.
   * @return ku; expanded band-LU factor storage instead uses kl+ku.
   */
  [[nodiscard]] extent_t diagonal_row() const noexcept { return upper_; }
  /** @brief Returns full borrowed physical storage, including corner/padding.
   * @return Column-major ld-by-n storage; no lifetime is extended.
   */
  [[nodiscard]] DenseBlasMatrixView<Element> storage() const noexcept {
    return storage_;
  }

 private:
  ReferenceGeneralBandView(DenseBlasMatrixView<Element> storage, extent_t rows,
                           extent_t lower, extent_t upper)
      : storage_(storage), rows_(rows), lower_(lower), upper_(upper) {}
  DenseBlasMatrixView<Element> storage_;
  extent_t rows_;
  extent_t lower_;
  extent_t upper_;
};

/** @brief Borrows signed one-based sequential general-band LU swaps.
 *
 * At zero-based step j, p[j] lies in [j+1,min(m,j+kl+1)]. Entries describe
 * interleaved swaps, not a final permutation or a dense GETRF factor tag.
 * Construction checks storage only; consumers validate every value after
 * provider/context admission. The caller supplies matching immutable factors
 * and preserves common provenance/lifetimes. No successful factor is certified.
 */
class ReferenceLuBandPivotView {
 public:
  /** @brief Checks contiguous CPU metadata without reading raw swap values.
   * @param values Borrowed ASC signed index_t entries, increment one.
   * @return Nominal band pivot payload or shape/placement failure.
   */
  static Result<ReferenceLuBandPivotView> Create(
      DenseBlasVectorView<const index_t> values) {
    if (values.increment() != 1) {
      return Status(ErrorCode::kShape);
    }
    if (values.memory_space() != MemorySpace::kHost &&
        values.memory_space() != MemorySpace::kPinnedHost) {
      return Status(ErrorCode::kMemoryAccess);
    }
    return ReferenceLuBandPivotView(values);
  }
  /** @brief Returns the original borrowed contiguous ASC pivot descriptor.
   * @return Immutable signed raw swaps; values are not validated here.
   */
  [[nodiscard]] DenseBlasVectorView<const index_t> storage() const noexcept {
    return values_;
  }
  /** @brief Returns the complete signed raw swap sequence.
   * @return Borrowed span; mutation or expiration invalidates every consumer.
   */
  [[nodiscard]] std::span<const index_t> values() const noexcept {
    return {values_.data(), static_cast<std::size_t>(values_.size())};
  }
  /** @brief Returns the exact bytes checked for context access and aliasing.
   * @return Borrowed CPU memory metadata, without extending lifetime.
   */
  [[nodiscard]] ConstMemoryView reachable_storage() const noexcept {
    return values_.reachable_storage();
  }

 private:
  explicit ReferenceLuBandPivotView(DenseBlasVectorView<const index_t> values)
      : values_(values) {}
  DenseBlasVectorView<const index_t> values_;
};

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_GENERAL_BAND_H_
