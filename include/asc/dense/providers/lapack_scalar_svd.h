#ifndef ASC_DENSE_PROVIDERS_LAPACK_SCALAR_SVD_H_
#define ASC_DENSE_PROVIDERS_LAPACK_SCALAR_SVD_H_

/** @file
 * @brief Checked real singular decomposition of a 2-by-2 triangular matrix.
 *
 * LASV2 diagonalizes [f g; 0 h] as
 * [cl sl; -sl cl] * [f g; 0 h] * [cr -sr; sr cr] = [large 0; 0 small].
 * The signed diagonal entries have abs(small)<=abs(large). The two pairs
 * (cl,sl) and (cr,sr) are approximately unit vectors; native rounding may put
 * an individual coefficient slightly outside [-1,1]. Values are not clipped.
 *
 * Inputs are copied by value and may be finite or contain exactly one infinity.
 * Any NaN or multiple infinities return kNumerical with an accuracy warning,
 * unchanged output validity and no native entry. Six distinct live scalar
 * outputs each require length and increment one and host or admitted
 * pinned-host storage. All outputs and provider/report metadata must be
 * mutually disjoint. Metadata overlap rejects without changing any bytes; other
 * structural errors reset the report and preserve all outputs. Old output
 * values are never read.
 *
 * Native calls use private input copies and six staged outputs. Changed inputs,
 * a nonfinite small value, NaN large value, unsorted magnitudes, nonfinite
 * coefficients, or coefficient magnitude above 1+16*epsilon are provider
 * defects. Epsilon is the scalar type's C++ machine epsilon. Defects return
 * kProvider, partial/unusable and no publication. Otherwise all six outputs
 * publish together: finite large succeeds; either infinite large returns
 * kNumerical with an accuracy warning and complete publication validity.
 * These checks do not certify arbitrary finite native tuples. The provider
 * permits overflow when the largest singular value is near its finite limit.
 *
 * There is no native INFO, workspace, query, allocation, transfer or
 * synchronization. Floating-point controls are unchanged; arithmetic may raise
 * ordinary flags. Independent calls may share an immutable provider and keep
 * mutable outputs and reports private. No alternative SVD algorithm is used.
 */

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Diagonalizes a real triangular 2-by-2 matrix through SLASV2.
 * @param provider Explicit checked serial Reference provider.
 * @param f The first diagonal entry, copied before native execution.
 * @param g The upper off-diagonal entry, copied before native execution.
 * @param h The second diagonal entry, copied before native execution.
 * @param small One live float receiving the signed smaller singular value.
 * @param large One live float receiving the signed larger singular value.
 * @param right_sine One live float receiving sr of the right rotation.
 * @param right_cosine One live float receiving cr of the right rotation.
 * @param left_sine One live float receiving sl of the left rotation.
 * @param left_cosine One live float receiving cl of the left rotation.
 * @param report Records native entry, absent INFO and publication validity.
 * @return Success for finite valid output, kNumerical for rejected input or
 * published infinite large, kProvider for invalid native output, or a
 * structural error. See the file contract for input and coefficient limits.
 * @post Six outputs publish together only after native validation. Input values
 * may originate in output objects because they are copied before execution.
 */
ASC_DENSE_LAPACK_EXPORT Status Lasv2(const ReferenceLapackProvider& provider,
                                     float f, float g, float h,
                                     DenseBlasVectorView<float> small,
                                     DenseBlasVectorView<float> large,
                                     DenseBlasVectorView<float> right_sine,
                                     DenseBlasVectorView<float> right_cosine,
                                     DenseBlasVectorView<float> left_sine,
                                     DenseBlasVectorView<float> left_cosine,
                                     LapackReport& report);

/** @brief Diagonalizes a real triangular 2-by-2 matrix through DLASV2.
 * @param provider Explicit checked serial Reference provider.
 * @param f The first diagonal entry, copied before native execution.
 * @param g The upper off-diagonal entry, copied before native execution.
 * @param h The second diagonal entry, copied before native execution.
 * @param small One live double receiving the signed smaller singular value.
 * @param large One live double receiving the signed larger singular value.
 * @param right_sine One live double receiving sr of the right rotation.
 * @param right_cosine One live double receiving cr of the right rotation.
 * @param left_sine One live double receiving sl of the left rotation.
 * @param left_cosine One live double receiving cl of the left rotation.
 * @param report Records native entry, absent INFO and publication validity.
 * @return Success for finite valid output, kNumerical for rejected input or
 * published infinite large, kProvider for invalid native output, or a
 * structural error. See the file contract for input and coefficient limits.
 * @post Six outputs publish together only after native validation. Input values
 * may originate in output objects because they are copied before execution.
 */
ASC_DENSE_LAPACK_EXPORT Status Lasv2(const ReferenceLapackProvider& provider,
                                     double f, double g, double h,
                                     DenseBlasVectorView<double> small,
                                     DenseBlasVectorView<double> large,
                                     DenseBlasVectorView<double> right_sine,
                                     DenseBlasVectorView<double> right_cosine,
                                     DenseBlasVectorView<double> left_sine,
                                     DenseBlasVectorView<double> left_cosine,
                                     LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_SCALAR_SVD_H_
