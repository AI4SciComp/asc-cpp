#ifndef ASC_DENSE_PROVIDERS_LAPACK_MACHINE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_MACHINE_H_

/** @file
 * @brief Checked Reference machine parameters and threshold compatibility.
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
 * Valid LABAD calls enter native code once with private copies. Any native
 * change to either representation returns kProvider with partial/unusable
 * reporting; the caller's operands remain unchanged. LABAD has no INFO,
 * workspace, query, allocation, transfer, synchronization or floating-point
 * arithmetic. Calls may share an immutable provider with separate operands and
 * reports.
 *
 * LAMCH returns one selected machine parameter through an explicit native call.
 * Its relative precision is half C++ epsilon on the admitted binary profiles;
 * its rounding flag assumes rounding and does not inspect current rounding
 * controls. Its output has length and increment one and host or admitted
 * pinned-host storage, disjoint from provider/report metadata. Metadata aliases
 * preserve every byte; other structural errors reset the report. Invalid
 * parameter values reject without native entry. The old output is never read.
 * A changed native selector, nonfinite result, or unexpected sign returns
 * kProvider with partial/unusable reporting and preserves the output. Otherwise
 * the scalar publishes successfully with complete validity and absent INFO.
 * These checks do not certify the accuracy of arbitrary finite native values.
 * LAMCH requires no workspace or allocation and does not change floating-point
 * controls; native arithmetic may raise ordinary flags. It is an execution
 * operation, not a metadata-only workspace query.
 */

#include <cstdint>

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Selects one of the ten documented native LAMCH machine parameters. */
enum class LapackMachineParameter : std::uint8_t {
  /** Relative precision E; half C++ epsilon on admitted binary profiles. */
  kRelativePrecision,
  /** Safe minimum S, whose reciprocal does not overflow. */
  kSafeMinimum,
  /** Floating-point radix B. */
  kRadix,
  /** Precision P, the relative precision multiplied by the radix. */
  kPrecision,
  /** Number N of radix digits in the significand. */
  kMantissaDigits,
  /** Flag R: the pinned source assumes rounding and returns one. */
  kRoundingFlag,
  /** Minimum exponent M before gradual underflow. */
  kMinimumExponent,
  /** Underflow threshold U, the minimum positive normal value. */
  kUnderflowThreshold,
  /** Maximum exponent L before overflow. */
  kMaximumExponent,
  /** Overflow threshold O, the largest finite value. */
  kOverflowThreshold
};

/** @brief Obtains one single-precision machine parameter through SLAMCH.
 * @param provider Explicit checked serial Reference provider.
 * @param parameter One of the ten documented parameter selectors.
 * @param value One live float receiving the selected parameter.
 * @param report Records actual entry, absent INFO and publication validity.
 * @return Success for a finite native result of the expected sign, kProvider
 * for a changed selector or invalid result, or a structural error.
 * @post Output publishes only after validation; it is negative for the minimum
 * exponent and positive for every other selector. See the file contract for
 * epsilon, assumed rounding, aliasing and numeric validation limits.
 */
ASC_DENSE_LAPACK_EXPORT Status Lamch(const ReferenceLapackProvider& provider,
                                     LapackMachineParameter parameter,
                                     DenseBlasVectorView<float> value,
                                     LapackReport& report);

/** @brief Obtains one double-precision machine parameter through DLAMCH.
 * @param provider Explicit checked serial Reference provider.
 * @param parameter One of the ten documented parameter selectors.
 * @param value One live double receiving the selected parameter.
 * @param report Records actual entry, absent INFO and publication validity.
 * @return Success for a finite native result of the expected sign, kProvider
 * for a changed selector or invalid result, or a structural error.
 * @post Output publishes only after validation; it is negative for the minimum
 * exponent and positive for every other selector. See the file contract for
 * epsilon, assumed rounding, aliasing and numeric validation limits.
 */
ASC_DENSE_LAPACK_EXPORT Status Lamch(const ReferenceLapackProvider& provider,
                                     LapackMachineParameter parameter,
                                     DenseBlasVectorView<double> value,
                                     LapackReport& report);

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
