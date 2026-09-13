#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_H_

/** @file
 * @brief Checked single-stage Aasen symmetric/Hermitian factorizations.
 *
 * The selected diagonal and first offdiagonal store tridiagonal T. Remaining
 * selected entries store shifted unit triangular multipliers: lower A(i,j)
 * stores L(i,j+1) for i>j+1, and upper A(i,j) stores U(i+1,j) for j>i+1
 * (zero-based indices). The first column of L and first row of U are those
 * of the identity. Positive one-based pivots describe symmetric swaps in
 * increasing index order, with pivots[0]=1 and i+1<=pivots[i]<=n. Undoing
 * these swaps in reverse reconstructs the original matrix from L*T*L**T/H
 * or U**T/H*T*U. Complex symmetric factors use transpose, not adjoint.
 *
 * Original Hermitian diagonal imaginary parts and the unselected triangle
 * are ignored; row storage and all Hermitian inputs use explicit caller
 * packing. Padding and the unselected triangle are preserved. No numerical
 * input finiteness restriction, hidden allocation or fallback is introduced.
 * INFO=0 denotes factorization completion, including singular T, and does
 * not certify invertibility or numerical accuracy. These factors belong to
 * the Aasen family, not classic, ROOK, RK or two-stage factor views.
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
/** @brief Queries single real Aasen factor storage without reading arrays.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower triangle of the original matrix.
 * @param matrix Square input/output descriptor; entries remain unread.
 * @param pivots Disjoint contiguous exact-n ASC pivot output; entries unread.
 * @return Checked matching plan or structural error; no native query call.
 * Active scalar WORK uses minimum 2*n and preferred rounded 65*n entries.
 * For n=1, real and Hermitian routines use one entry for both; complex
 * symmetric routines retain minimum two and preferred 65. An empty problem
 * needs no workspace. Native INTEGER pivots use caller byte storage with
 * explicit lifetimes; required layout packing is separately reported.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots);

/** @brief Executes single real single-stage Aasen factorization.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected input/output triangle; determines shifted storage.
 * @param matrix Square original matrix, replaced by selected T and multipliers.
 * @param pivots Disjoint exact-n contiguous positive one-based swap output.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller scalar, native INTEGER and packing storage.
 * @param report Required native INFO, Aasen provenance and output validity.
 * @return OK on completion, or structural/provider error. Singular factors
 * may complete successfully. Unexpected INFO, WORK or malformed full-width
 * pivots are provider defects: public pivots and packed A are withheld,
 * while direct column-major symmetric A may have changed. Empty execution
 * validates metadata and completes without accessing arrays or the provider.
 */
ASC_DENSE_LAPACK_EXPORT Status SytrfAa(const ReferenceLapackProvider& provider,
                                       DenseBlasTriangle triangle,
                                       DenseBlasMatrixView<float> matrix,
                                       DenseBlasVectorView<index_t> pivots,
                                       const LapackWorkspacePlan& plan,
                                       const LapackWorkspace& workspace,
                                       LapackReport& report);

/** @brief Queries double real Aasen factor storage without reading arrays.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower triangle of the original matrix.
 * @param matrix Square input/output descriptor; entries remain unread.
 * @param pivots Disjoint contiguous exact-n ASC pivot output; entries unread.
 * @return Checked matching plan or structural error; no native query call.
 * Active scalar WORK uses minimum 2*n and preferred rounded 65*n entries.
 * For n=1, real and Hermitian routines use one entry for both; complex
 * symmetric routines retain minimum two and preferred 65. An empty problem
 * needs no workspace. Native INTEGER pivots use caller byte storage with
 * explicit lifetimes; required layout packing is separately reported.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots);

/** @brief Executes double real single-stage Aasen factorization.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected input/output triangle; determines shifted storage.
 * @param matrix Square original matrix, replaced by selected T and multipliers.
 * @param pivots Disjoint exact-n contiguous positive one-based swap output.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller scalar, native INTEGER and packing storage.
 * @param report Required native INFO, Aasen provenance and output validity.
 * @return OK on completion, or structural/provider error. Singular factors
 * may complete successfully. Unexpected INFO, WORK or malformed full-width
 * pivots are provider defects: public pivots and packed A are withheld,
 * while direct column-major symmetric A may have changed. Empty execution
 * validates metadata and completes without accessing arrays or the provider.
 */
ASC_DENSE_LAPACK_EXPORT Status SytrfAa(const ReferenceLapackProvider& provider,
                                       DenseBlasTriangle triangle,
                                       DenseBlasMatrixView<double> matrix,
                                       DenseBlasVectorView<index_t> pivots,
                                       const LapackWorkspacePlan& plan,
                                       const LapackWorkspace& workspace,
                                       LapackReport& report);

