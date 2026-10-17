#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_H_
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
/** @brief Queries single real packed symmetric factor storage without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower packed storage and factor direction.
 * @param matrix Original order-n packed matrix descriptor; entries remain
 * unread.
 * @param pivots Disjoint contiguous exact-n ASC signed pivot output; entries
 * unread.
 * @return Matching checked plan or structural error; no native query call.
 * Nonempty execution needs n+2 native INTEGER objects in caller byte storage
 * and n*(n+1)/2+2 live scalar objects for guarded packed execution in either
 * layout. Each region includes one guard before and after the native array.
 * There is no native scalar WORK requirement. Empty execution needs no scratch.
 * Native packed index products and byte counts are checked before any call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Computes single real packed symmetric Bunch-Kaufman factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Upper U*D*U^T or lower L*D*L^T storage, with conjugate
 * transpose for Hermitian matrices; determines packed indexing and pivot order.
 * @param matrix Original packed matrix, replaced by D and unit-triangular
 * multipliers with interchanges. Hermitian input diagonal imaginary parts are
 * ignored and output diagonal imaginary parts become zero. Complex symmetric
 * matrices retain their full complex diagonals and use ordinary transpose.
 * @param pivots Disjoint exact-n contiguous signed one-based native encoding:
 * positive entries denote 1x1 blocks; equal negative pairs denote 2x2 blocks.
 * These factors and pivots must retain their scalar, provider, triangle,
 * symmetry and common-call provenance for a matching packed consumer.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller-owned native INTEGER and live scalar packing
 * regions; native INTEGER object lifetimes are explicitly started here.
 * @param report Native INFO, Bunch-Kaufman family and output validity.
 * @return OK with complete factors for INFO=0. Positive INFO returns numerical
 * failure with all completed factors/pivots and documented-partial validity;
 * diagnostic_index is INFO-1. A zero reported diagonal yields singular outcome;
 * otherwise partial-result preserves native nonfinite-input behavior.
 * Invalid or missing full-width INFO/pivots or changed native array guards
 * are provider defects: all matrix output and public pivots are withheld.
 * A significant NaN in an order-one input returns numerical failure with
 * unusable output and diagnostic_index=0 before scratch or native access,
 * because the pinned scalar path may use an uninitialized pivot index.
 * Hermitian imaginary diagonal NaNs remain ignored; no finite input is
 * removed by this safety check. Empty execution validates metadata and
 * completes without reading arrays, scratch or calling the provider.
 * Structural rejection preserves numeric operands and scratch. No allocation,
 * implicit transfer, whole-matrix finite-data scan, factor substitution or
 * hidden fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status Sptrf(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasPackedMatrixView<float> matrix,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real packed symmetric factor storage without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower packed storage and factor direction.
 * @param matrix Original order-n packed matrix descriptor; entries remain
 * unread.
 * @param pivots Disjoint contiguous exact-n ASC signed pivot output; entries
 * unread.
 * @return Matching checked plan or structural error; no native query call.
 * Nonempty execution needs n+2 native INTEGER objects in caller byte storage
 * and n*(n+1)/2+2 live scalar objects for guarded packed execution in either
 * layout. Each region includes one guard before and after the native array.
 * There is no native scalar WORK requirement. Empty execution needs no scratch.
 * Native packed index products and byte counts are checked before any call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Computes double real packed symmetric Bunch-Kaufman factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Upper U*D*U^T or lower L*D*L^T storage, with conjugate
 * transpose for Hermitian matrices; determines packed indexing and pivot order.
 * @param matrix Original packed matrix, replaced by D and unit-triangular
 * multipliers with interchanges. Hermitian input diagonal imaginary parts are
 * ignored and output diagonal imaginary parts become zero. Complex symmetric
 * matrices retain their full complex diagonals and use ordinary transpose.
 * @param pivots Disjoint exact-n contiguous signed one-based native encoding:
 * positive entries denote 1x1 blocks; equal negative pairs denote 2x2 blocks.
 * These factors and pivots must retain their scalar, provider, triangle,
 * symmetry and common-call provenance for a matching packed consumer.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller-owned native INTEGER and live scalar packing
 * regions; native INTEGER object lifetimes are explicitly started here.
 * @param report Native INFO, Bunch-Kaufman family and output validity.
 * @return OK with complete factors for INFO=0. Positive INFO returns numerical
 * failure with all completed factors/pivots and documented-partial validity;
 * diagnostic_index is INFO-1. A zero reported diagonal yields singular outcome;
 * otherwise partial-result preserves native nonfinite-input behavior.
 * Invalid or missing full-width INFO/pivots or changed native array guards
 * are provider defects: all matrix output and public pivots are withheld.
 * A significant NaN in an order-one input returns numerical failure with
 * unusable output and diagnostic_index=0 before scratch or native access,
 * because the pinned scalar path may use an uninitialized pivot index.
 * Hermitian imaginary diagonal NaNs remain ignored; no finite input is
 * removed by this safety check. Empty execution validates metadata and
 * completes without reading arrays, scratch or calling the provider.
 * Structural rejection preserves numeric operands and scratch. No allocation,
 * implicit transfer, whole-matrix finite-data scan, factor substitution or
 * hidden fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status Sptrf(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasPackedMatrixView<double> matrix,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single complex packed symmetric factor storage without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower packed storage and factor direction.
 * @param matrix Original order-n packed matrix descriptor; entries remain
 * unread.
 * @param pivots Disjoint contiguous exact-n ASC signed pivot output; entries
 * unread.
 * @return Matching checked plan or structural error; no native query call.
 * Nonempty execution needs n+2 native INTEGER objects in caller byte storage
 * and n*(n+1)/2+2 live scalar objects for guarded packed execution in either
 * layout. Each region includes one guard before and after the native array.
 * There is no native scalar WORK requirement. Empty execution needs no scratch.
 * Native packed index products and byte counts are checked before any call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Computes single complex packed symmetric Bunch-Kaufman factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Upper U*D*U^T or lower L*D*L^T storage, with conjugate
 * transpose for Hermitian matrices; determines packed indexing and pivot order.
 * @param matrix Original packed matrix, replaced by D and unit-triangular
 * multipliers with interchanges. Hermitian input diagonal imaginary parts are
 * ignored and output diagonal imaginary parts become zero. Complex symmetric
 * matrices retain their full complex diagonals and use ordinary transpose.
 * @param pivots Disjoint exact-n contiguous signed one-based native encoding:
 * positive entries denote 1x1 blocks; equal negative pairs denote 2x2 blocks.
 * These factors and pivots must retain their scalar, provider, triangle,
 * symmetry and common-call provenance for a matching packed consumer.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller-owned native INTEGER and live scalar packing
 * regions; native INTEGER object lifetimes are explicitly started here.
 * @param report Native INFO, Bunch-Kaufman family and output validity.
 * @return OK with complete factors for INFO=0. Positive INFO returns numerical
 * failure with all completed factors/pivots and documented-partial validity;
 * diagnostic_index is INFO-1. A zero reported diagonal yields singular outcome;
 * otherwise partial-result preserves native nonfinite-input behavior.
 * Invalid or missing full-width INFO/pivots or changed native array guards
 * are provider defects: all matrix output and public pivots are withheld.
 * A significant NaN in an order-one input returns numerical failure with
 * unusable output and diagnostic_index=0 before scratch or native access,
 * because the pinned scalar path may use an uninitialized pivot index.
 * Hermitian imaginary diagonal NaNs remain ignored; no finite input is
 * removed by this safety check. Empty execution validates metadata and
 * completes without reading arrays, scratch or calling the provider.
 * Structural rejection preserves numeric operands and scratch. No allocation,
 * implicit transfer, whole-matrix finite-data scan, factor substitution or
 * hidden fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sptrf(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<float>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex packed symmetric factor storage without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower packed storage and factor direction.
 * @param matrix Original order-n packed matrix descriptor; entries remain
 * unread.
 * @param pivots Disjoint contiguous exact-n ASC signed pivot output; entries
 * unread.
 * @return Matching checked plan or structural error; no native query call.
 * Nonempty execution needs n+2 native INTEGER objects in caller byte storage
 * and n*(n+1)/2+2 live scalar objects for guarded packed execution in either
 * layout. Each region includes one guard before and after the native array.
 * There is no native scalar WORK requirement. Empty execution needs no scratch.
 * Native packed index products and byte counts are checked before any call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Computes double complex packed symmetric Bunch-Kaufman factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Upper U*D*U^T or lower L*D*L^T storage, with conjugate
 * transpose for Hermitian matrices; determines packed indexing and pivot order.
 * @param matrix Original packed matrix, replaced by D and unit-triangular
 * multipliers with interchanges. Hermitian input diagonal imaginary parts are
 * ignored and output diagonal imaginary parts become zero. Complex symmetric
 * matrices retain their full complex diagonals and use ordinary transpose.
 * @param pivots Disjoint exact-n contiguous signed one-based native encoding:
 * positive entries denote 1x1 blocks; equal negative pairs denote 2x2 blocks.
 * These factors and pivots must retain their scalar, provider, triangle,
 * symmetry and common-call provenance for a matching packed consumer.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller-owned native INTEGER and live scalar packing
 * regions; native INTEGER object lifetimes are explicitly started here.
 * @param report Native INFO, Bunch-Kaufman family and output validity.
 * @return OK with complete factors for INFO=0. Positive INFO returns numerical
 * failure with all completed factors/pivots and documented-partial validity;
 * diagnostic_index is INFO-1. A zero reported diagonal yields singular outcome;
 * otherwise partial-result preserves native nonfinite-input behavior.
 * Invalid or missing full-width INFO/pivots or changed native array guards
 * are provider defects: all matrix output and public pivots are withheld.
 * A significant NaN in an order-one input returns numerical failure with
 * unusable output and diagnostic_index=0 before scratch or native access,
 * because the pinned scalar path may use an uninitialized pivot index.
 * Hermitian imaginary diagonal NaNs remain ignored; no finite input is
 * removed by this safety check. Empty execution validates metadata and
 * completes without reading arrays, scratch or calling the provider.
 * Structural rejection preserves numeric operands and scratch. No allocation,
 * implicit transfer, whole-matrix finite-data scan, factor substitution or
 * hidden fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sptrf(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<double>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex packed Hermitian factor storage without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower packed storage and factor direction.
 * @param matrix Original order-n packed matrix descriptor; entries remain
 * unread.
 * @param pivots Disjoint contiguous exact-n ASC signed pivot output; entries
 * unread.
 * @return Matching checked plan or structural error; no native query call.
 * Nonempty execution needs n+2 native INTEGER objects in caller byte storage
 * and n*(n+1)/2+2 live scalar objects for guarded packed execution in either
 * layout. Each region includes one guard before and after the native array.
 * There is no native scalar WORK requirement. Empty execution needs no scratch.
 * Native packed index products and byte counts are checked before any call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Computes single complex packed Hermitian Bunch-Kaufman factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Upper U*D*U^T or lower L*D*L^T storage, with conjugate
 * transpose for Hermitian matrices; determines packed indexing and pivot order.
 * @param matrix Original packed matrix, replaced by D and unit-triangular
 * multipliers with interchanges. Hermitian input diagonal imaginary parts are
 * ignored and output diagonal imaginary parts become zero. Complex symmetric
 * matrices retain their full complex diagonals and use ordinary transpose.
 * @param pivots Disjoint exact-n contiguous signed one-based native encoding:
 * positive entries denote 1x1 blocks; equal negative pairs denote 2x2 blocks.
 * These factors and pivots must retain their scalar, provider, triangle,
 * symmetry and common-call provenance for a matching packed consumer.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller-owned native INTEGER and live scalar packing
 * regions; native INTEGER object lifetimes are explicitly started here.
 * @param report Native INFO, Bunch-Kaufman family and output validity.
 * @return OK with complete factors for INFO=0. Positive INFO returns numerical
 * failure with all completed factors/pivots and documented-partial validity;
 * diagnostic_index is INFO-1. A zero reported diagonal yields singular outcome;
 * otherwise partial-result preserves native nonfinite-input behavior.
 * Invalid or missing full-width INFO/pivots or changed native array guards
 * are provider defects: all matrix output and public pivots are withheld.
 * A significant NaN in an order-one input returns numerical failure with
 * unusable output and diagnostic_index=0 before scratch or native access,
 * because the pinned scalar path may use an uninitialized pivot index.
 * Hermitian imaginary diagonal NaNs remain ignored; no finite input is
 * removed by this safety check. Empty execution validates metadata and
 * completes without reading arrays, scratch or calling the provider.
 * Structural rejection preserves numeric operands and scratch. No allocation,
 * implicit transfer, whole-matrix finite-data scan, factor substitution or
 * hidden fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hptrf(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<float>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex packed Hermitian factor storage without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower packed storage and factor direction.
 * @param matrix Original order-n packed matrix descriptor; entries remain
 * unread.
 * @param pivots Disjoint contiguous exact-n ASC signed pivot output; entries
 * unread.
 * @return Matching checked plan or structural error; no native query call.
 * Nonempty execution needs n+2 native INTEGER objects in caller byte storage
 * and n*(n+1)/2+2 live scalar objects for guarded packed execution in either
 * layout. Each region includes one guard before and after the native array.
 * There is no native scalar WORK requirement. Empty execution needs no scratch.
 * Native packed index products and byte counts are checked before any call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Computes double complex packed Hermitian Bunch-Kaufman factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Upper U*D*U^T or lower L*D*L^T storage, with conjugate
 * transpose for Hermitian matrices; determines packed indexing and pivot order.
 * @param matrix Original packed matrix, replaced by D and unit-triangular
 * multipliers with interchanges. Hermitian input diagonal imaginary parts are
 * ignored and output diagonal imaginary parts become zero. Complex symmetric
 * matrices retain their full complex diagonals and use ordinary transpose.
 * @param pivots Disjoint exact-n contiguous signed one-based native encoding:
 * positive entries denote 1x1 blocks; equal negative pairs denote 2x2 blocks.
 * These factors and pivots must retain their scalar, provider, triangle,
 * symmetry and common-call provenance for a matching packed consumer.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller-owned native INTEGER and live scalar packing
 * regions; native INTEGER object lifetimes are explicitly started here.
 * @param report Native INFO, Bunch-Kaufman family and output validity.
 * @return OK with complete factors for INFO=0. Positive INFO returns numerical
 * failure with all completed factors/pivots and documented-partial validity;
 * diagnostic_index is INFO-1. A zero reported diagonal yields singular outcome;
 * otherwise partial-result preserves native nonfinite-input behavior.
 * Invalid or missing full-width INFO/pivots or changed native array guards
 * are provider defects: all matrix output and public pivots are withheld.
 * A significant NaN in an order-one input returns numerical failure with
 * unusable output and diagnostic_index=0 before scratch or native access,
 * because the pinned scalar path may use an uninitialized pivot index.
 * Hermitian imaginary diagonal NaNs remain ignored; no finite input is
 * removed by this safety check. Empty execution validates metadata and
 * completes without reading arrays, scratch or calling the provider.
 * Structural rejection preserves numeric operands and scratch. No allocation,
 * implicit transfer, whole-matrix finite-data scan, factor substitution or
 * hidden fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hptrf(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<double>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_H_
