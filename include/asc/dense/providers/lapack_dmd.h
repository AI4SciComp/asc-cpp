#ifndef ASC_DENSE_PROVIDERS_LAPACK_DMD_H_
#define ASC_DENSE_PROVIDERS_LAPACK_DMD_H_

/** @file
 * @brief Explicit pinned GEDMD snapshot analysis with caller-owned storage.
 *
 * Given snapshot pairs X,Y with m>=n, GEDMD computes a rank-selected POD basis
 * U and Ritz pairs of U^H*Y*V*Sigma^{-1}. It does not construct an unknown
 * full operator A. Interpreting the result as approximating A requires Y=A*X;
 * residuals are evaluated through the selected snapshot model.
 *
 * Each matrix has its own row/column layout and padding. All numerical
 * operands and workspace roles must be disjoint. Queries inspect metadata
 * only and evaluate source-pinned counts without a native call. Every plan
 * binds all dimensions, layouts, leading dimensions, options and tolerance
 * bits, and the explicit provider identity. No allocation, implicit transfer,
 * synchronization, provider selection or fallback occurs.
 *
 * Execution stages X,Y and every output in explicit live typed caller
 * workspace. Finite X,Y and nonzero X are required for nonempty execution;
 * rejected numerical input leaves all outputs unchanged with absent INFO.
 * The provider assumes normalized-range snapshot column norms; finite input
 * alone does not certify finite output or convergence at arbitrary extremes.
 * No input is silently rescaled beyond the explicitly selected provider mode.
 *
 * INFO=0 publishes the documented outputs. INFO=4 publishes the same outputs
 * with kAccuracyWarning and non-OK numerical Status because zero snapshot
 * columns were inconsistent with their paired columns. INFO=2/3 means SVD/
 * eigenvalue nonconvergence: caller outputs and rank remain unchanged. Other
 * INFO values on a nonempty checked call are provider defects and preserve
 * caller outputs. Raw INFO survives every returned native call. Empty n=0
 * completes locally with rank zero, absent INFO and unchanged arrays.
 *
 * Only leading rank columns of X are published as the POD basis; its tail is
 * unchanged. Real mode vectors retain the provider's paired-column convention:
 * consecutive positive/negative imaginary eigenvalues correspond to z_i+i*z_j
 * and z_i-i*z_j, with joint Frobenius norm one. Complex vectors have norm one.
 * Factored modes use X(:,1:k)*W(1:k,1:k). Eigenvalues are exposed as complex
 * values in both real and complex APIs, without reordering or normalization.
 *
 * Y publishes leading k residual columns when requested, otherwise all scaled
 * successor snapshots. Explicit Z, additional B, eigenvalues and residual
 * estimates publish only their defined leading k entries/columns. W publishes
 * k-by-k reduced eigenvectors only when vectors or exact modes are requested.
 * S publishes the raw k-by-k overwritten eigensolver state, not an intact
 * Rayleigh quotient. Singular values have n entries and describe the provider's
 * scaled snapshot matrix, not necessarily original X. All other cells and
 * padding remain unchanged. Preserve original snapshots for independent checks.
 *
 * CPU-only and reentrant with independent live buffers, workspaces, rank and
 * reports. An immutable plan may be reused for matching independent storage.
 * Foreign provider code is subject to its separately recorded platform scope.
 */

