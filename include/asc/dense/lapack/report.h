#ifndef ASC_DENSE_LAPACK_REPORT_H_
#define ASC_DENSE_LAPACK_REPORT_H_

/** @file
 * @brief Caller-owned LAPACK diagnostics remain accessible on non-OK Status.
 */

#include <array>
#include <cstdint>
#include <optional>

#include "asc/core/types.h"
#include "asc/dense/export.h"
#include "asc/dense/lapack/types.h"

namespace asc {

/** @brief Routine-specific numerical result, separate from execution status. */
enum class LapackOutcome : std::uint8_t {
  kNotRun,               ///< Validation failed or execution has not begun.
  kSuccess,              ///< Routine-specific numerical goal succeeded.
  kAccuracyWarning,      ///< Computed output carries an accuracy warning.
  kSingular,             ///< Routine identified exact singularity.
  kNotPositiveDefinite,  ///< Positive-definite factorization failed.
  kRankDecision,         ///< Rank was determined; this need not be an error.
  kNonconvergence,   ///< Iteration did not meet the routine's stopping rule.
  kPartialResult,    ///< Only documented parts of the result are usable.
  kProviderArgument  ///< Negative INFO after checked entry; preserve defect.
};

/** @brief Usability of caller-owned numerical output after an operation. */
enum class LapackOutputValidity : std::uint8_t {
  kUnchanged,          ///< No numerical mutation has occurred.
  kComplete,           ///< Output satisfies the successful-operation contract.
  kDocumentedPartial,  ///< Only the documented partial output is valid.
  kUnusable            ///< Output must not be reused as a successful factor.
};

/** @brief Mandatory per-call report, with owned identity and optional raw INFO.
 *
 * A report never owns numerical buffers or extends factor/provider lifetimes.
 * A caller must not concurrently use one report for multiple calls. Raw INFO is
 * absent until execution actually returns it; zero is not fabricated on a
 * structural validation failure. Positive INFO is interpreted by each binding.
 */
struct LapackReport {
  std::array<char, 32> routine{};  ///< Copied null-terminated operation name.
  LapackProviderIdentity provider{};  ///< Copied selected build provenance.
  bool called_provider = false;  ///< Whether an external call was attempted.
  std::optional<std::int64_t>
      native_info;  ///< Exact signed foreign INFO, if returned.
  LapackOutcome outcome = LapackOutcome::kNotRun;  ///< Numerical category.
  LapackOutputValidity output_validity = LapackOutputValidity::kUnchanged;
  ///< Describes numerical output, independently of report accessibility.
  std::optional<index_t>
      diagnostic_index;  ///< Zero-based index only if meaningful.
  std::optional<std::int64_t>
      native_argument;  ///< One-based position from negative INFO.
  std::optional<LapackFactorFamily>
      factor_family;  ///< Origin of a produced factor.
};

/** @brief Resets a caller report before any structural validation or mutation.
 * @param identity Owned metadata identifying the attempted call.
 * @param report Replaced deterministically; caller retains ownership.
 * @post INFO and diagnostic fields are absent, called_provider is false, and
 * outputs are marked unchanged. CPU-only, reentrant and allocation-free.
 */
ASC_DENSE_EXPORT void InitializeLapackReport(const LapackPlanIdentity& identity,
                                             LapackReport& report) noexcept;

}  // namespace asc

#endif  // ASC_DENSE_LAPACK_REPORT_H_
