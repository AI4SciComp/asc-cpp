#ifndef ASC_DENSE_PROVIDERS_LAPACK_SCALAR_SORT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_SCALAR_SORT_H_
/** @file
 * @brief Checked explicit Reference sorting of contiguous real values.
 *
 * LASRT sorts a live unit-contiguous host or admitted pinned-host vector in
 * place. No workspace, query, allocation, transfer or synchronization occurs.
 * Equal values need not retain their original order; signed-zero components
 * are not assigned a total order. Infinities participate in the usual order.
 * NaNs have no promised position: native output stays visible with an accuracy
 * warning, and no sorted-order guarantee is made when a NaN is present.
 *
 * The size is bounded by min(native INTEGER maximum/2, 2^32), covering the
 * pinned midpoint/terminal arithmetic and its documented fixed stack domain.
 * Empty calls complete locally without numeric access or native INFO. Nonempty
 * calls initialize the full actual INFO integer before native entry. Any
 * nonzero INFO is a provider error. INFO=0 output without NaNs is checked for
 * the requested order; an inversion is a provider error. This check does not
 * independently certify that arbitrary provider output preserves the input
 * multiset. The pinned implementation and its exact behavior are explicit.
 *
 * Reachable storage, provider and report objects must be disjoint. Metadata
 * aliasing rejects before report reset; all other preflight errors reset the
 * report and preserve the complete numerical buffer. After entry, native
 * mutation is in place and is not rolled back on provider errors; the report
 * marks that output unusable. Normal INFO=0 publication is complete, including
 * NaN-warning output. Floating-point control settings are unchanged;
 * comparisons may raise ordinary status flags. Concurrent calls use private
 * storage/reports and may share the immutable provider.
 */
#include <cstdint>

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
namespace asc {
/** @brief Selects the native increasing or decreasing numerical order. */
enum class LapackSortOrder : std::uint8_t {
  kIncreasing,  ///< Nondecreasing values, native ID='I'.
  kDecreasing   ///< Nonincreasing values, native ID='D'.
};
/** @brief Sorts a real single-precision vector in place through SLASRT.
 * @param provider Explicit checked serial Reference provider.
 * @param order Requested numerical order; unknown enumerators are rejected.
 * @param values Borrowed live physical vector, with view increment one.
 * @param report Records actual entry, full-width INFO and output validity.
 * @return Success for ordered output, kNumerical for visible NaN-containing
 * output, kProvider for invalid native INFO or an inversion without NaNs,
 * or a preflight shape/access/alias/integer error.
 * @post Preflight errors preserve values. Native mutations remain visible on
 * later errors; see the file contract for validity and lack of stable ordering.
 */
ASC_DENSE_LAPACK_EXPORT Status Lasrt(const ReferenceLapackProvider& provider,
                                     LapackSortOrder order,
                                     DenseBlasVectorView<float> values,
                                     LapackReport& report);
/** @brief Sorts a real double-precision vector in place through DLASRT.
 * @param provider Explicit checked serial Reference provider.
 * @param order Requested numerical order; unknown enumerators are rejected.
 * @param values Borrowed live physical vector, with view increment one.
 * @param report Records actual entry, full-width INFO and output validity.
 * @return Success for ordered output, kNumerical for visible NaN-containing
 * output, kProvider for invalid native INFO or an inversion without NaNs,
 * or a preflight shape/access/alias/integer error.
 * @post Preflight errors preserve values. Native mutations remain visible on
 * later errors; see the file contract for validity and lack of stable ordering.
 */
ASC_DENSE_LAPACK_EXPORT Status Lasrt(const ReferenceLapackProvider& provider,
                                     LapackSortOrder order,
                                     DenseBlasVectorView<double> values,
                                     LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_SCALAR_SORT_H_
