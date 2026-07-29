#ifndef ASC_UTILITIES_COMMAND_LINE_H_
#define ASC_UTILITIES_COMMAND_LINE_H_

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "asc/core/configuration.h"
#include "asc/core/result.h"
#include "asc/utilities/export.h"

namespace asc {

struct CommandLineOption {
  std::string long_name;
  std::optional<char> short_name;
  std::string destination;
  std::string value_name;
  std::string help;
};

struct CommandLineParseResult {
  Configuration configuration;
  std::vector<std::string> positional_arguments;
};

class CommandLineParser {
 public:
  static ASC_UTILITIES_EXPORT Result<CommandLineParser> Create(
      ConfigurationSchema schema, std::span<const CommandLineOption> options);

  [[nodiscard]] ASC_UTILITIES_EXPORT Result<CommandLineParseResult> Parse(
      std::span<const std::string_view> arguments) const;

  [[nodiscard]] ASC_UTILITIES_EXPORT std::string RenderHelp() const;

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
