#ifndef ASC_DENSE_PROVIDERS_LAPACK_SCALAR_NORM_H_
#define ASC_DENSE_PROVIDERS_LAPACK_SCALAR_NORM_H_
/** @file
 * @brief Explicit checked Reference norms of two or three real scalar values.
 *
 * LAPY2/LAPY3 evaluate the Euclidean norm with the pinned provider's scaled
 * arithmetic. Inputs are passed by value. The output is one live host or
 * admitted pinned-host scalar, with length and increment both one. No workspace
 * or query is needed. Calls allocate, transfer and synchronize nothing; they do
 * not change floating-point control settings; ordinary status flags may be
 * raised by arithmetic. Even all-zero inputs call the
 * provider. There is no native INFO argument, so reports always leave it
 * absent.
 *
 * A finite nonnegative native result is published successfully. A NaN or
 * positive infinity remains visible with a numerical accuracy warning and
 * complete publication validity. A negative result is a provider defect and
 * preserves the caller output. Completion is not a claim of correctly rounded
 * arithmetic or finite output for an unrepresentable norm. NaN-input behavior
 * follows the pinned routine, including NaN in combination with infinity.
 *
 * Output, provider and report storage must be disjoint. Unsafe metadata
 * aliasing rejects without modifying the report; other structural errors reset
 * it and preserve output. No old output value is read. Independent calls share
 * only immutable provider state and keep mutable output/report storage
 * disjoint.
 */
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
namespace asc {
/** @brief Evaluates sqrt(x*x+y*y) through native SLAPY2 scaled arithmetic.
 * @param provider Explicit checked serial Reference provider.
 * @param x First single-precision component, copied before native entry.
 * @param y Second single-precision component, copied before native entry.
 * @param result One live float, length/increment one; borrowed for the call.
 * @param report Records actual provider entry, absent INFO and output validity.
 * @return Success for a finite nonnegative result; kNumerical for published
 * nonfinite output, kProvider for a negative result, or a structural error.
 * @post Structural/provider errors preserve result. Native nonfinite values are
 * published without clipping or a fallback algorithm; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Lapy2(const ReferenceLapackProvider& provider,
                                     float x, float y,
                                     DenseBlasVectorView<float> result,
                                     LapackReport& report);
/** @brief Evaluates sqrt(x*x+y*y) through native DLAPY2 scaled arithmetic.
 * @param provider Explicit checked serial Reference provider.
 * @param x First double-precision component, copied before native entry.
 * @param y Second double-precision component, copied before native entry.
 * @param result One live double, length/increment one; borrowed for the call.
 * @param report Records actual provider entry, absent INFO and output validity.
 * @return Success for a finite nonnegative result; kNumerical for published
 * nonfinite output, kProvider for a negative result, or a structural error.
 * @post Structural/provider errors preserve result. Native nonfinite values are
 * published without clipping or a fallback algorithm; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Lapy2(const ReferenceLapackProvider& provider,
                                     double x, double y,
                                     DenseBlasVectorView<double> result,
                                     LapackReport& report);
/** @brief Evaluates sqrt(x*x+y*y+z*z) through native SLAPY3 scaled arithmetic.
 * @param provider Explicit checked serial Reference provider.
 * @param x First single-precision component, copied before native entry.
 * @param y Second single-precision component, copied before native entry.
 * @param z Third single-precision component, copied before native entry.
 * @param result One live float, length/increment one; borrowed for the call.
 * @param report Records actual provider entry, absent INFO and output validity.
 * @return Success for a finite nonnegative result; kNumerical for published
 * nonfinite output, kProvider for a negative result, or a structural error.
 * @post Structural/provider errors preserve result. Native nonfinite values are
 * published without clipping or a fallback algorithm; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Lapy3(const ReferenceLapackProvider& provider,
                                     float x, float y, float z,
                                     DenseBlasVectorView<float> result,
                                     LapackReport& report);
/** @brief Evaluates sqrt(x*x+y*y+z*z) through native DLAPY3 scaled arithmetic.
 * @param provider Explicit checked serial Reference provider.
 * @param x First double-precision component, copied before native entry.
 * @param y Second double-precision component, copied before native entry.
 * @param z Third double-precision component, copied before native entry.
 * @param result One live double, length/increment one; borrowed for the call.
 * @param report Records actual provider entry, absent INFO and output validity.
 * @return Success for a finite nonnegative result; kNumerical for published
 * nonfinite output, kProvider for a negative result, or a structural error.
 * @post Structural/provider errors preserve result. Native nonfinite values are
 * published without clipping or a fallback algorithm; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Lapy3(const ReferenceLapackProvider& provider,
                                     double x, double y, double z,
                                     DenseBlasVectorView<double> result,
                                     LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_SCALAR_NORM_H_
