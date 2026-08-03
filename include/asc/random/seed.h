#ifndef ASC_RANDOM_SEED_H_
#define ASC_RANDOM_SEED_H_

/**
 * @file
 * @brief Public Random engine declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_random_engines
 */

#include <cstdint>
#include <random>

#include "asc/core/result.h"
#include "asc/random/export.h"

namespace asc {

/**
 * @brief Performs the public AcquireNondeterministicSeed operation defined by
 * the Random engine contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @param[in] source Input source, valid and accessible for the operation.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_random_engines
 */
[[nodiscard]] ASC_RANDOM_EXPORT Result<std::uint64_t>
AcquireNondeterministicSeed(std::random_device& source);

}  // namespace asc

#endif  // ASC_RANDOM_SEED_H_
