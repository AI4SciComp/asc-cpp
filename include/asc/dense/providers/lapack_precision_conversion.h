#ifndef ASC_DENSE_PROVIDERS_LAPACK_PRECISION_CONVERSION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_PRECISION_CONVERSION_H_
/** @file
 * @brief Explicit checked native LAG2 precision conversion of full matrices.
 *
 * These optional provider calls preserve the input and all destination padding.
 * Input/output layouts are independent. Queries inspect metadata only. Every
 * destination is staged in caller-owned live output scalars, so no old output
 * numeric value is read and a range failure preserves the caller destination.
 * Row input packing uses separate live input scalars in kLayoutConversion;
 * output staging uses kScratch. Calls allocate, transfer and synchronize
 * nothing. Narrowing follows the pinned provider conversion, including rounding
 * and possible underflow/information loss; it is not a reversible conversion.
 * Native INFO=1 means a component exceeds single precision's finite range.
 * NaN comparisons do not signal range overflow in the pinned algorithm.
 * Nonfinite published values remain visible with an accuracy warning, without
 * clamping. No floating-point environment setting is changed. Empty operations
 * complete locally with absent native INFO. Independent caller state is
 * reentrant.
 */
#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
namespace asc {
/** @brief Plans the explicitly selected SLAG2D matrix conversion.
 * @param provider Checked host Reference provider; exact actual ABI is
 * retained.
 * @param input Immutable full matrix with a supported padded layout.
 * @param output Same shape; independently laid out and unchanged by query.
 * @return Exact simultaneous input-packing/output-staging requirements, or
 * structural failure without a native call or numeric access.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySlag2dWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> input, DenseBlasMatrixView<double> output);
/** @brief Executes SLAG2D with explicit output staging.
 * @param provider Same immutable provider identity used by the query.
 * @param input Immutable input; disjoint from output and workspace.
 * @param output Defined only after valid INFO=0; padding is never modified.
 * @param plan Matching scalar, shape, layout, stride and provider query.
 * @param workspace Explicit live input/output scalars of the queried types.
 * @param report Reset before validation; actual INFO is retained after native
 * return. Only INFO=0 is legal for widening; INFO=1 is a provider defect.
 * @return Success for finite converted values, kNumerical for range failure or
 * visible nonfinite output, kProvider for malformed INFO, or structural error.
 * @post Input is unchanged. Preflight, range and provider failures preserve the
 * entire caller output; scratch mutation is separate. INFO is absent locally.
 */
ASC_DENSE_LAPACK_EXPORT Status Slag2d(const ReferenceLapackProvider& provider,
                                      DenseBlasMatrixView<const float> input,
                                      DenseBlasMatrixView<double> output,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);
/** @brief Plans the explicitly selected DLAG2S matrix conversion.
 * @param provider Checked host Reference provider; exact actual ABI is
 * retained.
 * @param input Immutable full matrix with a supported padded layout.
 * @param output Same shape; independently laid out and unchanged by query.
 * @return Exact simultaneous input-packing/output-staging requirements, or
 * structural failure without a native call or numeric access.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryDlag2sWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> input, DenseBlasMatrixView<float> output);
/** @brief Executes DLAG2S with explicit output staging.
 * @param provider Same immutable provider identity used by the query.
 * @param input Immutable input; disjoint from output and workspace.
 * @param output Defined only after valid INFO=0; padding is never modified.
 * @param plan Matching scalar, shape, layout, stride and provider query.
 * @param workspace Explicit live input/output scalars of the queried types.
 * @param report Reset before validation; actual INFO is retained after native
 * return. INFO=1 reports kNumerical/kAccuracyWarning with unchanged output.
 * @return Success for finite converted values, kNumerical for range failure or
 * visible nonfinite output, kProvider for malformed INFO, or structural error.
 * @post Input is unchanged. Preflight, range and provider failures preserve the
 * entire caller output; scratch mutation is separate. INFO is absent locally.
 */
ASC_DENSE_LAPACK_EXPORT Status Dlag2s(const ReferenceLapackProvider& provider,
                                      DenseBlasMatrixView<const double> input,
                                      DenseBlasMatrixView<float> output,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);
/** @brief Plans the explicitly selected CLAG2Z matrix conversion.
 * @param provider Checked host Reference provider; exact actual ABI is
 * retained.
 * @param input Immutable full matrix with a supported padded layout.
 * @param output Same shape; independently laid out and unchanged by query.
 * @return Exact simultaneous input-packing/output-staging requirements, or
 * structural failure without a native call or numeric access.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryClag2zWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> input,
    DenseBlasMatrixView<std::complex<double>> output);
/** @brief Executes CLAG2Z with explicit output staging.
 * @param provider Same immutable provider identity used by the query.
 * @param input Immutable input; disjoint from output and workspace.
 * @param output Defined only after valid INFO=0; padding is never modified.
 * @param plan Matching scalar, shape, layout, stride and provider query.
 * @param workspace Explicit live input/output scalars of the queried types.
 * @param report Reset before validation; actual INFO is retained after native
 * return. Only INFO=0 is legal for widening; INFO=1 is a provider defect.
 * @return Success for finite converted values, kNumerical for range failure or
 * visible nonfinite output, kProvider for malformed INFO, or structural error.
 * @post Input is unchanged. Preflight, range and provider failures preserve the
 * entire caller output; scratch mutation is separate. INFO is absent locally.
 */
ASC_DENSE_LAPACK_EXPORT Status
Clag2z(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<const std::complex<float>> input,
       DenseBlasMatrixView<std::complex<double>> output,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Plans the explicitly selected ZLAG2C matrix conversion.
 * @param provider Checked host Reference provider; exact actual ABI is
 * retained.
 * @param input Immutable full matrix with a supported padded layout.
 * @param output Same shape; independently laid out and unchanged by query.
 * @return Exact simultaneous input-packing/output-staging requirements, or
 * structural failure without a native call or numeric access.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryZlag2cWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> input,
    DenseBlasMatrixView<std::complex<float>> output);
/** @brief Executes ZLAG2C with explicit output staging.
 * @param provider Same immutable provider identity used by the query.
 * @param input Immutable input; disjoint from output and workspace.
 * @param output Defined only after valid INFO=0; padding is never modified.
 * @param plan Matching scalar, shape, layout, stride and provider query.
 * @param workspace Explicit live input/output scalars of the queried types.
 * @param report Reset before validation; actual INFO is retained after native
 * return. INFO=1 reports kNumerical/kAccuracyWarning with unchanged output.
 * @return Success for finite converted values, kNumerical for range failure or
 * visible nonfinite output, kProvider for malformed INFO, or structural error.
 * @post Input is unchanged. Preflight, range and provider failures preserve the
 * entire caller output; scratch mutation is separate. INFO is absent locally.
 */
ASC_DENSE_LAPACK_EXPORT Status
Zlag2c(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<const std::complex<double>> input,
       DenseBlasMatrixView<std::complex<float>> output,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_PRECISION_CONVERSION_H_
