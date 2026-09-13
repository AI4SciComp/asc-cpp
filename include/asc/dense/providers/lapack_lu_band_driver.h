#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_DRIVER_H_
/** @file
 * @brief Exact Reference-LAPACK general-band solve drivers.
 *
 * GBSV factors expanded square band storage in place, then overwrites B with
 * X for A*X=B. The physical factor diagonal is KL+KU and LD includes all
 * fill-in rows. All nonnegative bandwidths and empty sizes are accepted
 * within checked source-integer arithmetic. B independently uses either
 * layout; row-major conversion occupies caller storage. Signed sequential
 * one-based band swaps are preserved, never interpreted as a final permutation.
 *
 * Full backing, pivots, B, metadata and nonempty scratch must be live, disjoint
 * and host/pinned-host accessible to the explicit provider. Every detected
 * metadata alias leaves report unchanged; otherwise it resets before remaining
 * preflight. Structural failure changes no numerical/workspace byte and makes
 * no native call. Queries are metadata-only formulas. Plans bind routine,
 * scalar, shape, bands, original/foreign strides, B layout and provider.
 *
 * Native-width integer scratch contains n output pivots. Scalar conversion
 * objects are caller-owned and live; native integer lifetimes begin only
 * after preflight and are initialized to an invalid full-width sentinel.
 * There is no allocation, densification, hidden workspace, transfer, fallback
 * or handler change. Actual calls include n=0/nrhs=0; zero RHS count does not
 * suppress factorization. INFO in [1,n] retains raw factors/pivots with
 * documented-partial validity, kNumerical/kSingular and zero-based index; B
 * stays unchanged. Negative/impossible/unwritten INFO or malformed native
 * pivots are unusable provider defects. No successful factor is certified.
 */
#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
namespace asc {
/** @brief Queries single real GBSV caller capacities without mutation.
 * @param provider Explicit serial reference provider.
 * @param matrix Square expanded band A, overwritten with raw LU.
 * @param pivots Exact n-entry signed raw band swap output.
 * @param rhs Independent-layout n-by-nrhs B, overwritten by X on success.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvWorkspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<float> matrix,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<float> rhs);
/** @brief Executes actual single real band LU factor-and-solve.
 * @param provider Explicit serial reference provider.
 * @param matrix Square expanded band A, overwritten with raw LU.
 * @param pivots Exact n-entry signed raw band swap output.
 * @param rhs Independent-layout n-by-nrhs B, overwritten by X on success.
 * @param plan Unmodified matching metadata formula plan.
 * @param workspace Exact-capacity native pivot and optional B conversion
 * storage.
 * @param report Mandatory disjoint raw INFO and partial-output outcome.
 * @return OK, structural rejection, exact-zero-U numerical failure or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Gbsv(const ReferenceLapackProvider& provider,
                                    LapackLuBandView<float> matrix,
                                    DenseBlasVectorView<index_t> pivots,
                                    DenseBlasMatrixView<float> rhs,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);
/** @brief Queries double real GBSV caller capacities without mutation.
 * @param provider Explicit serial reference provider.
 * @param matrix Square expanded band A, overwritten with raw LU.
 * @param pivots Exact n-entry signed raw band swap output.
 * @param rhs Independent-layout n-by-nrhs B, overwritten by X on success.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvWorkspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<double> matrix,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<double> rhs);
/** @brief Executes actual double real band LU factor-and-solve.
 * @param provider Explicit serial reference provider.
 * @param matrix Square expanded band A, overwritten with raw LU.
 * @param pivots Exact n-entry signed raw band swap output.
 * @param rhs Independent-layout n-by-nrhs B, overwritten by X on success.
 * @param plan Unmodified matching metadata formula plan.
 * @param workspace Exact-capacity native pivot and optional B conversion
 * storage.
 * @param report Mandatory disjoint raw INFO and partial-output outcome.
 * @return OK, structural rejection, exact-zero-U numerical failure or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Gbsv(const ReferenceLapackProvider& provider,
                                    LapackLuBandView<double> matrix,
                                    DenseBlasVectorView<index_t> pivots,
                                    DenseBlasMatrixView<double> rhs,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);
/** @brief Queries single complex GBSV caller capacities without mutation.
 * @param provider Explicit serial reference provider.
 * @param matrix Square expanded band A, overwritten with raw LU.
 * @param pivots Exact n-entry signed raw band swap output.
 * @param rhs Independent-layout n-by-nrhs B, overwritten by X on success.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes actual single complex band LU factor-and-solve.
 * @param provider Explicit serial reference provider.
 * @param matrix Square expanded band A, overwritten with raw LU.
 * @param pivots Exact n-entry signed raw band swap output.
 * @param rhs Independent-layout n-by-nrhs B, overwritten by X on success.
 * @param plan Unmodified matching metadata formula plan.
 * @param workspace Exact-capacity native pivot and optional B conversion
 * storage.
 * @param report Mandatory disjoint raw INFO and partial-output outcome.
 * @return OK, structural rejection, exact-zero-U numerical failure or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbsv(const ReferenceLapackProvider& provider,
     LapackLuBandView<std::complex<float>> matrix,
     DenseBlasVectorView<index_t> pivots,
     DenseBlasMatrixView<std::complex<float>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);
/** @brief Queries double complex GBSV caller capacities without mutation.
 * @param provider Explicit serial reference provider.
 * @param matrix Square expanded band A, overwritten with raw LU.
 * @param pivots Exact n-entry signed raw band swap output.
 * @param rhs Independent-layout n-by-nrhs B, overwritten by X on success.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes actual double complex band LU factor-and-solve.
 * @param provider Explicit serial reference provider.
 * @param matrix Square expanded band A, overwritten with raw LU.
 * @param pivots Exact n-entry signed raw band swap output.
 * @param rhs Independent-layout n-by-nrhs B, overwritten by X on success.
 * @param plan Unmodified matching metadata formula plan.
 * @param workspace Exact-capacity native pivot and optional B conversion
 * storage.
 * @param report Mandatory disjoint raw INFO and partial-output outcome.
 * @return OK, structural rejection, exact-zero-U numerical failure or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbsv(const ReferenceLapackProvider& provider,
     LapackLuBandView<std::complex<double>> matrix,
     DenseBlasVectorView<index_t> pivots,
     DenseBlasMatrixView<std::complex<double>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_DRIVER_H_
