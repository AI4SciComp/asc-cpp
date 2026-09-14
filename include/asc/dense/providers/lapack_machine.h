#ifndef ASC_DENSE_PROVIDERS_LAPACK_MACHINE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_MACHINE_H_

/** @file
 * @brief Checked Reference compatibility operation for machine thresholds.
 *
 * LABAD is a no-op in the pinned provider. It retains the historical interface
 * for adjusting underflow and overflow thresholds on non-IEEE machines. Both
 * scalar representations remain unchanged, including signed zeros, infinities
 * and NaNs. This operation does not compute or validate machine thresholds.
 *
 * Each operand must contain one live scalar with increment one, in host or
 * admitted pinned-host storage. The two operands, provider and report must be
 * mutually disjoint. Metadata overlap rejects without changing any bytes;
 * other structural failures reset the report and preserve both operands.
 * Valid calls enter native LABAD once with private copies. Any native change
 * to either representation returns kProvider with partial/unusable reporting;
 * the caller's operands remain unchanged. There is no INFO, workspace, query,
 * allocation, transfer, synchronization or floating-point arithmetic. Calls
 * may share an immutable provider with separate operands and reports.
 */

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Executes the pinned single-precision LABAD compatibility operation.
 * @param provider Explicit checked serial Reference provider.
 * @param small One live float holding the historical underflow threshold.
 * @param large One live float holding the historical overflow threshold.
 * @param report Records actual native entry, absent INFO and result validity.
 * @return Success for the required unchanged native result, kProvider for a
 * changed native representation, or a structural error.
 * @post Both caller operands retain their exact representations on every
 * return; no numeric ordering or finiteness requirement is imposed.
 */
ASC_DENSE_LAPACK_EXPORT Status Labad(const ReferenceLapackProvider& provider,
                                     DenseBlasVectorView<float> small,
                                     DenseBlasVectorView<float> large,
                                     LapackReport& report);

/** @brief Executes the pinned double-precision LABAD compatibility operation.
 * @param provider Explicit checked serial Reference provider.
 * @param small One live double holding the historical underflow threshold.
 * @param large One live double holding the historical overflow threshold.
 * @param report Records actual native entry, absent INFO and result validity.
 * @return Success for the required unchanged native result, kProvider for a
 * changed native representation, or a structural error.
 * @post Both caller operands retain their exact representations on every
 * return; no numeric ordering or finiteness requirement is imposed.
 */
ASC_DENSE_LAPACK_EXPORT Status Labad(const ReferenceLapackProvider& provider,
                                     DenseBlasVectorView<double> small,
                                     DenseBlasVectorView<double> large,
                                     LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_MACHINE_H_
