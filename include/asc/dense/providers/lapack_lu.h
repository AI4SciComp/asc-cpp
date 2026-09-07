#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_H_

/** @file
 * @brief Explicit reference recursive/unblocked LU, inversion and solve driver.
 *
 * These optional Dense-owned symbols require the checked Reference-LAPACK
 * facet. Matrices are row-major or column-major host/pinned-host with explicit
 * leading dimensions. Row-major execution packs the original orientation in
 * caller-owned live kLayoutConversion scalar objects: m*n for factorizations,
 * n*n for inversion, and the sum of row-major A/B element counts for GESV.
 * Layouts of A and B are independent. Only defined outputs are unpacked,
 * preserving padding; no undefined provider result or malformed pivot vector
 * is published. There is no
 * allocation, transfer, hidden packing, fallback or synchronization.
 * Independent disjoint calls are reentrant. Input/output/workspace lifetimes
 * cover the call.
 *
 * GETRF2/GETF2/GESV use kInteger provider-width pivot conversion entries.
 * GETRI additionally uses live scalar objects in kScalar: minimum max(1,n);
 * preferred comes from an actual LWORK=-1 query. The query uses caller-owned
 * kInteger capacity for n converted pivots and one private, live scalar WORK
 * object. It reads no factor entries, needs no layout packing even for a
 * row-major factor, changes no A/pivots, and reports the name
 * suffixed ".query"; conversion scratch may change. Query preflight checks
 * scratch ownership, alignment, capacity and overlap before calling. The pinned
 * ILAENV block size 64 makes n*64 a checked provider-integer bound before both
 * query and execution. Plans bind the returned minimum/preferred counts,
 * scalar, shape/leading dimensions/layout and exact provider identity.
 * Execution validates the plan without another foreign workspace query and
 * uses min(supplied scalar entries, preferred entries), at least minimum.
 *
 * Integer array lifetimes are established in caller workspace by nonallocating
 * placement construction. Scalar workspace must already contain live objects.
 * Every simultaneously live operand and workspace region must be disjoint.
 * Structural failures precede mutation, with absent INFO and called_provider
 * false. Reports reset even on failure. Negative raw INFO is a provider defect;
 * positive INFO denotes singularity, preserving its zero-based diagonal index.
 * Factorizations and GESV preserve completed raw LU/pivots on singularity;
 * GESV leaves B unchanged then. GETRI leaves the singular input unchanged.
 * Successful GETRI is an inverse, not a reusable LU factor. Numerical
 * nonfiniteness follows Reference LAPACK; success is not a conditioning claim.
 * Empty factorizations/drivers/inversion execution succeed without a foreign
 * call; GESV with n>0 and nrhs=0 still factors A. An empty GETRI query is a
 * real query and requires no pivot entries. Raw GETRI pivots must be valid
 * one-based LU sequential swaps; provenance of those raw buffers is a caller
 * responsibility.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Queries single real GETRF2 without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Rectangular row/column-major host matrix, unchanged.
 * @param pivots Contiguous min(m,n) one-based output, unchanged by query.
 * @return Checked exact conversion capacities or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Executes single real GETRF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place rectangular packed L/U; padding is preserved.
 * @param pivots Contiguous min(m,n) signed one-based sequential row swaps.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion buffers; see file contract.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status Getrf2(const ReferenceLapackProvider& provider,
                                      DenseBlasMatrixView<float> matrix,
                                      DenseBlasVectorView<index_t> pivots,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);

/** @brief Queries single real GETF2 without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Rectangular row/column-major host matrix, unchanged.
 * @param pivots Contiguous min(m,n) one-based output, unchanged by query.
 * @return Checked exact conversion capacities or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Executes single real GETF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place rectangular packed L/U; padding is preserved.
 * @param pivots Contiguous min(m,n) signed one-based sequential row swaps.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion buffers; see file contract.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status Getf2(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<float> matrix,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single real GETRI by a real nonmutating LWORK=-1 call.
 * @param provider Explicit checked reference provider.
 * @param factors Full live square row/column-major packed LU; unchanged.
 * @param pivots Valid immutable raw one-based LU pivots, size n; unchanged.
 * @param query_workspace Caller conversion storage with n kInteger entries.
 * @param report Mandatory query diagnostics; success leaves outputs unchanged.
 * @return Checked minimum/preferred execution plan or structural/provider
 * error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> factors,
    RawLapackPivotView pivots, const LapackWorkspace& query_workspace,
    LapackReport& report);
/** @brief Inverts a single real raw LU without factoring or querying again.
 * @param provider Explicit checked reference provider.
 * @param factors In-place LU replaced by inverse on success; padding preserved.
 * @param pivots Valid immutable one-based LU pivots, size n.
 * @param plan Unmodified matching result from QueryGetriWorkspace.
 * @param workspace Disjoint live scalar WORK and integer conversion storage.
 * @param report Mandatory reset-before-preflight diagnostics; no LU result tag.
 * @return OK, structural failure, or kNumerical with unchanged singular LU.
 */
