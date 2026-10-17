#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_BLOCK_INVERSE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_BLOCK_INVERSE_H_

/** @file
 * @brief Explicit in-place blocked symmetric and Hermitian inversion.
 *
 * SYTRI2/SYTRI2X invert transpose-symmetric factors, including complex
 * symmetric factors. HETRI2/HETRI2X invert conjugate-transpose Hermitian
 * factors. The caller supplies mutable selected square factor storage and
 * immutable raw kBunchKaufman pivots from one same-provider, same-scalar
 * SYTRF/SYTF2 or HETRF/HETF2 operation (including the corresponding direct
 * driver). Adjacent equal negative entries encode one ordered interchange for a
 * 2-by-2 block. Rook pivots are not interchangeable with classic pivots.
 * Provenance and numerical content are caller preconditions, not inferred
 * certificates. Completed singular factors are accepted for native INFO.
 * Active calls can overwrite factors even on singular results, invalidating
 * borrowed views of those buffers.
 *
 * Only the selected triangle is overwritten by the inverse. Other entries and
 * padding, and every public pivot entry, remain unchanged. Hermitian factor
 * packing retains complete raw diagonal and block coefficients: the native
 * singular check compares a complete complex diagonal with zero before the
 * inversion arithmetic uses its real part. No original-input normalization is
 * applied to factor storage. No full symmetric/Hermitian output is
 * materialized.
 *
 * Active TRI2/TRI2X calls use (n+nb+1)*(nb+3) live T scalar WORK entries.
 * TRI2 uses the pinned ILAENV choices: SSYTRI2 and C/Z HETRI2 nb=64;
 * D/C/Z SYTRI2 nb=1. The formula honors documented workspace even when the
 * native query accepts less for small orders. TRI2X takes explicit nb>0;
 * zero is rejected because the source would not advance its block loop.
 * TRI2X has no LWORK/query argument. Compute WORK has no returned-value
 * contract. n provider-width input INTEGER pivots occupy caller byte storage;
 * row-major A also uses n*n live T layout entries. Metadata-only queries read
 * no numerical arrays and make no foreign call. Plans bind the exact variant,
 * block size, scalar/provider ABI, triangle, symmetry, order and layout/LDA.
 * N=0 succeeds without workspace, numerical access or a foreign call.
 * Workspace dimensions, byte products, nested source loops and BLAS cursors
 * are checked before access; TRI2 additionally checks the native LWORK product.
 *
 * All numerical operands, scratch and live metadata must be disjoint and
 * accessible to the explicit CPU context. Structural errors preserve numerical
 * buffers and scratch. Metadata/report alias rejection also preserves the
 * report; otherwise the report is reset with no INFO and called_provider=false
 * before subsequent preflight. Independent concurrent calls need disjoint
 * writable buffers/reports. Operations allocate nothing, transfer nothing,
 * change no global state and use no fallback algorithm or provider.
 *
 * Native INFO>0 reports the first exactly zero 1-by-1 D entry in the source's
 * scan order (upper: n to 1; lower: 1 to n). TRI2X first converts the factors
 * using SYCONV(C), so A and WORK may already have changed on positive INFO. Its
 * selected partial output is published in both layouts and must not be reused
 * as original factors. The TRI2 small-order classic branch preserves A on
 * positive INFO. No inverse exists in either singular output. The report
 * retains the one-based INFO and a zero-based diagnostic index, with
 * kSingular/kDocumentedPartial. Native INFO=0 records completion, without a
 * finiteness or conditioning guarantee. The source does not diagnose singular
 * 2-by-2 blocks or scale all intermediate inverse arithmetic. Numerical
 * acceptance is separate from native fidelity.
 *
 * Full-width INFO starts at INTEGER minimum after preflight. An unwritten,
 * partial-width, negative or source-inconsistent INFO is a provider defect,
 * as is unexpected mutation of converted input pivots. Raw INFO is retained
 * with unusable output. Packed output is withheld; direct
 * column-major factor storage may already have changed. WORK has no documented
 * returned value and is not a query/result channel. No RCOND/FERR/BERR exist.
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

/** @brief Queries single real Sytri2 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> factors, RawLapackPivotView pivots);

/** @brief Executes native single real Sytri2 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Sytri2(const ReferenceLapackProvider& provider,
                                      DenseBlasTriangle triangle,
                                      DenseBlasMatrixView<float> factors,
                                      RawLapackPivotView pivots,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);

/** @brief Queries double real Sytri2 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> factors, RawLapackPivotView pivots);

/** @brief Executes native double real Sytri2 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Sytri2(const ReferenceLapackProvider& provider,
                                      DenseBlasTriangle triangle,
                                      DenseBlasMatrixView<double> factors,
                                      RawLapackPivotView pivots,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);

/** @brief Queries single complex Sytri2 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native single complex Sytri2 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<float>> factors,
       RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex Sytri2 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native double complex Sytri2 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<double>> factors,
       RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single Hermitian Hetri2 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native single Hermitian Hetri2 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetri2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<float>> factors,
       RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double Hermitian Hetri2 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetri2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native double Hermitian Hetri2 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetri2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<double>> factors,
       RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single real Sytri2x workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<float> factors,
    RawLapackPivotView pivots);

/** @brief Executes native single real Sytri2x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri2x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<float> factors,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real Sytri2x workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<double> factors,
    RawLapackPivotView pivots);

/** @brief Executes native double real Sytri2x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri2x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<double> factors,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex Sytri2x workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native single complex Sytri2x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri2x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex Sytri2x workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native double complex Sytri2x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri2x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single Hermitian Hetri2x workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native single Hermitian Hetri2x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetri2x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double Hermitian Hetri2x workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetri2xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native double Hermitian Hetri2x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor and output triangle.
 * @param block_size Positive native block size, bound into the plan.
 * @param factors Mutable raw square classic factors, with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * Active output may invalidate borrowed factor views even on positive INFO.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetri2x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_BLOCK_INVERSE_H_
