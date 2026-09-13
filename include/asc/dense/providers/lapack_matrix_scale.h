#ifndef ASC_DENSE_PROVIDERS_LAPACK_MATRIX_SCALE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_MATRIX_SCALE_H_
/** @file
 * @brief Explicit checked LASCL scaling of full and band storage.
 *
 * Only selected coefficients are input/output; padding, unused corners and
 * GBTRF fill-in remain unchanged. Row layouts use explicit live caller packing;
 * column layouts enter the pinned provider directly. No band densification,
 * conjugation, implicit allocation, transfer or synchronization occurs.
 * Queries inspect metadata only. Independent buffers, workspace and reports
 * support concurrent calls; an immutable matching plan may be reused.
 */
#include <complex>
#include <cstdint>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
namespace asc {
/** @brief Selected coefficients in full matrix storage. */
enum class LapackMatrixScalePart : std::uint8_t {
  kAll,             ///< G: all matrix coefficients.
  kLower,           ///< L: row >= column, including rectangular trapezoids.
  kUpper,           ///< U: row <= column, including rectangular trapezoids.
  kUpperHessenberg  ///< H: row <= column+1, including rectangular shapes.
};
/** @brief Plans SLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param part Selected full-matrix pattern; invalid enums are rejected.
 * @param matrix Full matrix in either layout; only the selected part changes.
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixScalePart part,
    DenseBlasMatrixView<float> matrix);
/** @brief Executes the explicitly selected SLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param part Selected full-matrix pattern; invalid enums are rejected.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix Full matrix in either layout; only the selected part changes.
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status Lascl(const ReferenceLapackProvider& provider,
                                     LapackMatrixScalePart part, float cfrom,
                                     float cto,
                                     DenseBlasMatrixView<float> matrix,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans SLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param matrix B/Q selected lower/upper band, in either layout. Bandwidth
 * must be <=max(order-1,0). This operation scales raw stored coefficients,
 * including imaginary diagonal parts; it does not certify definiteness or
 * infer an unstored complex triangle.
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<float> matrix);
/** @brief Executes the explicitly selected SLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix B/Q selected lower/upper band, in either layout. Bandwidth
 * must be <=max(order-1,0). This operation scales raw stored coefficients,
 * including imaginary diagonal parts; it does not certify definiteness or
 * infer an unstored complex triangle.
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lascl(const ReferenceLapackProvider& provider, float cfrom, float cto,
      LapackPositiveDefiniteBandView<float> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans SLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param matrix Z general band in existing column-major GBTRF storage.
 * Only original band entries change; the reserved factor-fill rows stay
 * untouched. Bandwidths must not exceed max(rows-1,0) and max(columns-1,0).
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<float> matrix);
/** @brief Executes the explicitly selected SLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix Z general band in existing column-major GBTRF storage.
 * Only original band entries change; the reserved factor-fill rows stay
 * untouched. Bandwidths must not exceed max(rows-1,0) and max(columns-1,0).
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status Lascl(const ReferenceLapackProvider& provider,
                                     float cfrom, float cto,
                                     LapackLuBandView<float> matrix,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans DLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param part Selected full-matrix pattern; invalid enums are rejected.
 * @param matrix Full matrix in either layout; only the selected part changes.
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixScalePart part,
    DenseBlasMatrixView<double> matrix);
/** @brief Executes the explicitly selected DLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param part Selected full-matrix pattern; invalid enums are rejected.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix Full matrix in either layout; only the selected part changes.
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status Lascl(const ReferenceLapackProvider& provider,
                                     LapackMatrixScalePart part, double cfrom,
                                     double cto,
                                     DenseBlasMatrixView<double> matrix,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans DLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param matrix B/Q selected lower/upper band, in either layout. Bandwidth
 * must be <=max(order-1,0). This operation scales raw stored coefficients,
 * including imaginary diagonal parts; it does not certify definiteness or
 * infer an unstored complex triangle.
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<double> matrix);
/** @brief Executes the explicitly selected DLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix B/Q selected lower/upper band, in either layout. Bandwidth
 * must be <=max(order-1,0). This operation scales raw stored coefficients,
 * including imaginary diagonal parts; it does not certify definiteness or
 * infer an unstored complex triangle.
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lascl(const ReferenceLapackProvider& provider, double cfrom, double cto,
      LapackPositiveDefiniteBandView<double> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans DLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param matrix Z general band in existing column-major GBTRF storage.
 * Only original band entries change; the reserved factor-fill rows stay
 * untouched. Bandwidths must not exceed max(rows-1,0) and max(columns-1,0).
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<double> matrix);
/** @brief Executes the explicitly selected DLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix Z general band in existing column-major GBTRF storage.
 * Only original band entries change; the reserved factor-fill rows stay
 * untouched. Bandwidths must not exceed max(rows-1,0) and max(columns-1,0).
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status Lascl(const ReferenceLapackProvider& provider,
                                     double cfrom, double cto,
                                     LapackLuBandView<double> matrix,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans CLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param part Selected full-matrix pattern; invalid enums are rejected.
 * @param matrix Full matrix in either layout; only the selected part changes.
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixScalePart part,
    DenseBlasMatrixView<std::complex<float>> matrix);
/** @brief Executes the explicitly selected CLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param part Selected full-matrix pattern; invalid enums are rejected.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix Full matrix in either layout; only the selected part changes.
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lascl(const ReferenceLapackProvider& provider, LapackMatrixScalePart part,
      float cfrom, float cto, DenseBlasMatrixView<std::complex<float>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans CLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param matrix B/Q selected lower/upper band, in either layout. Bandwidth
 * must be <=max(order-1,0). This operation scales raw stored coefficients,
 * including imaginary diagonal parts; it does not certify definiteness or
 * infer an unstored complex triangle.
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<float>> matrix);
/** @brief Executes the explicitly selected CLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix B/Q selected lower/upper band, in either layout. Bandwidth
 * must be <=max(order-1,0). This operation scales raw stored coefficients,
 * including imaginary diagonal parts; it does not certify definiteness or
 * infer an unstored complex triangle.
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lascl(const ReferenceLapackProvider& provider, float cfrom, float cto,
      LapackPositiveDefiniteBandView<std::complex<float>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans CLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param matrix Z general band in existing column-major GBTRF storage.
 * Only original band entries change; the reserved factor-fill rows stay
 * untouched. Bandwidths must not exceed max(rows-1,0) and max(columns-1,0).
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<float>> matrix);
/** @brief Executes the explicitly selected CLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix Z general band in existing column-major GBTRF storage.
 * Only original band entries change; the reserved factor-fill rows stay
 * untouched. Bandwidths must not exceed max(rows-1,0) and max(columns-1,0).
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lascl(const ReferenceLapackProvider& provider, float cfrom, float cto,
      LapackLuBandView<std::complex<float>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans ZLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param part Selected full-matrix pattern; invalid enums are rejected.
 * @param matrix Full matrix in either layout; only the selected part changes.
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixScalePart part,
    DenseBlasMatrixView<std::complex<double>> matrix);
/** @brief Executes the explicitly selected ZLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param part Selected full-matrix pattern; invalid enums are rejected.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix Full matrix in either layout; only the selected part changes.
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status Lascl(
    const ReferenceLapackProvider& provider, LapackMatrixScalePart part,
    double cfrom, double cto, DenseBlasMatrixView<std::complex<double>> matrix,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Plans ZLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param matrix B/Q selected lower/upper band, in either layout. Bandwidth
 * must be <=max(order-1,0). This operation scales raw stored coefficients,
 * including imaginary diagonal parts; it does not certify definiteness or
 * infer an unstored complex triangle.
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<double>> matrix);
/** @brief Executes the explicitly selected ZLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix B/Q selected lower/upper band, in either layout. Bandwidth
 * must be <=max(order-1,0). This operation scales raw stored coefficients,
 * including imaginary diagonal parts; it does not certify definiteness or
 * infer an unstored complex triangle.
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lascl(const ReferenceLapackProvider& provider, double cfrom, double cto,
      LapackPositiveDefiniteBandView<std::complex<double>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans ZLASCL with explicit selected-storage packing.
 * @param provider Checked host Reference provider and actual integer ABI.
 * @param matrix Z general band in existing column-major GBTRF storage.
 * Only original band entries change; the reserved factor-fill rows stay
 * untouched. Bandwidths must not exceed max(rows-1,0) and max(columns-1,0).
 * @return Metadata-only plan or structural/count failure. Column layouts need
 * no workspace; row layouts need compact band/full entries in
 * kLayoutConversion. Scale factors are operation inputs and do not affect or
 * invalidate this plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLasclWorkspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<double>> matrix);
/** @brief Executes the explicitly selected ZLASCL arithmetic.
 * @param provider Same provider identity used by the query.
 * @param cfrom Nonzero, non-NaN real denominator; infinities are permitted.
 * @param cto Non-NaN real numerator; zero and infinities are permitted.
 * @param matrix Z general band in existing column-major GBTRF storage.
 * Only original band entries change; the reserved factor-fill rows stay
 * untouched. Bandwidths must not exceed max(rows-1,0) and max(columns-1,0).
 * @param plan Matching scalar, storage pattern, dimensions, strides and
 * provider.
 * @param workspace Independent caller-owned live scalar packing objects.
 * @param report Reset before validation; records actual native INFO after
 * entry.
 * @return Success on native INFO0, without a finiteness or accuracy
 * certificate. Native exceptional arithmetic remains visible. Missing/nonzero
 * INFO is a provider defect: direct column writes survive with unusable
 * validity, while row publication is withheld. Preflight rejection preserves
 * numeric storage. Empty shapes and equal finite scales complete locally
 * without numeric access or native INFO, after validating metadata, factors and
 * workspace. There is no hidden fallback, diagnostic clamp or floating-point
 * environment change.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lascl(const ReferenceLapackProvider& provider, double cfrom, double cto,
      LapackLuBandView<std::complex<double>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_MATRIX_SCALE_H_
