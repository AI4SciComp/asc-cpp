#ifndef ASC_DENSE_PROVIDERS_LAPACK_DMD_QR_H_
#define ASC_DENSE_PROVIDERS_LAPACK_DMD_QR_H_
/** @file
 * @brief Explicit pinned GEDMDQ analysis of a single snapshot trajectory.
 *
 * For m>=n trajectory snapshots F, the routine compresses the leading/trailing
 * n-1 pairs through a QR factorization, then performs the explicitly selected
 * GEDMD algorithm. It does not construct an unknown full operator.
 * Interpretation as dynamics of A requires F(:,j+1)=A*F(:,j). Preserve original
 * snapshots for independent checks. Normalized-range column norms are provider
 * assumptions; finite input alone is not a universal accuracy/convergence
 * certificate.
 *
 * All buffers have independent padded row/column layouts and disjoint live
 * storage. Queries inspect metadata only, without native calls. Plans bind
 * provider/scalar, all dimensions/strides/layouts and exact option/tolerance
 * bits. Execution packs F and stages every numeric output in caller-owned live
 * typed workspace. There is no implicit allocation, transfer, synchronization,
 * provider selection, mode substitution or fallback.
 *
 * Successful publication returns F as explicit Q if requested, otherwise native
 * QR storage: upper R plus strict-lower reflector vectors. Reflector
 * coefficients are returned separately; they can reconstruct Q from that packed
 * QR storage. X publishes only leading k compressed POD columns. Y publishes
 * the complete n-by-n triangular R only when requested; otherwise caller Y
 * stays unchanged. Explicit modes publish m-by-k Z. POD-factored modes publish
 * m-by-k Z and k-by-k W, representing Z*W. QR-factored modes publish only
 * n-by-k Z, representing Q*Z using explicit Q or the retained QR reflectors.
 * Real conjugate pairs retain the GEDMD paired-column convention. B contains
 * compressed refinement/exact data; lift with Q to interpret A*U or A*U*W. S is
 * raw overwritten eigensolver state, not a Rayleigh quotient certificate.
 * Singular values describe the source-selected scaled compressed snapshots. All
 * inactive cells/tails/padding remain unchanged. Residual estimates refer to
 * that selected snapshot model.
 *
 * The pinned single-real SGEDMDQ omits the QR-factored vector request in its
 * nested call. That mode has a retained failing mathematical gate despite
 * INFO0; its raw output must not be treated as validated mode vectors. Other
 * scalar routes and the separately named GEDMD capability retain their own
 * evidence. This adapter does not silently repair or replace that provider
 * algorithm.
 *
 * INFO0 publishes outputs and raw native success, which is not a mathematical
 * acceptance certificate. INFO4 publishes warning results with kAccuracyWarning
 * and non-OK numerical Status. INFO2/3 preserves all caller numeric output and
 * rank, reporting nonconvergence. Missing/invalid INFO or rank preserves output
 * and reports a provider defect. Nonfinite F or allzero leading n-1 snapshots
 * rejects before native entry. N0/1 completes locally with rank0, absent INFO
 * and every numeric array unchanged. Independent contexts, buffers, workspace,
 * rank and reports are reentrant; immutable matching plans may be reused.
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
#include "asc/dense/providers/lapack_dmd.h"
#include "asc/dense/providers/lapack_export.h"
namespace asc {
/** @brief Explicit GEDMDQ mode-vector representation; no implicit default. */
enum class LapackDmdQrVectors : std::uint8_t {
  kNone = 'N',         ///< No mode-vector publication.
  kExplicit = 'V',     ///< Full vectors in Z; permits residual estimates.
  kPodFactored = 'F',  ///< Full vectors represented by Z*W.
  kQrFactored =
      'Q',  ///< Vectors represented by Q*Z; pinned S mode is defective.
};
/** @brief Explicit QR-compressed snapshot modes and rank policy. */
struct LapackDmdQrOptions {
  LapackDmdScaling scaling;    ///< Explicit nested snapshot scaling.
  LapackDmdQrVectors vectors;  ///< Requested vector representation.
  LapackDmdExtra extra;        ///< Optional compressed refinement/exact data.
  LapackDmdSvd svd;            ///< Explicit pinned SVD choice; no substitution.
  bool residuals;   ///< Requires explicit vectors; publish residual estimates.
  bool orthogonal;  ///< Return explicit Q in F, otherwise native packed QR.
  bool triangular;  ///< Return R in Y, otherwise preserve caller Y.
  index_t rank_selection;  ///< -1, -2 or maximum1..n-1; none exist for n<=1.
};
/** @brief Disjoint borrowed GEDMDQ buffers; all capacities required.
 * @tparam T One of float, double, complex<float>, complex<double>.
 * Here p=max(n-1,0); all vector views have unit increment.
 */
