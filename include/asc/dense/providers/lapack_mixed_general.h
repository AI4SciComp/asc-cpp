#ifndef ASC_DENSE_PROVIDERS_LAPACK_MIXED_GENERAL_H_
#define ASC_DENSE_PROVIDERS_LAPACK_MIXED_GENERAL_H_

/** @file
 * @brief Explicit pinned DSGESV/ZCGESV mixed-precision general solves.
 *
 * These two actual routines attempt single-precision LU and refine in double
 * precision, then use the provider's documented working-precision fallback
 * when indicated by ITER. This is an explicit mixed driver, not a new ASC
 * default or hidden alternate algorithm. B is immutable. A is unchanged on
 * mixed success and contains working-precision LU on fallback; singular
 * fallback leaves its documented completed singular factors. Pivots use
 * one-based interchange indices from the actual selected factorization.
 *
 * All A/B/X layouts are independent. Query inspects metadata only, never X's
 * old values. Caller workspace contains live working-precision kScalar and
 * kLayoutConversion objects, live lower-precision kScratch objects, underlying
 * double kReal objects for ZCGESV, and raw aligned kInteger bytes in which ASC
 * starts native INTEGER objects. No native LWORK query exists. A/B pack only
 * when row-major; X always stages explicitly and publishes only after valid
 * successful native diagnostics. No operation allocation or transfer occurs.
 *
 * Metadata/workspace validation precedes numeric writes or native entry.
 * Preflight preserves numeric operands, scratch and statistics; report resets
 * except when its metadata aliases. N=0 completes locally with absent INFO and
 * ITER and no numeric reads. N>0,NRHS=0 still enters the factorization path;
 * workspace accounts for its norm evaluation and inactive SX address.
 *
 * On provider defects, packed A, caller X and pivots are withheld; direct A
 * writes cannot be rolled back. On singular fallback, valid A/pivots publish
 * but X stays unchanged. Nonfinite computed X is published as a numerical
 * warning without clamping. Native INFO and ITER remain inspectable. A low
 * precision success does not certify A as LU factors. Returned working LU may
 * be reused through the documented general-LU raw-factor APIs, retaining the
 * matching pivots and provider identity; scratch low factors are not A.
 *
 * ITER is the provider stopping/fallback diagnostic, not a certificate of
 * forward accuracy or conditioning. No RCOND/FERR/BERR estimates are returned.
 * The pinned stopping comparison uses a working residual and scaled norm
 * threshold; extreme overflow/underflow may limit its numerical meaning.
 *
 * Costs are O(n^3) factorization plus O(n^2*nrhs) per refinement (at most 30),
 * with possible second working-precision factorization, and explicit
 * O(n^2+n*nrhs) scratch. Independent calls require separate mutable buffers,
 * statistics, reports and workspace. Immutable B and query plans may be
 * shared when their metadata match. The selected provider context must remain
 * valid. This facet retains the existing admitted platform/ABI restrictions.
 */

#include <complex>
#include <cstdint>
#include <optional>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {
/** @brief Meaning of the actual provider's negative ITER, when recognized. */
enum class LapackMixedFallback : std::uint8_t {
  kNone,  ///< ITER=0..30: the low factor and working refinement succeeded.
  kImplementation,    ///< ITER=-1: provider implementation/machine choice.
  kConversionRange,   ///< ITER=-2: narrowing overflowed; working factorization
                      ///< tried.
  kLowFactorization,  ///< ITER=-3: low factorization failed; working precision
                      ///< tried.
  kIterationLimit     ///< ITER=-31: 30 refinements failed the stopping rule.
};
/** @brief Caller-owned mixed-driver diagnostics, separate from native INFO.
 *
 * Values survive non-OK native outcomes. Preflight leaves old statistics
 * unchanged; local N=0 replaces them with empty diagnostics. An unrecognized
 * or unwritten ITER is a provider defect and has no interpreted fallback.
 */
struct LapackMixedSolveStatistics {
  std::optional<std::int64_t> native_iteration;  ///< Exact post-call ITER bits.
  std::optional<LapackMixedFallback> fallback;   ///< Recognized ITER meaning.
  std::optional<LapackScalarKind>
      factor_scalar;  ///< Selected factor precision,
  ///< present after validated successful or singular factor publication.
};

/** @brief Queries checked explicit workspace for the actual real mixed driver.
 * @param provider Admitted pinned provider with actual integer ABI identity.
 * @param a Mutable square double A; entries are unread by this query.
 * @param pivots Output length-n contiguous ASC indices, unread by query.
 * @param b Immutable n-by-nrhs input RHS, unread by query.
 * @param x Output n-by-nrhs solution; old values never become inputs.
 * @return Metadata-bound plan or structural error, without allocation/entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryDsgesvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> a,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<const double> b,
    DenseBlasMatrixView<double> x);
/** @brief Executes the explicit DSGESV refinement/fallback contract above.
 * @param provider Same provider/build/ABI bound by the query.
 * @param a Original A, overwritten only by the native working fallback.
 * @param pivots Published one-based native interchanges after validation.
 * @param b Immutable original RHS, disjoint from every mutable operand.
 * @param x Staged solution, published only on INFO=0 and valid ITER/pivots.
 * @param plan Unmodified matching metadata query result.
 * @param workspace Live typed scratch and raw native-integer storage above.
 * @param statistics Retained ITER, fallback reason and selected factor type.
 * @param report Retained exact INFO and qualified numerical outcome.
 * @return OK, preflight/provider failure, singularity or accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Dsgesv(const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> a,
       DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<const double> b,
       DenseBlasMatrixView<double> x, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackMixedSolveStatistics& statistics,
       LapackReport& report);
/** @brief Queries checked workspace for the actual complex mixed driver.
 * @param provider Admitted pinned provider with actual integer ABI identity.
 * @param a Mutable square complex-double A; query reads only metadata.
 * @param pivots Output length-n contiguous ASC indices, unread by query.
 * @param b Immutable n-by-nrhs complex-double RHS, unread by query.
 * @param x Output n-by-nrhs solution, with no old-value input dependence.
 * @return Plan with complex-float lower workspace and double real norm work.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryZcgesvWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> a,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> b,
    DenseBlasMatrixView<std::complex<double>> x);
/** @brief Executes the explicit ZCGESV refinement/fallback contract above.
 * @param provider Same provider/build/ABI bound by the query.
 * @param a Original A, overwritten only by the native working fallback.
 * @param pivots Published one-based native interchanges after validation.
 * @param b Immutable original RHS, disjoint from every mutable operand.
 * @param x Staged solution, published only on INFO=0 and valid ITER/pivots.
 * @param plan Unmodified matching metadata query result.
 * @param workspace Live typed scratch and raw native-integer storage above.
 * @param statistics Retained ITER, fallback reason and selected factor type.
 * @param report Retained exact INFO and qualified numerical outcome.
 * @return OK, preflight/provider failure, singularity or accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Zcgesv(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<std::complex<double>> a,
       DenseBlasVectorView<index_t> pivots,
       DenseBlasMatrixView<const std::complex<double>> b,
       DenseBlasMatrixView<std::complex<double>> x,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackMixedSolveStatistics& statistics, LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_MIXED_GENERAL_H_
