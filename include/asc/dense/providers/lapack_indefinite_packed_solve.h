#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_SOLVE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_SOLVE_H_

/** @file
 * @brief Explicit solves from packed symmetric or Hermitian block factors.
 *
 * SPTRS uses ordinary transpose and HPTRS uses conjugate transpose. Factors
 * and raw signed one-based paired pivots must retain successful same-call
 * SPTRF/HPTRF provenance, scalar type, provider, triangle and symmetry. Raw
 * descriptors cannot prove that history. Only the Bunch-Kaufman pivot tag is
 * accepted; existing dense-factor factories gain no packed interpretation.
 *
 * Queries inspect metadata only. Plans bind the provider/scalar, N/NRHS,
 * triangle, both layouts, exact pivot count and original/effective RHS stride.
 * Nonempty execution uses N caller-owned native INTEGER objects, plus
 * N*(N+1)/2 live scalar entries for row-major factors and N*NRHS live scalar
 * entries for row-major RHS. These simultaneous packing spans share the
 * layout-conversion region; no native WORK or foreign query exists.
 * Native packed products, BLAS terminal cursors and byte totals are checked.
 *
 * All referenced operands, scratch and live provider/plan/workspace/report
 * objects must be disjoint and accessible to the explicit serial CPU context.
 * Structural rejection preserves operands and scratch. Unsafe metadata aliases
 * also preserve the report; otherwise the report resets before validation.
 * Empty order or zero RHS is a successful noncall with no numerical reads,
 * scratch access or native INFO. Nonempty calls validate every paired pivot
 * and directional swap bound, then reject exactly zero source-evaluated block
 * divisors before mutation with numerical/singular outcome and the zero-based
 * block index. This is not a whole-factor finiteness scan or a success
 * certificate. Hermitian factor entries are not original-matrix inputs and
 * are not normalized during packing or divisor checks.
 *
 * INFO is initialized at full provider width; only zero is valid after a
 * checked solve. Missing/nonzero INFO is a provider defect with unusable
 * output; row-major RHS publication is withheld, while direct column-major
 * RHS can retain native effects. INFO=0 publishes completed RHS and preserves
 * padding. Factors and pivots remain immutable. Native numerical behavior,
 * including intermediate overflow, is not replaced by a hidden algorithm.
 * No allocation, implicit transfer, fallback, synchronization or process-global
 * handler change occurs. Concurrent calls may share immutable inputs/provider
 * and plans, with separate writable RHS, scratch and reports.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Queries single real symmetric packed solve workspace without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected packed factor triangle and pivot direction.
 * @param factors Immutable order-N matching packed block factors; unread.
 * @param pivots Exact-N raw Bunch-Kaufman paired pivots; entries unread.
 * @param rhs Mutable N-by-NRHS descriptor in either layout; entries unread.
 * @return Complete matching caller plan or structural error; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<float> rhs);

/** @brief Solves a single real symmetric system from packed block factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected packed triangle, matching the original producer.
 * @param factors Immutable successful same-call packed factors; see file.
 * @param pivots Matching raw signed one-based paired pivots, exact N.
 * @param rhs Original right-hand sides replaced by solutions; padding retained.
 * @param plan Unmodified matching metadata-only query result.
 * @param workspace Disjoint caller INTEGER and live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; see file for
 * exact noncall, singular-divisor, publication and native-INFO semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sptrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const float> factors, RawLapackPivotView pivots,
      DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real symmetric packed solve workspace without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected packed factor triangle and pivot direction.
 * @param factors Immutable order-N matching packed block factors; unread.
 * @param pivots Exact-N raw Bunch-Kaufman paired pivots; entries unread.
 * @param rhs Mutable N-by-NRHS descriptor in either layout; entries unread.
 * @return Complete matching caller plan or structural error; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<double> rhs);

/** @brief Solves a double real symmetric system from packed block factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected packed triangle, matching the original producer.
 * @param factors Immutable successful same-call packed factors; see file.
 * @param pivots Matching raw signed one-based paired pivots, exact N.
 * @param rhs Original right-hand sides replaced by solutions; padding retained.
 * @param plan Unmodified matching metadata-only query result.
 * @param workspace Disjoint caller INTEGER and live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; see file for
 * exact noncall, singular-divisor, publication and native-INFO semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Sptrs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex symmetric packed solve workspace without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected packed factor triangle and pivot direction.
 * @param factors Immutable order-N matching packed block factors; unread.
 * @param pivots Exact-N raw Bunch-Kaufman paired pivots; entries unread.
 * @param rhs Mutable N-by-NRHS descriptor in either layout; entries unread.
 * @return Complete matching caller plan or structural error; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs);

/** @brief Solves a single complex symmetric system from packed block factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected packed triangle, matching the original producer.
 * @param factors Immutable successful same-call packed factors; see file.
 * @param pivots Matching raw signed one-based paired pivots, exact N.
 * @param rhs Original right-hand sides replaced by solutions; padding retained.
 * @param plan Unmodified matching metadata-only query result.
 * @param workspace Disjoint caller INTEGER and live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; see file for
 * exact noncall, singular-divisor, publication and native-INFO semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sptrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<float>> factors,
      RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex symmetric packed solve workspace without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected packed factor triangle and pivot direction.
 * @param factors Immutable order-N matching packed block factors; unread.
 * @param pivots Exact-N raw Bunch-Kaufman paired pivots; entries unread.
 * @param rhs Mutable N-by-NRHS descriptor in either layout; entries unread.
 * @return Complete matching caller plan or structural error; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs);

/** @brief Solves a double complex symmetric system from packed block factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected packed triangle, matching the original producer.
 * @param factors Immutable successful same-call packed factors; see file.
 * @param pivots Matching raw signed one-based paired pivots, exact N.
 * @param rhs Original right-hand sides replaced by solutions; padding retained.
 * @param plan Unmodified matching metadata-only query result.
 * @param workspace Disjoint caller INTEGER and live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; see file for
 * exact noncall, singular-divisor, publication and native-INFO semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sptrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<double>> factors,
      RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single complex Hermitian packed solve workspace without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected packed factor triangle and pivot direction.
 * @param factors Immutable order-N matching packed block factors; unread.
 * @param pivots Exact-N raw Bunch-Kaufman paired pivots; entries unread.
 * @param rhs Mutable N-by-NRHS descriptor in either layout; entries unread.
 * @return Complete matching caller plan or structural error; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs);

/** @brief Solves a single complex Hermitian system from packed block factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected packed triangle, matching the original producer.
 * @param factors Immutable successful same-call packed factors; see file.
 * @param pivots Matching raw signed one-based paired pivots, exact N.
 * @param rhs Original right-hand sides replaced by solutions; padding retained.
 * @param plan Unmodified matching metadata-only query result.
 * @param workspace Disjoint caller INTEGER and live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; see file for
 * exact noncall, singular-divisor, publication and native-INFO semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hptrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<float>> factors,
      RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex Hermitian packed solve workspace without array
 * reads.
 * @param provider Explicit checked Reference provider; unchanged.
 * @param triangle Selected packed factor triangle and pivot direction.
 * @param factors Immutable order-N matching packed block factors; unread.
 * @param pivots Exact-N raw Bunch-Kaufman paired pivots; entries unread.
 * @param rhs Mutable N-by-NRHS descriptor in either layout; entries unread.
 * @return Complete matching caller plan or structural error; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs);

/** @brief Solves a double complex Hermitian system from packed block factors.
 * @param provider Explicit same-build Reference provider; no fallback.
 * @param triangle Selected packed triangle, matching the original producer.
 * @param factors Immutable successful same-call packed factors; see file.
 * @param pivots Matching raw signed one-based paired pivots, exact N.
 * @param rhs Original right-hand sides replaced by solutions; padding retained.
 * @param plan Unmodified matching metadata-only query result.
 * @param workspace Disjoint caller INTEGER and live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; see file for
 * exact noncall, singular-divisor, publication and native-INFO semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hptrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<double>> factors,
      RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_SOLVE_H_