ASC_DENSE_LAPACK_EXPORT Status Getri(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<float> factors,
                                     RawLapackPivotView pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Queries single real GESV capacities without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Square row/column-major A, unchanged by query.
 * @param pivots Contiguous n-entry output, unchanged by query.
 * @param rhs Column-major n-by-nrhs B, disjoint and unchanged by query.
 * @return Checked exact conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<float> rhs);
/** @brief Factors and solves a single real system through actual GESV.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place square A/LU; factored even when nrhs is zero.
 * @param pivots Contiguous n-entry raw signed one-based sequential swaps.
 * @param rhs In-place B/X; unchanged if factorization is singular.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical with raw singular LU/pivots.
 */
ASC_DENSE_LAPACK_EXPORT Status Gesv(const ReferenceLapackProvider& provider,
                                    DenseBlasMatrixView<float> matrix,
                                    DenseBlasVectorView<index_t> pivots,
                                    DenseBlasMatrixView<float> rhs,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);

/** @brief Queries double real GETRF2 without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Rectangular row/column-major host matrix, unchanged.
 * @param pivots Contiguous min(m,n) one-based output, unchanged by query.
 * @return Checked exact conversion capacities or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Executes double real GETRF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place rectangular packed L/U; padding is preserved.
 * @param pivots Contiguous min(m,n) signed one-based sequential row swaps.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion buffers; see file contract.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status Getrf2(const ReferenceLapackProvider& provider,
                                      DenseBlasMatrixView<double> matrix,
                                      DenseBlasVectorView<index_t> pivots,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);

/** @brief Queries double real GETF2 without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Rectangular row/column-major host matrix, unchanged.
 * @param pivots Contiguous min(m,n) one-based output, unchanged by query.
 * @return Checked exact conversion capacities or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Executes double real GETF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place rectangular packed L/U; padding is preserved.
 * @param pivots Contiguous min(m,n) signed one-based sequential row swaps.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion buffers; see file contract.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status Getf2(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<double> matrix,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real GETRI by a real nonmutating LWORK=-1 call.
 * @param provider Explicit checked reference provider.
 * @param factors Full live square row/column-major packed LU; unchanged.
 * @param pivots Valid immutable raw one-based LU pivots, size n; unchanged.
 * @param query_workspace Caller conversion storage with n kInteger entries.
 * @param report Mandatory query diagnostics; success leaves outputs unchanged.
 * @return Checked minimum/preferred execution plan or structural/provider
 * error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetriWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<double> factors, RawLapackPivotView pivots,
    const LapackWorkspace& query_workspace, LapackReport& report);
/** @brief Inverts a double real raw LU without factoring or querying again.
 * @param provider Explicit checked reference provider.
 * @param factors In-place LU replaced by inverse on success; padding preserved.
 * @param pivots Valid immutable one-based LU pivots, size n.
 * @param plan Unmodified matching result from QueryGetriWorkspace.
 * @param workspace Disjoint live scalar WORK and integer conversion storage.
 * @param report Mandatory reset-before-preflight diagnostics; no LU result tag.
 * @return OK, structural failure, or kNumerical with unchanged singular LU.
 */
ASC_DENSE_LAPACK_EXPORT Status Getri(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<double> factors,
                                     RawLapackPivotView pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Queries double real GESV capacities without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Square row/column-major A, unchanged by query.
 * @param pivots Contiguous n-entry output, unchanged by query.
 * @param rhs Column-major n-by-nrhs B, disjoint and unchanged by query.
 * @return Checked exact conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<double> rhs);
/** @brief Factors and solves a double real system through actual GESV.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place square A/LU; factored even when nrhs is zero.
 * @param pivots Contiguous n-entry raw signed one-based sequential swaps.
 * @param rhs In-place B/X; unchanged if factorization is singular.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical with raw singular LU/pivots.
 */
ASC_DENSE_LAPACK_EXPORT Status Gesv(const ReferenceLapackProvider& provider,
                                    DenseBlasMatrixView<double> matrix,
                                    DenseBlasVectorView<index_t> pivots,
                                    DenseBlasMatrixView<double> rhs,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);