#include <complex>
#include <cstdint>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Explicit provider snapshot scaling; no mode is chosen implicitly. */
enum class LapackDmdScaling : std::uint8_t {
  kNone = 'N',       ///< Preserve input scales.
  kSnapshots = 'S',  ///< Normalize X columns and apply their scales to Y.
  kConsistentSnapshots =
      'C',            ///< Also zero Y paired with zero X; report warning.
  kSuccessors = 'Y',  ///< Normalize Y columns and apply their scales to X.
};
/** @brief How to publish Ritz vectors. */
enum class LapackDmdVectors : std::uint8_t {
  kNone = 'N',      ///< Do not publish mode vectors.
  kExplicit = 'V',  ///< Publish vectors in Z, with real conjugate pairs.
  kFactored = 'F',  ///< Publish the factors X(:,1:k) and W(1:k,1:k).
};
/** @brief Optional additional output from the selected snapshot model. */
enum class LapackDmdExtra : std::uint8_t {
  kNone = 'N',        ///< Leave B unchanged.
  kRefinement = 'R',  ///< Publish A*U in B when Y=A*X is consistent.
  kExact = 'E',  ///< Publish A*U*W in B; inverse eigenvalues are not applied.
};
/** @brief Distinct pinned SVD algorithms; no implicit substitution. */
enum class LapackDmdSvd : std::uint8_t {
  kBidiagonalQr = 1,      ///< GESVD bidiagonal QR iteration.
  kDivideAndConquer = 2,  ///< GESDD divide-and-conquer iteration.
  kQrPreconditioned = 3,  ///< GESVDQ with H,P,N,R,R options.
  kJacobi = 4,  ///< GEJSV with the exact scalar-specific GEDMD options.
};
/** @brief Explicit GEDMD modes and rank selector, validated by each query. */
struct LapackDmdOptions {
  LapackDmdScaling scaling;  ///< Requested provider column scaling.
  LapackDmdVectors vectors;  ///< Explicit, factored or no vector output.
  LapackDmdExtra extra;      ///< Optional refinement or exact-mode columns.
  LapackDmdSvd svd;        ///< Explicit SVD algorithm; no default or fallback.
  bool residuals;          ///< Requires explicit vectors; publish residual
                           ///< columns/norms.
  index_t rank_selection;  ///< -1 relative, -2 consecutive gap, or maximum1..n.
};
/** @brief Borrowed mutable GEDMD buffers; no allocation or ownership transfer.
 * @tparam T One of float, double, complex<float>, complex<double>.
 * All six matrix capacities are required even when only used internally by a
 * particular mode. All three vectors have exactly n contiguous entries.
 */
template <DenseBlasScalar T>
struct LapackDmdBuffers {
  DenseBlasMatrixView<T>
      snapshots;  ///< m-by-n X; publish leading k POD columns.
  DenseBlasMatrixView<T> successors;  ///< m-by-n Y; scaled data or residuals.
  DenseBlasMatrixView<T> modes;  ///< m-by-n Z; publish only explicit modes.
  DenseBlasMatrixView<T>
      extra;  ///< m-by-n B; publish only requested extra modes.
  DenseBlasMatrixView<T> reduced_modes;  ///< n-by-n W; reduced eigenvectors.
  DenseBlasMatrixView<T>
      reduced_workspace;  ///< n-by-n S; raw overwritten state.
  DenseBlasVectorView<std::complex<DenseBlasRealType<T>>> eigenvalues;
  ///< n-entry complex eigenvalues; only the leading k values are published.
  DenseBlasVectorView<DenseBlasRealType<T>> singular_values;
  ///< n-entry singular values of the source-selected scaled snapshots.
  DenseBlasVectorView<DenseBlasRealType<T>> residual_norms;
  ///< n-entry residual estimates; only requested leading k values are
  ///< published.
};