template <DenseBlasScalar T>
struct LapackDmdQrBuffers {
  DenseBlasMatrixView<T> trajectory;  ///< m-by-n F; input trajectory/output QR.
  DenseBlasMatrixView<T> compressed_snapshots;  ///< n-by-p X; leading k POD.
  DenseBlasMatrixView<T> triangular_factor;     ///< n-by-n Y; requested R only.
  DenseBlasMatrixView<T>
      modes;  ///< m-by-p Z; mode-dependent leading rows/columns.
  DenseBlasMatrixView<T> extra;  ///< n-by-p B; compressed requested extra data.
  DenseBlasMatrixView<T> reduced_modes;  ///< p-by-p W; reduced eigenvectors.
  DenseBlasMatrixView<T> reduced_workspace;  ///< p-by-p S; overwritten state.
  DenseBlasVectorView<std::complex<DenseBlasRealType<T>>> eigenvalues;
  ///< p entries; publish leading k complex eigenvalues without reordering.
  DenseBlasVectorView<DenseBlasRealType<T>> singular_values;
  ///< p singular values of the scaled compressed snapshots.
  DenseBlasVectorView<DenseBlasRealType<T>> residual_norms;
  ///< p entries; publish leading k requested residual estimates.
  DenseBlasVectorView<T> reflector_coefficients;
  ///< n native QR coefficients, paired with packed F when orthogonal=false.
};
/** @brief Queries single-real GEDMDQ resources without numerical reads.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param options Explicit scaling, vectors, SVD, QR output and rank choices.
 * @param buffers Borrowed full capacities described by the file-level contract.
 * @param tolerance Finite cutoff in [0,1); exact bits bind the plan.
 * @return Checked minimum/preferred plan or structural failure; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGedmdqWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdQrOptions options,
    LapackDmdQrBuffers<float> buffers, float tolerance);
/** @brief Executes single-real GEDMDQ with staged rank-bounded publication.
 * @param provider Same explicit provider as the unmodified plan.
 * @param options Exact query choices, including explicit vector representation.
 * @param buffers Disjoint borrowed storage following the file-level contract.
 * @param tolerance Exact finite query cutoff; not an accuracy guarantee.
 * @param rank Caller ASC64 output, published only on INFO0/4 or local void
 * input.
 * @param plan Immutable matching query result; reusable with independent data.
 * @param workspace Disjoint live typed native and staging roles; no allocation.
 * @param report Mandatory raw INFO and output validity; aliases reject before
 * reset.
 * @return OK, numerical warning/nonconvergence, structural or provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status Gedmdq(const ReferenceLapackProvider& provider,
                                      LapackDmdQrOptions options,
                                      LapackDmdQrBuffers<float> buffers,
                                      float tolerance, index_t& rank,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);
/** @brief Queries double-real GEDMDQ resources without numerical reads.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param options Explicit scaling, vectors, SVD, QR output and rank choices.
 * @param buffers Borrowed full capacities described by the file-level contract.
 * @param tolerance Finite cutoff in [0,1); exact bits bind the plan.
 * @return Checked minimum/preferred plan or structural failure; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGedmdqWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdQrOptions options,
    LapackDmdQrBuffers<double> buffers, double tolerance);
/** @brief Executes double-real GEDMDQ with staged rank-bounded publication.
 * @param provider Same explicit provider as the unmodified plan.
 * @param options Exact query choices, including explicit vector representation.
 * @param buffers Disjoint borrowed storage following the file-level contract.
 * @param tolerance Exact finite query cutoff; not an accuracy guarantee.
 * @param rank Caller ASC64 output, published only on INFO0/4 or local void
 * input.
 * @param plan Immutable matching query result; reusable with independent data.
 * @param workspace Disjoint live typed native and staging roles; no allocation.
 * @param report Mandatory raw INFO and output validity; aliases reject before
 * reset.
 * @return OK, numerical warning/nonconvergence, structural or provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status Gedmdq(const ReferenceLapackProvider& provider,
                                      LapackDmdQrOptions options,
                                      LapackDmdQrBuffers<double> buffers,
                                      double tolerance, index_t& rank,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);
/** @brief Queries single-complex GEDMDQ resources without numerical reads.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param options Explicit scaling, vectors, SVD, QR output and rank choices.
 * @param buffers Borrowed full capacities described by the file-level contract.
 * @param tolerance Finite cutoff in [0,1); exact bits bind the plan.
 * @return Checked minimum/preferred plan or structural failure; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGedmdqWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdQrOptions options,
    LapackDmdQrBuffers<std::complex<float>> buffers, float tolerance);
/** @brief Executes single-complex GEDMDQ with staged rank-bounded publication.
 * @param provider Same explicit provider as the unmodified plan.
 * @param options Exact query choices, including explicit vector representation.
 * @param buffers Disjoint borrowed storage following the file-level contract.
 * @param tolerance Exact finite query cutoff; not an accuracy guarantee.
 * @param rank Caller ASC64 output, published only on INFO0/4 or local void
 * input.
 * @param plan Immutable matching query result; reusable with independent data.
 * @param workspace Disjoint live typed native and staging roles; no allocation.
 * @param report Mandatory raw INFO and output validity; aliases reject before
 * reset.
 * @return OK, numerical warning/nonconvergence, structural or provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gedmdq(const ReferenceLapackProvider& provider, LapackDmdQrOptions options,
       LapackDmdQrBuffers<std::complex<float>> buffers, float tolerance,
       index_t& rank, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double-complex GEDMDQ resources without numerical reads.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param options Explicit scaling, vectors, SVD, QR output and rank choices.
 * @param buffers Borrowed full capacities described by the file-level contract.
 * @param tolerance Finite cutoff in [0,1); exact bits bind the plan.
 * @return Checked minimum/preferred plan or structural failure; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGedmdqWorkspace(
    const ReferenceLapackProvider& provider, LapackDmdQrOptions options,
    LapackDmdQrBuffers<std::complex<double>> buffers, double tolerance);
/** @brief Executes double-complex GEDMDQ with staged rank-bounded publication.
 * @param provider Same explicit provider as the unmodified plan.
 * @param options Exact query choices, including explicit vector representation.
 * @param buffers Disjoint borrowed storage following the file-level contract.
 * @param tolerance Exact finite query cutoff; not an accuracy guarantee.
 * @param rank Caller ASC64 output, published only on INFO0/4 or local void
 * input.
 * @param plan Immutable matching query result; reusable with independent data.
 * @param workspace Disjoint live typed native and staging roles; no allocation.
 * @param report Mandatory raw INFO and output validity; aliases reject before
 * reset.
 * @return OK, numerical warning/nonconvergence, structural or provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gedmdq(const ReferenceLapackProvider& provider, LapackDmdQrOptions options,
       LapackDmdQrBuffers<std::complex<double>> buffers, double tolerance,
       index_t& rank, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_DMD_QR_H_