/** @brief Queries single complex symmetric Aasen factor storage without reading
 * arrays.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower triangle of the original matrix.
 * @param matrix Square input/output descriptor; entries remain unread.
 * @param pivots Disjoint contiguous exact-n ASC pivot output; entries unread.
 * @return Checked matching plan or structural error; no native query call.
 * Active scalar WORK uses minimum 2*n and preferred rounded 65*n entries.
 * For n=1, real and Hermitian routines use one entry for both; complex
 * symmetric routines retain minimum two and preferred 65. An empty problem
 * needs no workspace. Native INTEGER pivots use caller byte storage with
 * explicit lifetimes; required layout packing is separately reported.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes single complex symmetric single-stage Aasen factorization.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected input/output triangle; determines shifted storage.
 * @param matrix Square original matrix, replaced by selected T and multipliers.
 * @param pivots Disjoint exact-n contiguous positive one-based swap output.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller scalar, native INTEGER and packing storage.
 * @param report Required native INFO, Aasen provenance and output validity.
 * @return OK on completion, or structural/provider error. Singular factors
 * may complete successfully. Unexpected INFO, WORK or malformed full-width
 * pivots are provider defects: public pivots and packed A are withheld,
 * while direct column-major symmetric A may have changed. Empty execution
 * validates metadata and completes without accessing arrays or the provider.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrfAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<float>> matrix,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex symmetric Aasen factor storage without reading
 * arrays.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower triangle of the original matrix.
 * @param matrix Square input/output descriptor; entries remain unread.
 * @param pivots Disjoint contiguous exact-n ASC pivot output; entries unread.
 * @return Checked matching plan or structural error; no native query call.
 * Active scalar WORK uses minimum 2*n and preferred rounded 65*n entries.
 * For n=1, real and Hermitian routines use one entry for both; complex
 * symmetric routines retain minimum two and preferred 65. An empty problem
 * needs no workspace. Native INTEGER pivots use caller byte storage with
 * explicit lifetimes; required layout packing is separately reported.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes double complex symmetric single-stage Aasen factorization.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected input/output triangle; determines shifted storage.
 * @param matrix Square original matrix, replaced by selected T and multipliers.
 * @param pivots Disjoint exact-n contiguous positive one-based swap output.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller scalar, native INTEGER and packing storage.
 * @param report Required native INFO, Aasen provenance and output validity.
 * @return OK on completion, or structural/provider error. Singular factors
 * may complete successfully. Unexpected INFO, WORK or malformed full-width
 * pivots are provider defects: public pivots and packed A are withheld,
 * while direct column-major symmetric A may have changed. Empty execution
 * validates metadata and completes without accessing arrays or the provider.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrfAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<double>> matrix,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex Hermitian Aasen factor storage without reading
 * arrays.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower triangle of the original matrix.
 * @param matrix Square input/output descriptor; entries remain unread.
 * @param pivots Disjoint contiguous exact-n ASC pivot output; entries unread.
 * @return Checked matching plan or structural error; no native query call.
 * Active scalar WORK uses minimum 2*n and preferred rounded 65*n entries.
 * For n=1, real and Hermitian routines use one entry for both; complex
 * symmetric routines retain minimum two and preferred 65. An empty problem
 * needs no workspace. Native INTEGER pivots use caller byte storage with
 * explicit lifetimes; required layout packing is separately reported.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes single complex Hermitian single-stage Aasen factorization.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected input/output triangle; determines shifted storage.
 * @param matrix Square original matrix, replaced by selected T and multipliers.
 * @param pivots Disjoint exact-n contiguous positive one-based swap output.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller scalar, native INTEGER and packing storage.
 * @param report Required native INFO, Aasen provenance and output validity.
 * @return OK on completion, or structural/provider error. Singular factors
 * may complete successfully. Unexpected INFO, WORK or malformed full-width
 * pivots are provider defects: public pivots and packed A are withheld,
 * while direct column-major symmetric A may have changed. Empty execution
 * validates metadata and completes without accessing arrays or the provider.
 */
ASC_DENSE_LAPACK_EXPORT Status
HetrfAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<float>> matrix,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex Hermitian Aasen factor storage without reading
 * arrays.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected upper or lower triangle of the original matrix.
 * @param matrix Square input/output descriptor; entries remain unread.
 * @param pivots Disjoint contiguous exact-n ASC pivot output; entries unread.
 * @return Checked matching plan or structural error; no native query call.
 * Active scalar WORK uses minimum 2*n and preferred rounded 65*n entries.
 * For n=1, real and Hermitian routines use one entry for both; complex
 * symmetric routines retain minimum two and preferred 65. An empty problem
 * needs no workspace. Native INTEGER pivots use caller byte storage with
 * explicit lifetimes; required layout packing is separately reported.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes double complex Hermitian single-stage Aasen factorization.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected input/output triangle; determines shifted storage.
 * @param matrix Square original matrix, replaced by selected T and multipliers.
 * @param pivots Disjoint exact-n contiguous positive one-based swap output.
 * @param plan Unmodified matching query plan; revalidated before mutation.
 * @param workspace Disjoint caller scalar, native INTEGER and packing storage.
 * @param report Required native INFO, Aasen provenance and output validity.
 * @return OK on completion, or structural/provider error. Singular factors
 * may complete successfully. Unexpected INFO, WORK or malformed full-width
 * pivots are provider defects: public pivots and packed A are withheld,
 * while direct column-major symmetric A may have changed. Empty execution
 * validates metadata and completes without accessing arrays or the provider.
 */
ASC_DENSE_LAPACK_EXPORT Status
HetrfAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<double>> matrix,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_H_
