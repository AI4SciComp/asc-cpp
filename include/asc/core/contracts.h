// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_CORE_CONTRACTS_H_
#define ASC_CORE_CONTRACTS_H_

#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "asc/core/config.h"

namespace asc {

/// @brief Category of a failed program contract.
enum class ContractKind {
  kPrecondition,  ///< A caller supplied invalid arguments or state.
  kPostcondition,  ///< An operation failed to establish its result contract.
  kInvariant,     ///< An internal debug invariant failed.
};

/// @brief Exception raised for a contract violation in exception builds.
class ASC_EXPORT ContractException : public std::logic_error {
 public:
  /// @brief Construct the exception from a formatted contract diagnostic.
  explicit ContractException(const std::string& message)
      : std::logic_error(message) {}
};

/// @brief Report a program-contract violation and throw or abort.
/// @param kind Contract category.
/// @param expression Text of the failed expression.
/// @param message User-facing diagnostic detail.
/// @param location Source location of the contract boundary.
[[noreturn]] ASC_EXPORT void ContractFailure(
    ContractKind kind, std::string_view expression, std::string_view message,
    const std::source_location& location = std::source_location::current());

}  // namespace asc

/// @brief Internal implementation shared by public contract macros.
#define ASC_DETAIL_CONTRACT(kind, condition, message)                       \
  do {                                                                      \
    if (!(condition)) {                                                     \
      std::ostringstream asc_contract_message_stream;                       \
      asc_contract_message_stream << message;                               \
      ::asc::ContractFailure(kind, #condition,                              \
                             asc_contract_message_stream.str(),             \
                             std::source_location::current());              \
    }                                                                       \
  } while (false)

#if defined(DOXYGEN)
/// @brief Enforce an always-active public precondition.
#endif
#define ASC_REQUIRE(condition, message)                                     \
  ASC_DETAIL_CONTRACT(::asc::ContractKind::kPrecondition, condition, message)

#if defined(DOXYGEN)
/// @brief Enforce an always-active public postcondition.
#endif
#define ASC_ENSURE(condition, message)                                      \
  ASC_DETAIL_CONTRACT(::asc::ContractKind::kPostcondition, condition, message)

#ifdef ASC_DEBUG
#if defined(DOXYGEN)
/// @brief Enforce a debug-only internal invariant.
#endif
#define ASC_DCHECK(condition, message)                                      \
  ASC_DETAIL_CONTRACT(::asc::ContractKind::kInvariant, condition, message)
#else
#define ASC_DCHECK(condition, message) \
  do {                                 \
  } while (false)
#endif

#endif  // ASC_CORE_CONTRACTS_H_
