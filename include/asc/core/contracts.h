#ifndef ASC_CORE_CONTRACTS_H_
#define ASC_CORE_CONTRACTS_H_

/**
 * @file
 * @brief Public Core declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_core
 */

#include <string_view>

#include "asc/core/export.h"

namespace asc {

// Terminates the process after reporting a violated programmer contract.
// FatalContract never invokes a mutable process-global callback.
/**
 * @brief Performs the public FatalContract operation defined by the Core
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] expression The expression value required by this contract.
 * @param[in] file The file value required by this contract.
 * @param[in] line The line value required by this contract.
 * @param[in] message The message value required by this contract.
 * @ingroup asc_core
 */
[[noreturn]] ASC_CORE_EXPORT void FatalContract(
    const char* expression, const char* file, int line,
    std::string_view message = {}) noexcept;

}  // namespace asc

/**
 * @brief Controls the public ASC_CHECK declaration or contract behavior.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
#define ASC_CHECK(condition)          \
  ((condition) ? static_cast<void>(0) \
               : ::asc::FatalContract(#condition, __FILE__, __LINE__))

/**
 * @brief Controls the public ASC_CHECK_MESSAGE declaration or contract
 * behavior.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
#define ASC_CHECK_MESSAGE(condition, message) \
  ((condition)                                \
       ? static_cast<void>(0)                 \
       : ::asc::FatalContract(#condition, __FILE__, __LINE__, (message)))

#if defined(NDEBUG)
#define ASC_DCHECK(condition) static_cast<void>(0)
#define ASC_DCHECK_MESSAGE(condition, message) static_cast<void>(0)
#else
/**
 * @brief Controls the public ASC_DCHECK declaration or contract behavior.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
#define ASC_DCHECK(condition) ASC_CHECK(condition)
/**
 * @brief Controls the public ASC_DCHECK_MESSAGE declaration or contract
 * behavior.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
#define ASC_DCHECK_MESSAGE(condition, message) \
  ASC_CHECK_MESSAGE(condition, message)
#endif

#endif  // ASC_CORE_CONTRACTS_H_
