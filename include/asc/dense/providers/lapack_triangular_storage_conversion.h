#ifndef ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_STORAGE_CONVERSION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_STORAGE_CONVERSION_H_
/** @file
 * @brief Checked copies between full and packed triangular matrix storage.
 *
 * Dense and packed row/column layouts are independent. Selected values are
 * copied without scaling, conjugation, finite-value filtering or implicit unit
 * diagonals. Complex diagonal imaginary components are copied too. Empty order
 * completes locally without numeric access or native INFO. Explicit live
 * scalar workspace stages output and, for row input, converts its layout.
 * Queries inspect metadata only. Calls allocate, transfer and synchronize
 * nothing. Independent buffers, workspace, plans and reports are reentrant.
 * Metadata overlap rejects before report reset; other preflight errors reset
 * the report and preserve numeric buffers. Nonzero native INFO is a provider
 * defect and staged output is not published. INFO zero does not certify an
 * arbitrary provider's numerical writes or detect omitted writes.
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
/** @brief Plans the STRTTP full to packed copy without numeric access.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable square matrix; only the selected triangle is read.
 * @param output Packed destination of the same order, with independent layout.
 * @return Metadata-only plan binding scalar, triangle, order, layouts, dense
 * stride and provider, or structural/overflow failure. kScratch stages output;
 * kLayoutConversion packs row-major input. All regions hold live scalar arrays.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrttpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> input,
    DenseBlasPackedMatrixView<float> output);
/** @brief Executes the STRTTP full to packed copy using explicit workspace.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable square matrix; only the selected triangle is read.
 * @param output Packed destination of the same order, with independent layout.
 * @param plan Matching metadata-only plan, reusable for independent copies.
 * @param workspace Disjoint live scalar storage meeting the plan capacities.
 * @param report Metadata aliases reject without reset; otherwise reset before
 * validation. Records native entry and the complete actual INFO value.
 * @return Success after local empty completion or native INFO zero. Preflight
 * errors and nonzero INFO preserve caller numeric output; native INFO failures
 * report unusable output. Signed zeros, subnormals, infinities and quiet NaNs
 * are valid copied values and do not imply a numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status Trttp(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasMatrixView<const float> input,
                                     DenseBlasPackedMatrixView<float> output,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans the DTRTTP full to packed copy without numeric access.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable square matrix; only the selected triangle is read.
 * @param output Packed destination of the same order, with independent layout.
 * @return Metadata-only plan binding scalar, triangle, order, layouts, dense
 * stride and provider, or structural/overflow failure. kScratch stages output;
 * kLayoutConversion packs row-major input. All regions hold live scalar arrays.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrttpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> input,
    DenseBlasPackedMatrixView<double> output);
/** @brief Executes the DTRTTP full to packed copy using explicit workspace.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable square matrix; only the selected triangle is read.
 * @param output Packed destination of the same order, with independent layout.
 * @param plan Matching metadata-only plan, reusable for independent copies.
 * @param workspace Disjoint live scalar storage meeting the plan capacities.
 * @param report Metadata aliases reject without reset; otherwise reset before
 * validation. Records native entry and the complete actual INFO value.
 * @return Success after local empty completion or native INFO zero. Preflight
 * errors and nonzero INFO preserve caller numeric output; native INFO failures
 * report unusable output. Signed zeros, subnormals, infinities and quiet NaNs
 * are valid copied values and do not imply a numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status Trttp(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasMatrixView<const double> input,
                                     DenseBlasPackedMatrixView<double> output,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans the CTRTTP full to packed copy without numeric access.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable square matrix; only the selected triangle is read.
 * @param output Packed destination of the same order, with independent layout.
 * @return Metadata-only plan binding scalar, triangle, order, layouts, dense
 * stride and provider, or structural/overflow failure. kScratch stages output;
 * kLayoutConversion packs row-major input. All regions hold live scalar arrays.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrttpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> input,
    DenseBlasPackedMatrixView<std::complex<float>> output);
/** @brief Executes the CTRTTP full to packed copy using explicit workspace.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable square matrix; only the selected triangle is read.
 * @param output Packed destination of the same order, with independent layout.
 * @param plan Matching metadata-only plan, reusable for independent copies.
 * @param workspace Disjoint live scalar storage meeting the plan capacities.
 * @param report Metadata aliases reject without reset; otherwise reset before
 * validation. Records native entry and the complete actual INFO value.
 * @return Success after local empty completion or native INFO zero. Preflight
 * errors and nonzero INFO preserve caller numeric output; native INFO failures
 * report unusable output. Signed zeros, subnormals, infinities and quiet NaNs
 * are valid copied values and do not imply a numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trttp(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<const std::complex<float>> input,
      DenseBlasPackedMatrixView<std::complex<float>> output,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans the ZTRTTP full to packed copy without numeric access.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable square matrix; only the selected triangle is read.
 * @param output Packed destination of the same order, with independent layout.
 * @return Metadata-only plan binding scalar, triangle, order, layouts, dense
 * stride and provider, or structural/overflow failure. kScratch stages output;
 * kLayoutConversion packs row-major input. All regions hold live scalar arrays.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrttpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> input,
    DenseBlasPackedMatrixView<std::complex<double>> output);
/** @brief Executes the ZTRTTP full to packed copy using explicit workspace.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable square matrix; only the selected triangle is read.
 * @param output Packed destination of the same order, with independent layout.
 * @param plan Matching metadata-only plan, reusable for independent copies.
 * @param workspace Disjoint live scalar storage meeting the plan capacities.
 * @param report Metadata aliases reject without reset; otherwise reset before
 * validation. Records native entry and the complete actual INFO value.
 * @return Success after local empty completion or native INFO zero. Preflight
 * errors and nonzero INFO preserve caller numeric output; native INFO failures
 * report unusable output. Signed zeros, subnormals, infinities and quiet NaNs
 * are valid copied values and do not imply a numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trttp(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<const std::complex<double>> input,
      DenseBlasPackedMatrixView<std::complex<double>> output,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans the STPTTR packed to full copy without numeric access.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable packed triangle, with explicit row/column layout.
 * @param output Square destination of the same order; unused triangle and
 * padding stay unchanged.
 * @return Metadata-only plan binding scalar, triangle, order, layouts, dense
 * stride and provider, or structural/overflow failure. kScratch stages output;
 * kLayoutConversion packs row-major input. All regions hold live scalar arrays.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTpttrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> input,
    DenseBlasMatrixView<float> output);
/** @brief Executes the STPTTR packed to full copy using explicit workspace.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable packed triangle, with explicit row/column layout.
 * @param output Square destination of the same order; unused triangle and
 * padding stay unchanged.
 * @param plan Matching metadata-only plan, reusable for independent copies.
 * @param workspace Disjoint live scalar storage meeting the plan capacities.
 * @param report Metadata aliases reject without reset; otherwise reset before
 * validation. Records native entry and the complete actual INFO value.
 * @return Success after local empty completion or native INFO zero. Preflight
 * errors and nonzero INFO preserve caller numeric output; native INFO failures
 * report unusable output. Signed zeros, subnormals, infinities and quiet NaNs
 * are valid copied values and do not imply a numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Tpttr(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const float> input,
      DenseBlasMatrixView<float> output, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans the DTPTTR packed to full copy without numeric access.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable packed triangle, with explicit row/column layout.
 * @param output Square destination of the same order; unused triangle and
 * padding stay unchanged.
 * @return Metadata-only plan binding scalar, triangle, order, layouts, dense
 * stride and provider, or structural/overflow failure. kScratch stages output;
 * kLayoutConversion packs row-major input. All regions hold live scalar arrays.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTpttrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> input,
    DenseBlasMatrixView<double> output);
/** @brief Executes the DTPTTR packed to full copy using explicit workspace.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable packed triangle, with explicit row/column layout.
 * @param output Square destination of the same order; unused triangle and
 * padding stay unchanged.
 * @param plan Matching metadata-only plan, reusable for independent copies.
 * @param workspace Disjoint live scalar storage meeting the plan capacities.
 * @param report Metadata aliases reject without reset; otherwise reset before
 * validation. Records native entry and the complete actual INFO value.
 * @return Success after local empty completion or native INFO zero. Preflight
 * errors and nonzero INFO preserve caller numeric output; native INFO failures
 * report unusable output. Signed zeros, subnormals, infinities and quiet NaNs
 * are valid copied values and do not imply a numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Tpttr(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const double> input,
      DenseBlasMatrixView<double> output, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans the CTPTTR packed to full copy without numeric access.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable packed triangle, with explicit row/column layout.
 * @param output Square destination of the same order; unused triangle and
 * padding stay unchanged.
 * @return Metadata-only plan binding scalar, triangle, order, layouts, dense
 * stride and provider, or structural/overflow failure. kScratch stages output;
 * kLayoutConversion packs row-major input. All regions hold live scalar arrays.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTpttrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> input,
    DenseBlasMatrixView<std::complex<float>> output);
/** @brief Executes the CTPTTR packed to full copy using explicit workspace.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable packed triangle, with explicit row/column layout.
 * @param output Square destination of the same order; unused triangle and
 * padding stay unchanged.
 * @param plan Matching metadata-only plan, reusable for independent copies.
 * @param workspace Disjoint live scalar storage meeting the plan capacities.
 * @param report Metadata aliases reject without reset; otherwise reset before
 * validation. Records native entry and the complete actual INFO value.
 * @return Success after local empty completion or native INFO zero. Preflight
 * errors and nonzero INFO preserve caller numeric output; native INFO failures
 * report unusable output. Signed zeros, subnormals, infinities and quiet NaNs
 * are valid copied values and do not imply a numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Tpttr(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<float>> input,
      DenseBlasMatrixView<std::complex<float>> output,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans the ZTPTTR packed to full copy without numeric access.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable packed triangle, with explicit row/column layout.
 * @param output Square destination of the same order; unused triangle and
 * padding stay unchanged.
 * @return Metadata-only plan binding scalar, triangle, order, layouts, dense
 * stride and provider, or structural/overflow failure. kScratch stages output;
 * kLayoutConversion packs row-major input. All regions hold live scalar arrays.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTpttrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> input,
    DenseBlasMatrixView<std::complex<double>> output);
/** @brief Executes the ZTPTTR packed to full copy using explicit workspace.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param triangle Upper or lower, including every diagonal value.
 * @param input Immutable packed triangle, with explicit row/column layout.
 * @param output Square destination of the same order; unused triangle and
 * padding stay unchanged.
 * @param plan Matching metadata-only plan, reusable for independent copies.
 * @param workspace Disjoint live scalar storage meeting the plan capacities.
 * @param report Metadata aliases reject without reset; otherwise reset before
 * validation. Records native entry and the complete actual INFO value.
 * @return Success after local empty completion or native INFO zero. Preflight
 * errors and nonzero INFO preserve caller numeric output; native INFO failures
 * report unusable output. Signed zeros, subnormals, infinities and quiet NaNs
 * are valid copied values and do not imply a numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Tpttr(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<double>> input,
      DenseBlasMatrixView<std::complex<double>> output,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_STORAGE_CONVERSION_H_
