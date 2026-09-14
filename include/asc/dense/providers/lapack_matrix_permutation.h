#ifndef ASC_DENSE_PROVIDERS_LAPACK_MATRIX_PERMUTATION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_MATRIX_PERMUTATION_H_
/** @file
 * @brief Checked row and column permutations with immutable one-based indices.
 *
 * LAPMR permutes rows and LAPMT permutes columns. For forward direction,
 * output position i receives input position K(i); backward moves input i to
 * output K(i). The permutation is a unit-contiguous vector of public index_t
 * values containing each integer from one through the permuted dimension once.
 * Queries inspect metadata only. Execution checks permutation values before
 * native entry and preserves the caller's permutation. Both matrix layouts
 * use explicit compact column staging in live scalar kScratch storage and
 * native integer objects constructed in raw aligned kInteger workspace.
 * Empty matrices validate metadata and complete locally without scanning K
 * or accessing numeric storage. Every nonempty matrix validates K, including
 * singleton permutations, and enters the provider. There is no native INFO.
 * Metadata aliases reject before report reset; other structural failures reset
 * the report and preserve operands and workspace. Invalid permutation values
 * return kIndex and can change integer workspace, preserving caller operands.
 * Failure to restore native K returns kProvider with unusable output and no
 * matrix publication. Ordinary return with restored K publishes the result;
 * this does not certify arbitrary provider writes or detect omitted writes.
 * Copies preserve nonfinite values, signed zeros and complex components without
 * warnings, conjugation or arithmetic. Padding and input indices are unchanged.
 * Calls allocate, transfer and synchronize nothing implicitly. Independent
 * provider contexts, arrays, plans, workspace and reports are reentrant.
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
/** @brief Meaning of a one-based permutation applied to rows or columns. */
enum class LapackPermutationDirection : std::uint8_t {
  kForward,  ///< Output position i receives input position K(i).
  kBackward  ///< Input position i moves to output position K(i).
};
/** @brief Plans SLAPMR row permutation without numeric access.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based row permutation, unit increment and
 * length equal to the matrix's number of rows. Must be disjoint from matrix.
 * @return Plan binding routine, scalar, dimensions, layout, matrix stride,
 * direction, index shape and provider, or metadata/overflow failure. Plans may
 * be reused with different valid indices of the same shape. Nonempty work uses
 * live scalar kScratch and aligned raw kInteger storage; empty work uses none.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLapmrWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<const index_t> permutation);
/** @brief Executes SLAPMR after complete permutation validation.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based row permutation, unit increment and
 * length equal to the matrix's number of rows. Must be disjoint from matrix.
 * @param plan Matching metadata-only plan; no native workspace query is made.
 * @param workspace Disjoint caller storage meeting every planned region.
 * @param report Records provider entry, success or unusable output; native INFO
 * and native argument are absent. Metadata aliases reject without resetting it.
 * @return Success after empty local completion or native return with restored
 * K; structural error before numeric mutation, kIndex for invalid indices, or
 * kProvider if native K is not restored. Caller matrix publication occurs only
 * after successful return. Nonfinite copied values imply no numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lapmr(const ReferenceLapackProvider& provider,
      LapackPermutationDirection direction, DenseBlasMatrixView<float> matrix,
      DenseBlasVectorView<const index_t> permutation,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans DLAPMR row permutation without numeric access.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based row permutation, unit increment and
 * length equal to the matrix's number of rows. Must be disjoint from matrix.
 * @return Plan binding routine, scalar, dimensions, layout, matrix stride,
 * direction, index shape and provider, or metadata/overflow failure. Plans may
 * be reused with different valid indices of the same shape. Nonempty work uses
 * live scalar kScratch and aligned raw kInteger storage; empty work uses none.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLapmrWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<const index_t> permutation);
/** @brief Executes DLAPMR after complete permutation validation.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based row permutation, unit increment and
 * length equal to the matrix's number of rows. Must be disjoint from matrix.
 * @param plan Matching metadata-only plan; no native workspace query is made.
 * @param workspace Disjoint caller storage meeting every planned region.
 * @param report Records provider entry, success or unusable output; native INFO
 * and native argument are absent. Metadata aliases reject without resetting it.
 * @return Success after empty local completion or native return with restored
 * K; structural error before numeric mutation, kIndex for invalid indices, or
 * kProvider if native K is not restored. Caller matrix publication occurs only
 * after successful return. Nonfinite copied values imply no numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lapmr(const ReferenceLapackProvider& provider,
      LapackPermutationDirection direction, DenseBlasMatrixView<double> matrix,
      DenseBlasVectorView<const index_t> permutation,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans CLAPMR row permutation without numeric access.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based row permutation, unit increment and
 * length equal to the matrix's number of rows. Must be disjoint from matrix.
 * @return Plan binding routine, scalar, dimensions, layout, matrix stride,
 * direction, index shape and provider, or metadata/overflow failure. Plans may
 * be reused with different valid indices of the same shape. Nonempty work uses
 * live scalar kScratch and aligned raw kInteger storage; empty work uses none.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLapmrWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<const index_t> permutation);
/** @brief Executes CLAPMR after complete permutation validation.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based row permutation, unit increment and
 * length equal to the matrix's number of rows. Must be disjoint from matrix.
 * @param plan Matching metadata-only plan; no native workspace query is made.
 * @param workspace Disjoint caller storage meeting every planned region.
 * @param report Records provider entry, success or unusable output; native INFO
 * and native argument are absent. Metadata aliases reject without resetting it.
 * @return Success after empty local completion or native return with restored
 * K; structural error before numeric mutation, kIndex for invalid indices, or
 * kProvider if native K is not restored. Caller matrix publication occurs only
 * after successful return. Nonfinite copied values imply no numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lapmr(const ReferenceLapackProvider& provider,
      LapackPermutationDirection direction,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasVectorView<const index_t> permutation,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans ZLAPMR row permutation without numeric access.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based row permutation, unit increment and
 * length equal to the matrix's number of rows. Must be disjoint from matrix.
 * @return Plan binding routine, scalar, dimensions, layout, matrix stride,
 * direction, index shape and provider, or metadata/overflow failure. Plans may
 * be reused with different valid indices of the same shape. Nonempty work uses
 * live scalar kScratch and aligned raw kInteger storage; empty work uses none.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLapmrWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<const index_t> permutation);
/** @brief Executes ZLAPMR after complete permutation validation.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based row permutation, unit increment and
 * length equal to the matrix's number of rows. Must be disjoint from matrix.
 * @param plan Matching metadata-only plan; no native workspace query is made.
 * @param workspace Disjoint caller storage meeting every planned region.
 * @param report Records provider entry, success or unusable output; native INFO
 * and native argument are absent. Metadata aliases reject without resetting it.
 * @return Success after empty local completion or native return with restored
 * K; structural error before numeric mutation, kIndex for invalid indices, or
 * kProvider if native K is not restored. Caller matrix publication occurs only
 * after successful return. Nonfinite copied values imply no numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lapmr(const ReferenceLapackProvider& provider,
      LapackPermutationDirection direction,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasVectorView<const index_t> permutation,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans SLAPMT column permutation without numeric access.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based column permutation, unit increment and
 * length equal to the matrix's number of columns. Must be disjoint from matrix.
 * @return Plan binding routine, scalar, dimensions, layout, matrix stride,
 * direction, index shape and provider, or metadata/overflow failure. Plans may
 * be reused with different valid indices of the same shape. Nonempty work uses
 * live scalar kScratch and aligned raw kInteger storage; empty work uses none.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLapmtWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<const index_t> permutation);
/** @brief Executes SLAPMT after complete permutation validation.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based column permutation, unit increment and
 * length equal to the matrix's number of columns. Must be disjoint from matrix.
 * @param plan Matching metadata-only plan; no native workspace query is made.
 * @param workspace Disjoint caller storage meeting every planned region.
 * @param report Records provider entry, success or unusable output; native INFO
 * and native argument are absent. Metadata aliases reject without resetting it.
 * @return Success after empty local completion or native return with restored
 * K; structural error before numeric mutation, kIndex for invalid indices, or
 * kProvider if native K is not restored. Caller matrix publication occurs only
 * after successful return. Nonfinite copied values imply no numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lapmt(const ReferenceLapackProvider& provider,
      LapackPermutationDirection direction, DenseBlasMatrixView<float> matrix,
      DenseBlasVectorView<const index_t> permutation,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans DLAPMT column permutation without numeric access.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based column permutation, unit increment and
 * length equal to the matrix's number of columns. Must be disjoint from matrix.
 * @return Plan binding routine, scalar, dimensions, layout, matrix stride,
 * direction, index shape and provider, or metadata/overflow failure. Plans may
 * be reused with different valid indices of the same shape. Nonempty work uses
 * live scalar kScratch and aligned raw kInteger storage; empty work uses none.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLapmtWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<const index_t> permutation);
/** @brief Executes DLAPMT after complete permutation validation.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based column permutation, unit increment and
 * length equal to the matrix's number of columns. Must be disjoint from matrix.
 * @param plan Matching metadata-only plan; no native workspace query is made.
 * @param workspace Disjoint caller storage meeting every planned region.
 * @param report Records provider entry, success or unusable output; native INFO
 * and native argument are absent. Metadata aliases reject without resetting it.
 * @return Success after empty local completion or native return with restored
 * K; structural error before numeric mutation, kIndex for invalid indices, or
 * kProvider if native K is not restored. Caller matrix publication occurs only
 * after successful return. Nonfinite copied values imply no numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lapmt(const ReferenceLapackProvider& provider,
      LapackPermutationDirection direction, DenseBlasMatrixView<double> matrix,
      DenseBlasVectorView<const index_t> permutation,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans CLAPMT column permutation without numeric access.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based column permutation, unit increment and
 * length equal to the matrix's number of columns. Must be disjoint from matrix.
 * @return Plan binding routine, scalar, dimensions, layout, matrix stride,
 * direction, index shape and provider, or metadata/overflow failure. Plans may
 * be reused with different valid indices of the same shape. Nonempty work uses
 * live scalar kScratch and aligned raw kInteger storage; empty work uses none.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLapmtWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<const index_t> permutation);
/** @brief Executes CLAPMT after complete permutation validation.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based column permutation, unit increment and
 * length equal to the matrix's number of columns. Must be disjoint from matrix.
 * @param plan Matching metadata-only plan; no native workspace query is made.
 * @param workspace Disjoint caller storage meeting every planned region.
 * @param report Records provider entry, success or unusable output; native INFO
 * and native argument are absent. Metadata aliases reject without resetting it.
 * @return Success after empty local completion or native return with restored
 * K; structural error before numeric mutation, kIndex for invalid indices, or
 * kProvider if native K is not restored. Caller matrix publication occurs only
 * after successful return. Nonfinite copied values imply no numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lapmt(const ReferenceLapackProvider& provider,
      LapackPermutationDirection direction,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasVectorView<const index_t> permutation,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans ZLAPMT column permutation without numeric access.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based column permutation, unit increment and
 * length equal to the matrix's number of columns. Must be disjoint from matrix.
 * @return Plan binding routine, scalar, dimensions, layout, matrix stride,
 * direction, index shape and provider, or metadata/overflow failure. Plans may
 * be reused with different valid indices of the same shape. Nonempty work uses
 * live scalar kScratch and aligned raw kInteger storage; empty work uses none.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLapmtWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<const index_t> permutation);
/** @brief Executes ZLAPMT after complete permutation validation.
 * @param provider Checked host Reference provider with its actual integer ABI.
 * @param direction Forward gather or backward scatter as documented above.
 * @param matrix Mutable rectangular matrix in either layout; padding is
 * preserved.
 * @param permutation Immutable one-based column permutation, unit increment and
 * length equal to the matrix's number of columns. Must be disjoint from matrix.
 * @param plan Matching metadata-only plan; no native workspace query is made.
 * @param workspace Disjoint caller storage meeting every planned region.
 * @param report Records provider entry, success or unusable output; native INFO
 * and native argument are absent. Metadata aliases reject without resetting it.
 * @return Success after empty local completion or native return with restored
 * K; structural error before numeric mutation, kIndex for invalid indices, or
 * kProvider if native K is not restored. Caller matrix publication occurs only
 * after successful return. Nonfinite copied values imply no numerical warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Lapmt(const ReferenceLapackProvider& provider,
      LapackPermutationDirection direction,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasVectorView<const index_t> permutation,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_MATRIX_PERMUTATION_H_
