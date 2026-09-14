#ifndef ASC_DENSE_PROVIDERS_LAPACK_MATRIX_SUPPORT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_MATRIX_SUPPORT_H_
/** @file
 * @brief Checked last nonzero row and column extents of a full matrix.
 *
 * Ilalc/Ilalr return the one-based last nonzero column/row, equivalently the
 * leading extent containing every nonzero entry. Zero means all-zero or empty.
 * Exact native equality treats either signed zero as zero and NaN/infinity
 * as nonzero. Complex entries are zero only when both components are zero.
 * Padding is excluded. No finiteness filter, conjugation or fallback is used.
 *
 * Workspace queries inspect metadata only. Column-major execution needs no
 * packing; row-major execution uses explicit live scalar packing storage.
 * Every empty shape completes locally with result zero and no native entry.
 * Metadata overlap rejects before report reset or any numeric access. Other
 * preflight failures reset the report and preserve the result and input.
 * Native integer arguments must remain unchanged and the returned extent must
 * lie in [0, columns] or [0, rows]. Otherwise return kProvider,
 * partial/unusable, and withhold the result. In-range results are not an
 * accuracy certificate.
 *
 * There is no native INFO or native workspace query. Native INFO, argument,
 * diagnostic index and factor metadata remain absent. Calls allocate, transfer
 * and synchronize nothing. Floating-point controls remain unchanged;
 * comparisons may raise ordinary flags. Independent calls keep mutable state
 * private.
 */
