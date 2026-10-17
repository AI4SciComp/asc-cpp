#ifndef ASC_DENSE_PROVIDERS_LAPACK_MATRIX_SET_H_
#define ASC_DENSE_PROVIDERS_LAPACK_MATRIX_SET_H_
/** @file
 * @brief Explicit native initialization of a full matrix or selected trapezoid.
 *
 * LASET has no INFO parameter; reports never manufacture native INFO. Prior
 * output values are not numeric inputs. Column-major output is written
 * directly; row-major output uses explicit selected-cell staging. Unselected
 * cells and padding remain unchanged. Queries and rejected calls inspect
 * metadata only. Empty dimensions complete locally. Calls allocate, transfer
 * and synchronize nothing. Independent buffers, workspace, reports and contexts
 * are reentrant; immutable plans may be reused with different alpha and beta
 * values.
 */
#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
#include "asc/dense/providers/lapack_matrix_copy.h"
namespace asc {
/** @brief Plans the explicit SLASET selected matrix initialization.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param part All, upper or lower trapezoid; invalid enums are rejected.
 * @param output Full matrix descriptor; numeric storage is unchanged here.
 * @return Metadata-only plan: no scratch for column-major or empty output;
 * M*N live scalar entries in kLayoutConversion for active row-major output.
 * Foreign integer/loop-terminal and storage-count overflow are rejected before
 * entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasetWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<float> output);
/** @brief Executes SLASET without reading prior output values.
 * @param provider Same provider identity used to query the plan.
 * @param part Same all/upper/lower selection used by the query.
 * @param alpha Value assigned to selected off-diagonal cells; passed by value.
 * @param beta Value assigned to diagonal cells; passed by value. Complex
 * imaginary diagonal components are retained exactly as supplied.
 * @param output Selected cells receive alpha or beta; unselected cells and
 * padding remain unchanged. No scaling, conjugation or finiteness filter is
 * applied. Scalar values are independent of the plan and may change on reuse.
 * @param plan Matching scalar, part, dimensions, stride, layout and provider.
 * @param workspace Explicit live scalar staging array, disjoint from output.
 * @param report Reset before validation; records actual native entry and absent
 * native INFO on every path. Ordinary native return completes publication.
 * @return Success after local empty completion or native return; preflight
 * failures leave caller numeric buffers unchanged. There is no native numerical
 * failure channel, so completion does not certify finiteness of scalar values.
 */
ASC_DENSE_LAPACK_EXPORT Status Laset(const ReferenceLapackProvider& provider,
                                     LapackMatrixPart part, float alpha,
                                     float beta,
                                     DenseBlasMatrixView<float> output,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans the explicit DLASET selected matrix initialization.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param part All, upper or lower trapezoid; invalid enums are rejected.
 * @param output Full matrix descriptor; numeric storage is unchanged here.
 * @return Metadata-only plan: no scratch for column-major or empty output;
 * M*N live scalar entries in kLayoutConversion for active row-major output.
 * Foreign integer/loop-terminal and storage-count overflow are rejected before
 * entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasetWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<double> output);
/** @brief Executes DLASET without reading prior output values.
 * @param provider Same provider identity used to query the plan.
 * @param part Same all/upper/lower selection used by the query.
 * @param alpha Value assigned to selected off-diagonal cells; passed by value.
 * @param beta Value assigned to diagonal cells; passed by value. Complex
 * imaginary diagonal components are retained exactly as supplied.
 * @param output Selected cells receive alpha or beta; unselected cells and
 * padding remain unchanged. No scaling, conjugation or finiteness filter is
 * applied. Scalar values are independent of the plan and may change on reuse.
 * @param plan Matching scalar, part, dimensions, stride, layout and provider.
 * @param workspace Explicit live scalar staging array, disjoint from output.
 * @param report Reset before validation; records actual native entry and absent
 * native INFO on every path. Ordinary native return completes publication.
 * @return Success after local empty completion or native return; preflight
 * failures leave caller numeric buffers unchanged. There is no native numerical
 * failure channel, so completion does not certify finiteness of scalar values.
 */
ASC_DENSE_LAPACK_EXPORT Status Laset(const ReferenceLapackProvider& provider,
                                     LapackMatrixPart part, double alpha,
                                     double beta,
                                     DenseBlasMatrixView<double> output,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans the explicit CLASET selected matrix initialization.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param part All, upper or lower trapezoid; invalid enums are rejected.
 * @param output Full matrix descriptor; numeric storage is unchanged here.
 * @return Metadata-only plan: no scratch for column-major or empty output;
 * M*N live scalar entries in kLayoutConversion for active row-major output.
 * Foreign integer/loop-terminal and storage-count overflow are rejected before
 * entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasetWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<std::complex<float>> output);
/** @brief Executes CLASET without reading prior output values.
 * @param provider Same provider identity used to query the plan.
 * @param part Same all/upper/lower selection used by the query.
 * @param alpha Value assigned to selected off-diagonal cells; passed by value.
 * @param beta Value assigned to diagonal cells; passed by value. Complex
 * imaginary diagonal components are retained exactly as supplied.
 * @param output Selected cells receive alpha or beta; unselected cells and
 * padding remain unchanged. No scaling, conjugation or finiteness filter is
 * applied. Scalar values are independent of the plan and may change on reuse.
 * @param plan Matching scalar, part, dimensions, stride, layout and provider.
 * @param workspace Explicit live scalar staging array, disjoint from output.
 * @param report Reset before validation; records actual native entry and absent
 * native INFO on every path. Ordinary native return completes publication.
 * @return Success after local empty completion or native return; preflight
 * failures leave caller numeric buffers unchanged. There is no native numerical
 * failure channel, so completion does not certify finiteness of scalar values.
 */
ASC_DENSE_LAPACK_EXPORT Status
Laset(const ReferenceLapackProvider& provider, LapackMatrixPart part,
      std::complex<float> alpha, std::complex<float> beta,
      DenseBlasMatrixView<std::complex<float>> output,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans the explicit ZLASET selected matrix initialization.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param part All, upper or lower trapezoid; invalid enums are rejected.
 * @param output Full matrix descriptor; numeric storage is unchanged here.
 * @return Metadata-only plan: no scratch for column-major or empty output;
 * M*N live scalar entries in kLayoutConversion for active row-major output.
 * Foreign integer/loop-terminal and storage-count overflow are rejected before
 * entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasetWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<std::complex<double>> output);
/** @brief Executes ZLASET without reading prior output values.
 * @param provider Same provider identity used to query the plan.
 * @param part Same all/upper/lower selection used by the query.
 * @param alpha Value assigned to selected off-diagonal cells; passed by value.
 * @param beta Value assigned to diagonal cells; passed by value. Complex
 * imaginary diagonal components are retained exactly as supplied.
 * @param output Selected cells receive alpha or beta; unselected cells and
 * padding remain unchanged. No scaling, conjugation or finiteness filter is
 * applied. Scalar values are independent of the plan and may change on reuse.
 * @param plan Matching scalar, part, dimensions, stride, layout and provider.
 * @param workspace Explicit live scalar staging array, disjoint from output.
 * @param report Reset before validation; records actual native entry and absent
 * native INFO on every path. Ordinary native return completes publication.
 * @return Success after local empty completion or native return; preflight
 * failures leave caller numeric buffers unchanged. There is no native numerical
 * failure channel, so completion does not certify finiteness of scalar values.
 */
ASC_DENSE_LAPACK_EXPORT Status
Laset(const ReferenceLapackProvider& provider, LapackMatrixPart part,
      std::complex<double> alpha, std::complex<double> beta,
      DenseBlasMatrixView<std::complex<double>> output,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_MATRIX_SET_H_
