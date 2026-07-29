#ifndef ASC_CORE_CONTRACTS_H_
#define ASC_CORE_CONTRACTS_H_

#include <string_view>

#include "asc/core/export.h"

namespace asc {

// Terminates the process after reporting a violated programmer contract.
// FatalContract never invokes a mutable process-global callback.
[[noreturn]] ASC_CORE_EXPORT void FatalContract(
    const char* expression, const char* file, int line,
    std::string_view message = {}) noexcept;

}  // namespace asc

#define ASC_CHECK(condition)          \
  ((condition) ? static_cast<void>(0) \
               : ::asc::FatalContract(#condition, __FILE__, __LINE__))

#define ASC_CHECK_MESSAGE(condition, message) \
  ((condition)                                \
       ? static_cast<void>(0)                 \
       : ::asc::FatalContract(#condition, __FILE__, __LINE__, (message)))

#if defined(NDEBUG)
#define ASC_DCHECK(condition) static_cast<void>(0)
#define ASC_DCHECK_MESSAGE(condition, message) static_cast<void>(0)
#else
#define ASC_DCHECK(condition) ASC_CHECK(condition)
#define ASC_DCHECK_MESSAGE(condition, message) \
  ASC_CHECK_MESSAGE(condition, message)
#endif

#endif  // ASC_CORE_CONTRACTS_H_
