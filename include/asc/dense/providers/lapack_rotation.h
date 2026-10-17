#ifndef ASC_DENSE_PROVIDERS_LAPACK_ROTATION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_ROTATION_H_

/** @file
 * @brief Checked Reference real plane rotations with explicit scalar outputs.
 *
 * For finite f and g, LARTG computes [c s; -s c] * [f; g] = [r; 0],
 * with c nonnegative and c*c+s*s approximately one. If g is zero, the result
 * is c=1, s=0, r=f; otherwise if f is zero, c=0, s=sign(g), r=abs(g).
 * The sign of r otherwise follows f. LARTGP instead chooses r nonnegative,
 * with signed c and s; if g is zero it returns c=sign(f), s=0, r=abs(f).
 * Negative-zero f and zero g give LARTGP c=-1 and r=+0.
 * Inputs are copied by value; outputs are
 * three distinct live host or admitted pinned-host scalars, each with length
 * and increment one. No previous output value is read.
 *
 * No workspace or query is needed. Calls allocate, transfer and synchronize
 * nothing and do not change floating-point controls. Ordinary arithmetic flags
 * may be raised. Even zero inputs enter the provider. There is no native INFO.
 * Nonfinite inputs return kNumerical without entry or output mutation. Finite
 * coefficients and a finite r are published successfully. An infinite r is
 * published with kNumerical, an accuracy warning and complete output validity;
 * finite inputs do not guarantee a representable r or correctly rounded output.
 * Invalid native coefficients, NaN r or changed native input copies are
 * provider defects: no output is published, and the report records
 * partial/unusable. These checks do not certify the accuracy of arbitrary
 * finite native writes. LARTGP also rejects negative r, including negative
 * infinity.
 *
 * Provider and report storage must be disjoint from each other and all outputs.
 * Metadata overlap rejects without changing any bytes. Other structural errors
 * reset the report and preserve outputs. Independent calls may share an
 * immutable provider and must keep mutable outputs and reports disjoint.
 */

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Generates a real single-precision rotation through native SLARTG.
 * @param provider Explicit checked serial Reference provider.
 * @param f Finite first component, copied before execution.
 * @param g Finite second component, copied before execution.
 * @param cosine One live float receiving the nonnegative cosine.
 * @param sine One live float receiving the signed sine.
 * @param rotated One live float receiving the first rotated component r.
 * @param report Records actual entry, absent INFO and publication validity.
 * @return Success for finite output, kNumerical for nonfinite input or
 * published infinite r, kProvider for invalid native output, or a structural
 * error.
 * @post The three outputs publish together only after native validation; see
 * the file contract for zero inputs, signs, aliasing and warning semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Lartg(const ReferenceLapackProvider& provider,
                                     float f, float g,
                                     DenseBlasVectorView<float> cosine,
                                     DenseBlasVectorView<float> sine,
                                     DenseBlasVectorView<float> rotated,
                                     LapackReport& report);

/** @brief Generates a real double-precision rotation through native DLARTG.
 * @param provider Explicit checked serial Reference provider.
 * @param f Finite first component, copied before execution.
 * @param g Finite second component, copied before execution.
 * @param cosine One live double receiving the nonnegative cosine.
 * @param sine One live double receiving the signed sine.
 * @param rotated One live double receiving the first rotated component r.
 * @param report Records actual entry, absent INFO and publication validity.
 * @return Success for finite output, kNumerical for nonfinite input or
 * published infinite r, kProvider for invalid native output, or a structural
 * error.
 * @post The three outputs publish together only after native validation; see
 * the file contract for zero inputs, signs, aliasing and warning semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Lartg(const ReferenceLapackProvider& provider,
                                     double f, double g,
                                     DenseBlasVectorView<double> cosine,
                                     DenseBlasVectorView<double> sine,
                                     DenseBlasVectorView<double> rotated,
                                     LapackReport& report);

/** @brief Generates a SLARTGP rotation with nonnegative rotated component.
 * @param provider Explicit checked serial Reference provider.
 * @param f Finite first component, copied before execution.
 * @param g Finite second component, copied before execution.
 * @param cosine One live float receiving the signed cosine in [-1,1].
 * @param sine One live float receiving the signed sine in [-1,1].
 * @param rotated One live float receiving the nonnegative component r.
 * @param report Records actual entry, absent INFO and publication validity.
 * @return Success for finite output, kNumerical for nonfinite input or
 * published positive infinity, kProvider for invalid native output, or a
 * structural error.
 * @post Outputs publish together after native validation. Zero g produces
 * cosine=sign(f), sine=0 and rotated=abs(f); see the shared file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Lartgp(const ReferenceLapackProvider& provider,
                                      float f, float g,
                                      DenseBlasVectorView<float> cosine,
                                      DenseBlasVectorView<float> sine,
                                      DenseBlasVectorView<float> rotated,
                                      LapackReport& report);

/** @brief Generates a DLARTGP rotation with nonnegative rotated component.
 * @param provider Explicit checked serial Reference provider.
 * @param f Finite first component, copied before execution.
 * @param g Finite second component, copied before execution.
 * @param cosine One live double receiving the signed cosine in [-1,1].
 * @param sine One live double receiving the signed sine in [-1,1].
 * @param rotated One live double receiving the nonnegative component r.
 * @param report Records actual entry, absent INFO and publication validity.
 * @return Success for finite output, kNumerical for nonfinite input or
 * published positive infinity, kProvider for invalid native output, or a
 * structural error.
 * @post Outputs publish together after native validation. Zero g produces
 * cosine=sign(f), sine=0 and rotated=abs(f); see the shared file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Lartgp(const ReferenceLapackProvider& provider,
                                      double f, double g,
                                      DenseBlasVectorView<double> cosine,
                                      DenseBlasVectorView<double> sine,
                                      DenseBlasVectorView<double> rotated,
                                      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_ROTATION_H_
