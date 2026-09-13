#ifndef ASC_DENSE_PROVIDERS_LAPACK_MATRIX_COPY_H_
#define ASC_DENSE_PROVIDERS_LAPACK_MATRIX_COPY_H_
/** @file
 * @brief Explicit native copy of a full matrix or selected trapezoid.
 *
 * The pinned LACPY routines have no INFO parameter. Reports never manufacture
 * native INFO. Caller output is staged without reading its old numeric values;
 * only selected cells publish. Input packing reads only the selected part.
 * Queries, stale plans and workspace rejection inspect metadata only. Empty
 * dimensions complete locally. Calls allocate, transfer and synchronize
 * nothing. Independent caller buffers/workspace/reports are reentrant.
 */
#include <complex>
#include <cstdint>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
namespace asc {
/** @brief Matrix cells selected by a native copying operation. */
enum class LapackMatrixPart : std::uint8_t {
  kAll,    ///< Every numeric cell, excluding padding.
  kUpper,  ///< Upper triangle/trapezoid including the diagonal: row <= column.
  kLower   ///< Lower triangle/trapezoid including the diagonal: row >= column.
};
/** @brief Plans the explicit SLACPY selected matrix copy.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param part All, upper or lower trapezoid; invalid enums are rejected.
 * @param input Immutable full matrix; unselected numeric cells are not read.
 * @param output Same shape with an independent padded layout; unchanged here.
 * @return Metadata-only plan with live scalar output staging in kScratch and
 * row-input packing in kLayoutConversion, or structural/overflow failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLacpyWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<const float> input, DenseBlasMatrixView<float> output);
/** @brief Executes SLACPY without reading prior destination values.
 * @param provider Same provider identity used to query the plan.
 * @param part Same selected trapezoid used by the query.
 * @param input Immutable matrix, disjoint from output and workspace.
 * @param output Selected cells receive corresponding input values. Unselected
 * cells and padding remain unchanged. Copying performs no numerical scaling,
 * conjugation, finiteness filter or NaN replacement.
 * @param plan Matching scalar, part, dimensions, layouts, strides and provider.
 * @param workspace Explicit simultaneous live scalar packing/staging arrays.
 * @param report Reset before validation; records actual native entry, with
 * absent native INFO on every path because LACPY has no INFO parameter.
 * @return Success after ordinary native return, or preflight error with caller
 * numeric buffers unchanged. There is no native numerical failure channel.
 */
ASC_DENSE_LAPACK_EXPORT Status Lacpy(const ReferenceLapackProvider& provider,
                                     LapackMatrixPart part,
                                     DenseBlasMatrixView<const float> input,
                                     DenseBlasMatrixView<float> output,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans the explicit DLACPY selected matrix copy.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param part All, upper or lower trapezoid; invalid enums are rejected.
 * @param input Immutable full matrix; unselected numeric cells are not read.
 * @param output Same shape with an independent padded layout; unchanged here.
 * @return Metadata-only plan with live scalar output staging in kScratch and
 * row-input packing in kLayoutConversion, or structural/overflow failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLacpyWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<const double> input,
    DenseBlasMatrixView<double> output);
/** @brief Executes DLACPY without reading prior destination values.
 * @param provider Same provider identity used to query the plan.
 * @param part Same selected trapezoid used by the query.
 * @param input Immutable matrix, disjoint from output and workspace.
 * @param output Selected cells receive corresponding input values. Unselected
 * cells and padding remain unchanged. Copying performs no numerical scaling,
 * conjugation, finiteness filter or NaN replacement.
 * @param plan Matching scalar, part, dimensions, layouts, strides and provider.
 * @param workspace Explicit simultaneous live scalar packing/staging arrays.
 * @param report Reset before validation; records actual native entry, with
 * absent native INFO on every path because LACPY has no INFO parameter.
 * @return Success after ordinary native return, or preflight error with caller
 * numeric buffers unchanged. There is no native numerical failure channel.
 */
ASC_DENSE_LAPACK_EXPORT Status Lacpy(const ReferenceLapackProvider& provider,
                                     LapackMatrixPart part,
                                     DenseBlasMatrixView<const double> input,
                                     DenseBlasMatrixView<double> output,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans the explicit CLACPY selected matrix copy.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param part All, upper or lower trapezoid; invalid enums are rejected.
 * @param input Immutable full matrix; unselected numeric cells are not read.
 * @param output Same shape with an independent padded layout; unchanged here.
 * @return Metadata-only plan with live scalar output staging in kScratch and
 * row-input packing in kLayoutConversion, or structural/overflow failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLacpyWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<const std::complex<float>> input,
    DenseBlasMatrixView<std::complex<float>> output);
/** @brief Executes CLACPY without reading prior destination values.
 * @param provider Same provider identity used to query the plan.
 * @param part Same selected trapezoid used by the query.
 * @param input Immutable matrix, disjoint from output and workspace.
 * @param output Selected cells receive corresponding input values. Unselected
 * cells and padding remain unchanged. Copying performs no numerical scaling,
 * conjugation, finiteness filter or NaN replacement.
 * @param plan Matching scalar, part, dimensions, layouts, strides and provider.
 * @param workspace Explicit simultaneous live scalar packing/staging arrays.
 * @param report Reset before validation; records actual native entry, with
 * absent native INFO on every path because LACPY has no INFO parameter.
 * @return Success after ordinary native return, or preflight error with caller
 * numeric buffers unchanged. There is no native numerical failure channel.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lacpy(const ReferenceLapackProvider& provider, LapackMatrixPart part,
      DenseBlasMatrixView<const std::complex<float>> input,
      DenseBlasMatrixView<std::complex<float>> output,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans the explicit ZLACPY selected matrix copy.
 * @param provider Checked host Reference provider with the actual integer ABI.
 * @param part All, upper or lower trapezoid; invalid enums are rejected.
 * @param input Immutable full matrix; unselected numeric cells are not read.
 * @param output Same shape with an independent padded layout; unchanged here.
 * @return Metadata-only plan with live scalar output staging in kScratch and
 * row-input packing in kLayoutConversion, or structural/overflow failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLacpyWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<const std::complex<double>> input,
    DenseBlasMatrixView<std::complex<double>> output);
/** @brief Executes ZLACPY without reading prior destination values.
 * @param provider Same provider identity used to query the plan.
 * @param part Same selected trapezoid used by the query.
 * @param input Immutable matrix, disjoint from output and workspace.
 * @param output Selected cells receive corresponding input values. Unselected
 * cells and padding remain unchanged. Copying performs no numerical scaling,
 * conjugation, finiteness filter or NaN replacement.
 * @param plan Matching scalar, part, dimensions, layouts, strides and provider.
 * @param workspace Explicit simultaneous live scalar packing/staging arrays.
 * @param report Reset before validation; records actual native entry, with
 * absent native INFO on every path because LACPY has no INFO parameter.
 * @return Success after ordinary native return, or preflight error with caller
 * numeric buffers unchanged. There is no native numerical failure channel.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lacpy(const ReferenceLapackProvider& provider, LapackMatrixPart part,
      DenseBlasMatrixView<const std::complex<double>> input,
      DenseBlasMatrixView<std::complex<double>> output,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_MATRIX_COPY_H_