/** @brief Queries single complex GETRF2 without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Rectangular row/column-major host matrix, unchanged.
 * @param pivots Contiguous min(m,n) one-based output, unchanged by query.
 * @return Checked exact conversion capacities or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrf2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Executes single complex GETRF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place rectangular packed L/U; padding is preserved.
 * @param pivots Contiguous min(m,n) signed one-based sequential row swaps.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion buffers; see file contract.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getrf2(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<std::complex<float>> matrix,
       DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex GETF2 without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Rectangular row/column-major host matrix, unchanged.
 * @param pivots Contiguous min(m,n) one-based output, unchanged by query.
 * @return Checked exact conversion capacities or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetf2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Executes single complex GETF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place rectangular packed L/U; padding is preserved.
 * @param pivots Contiguous min(m,n) signed one-based sequential row swaps.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion buffers; see file contract.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getf2(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex GETRI by a real nonmutating LWORK=-1 call.
 * @param provider Explicit checked reference provider.
 * @param factors Full live square row/column-major packed LU; unchanged.
 * @param pivots Valid immutable raw one-based LU pivots, size n; unchanged.
 * @param query_workspace Caller conversion storage with n kInteger entries.
 * @param report Mandatory query diagnostics; success leaves outputs unchanged.
 * @return Checked minimum/preferred execution plan or structural/provider
 * error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetriWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> factors, RawLapackPivotView pivots,
    const LapackWorkspace& query_workspace, LapackReport& report);
/** @brief Inverts a single complex raw LU without factoring or querying again.
 * @param provider Explicit checked reference provider.
 * @param factors In-place LU replaced by inverse on success; padding preserved.
 * @param pivots Valid immutable one-based LU pivots, size n.
 * @param plan Unmodified matching result from QueryGetriWorkspace.
 * @param workspace Disjoint live scalar WORK and integer conversion storage.
 * @param report Mandatory reset-before-preflight diagnostics; no LU result tag.
 * @return OK, structural failure, or kNumerical with unchanged singular LU.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getri(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> factors,
      RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries single complex GESV capacities without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Square row/column-major A, unchanged by query.
 * @param pivots Contiguous n-entry output, unchanged by query.
 * @param rhs Column-major n-by-nrhs B, disjoint and unchanged by query.
 * @return Checked exact conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Factors and solves a single complex system through actual GESV.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place square A/LU; factored even when nrhs is zero.
 * @param pivots Contiguous n-entry raw signed one-based sequential swaps.
 * @param rhs In-place B/X; unchanged if factorization is singular.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical with raw singular LU/pivots.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gesv(const ReferenceLapackProvider& provider,
     DenseBlasMatrixView<std::complex<float>> matrix,
     DenseBlasVectorView<index_t> pivots,
     DenseBlasMatrixView<std::complex<float>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

/** @brief Queries double complex GETRF2 without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Rectangular row/column-major host matrix, unchanged.
 * @param pivots Contiguous min(m,n) one-based output, unchanged by query.
 * @return Checked exact conversion capacities or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrf2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Executes double complex GETRF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place rectangular packed L/U; padding is preserved.
 * @param pivots Contiguous min(m,n) signed one-based sequential row swaps.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion buffers; see file contract.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getrf2(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<std::complex<double>> matrix,
       DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex GETF2 without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Rectangular row/column-major host matrix, unchanged.
 * @param pivots Contiguous min(m,n) one-based output, unchanged by query.
 * @return Checked exact conversion capacities or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetf2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Executes double complex GETF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place rectangular packed L/U; padding is preserved.
 * @param pivots Contiguous min(m,n) signed one-based sequential row swaps.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion buffers; see file contract.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getf2(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex GETRI by a real nonmutating LWORK=-1 call.
 * @param provider Explicit checked reference provider.
 * @param factors Full live square row/column-major packed LU; unchanged.
 * @param pivots Valid immutable raw one-based LU pivots, size n; unchanged.
 * @param query_workspace Caller conversion storage with n kInteger entries.
 * @param report Mandatory query diagnostics; success leaves outputs unchanged.
 * @return Checked minimum/preferred execution plan or structural/provider
 * error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetriWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots, const LapackWorkspace& query_workspace,
    LapackReport& report);
/** @brief Inverts a double complex raw LU without factoring or querying again.
 * @param provider Explicit checked reference provider.
 * @param factors In-place LU replaced by inverse on success; padding preserved.
 * @param pivots Valid immutable one-based LU pivots, size n.
 * @param plan Unmodified matching result from QueryGetriWorkspace.
 * @param workspace Disjoint live scalar WORK and integer conversion storage.
 * @param report Mandatory reset-before-preflight diagnostics; no LU result tag.
 * @return OK, structural failure, or kNumerical with unchanged singular LU.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getri(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> factors,
      RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double complex GESV capacities without a foreign call.
 * @param provider Explicit checked reference provider.
 * @param matrix Square row/column-major A, unchanged by query.
 * @param pivots Contiguous n-entry output, unchanged by query.
 * @param rhs Column-major n-by-nrhs B, disjoint and unchanged by query.
 * @return Checked exact conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Factors and solves a double complex system through actual GESV.
 * @param provider Explicit checked reference provider.
 * @param matrix In-place square A/LU; factored even when nrhs is zero.
 * @param pivots Contiguous n-entry raw signed one-based sequential swaps.
 * @param rhs In-place B/X; unchanged if factorization is singular.
 * @param plan Unmodified matching query result.
 * @param workspace Explicit disjoint conversion storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural failure, or kNumerical with raw singular LU/pivots.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gesv(const ReferenceLapackProvider& provider,
     DenseBlasMatrixView<std::complex<double>> matrix,
     DenseBlasVectorView<index_t> pivots,
     DenseBlasMatrixView<std::complex<double>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_H_