#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
namespace asc {
/** @brief Plans ILASLC for a full matrix.
 * @param provider Explicit checked serial Reference provider.
 * @param matrix Read-only matrix with a supported padded row/column layout.
 * @return Metadata-only identity and row-major packing requirement, or a
 * structural, placement or integer/span overflow error. No values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryIlalcWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix);
/** @brief Finds the last nonzero column through ILASLC.
 * @param provider Same provider identity as the workspace query.
 * @param matrix Read-only input, excluding padding, disjoint from mutable
 * state.
 * @param extent Receives the leading column extent, zero if empty/all-zero;
 * one-based otherwise. A live host index_t object; its old value is not read.
 * @param plan Matching routine, scalar, dimensions, layout, stride and
 * provider.
 * @param workspace Disjoint live packing scalars required by the plan.
 * @param report Metadata aliases preserve all bytes; other paths reset before
 * validation. Records actual native entry, publication validity and absent
 * INFO.
 * @return Success after bounded native result publication or local empty
 * result; kProvider for changed native integers/out-of-range result, or
 * preflight error.
 * @post Input and padding remain unchanged; result publishes only on success.
 */
ASC_DENSE_LAPACK_EXPORT Status Ilalc(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<const float> matrix,
                                     index_t& extent,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans ILADLC for a full matrix.
 * @param provider Explicit checked serial Reference provider.
 * @param matrix Read-only matrix with a supported padded row/column layout.
 * @return Metadata-only identity and row-major packing requirement, or a
 * structural, placement or integer/span overflow error. No values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryIlalcWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix);
/** @brief Finds the last nonzero column through ILADLC.
 * @param provider Same provider identity as the workspace query.
 * @param matrix Read-only input, excluding padding, disjoint from mutable
 * state.
 * @param extent Receives the leading column extent, zero if empty/all-zero;
 * one-based otherwise. A live host index_t object; its old value is not read.
 * @param plan Matching routine, scalar, dimensions, layout, stride and
 * provider.
 * @param workspace Disjoint live packing scalars required by the plan.
 * @param report Metadata aliases preserve all bytes; other paths reset before
 * validation. Records actual native entry, publication validity and absent
 * INFO.
 * @return Success after bounded native result publication or local empty
 * result; kProvider for changed native integers/out-of-range result, or
 * preflight error.
 * @post Input and padding remain unchanged; result publishes only on success.
 */
ASC_DENSE_LAPACK_EXPORT Status Ilalc(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<const double> matrix,
                                     index_t& extent,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans ILACLC for a full matrix.
 * @param provider Explicit checked serial Reference provider.
 * @param matrix Read-only matrix with a supported padded row/column layout.
 * @return Metadata-only identity and row-major packing requirement, or a
 * structural, placement or integer/span overflow error. No values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryIlalcWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix);
/** @brief Finds the last nonzero column through ILACLC.
 * @param provider Same provider identity as the workspace query.
 * @param matrix Read-only input, excluding padding, disjoint from mutable
 * state.
 * @param extent Receives the leading column extent, zero if empty/all-zero;
 * one-based otherwise. A live host index_t object; its old value is not read.
 * @param plan Matching routine, scalar, dimensions, layout, stride and
 * provider.
 * @param workspace Disjoint live packing scalars required by the plan.
 * @param report Metadata aliases preserve all bytes; other paths reset before
 * validation. Records actual native entry, publication validity and absent
 * INFO.
 * @return Success after bounded native result publication or local empty
 * result; kProvider for changed native integers/out-of-range result, or
 * preflight error.
 * @post Input and padding remain unchanged; result publishes only on success.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ilalc(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<const std::complex<float>> matrix, index_t& extent,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans ILAZLC for a full matrix.
 * @param provider Explicit checked serial Reference provider.
 * @param matrix Read-only matrix with a supported padded row/column layout.
 * @return Metadata-only identity and row-major packing requirement, or a
 * structural, placement or integer/span overflow error. No values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryIlalcWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix);
/** @brief Finds the last nonzero column through ILAZLC.
 * @param provider Same provider identity as the workspace query.
 * @param matrix Read-only input, excluding padding, disjoint from mutable
 * state.
 * @param extent Receives the leading column extent, zero if empty/all-zero;
 * one-based otherwise. A live host index_t object; its old value is not read.
 * @param plan Matching routine, scalar, dimensions, layout, stride and
 * provider.
 * @param workspace Disjoint live packing scalars required by the plan.
 * @param report Metadata aliases preserve all bytes; other paths reset before
 * validation. Records actual native entry, publication validity and absent
 * INFO.
 * @return Success after bounded native result publication or local empty
 * result; kProvider for changed native integers/out-of-range result, or
 * preflight error.
 * @post Input and padding remain unchanged; result publishes only on success.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ilalc(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<const std::complex<double>> matrix, index_t& extent,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans ILASLR for a full matrix.
 * @param provider Explicit checked serial Reference provider.
 * @param matrix Read-only matrix with a supported padded row/column layout.
 * @return Metadata-only identity and row-major packing requirement, or a
 * structural, placement or integer/span overflow error. No values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryIlalrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix);
/** @brief Finds the last nonzero row through ILASLR.
 * @param provider Same provider identity as the workspace query.
 * @param matrix Read-only input, excluding padding, disjoint from mutable
 * state.
 * @param extent Receives the leading row extent, zero if empty/all-zero;
 * one-based otherwise. A live host index_t object; its old value is not read.
 * @param plan Matching routine, scalar, dimensions, layout, stride and
 * provider.
 * @param workspace Disjoint live packing scalars required by the plan.
 * @param report Metadata aliases preserve all bytes; other paths reset before
 * validation. Records actual native entry, publication validity and absent
 * INFO.
 * @return Success after bounded native result publication or local empty
 * result; kProvider for changed native integers/out-of-range result, or
 * preflight error.
 * @post Input and padding remain unchanged; result publishes only on success.
 */
ASC_DENSE_LAPACK_EXPORT Status Ilalr(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<const float> matrix,
                                     index_t& extent,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans ILADLR for a full matrix.
 * @param provider Explicit checked serial Reference provider.
 * @param matrix Read-only matrix with a supported padded row/column layout.
 * @return Metadata-only identity and row-major packing requirement, or a
 * structural, placement or integer/span overflow error. No values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryIlalrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix);
/** @brief Finds the last nonzero row through ILADLR.
 * @param provider Same provider identity as the workspace query.
 * @param matrix Read-only input, excluding padding, disjoint from mutable
 * state.
 * @param extent Receives the leading row extent, zero if empty/all-zero;
 * one-based otherwise. A live host index_t object; its old value is not read.
 * @param plan Matching routine, scalar, dimensions, layout, stride and
 * provider.
 * @param workspace Disjoint live packing scalars required by the plan.
 * @param report Metadata aliases preserve all bytes; other paths reset before
 * validation. Records actual native entry, publication validity and absent
 * INFO.
 * @return Success after bounded native result publication or local empty
 * result; kProvider for changed native integers/out-of-range result, or
 * preflight error.
 * @post Input and padding remain unchanged; result publishes only on success.
 */
ASC_DENSE_LAPACK_EXPORT Status Ilalr(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<const double> matrix,
                                     index_t& extent,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans ILACLR for a full matrix.
 * @param provider Explicit checked serial Reference provider.
 * @param matrix Read-only matrix with a supported padded row/column layout.
 * @return Metadata-only identity and row-major packing requirement, or a
 * structural, placement or integer/span overflow error. No values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryIlalrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix);
/** @brief Finds the last nonzero row through ILACLR.
 * @param provider Same provider identity as the workspace query.
 * @param matrix Read-only input, excluding padding, disjoint from mutable
 * state.
 * @param extent Receives the leading row extent, zero if empty/all-zero;
 * one-based otherwise. A live host index_t object; its old value is not read.
 * @param plan Matching routine, scalar, dimensions, layout, stride and
 * provider.
 * @param workspace Disjoint live packing scalars required by the plan.
 * @param report Metadata aliases preserve all bytes; other paths reset before
 * validation. Records actual native entry, publication validity and absent
 * INFO.
 * @return Success after bounded native result publication or local empty
 * result; kProvider for changed native integers/out-of-range result, or
 * preflight error.
 * @post Input and padding remain unchanged; result publishes only on success.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ilalr(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<const std::complex<float>> matrix, index_t& extent,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans ILAZLR for a full matrix.
 * @param provider Explicit checked serial Reference provider.
 * @param matrix Read-only matrix with a supported padded row/column layout.
 * @return Metadata-only identity and row-major packing requirement, or a
 * structural, placement or integer/span overflow error. No values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryIlalrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix);
/** @brief Finds the last nonzero row through ILAZLR.
 * @param provider Same provider identity as the workspace query.
 * @param matrix Read-only input, excluding padding, disjoint from mutable
 * state.
 * @param extent Receives the leading row extent, zero if empty/all-zero;
 * one-based otherwise. A live host index_t object; its old value is not read.
 * @param plan Matching routine, scalar, dimensions, layout, stride and
 * provider.
 * @param workspace Disjoint live packing scalars required by the plan.
 * @param report Metadata aliases preserve all bytes; other paths reset before
 * validation. Records actual native entry, publication validity and absent
 * INFO.
 * @return Success after bounded native result publication or local empty
 * result; kProvider for changed native integers/out-of-range result, or
 * preflight error.
 * @post Input and padding remain unchanged; result publishes only on success.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ilalr(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<const std::complex<double>> matrix, index_t& extent,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_MATRIX_SUPPORT_H_
