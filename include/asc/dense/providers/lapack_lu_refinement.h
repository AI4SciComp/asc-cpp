#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_REFINEMENT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_REFINEMENT_H_

/** @file
 * @brief Explicit reference general-system iterative refinement and estimates.
 *
 * GERFS refines caller-owned X for op(A)*X=B, preserving original A/B, raw LU
 * and raw one-based pivots from the same factorization. The caller guarantees
 * those raw factors/pivots correspond to original A and that X is an initial
 * solution of that system. Raw descriptors do not fabricate provenance or a
 * successful-factor certificate. N/T/C are distinct legal complex modes.
 *
 * Formula queries do not call a provider or scan U diagonals. Real WORK uses
 * 3*n scalar entries and 2*n kInteger entries; complex WORK uses 2*n scalar,
 * n underlying-real kReal entries and n kInteger entries. The first n
 * ABI-width kInteger entries contain converted pivots; for real routines the
 * following n entries contain disjoint numerical IWORK. kPivotConversion is
 * not used: that common role describes ASC-width rather than foreign-width
 * entries. The implementation starts trivial foreign-integer lifetimes in
 * the supplied byte region. Scalar,
 * real and packing objects must already be live. Every independently row-major
 * A/AF/B/X requires its rows*columns entries in caller kLayoutConversion,
 * packed consecutively in that order. X alone is unpacked after raw INFO=0,
 * including the estimate-quality warnings below.
 *
 * All descriptors are host/pinned-host and all seven operand spans and live
 * workspace regions are pairwise disjoint. FERR/BERR are exact nrhs-length
 * contiguous underlying-real vectors. No allocation, implicit conversion
 * buffer, transfer, fallback or process-wide error-handler change occurs.
 * Plans bind routine/scalar/provider, all dimensions/leading dimensions,
 * layouts, transpose and output/pivot counts/strides, not numerical values.
 * Row-major source leading strides are ASC-sized metadata, not foreign LDA;
 * only the actual packed leading dimensions are narrowed to the selected ABI.
 *
 * Structural failure leaves X/FERR/BERR/workspaces unchanged, with absent raw
 * INFO. For n=0 or nrhs=0, X stays unchanged and FERR/BERR become zero without
 * foreign entry. Otherwise execution rejects an exactly zero U diagonal
 * before writes: kNumerical, LapackOutcome::kSingular, zero-based diagonal
 * index and absent INFO (an ASC preflight finding, not an upstream INFO).
 * No tolerance is used. Negative or unexpected positive foreign INFO is a
 * provider defect with unusable outputs; exact raw INFO is retained.
 *
 * INFO=0 means upstream completed refinement, not a guaranteed forward error
 * bound. ASC scans FERR/BERR afterward without allocation. Nonfinite estimates
 * return kNumerical with LapackOutcome::kAccuracyWarning; negative estimates
 * are a provider defect returning kProvider with LapackOutcome::kPartialResult
 * (a negative value takes precedence over a nonfinite warning). Both retain
 * raw INFO=0, called_provider=true and kDocumentedPartial validity: X completed
 * refinement and is preserved, all raw estimates remain available, and only
 * individual finite nonnegative estimates satisfy their output contract.
 * diagnostic_index is the first negative-estimate RHS, otherwise the first
 * nonfinite-estimate RHS. No foreign INFO or singularity is invented.
 * Finite inputs can still trigger intermediate estimate overflow in the
 * pinned routine. No finiteness or accuracy promise is made for X on arbitrary
 * nonfinite input. FERR estimates
 * relative forward error; BERR estimates componentwise backward error. Complex
 * GERFS componentwise magnitudes and FERR normalization use CABS1,
 * abs(real)+abs(imag). Small-denominator safeguards and the five-step stopping
 * rule are exactly those of the pinned routine, not new ASC convergence tests.
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

/** @brief Computes checked workspace for single real general-system solutions.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Selects original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable original n-by-n A in either layout.
 * @param factors Immutable corresponding raw n-by-n packed LU in either layout.
 * @param pivots Corresponding immutable n-entry one-based partial-pivot data.
 * @param rhs Immutable original n-by-nrhs B in either layout.
 * @param solution Caller-owned n-by-nrhs X in either layout, unchanged.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR, unchanged.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR, unchanged.
 * @return Checked formula-only plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGerfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Refines single real general-system solutions.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Selects original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable original n-by-n A in either layout.
 * @param factors Immutable corresponding raw n-by-n packed LU in either layout.
 * @param pivots Corresponding immutable n-entry one-based partial-pivot data.
 * @param rhs Immutable original n-by-nrhs B in either layout.
 * @param solution Caller-owned n-by-nrhs X in either layout, refined in place.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Disjoint explicit numerical, pivot and layout storage.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural failure, ASC-detected zero U diagonal, estimate
 * quality warning, or provider defect; see file-level validity/INFO contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Gerfs(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes checked workspace for double real general-system solutions.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Selects original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable original n-by-n A in either layout.
 * @param factors Immutable corresponding raw n-by-n packed LU in either layout.
 * @param pivots Corresponding immutable n-entry one-based partial-pivot data.
 * @param rhs Immutable original n-by-nrhs B in either layout.
 * @param solution Caller-owned n-by-nrhs X in either layout, unchanged.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR, unchanged.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR, unchanged.
 * @return Checked formula-only plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGerfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Refines double real general-system solutions.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Selects original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable original n-by-n A in either layout.
 * @param factors Immutable corresponding raw n-by-n packed LU in either layout.
 * @param pivots Corresponding immutable n-entry one-based partial-pivot data.
 * @param rhs Immutable original n-by-nrhs B in either layout.
 * @param solution Caller-owned n-by-nrhs X in either layout, refined in place.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Disjoint explicit numerical, pivot and layout storage.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural failure, ASC-detected zero U diagonal, estimate
 * quality warning, or provider defect; see file-level validity/INFO contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Gerfs(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes checked workspace for single complex general-system
 * solutions.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Selects original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable original n-by-n A in either layout.
 * @param factors Immutable corresponding raw n-by-n packed LU in either layout.
 * @param pivots Corresponding immutable n-entry one-based partial-pivot data.
 * @param rhs Immutable original n-by-nrhs B in either layout.
 * @param solution Caller-owned n-by-nrhs X in either layout, unchanged.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR, unchanged.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR, unchanged.
 * @return Checked formula-only plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGerfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Refines single complex general-system solutions.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Selects original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable original n-by-n A in either layout.
 * @param factors Immutable corresponding raw n-by-n packed LU in either layout.
 * @param pivots Corresponding immutable n-entry one-based partial-pivot data.
 * @param rhs Immutable original n-by-nrhs B in either layout.
 * @param solution Caller-owned n-by-nrhs X in either layout, refined in place.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Disjoint explicit numerical, pivot and layout storage.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural failure, ASC-detected zero U diagonal, estimate
 * quality warning, or provider defect; see file-level validity/INFO contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Gerfs(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes checked workspace for double complex general-system
 * solutions.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Selects original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable original n-by-n A in either layout.
 * @param factors Immutable corresponding raw n-by-n packed LU in either layout.
 * @param pivots Corresponding immutable n-entry one-based partial-pivot data.
 * @param rhs Immutable original n-by-nrhs B in either layout.
 * @param solution Caller-owned n-by-nrhs X in either layout, unchanged.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR, unchanged.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR, unchanged.
 * @return Checked formula-only plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGerfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Refines double complex general-system solutions.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Selects original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable original n-by-n A in either layout.
 * @param factors Immutable corresponding raw n-by-n packed LU in either layout.
 * @param pivots Corresponding immutable n-entry one-based partial-pivot data.
 * @param rhs Immutable original n-by-nrhs B in either layout.
 * @param solution Caller-owned n-by-nrhs X in either layout, refined in place.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Disjoint explicit numerical, pivot and layout storage.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural failure, ASC-detected zero U diagonal, estimate
 * quality warning, or provider defect; see file-level validity/INFO contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Gerfs(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_REFINEMENT_H_
