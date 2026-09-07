#ifndef ASC_DENSE_LAPACK_WORKSPACE_H_
#define ASC_DENSE_LAPACK_WORKSPACE_H_

/** @file
 * @brief Explicit allocation-free LAPACK workspace capacities and preflight.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/export.h"
#include "asc/dense/lapack/types.h"

namespace asc {

/** @brief Simultaneously live workspace roles are disjoint, with distinct
 * units. */
enum class LapackWorkspaceKind : std::uint8_t {
  kScalar,            ///< Primary numerical scalar entries.
  kReal,              ///< Underlying real entries for complex operations.
  kInteger,           ///< Private ABI-width foreign integer entries.
  kLogical,           ///< Private ABI-width foreign logical entries.
  kPivotConversion,   ///< Explicit pivot widening/narrowing storage.
  kLayoutConversion,  ///< Explicit pack/unpack storage.
  kProviderComplex,   ///< Explicit foreign complex-representation conversion.
  kScratch,           ///< Additional bounded byte or typed scratch entries.
  kCount              ///< Number of distinct roles, not a usable role.
};

/** @brief One role's minimum/preferred entry counts and required byte
 * alignment. */
struct LapackWorkspaceRequirement {
  extent_t minimum_entries =
      0;  ///< Mandatory entries; zero permits null storage.
  extent_t preferred_entries = 0;  ///< Recommended entries, at least minimum.
  std::size_t entry_bytes =
      1;  ///< Entry size; never confused with entry count.
  std::size_t alignment =
      1;  ///< Required positive power-of-two byte alignment.
};

/** @brief Copied query plan; no buffers, resources, or foreign handles are
 * owned.
 *
 * Operation-specific query code fills these facts from checked formulas or
 * verified provider queries. Constructing this metadata is not a query call.
 */
struct LapackWorkspacePlan {
  LapackPlanIdentity identity;  ///< Full immutable query key.
  std::array<LapackWorkspaceRequirement, 8> regions{};  ///< Indexed by role.
  std::size_t total_byte_limit = std::numeric_limits<std::size_t>::max();
  ///< Upper bound for the sum of supplied simultaneously live regions.
};

/** @brief Borrows CPU workspace buffers; lifetimes must cover query/execution.
 *
 * Each supplied region is an explicit byte span. Its role and validated plan
 * determine scalar/ABI interpretation. No allocation, conversion, transfer or
 * synchronization is implied; callers must establish typed object lifetimes.
 */
struct LapackWorkspace {
  std::array<MutableMemoryView, 8> regions{
      MutableMemoryView(nullptr, 0, MemorySpace::kHost),
      MutableMemoryView(nullptr, 0, MemorySpace::kHost),
      MutableMemoryView(nullptr, 0, MemorySpace::kHost),
      MutableMemoryView(nullptr, 0, MemorySpace::kHost),
      MutableMemoryView(nullptr, 0, MemorySpace::kHost),
      MutableMemoryView(nullptr, 0, MemorySpace::kHost),
      MutableMemoryView(nullptr, 0, MemorySpace::kHost),
      MutableMemoryView(nullptr, 0, MemorySpace::kHost)};
  ///< Disjoint borrowed regions indexed by LapackWorkspaceKind.
};

/** @brief Rounds a provider query upward after finite and integer-range checks.
 * @param value Actual floating query result, not an integer converted to float.
 * @param abi Selected integer interface, including suffixed versus true ILP64.
 * @param entry_bytes Byte size for a single requested entry; must be positive.
 * @return A representable entry count or invalid argument/overflow. Allocates
 * nothing and performs no provider call; it does not imply universal LWORK=-1.
 */
ASC_DENSE_EXPORT Result<extent_t> CheckedLapackQueryEntries(
    double value, LapackIntegerAbi abi, std::size_t entry_bytes);

/** @brief Validates query freshness, capacity, byte limits, alignment and
 * aliasing.
 * @param plan Checked operation query requirements.
 * @param actual_identity Key freshly built from the actual execution operands.
 * @param workspace Borrowed host/pinned-host buffers, not accessed or modified.
 * @param forbidden_operands Every simultaneously live input/output byte span.
 * @return OK or invalid argument/overflow/memory access/invalid state. All
 * failures precede mutation or foreign calls and allocate no memory. Runtime
 * storage allocation internal to a foreign provider is a separate guarantee.
 */
ASC_DENSE_EXPORT Status ValidateLapackWorkspace(
    const LapackWorkspacePlan& plan, const LapackPlanIdentity& actual_identity,
    const LapackWorkspace& workspace,
    std::span<const ConstMemoryView> forbidden_operands);

}  // namespace asc

#endif  // ASC_DENSE_LAPACK_WORKSPACE_H_
