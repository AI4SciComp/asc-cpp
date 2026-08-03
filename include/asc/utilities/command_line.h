#ifndef ASC_UTILITIES_COMMAND_LINE_H_
#define ASC_UTILITIES_COMMAND_LINE_H_

/**
 * @file
 * @brief Public Utilities declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_utilities
 */

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "asc/core/configuration.h"
#include "asc/core/result.h"
#include "asc/utilities/export.h"

namespace asc {

/**
 * @brief Describes one schema-backed command-line option.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Utilities module contract.
 * @ingroup asc_utilities
 */
struct CommandLineOption {
  /**
   * @brief Stores the long name value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @ingroup asc_utilities
   */
  std::string long_name;
  /**
   * @brief Stores the short name value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @ingroup asc_utilities
   */
  std::optional<char> short_name;
  /**
   * @brief Stores the destination value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @ingroup asc_utilities
   */
  std::string destination;
  /**
   * @brief Stores the value name value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @ingroup asc_utilities
   */
  std::string value_name;
  /**
   * @brief Stores the help value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @ingroup asc_utilities
   */
  std::string help;
};

/**
 * @brief Contains validated configuration and positional arguments.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Utilities module contract.
 * @ingroup asc_utilities
 */
struct CommandLineParseResult {
  /**
   * @brief Stores the configuration value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @ingroup asc_utilities
   */
  Configuration configuration;
  /**
   * @brief Stores the positional arguments value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @ingroup asc_utilities
   */
  std::vector<std::string> positional_arguments;
};

/**
 * @brief Parses command-line arguments into validated configuration.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Utilities module contract.
 * @ingroup asc_utilities
 */
class CommandLineParser {
 public:
  /**
   * @brief Validates inputs and creates the requested Utilities object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @param[in] schema The schema value required by this contract.
   * @param[in] options The options value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_utilities
   */
  static ASC_UTILITIES_EXPORT Result<CommandLineParser> Create(
      ConfigurationSchema schema, std::span<const CommandLineOption> options);

  /**
   * @brief Performs the public Parse operation defined by the Utilities
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @param[in] arguments The arguments value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_utilities
   */
  [[nodiscard]] ASC_UTILITIES_EXPORT Result<CommandLineParseResult> Parse(
      std::span<const std::string_view> arguments) const;

  /**
   * @brief Performs the public RenderHelp operation defined by the Utilities
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_utilities
   */
  [[nodiscard]] ASC_UTILITIES_EXPORT std::string RenderHelp() const;

  /**
   * @brief Performs the public options operation defined by the Utilities
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_utilities
   */
  [[nodiscard]] const std::vector<CommandLineOption>& options() const noexcept {
    return options_;
  }

 private:
  CommandLineParser(ConfigurationSchema schema,
                    std::vector<CommandLineOption> options);

  ConfigurationSchema schema_;
  std::vector<CommandLineOption> options_;
};

}  // namespace asc

#endif  // ASC_UTILITIES_COMMAND_LINE_H_