/** @brief Queries checked single-real GEDMD workspace without numerical reads.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param options Explicit scaling, vectors, SVD, residual and rank modes.
 * @param buffers Borrowed exact matrix/vector capacities; all values unchanged.
 * @param tolerance Finite rank cutoff in [0,1); exact bits bind the plan.
 * @return Checked minimum/preferred resource plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGedmdWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdOptions options,
    LapackDmdBuffers<float> buffers, float tolerance);

/** @brief Executes single-real GEDMD with staged, bounded publication.
 * @param provider Same pinned CPU provider as the unmodified plan.
 * @param options Exact options matching the plan.
 * @param buffers Borrowed operands following the file-level mutation contract.
 * @param tolerance Exact finite cutoff matching the plan.
 * @param rank Caller ASC64 output; published only for INFO0/4 or local empty.
 * @param plan Unmodified matching query result, reusable with independent data.
 * @param workspace Disjoint live typed WORK, real WORK, integer and staging
 * roles.
 * @param report Mandatory raw INFO and output validity; aliasing rejects before
 * reset.
 * @return OK, numerical warning/nonconvergence, structural or provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status Gedmd(const ReferenceLapackProvider& provider,
                                     LapackDmdOptions options,
                                     LapackDmdBuffers<float> buffers,
                                     float tolerance, index_t& rank,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries checked double-real GEDMD workspace without numerical reads.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param options Explicit scaling, vectors, SVD, residual and rank modes.
 * @param buffers Borrowed exact matrix/vector capacities; all values unchanged.
 * @param tolerance Finite rank cutoff in [0,1); exact bits bind the plan.
 * @return Checked minimum/preferred resource plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGedmdWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdOptions options,
    LapackDmdBuffers<double> buffers, double tolerance);

/** @brief Executes double-real GEDMD with staged, bounded publication.
 * @param provider Same pinned CPU provider as the unmodified plan.
 * @param options Exact options matching the plan.
 * @param buffers Borrowed operands following the file-level mutation contract.
 * @param tolerance Exact finite cutoff matching the plan.
 * @param rank Caller ASC64 output; published only for INFO0/4 or local empty.
 * @param plan Unmodified matching query result, reusable with independent data.
 * @param workspace Disjoint live typed WORK, real WORK, integer and staging
 * roles.
 * @param report Mandatory raw INFO and output validity; aliasing rejects before
 * reset.
 * @return OK, numerical warning/nonconvergence, structural or provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status Gedmd(const ReferenceLapackProvider& provider,
                                     LapackDmdOptions options,
                                     LapackDmdBuffers<double> buffers,
                                     double tolerance, index_t& rank,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries checked single-complex GEDMD workspace without numerical
 * reads.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param options Explicit scaling, vectors, SVD, residual and rank modes.
 * @param buffers Borrowed exact matrix/vector capacities; all values unchanged.
 * @param tolerance Finite rank cutoff in [0,1); exact bits bind the plan.
 * @return Checked minimum/preferred resource plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGedmdWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdOptions options,
    LapackDmdBuffers<std::complex<float>> buffers, float tolerance);

/** @brief Executes single-complex GEDMD with staged, bounded publication.
 * @param provider Same pinned CPU provider as the unmodified plan.
 * @param options Exact options matching the plan.
 * @param buffers Borrowed operands following the file-level mutation contract.
 * @param tolerance Exact finite cutoff matching the plan.
 * @param rank Caller ASC64 output; published only for INFO0/4 or local empty.
 * @param plan Unmodified matching query result, reusable with independent data.
 * @param workspace Disjoint live typed WORK, real WORK, integer and staging
 * roles.
 * @param report Mandatory raw INFO and output validity; aliasing rejects before
 * reset.
 * @return OK, numerical warning/nonconvergence, structural or provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gedmd(const ReferenceLapackProvider& provider, LapackDmdOptions options,
      LapackDmdBuffers<std::complex<float>> buffers, float tolerance,
      index_t& rank, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked double-complex GEDMD workspace without numerical
 * reads.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param options Explicit scaling, vectors, SVD, residual and rank modes.
 * @param buffers Borrowed exact matrix/vector capacities; all values unchanged.
 * @param tolerance Finite rank cutoff in [0,1); exact bits bind the plan.
 * @return Checked minimum/preferred resource plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGedmdWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdOptions options,
    LapackDmdBuffers<std::complex<double>> buffers, double tolerance);

/** @brief Executes double-complex GEDMD with staged, bounded publication.
 * @param provider Same pinned CPU provider as the unmodified plan.
 * @param options Exact options matching the plan.
 * @param buffers Borrowed operands following the file-level mutation contract.
 * @param tolerance Exact finite cutoff matching the plan.
 * @param rank Caller ASC64 output; published only for INFO0/4 or local empty.
 * @param plan Unmodified matching query result, reusable with independent data.
 * @param workspace Disjoint live typed WORK, real WORK, integer and staging
 * roles.
 * @param report Mandatory raw INFO and output validity; aliasing rejects before
 * reset.
 * @return OK, numerical warning/nonconvergence, structural or provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gedmd(const ReferenceLapackProvider& provider, LapackDmdOptions options,
      LapackDmdBuffers<std::complex<double>> buffers, double tolerance,
      index_t& rank, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_DMD_H_
