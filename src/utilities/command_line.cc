#include "asc/utilities/command_line.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ios>
#include <locale>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "asc/core/configuration.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {
namespace internal_utilities_command_line {

Status CommandLineError(std::string message) {
  return Status(ErrorCode::kInvalidArgument, std::move(message));
}

Result<std::vector<std::string>> ParsePointer(std::string_view path) {
  if (path.empty()) {
    return std::vector<std::string>{};
  }
  if (path.front() != '/') {
    return CommandLineError(
        "A command-line destination must be a JSON Pointer");
  }

  std::vector<std::string> tokens;
  std::size_t begin = 1;
  while (true) {
    const std::size_t end = path.find('/', begin);
    const std::string_view encoded = path.substr(
        begin,
        end == std::string_view::npos ? std::string_view::npos : end - begin);
    std::string token;
    token.reserve(encoded.size());
    for (std::size_t index = 0; index < encoded.size(); ++index) {
      if (encoded[index] != '~') {
        token.push_back(encoded[index]);
        continue;
      }
      if (index + 1 >= encoded.size()) {
        return CommandLineError(
            "A command-line destination has an incomplete JSON Pointer "
            "escape");
      }
      ++index;
      if (encoded[index] == '0') {
        token.push_back('~');
      } else if (encoded[index] == '1') {
        token.push_back('/');
      } else {
        return CommandLineError(
            "A command-line destination has an invalid JSON Pointer escape");
      }
    }
    tokens.push_back(std::move(token));
    if (end == std::string_view::npos) {
      break;
    }
    begin = end + 1;
  }
  return tokens;
}

Result<std::reference_wrapper<const ConfigurationSchema>> FindSchema(
    const ConfigurationSchema& schema,
    std::span<const std::string> path_tokens) {
  const ConfigurationSchema* current = &schema;
  for (const std::string& token : path_tokens) {
    if (current->type() != ConfigurationValueType::kObject) {
      return CommandLineError(
          "A command-line destination traverses a non-object schema");
    }
    const auto field = current->fields().find(token);
    if (field == current->fields().end()) {
      return CommandLineError(
          "A command-line destination is not present in the schema");
    }
    current = &field->second;
  }
  return std::cref(*current);
}

bool IsSupportedLeaf(ConfigurationValueType type) noexcept {
  return type == ConfigurationValueType::kBool ||
         type == ConfigurationValueType::kSignedInteger ||
         type == ConfigurationValueType::kUnsignedInteger ||
         type == ConfigurationValueType::kDouble ||
         type == ConfigurationValueType::kString;
}

bool IsAsciiName(std::string_view name) noexcept {
  if (name.empty() || name.front() == '-') {
    return false;
  }
  return std::ranges::all_of(name, [](unsigned char character) {
    return character <= 0x7fU && (std::isalnum(character) != 0 ||
                                  character == '-' || character == '_');
  });
}

bool IsAsciiShortName(char name) noexcept {
  const auto character = static_cast<unsigned char>(name);
  return character <= 0x7fU && std::isalnum(character) != 0;
}

bool HasNonzeroDecimalSignificand(std::string_view text) noexcept {
  const std::size_t exponent = text.find_first_of("eE");
  const std::size_t significand_end =
      exponent == std::string_view::npos ? text.size() : exponent;
  for (std::size_t index = 0; index < significand_end; ++index) {
    if (text[index] >= '1' && text[index] <= '9') {
      return true;
    }
  }
  return false;
}

std::optional<double> ParseDouble(std::string_view text) {
  if (text.empty() || text.front() == '+') {
    return std::nullopt;
  }

  std::istringstream input{std::string(text)};
  input.imbue(std::locale::classic());
  double value = 0.0;
  input >> std::noskipws >> value;
  if (!input || !input.eof()) {
    return std::nullopt;
  }
  if (value == 0.0 && HasNonzeroDecimalSignificand(text)) {
    return std::nullopt;
  }
  return value;
}

const CommandLineOption* FindLongOption(
    std::span<const CommandLineOption> options, std::string_view name) {
  for (const CommandLineOption& option : options) {
    if (option.long_name == name) {
      return &option;
    }
  }
  return nullptr;
}

const CommandLineOption* FindShortOption(
    std::span<const CommandLineOption> options, char name) {
  for (const CommandLineOption& option : options) {
    if (option.short_name == name) {
      return &option;
    }
  }
  return nullptr;
}

Result<ConfigurationValue> ParseValue(std::string_view text,
                                      ConfigurationValueType type) {
  if (type == ConfigurationValueType::kBool) {
    if (text == "true") {
      return ConfigurationValue(true);
    }
    if (text == "false") {
      return ConfigurationValue(false);
    }
    return CommandLineError(
        "A boolean command-line value must be 'true' or 'false'");
  }

  if (type == ConfigurationValueType::kSignedInteger) {
    std::int64_t value = 0;
    const auto conversion =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (conversion.ec != std::errc{} ||
        conversion.ptr != text.data() + text.size()) {
      return CommandLineError(
          "A signed command-line value is not an exact 64-bit integer");
    }
    return ConfigurationValue(value);
  }

  if (type == ConfigurationValueType::kUnsignedInteger) {
    std::uint64_t value = 0;
    const auto conversion =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (conversion.ec != std::errc{} ||
        conversion.ptr != text.data() + text.size()) {
      return CommandLineError(
          "An unsigned command-line value is not an exact 64-bit integer");
    }
    return ConfigurationValue(value);
  }

  if (type == ConfigurationValueType::kDouble) {
    const std::optional<double> value = ParseDouble(text);
    if (!value.has_value()) {
      return CommandLineError(
          "A double command-line value is not an exact number");
    }
    return ConfigurationValue(*value);
  }

  if (type == ConfigurationValueType::kString) {
    return ConfigurationValue::Utf8String(std::string(text));
  }

  return Status(ErrorCode::kUnsupported,
                "The command-line destination type is unsupported");
}

Result<ConfigurationValue> InsertValue(const ConfigurationValue& current,
                                       std::span<const std::string> path,
                                       ConfigurationValue value) {
  if (path.empty()) {
    return value;
  }

  ConfigurationValue::Object object;
  if (current.type() == ConfigurationValueType::kObject) {
    auto current_object = current.AsObject();
    if (!current_object.ok()) {
      return current_object.status();
    }
    object = current_object->get();
  } else if (!current.is_null()) {
    return Status(ErrorCode::kConfiguration,
                  "Command-line destinations overlap");
  }

  const auto existing = object.find(path.front());
  ConfigurationValue child;
  if (existing != object.end()) {
    child = existing->second;
  }

  auto inserted = InsertValue(child, path.subspan(1), std::move(value));
  if (!inserted.ok()) {
    return inserted.status();
  }
  object.insert_or_assign(path.front(), std::move(*inserted));
  return ConfigurationValue(std::move(object));
}

struct ParsedOptionValue {
  const CommandLineOption* option;
  ConfigurationValue value;
  std::size_t token_index;
};

Result<ConfigurationValueType> OptionType(const ConfigurationSchema& schema,
                                          const CommandLineOption& option) {
  auto tokens = ParsePointer(option.destination);
  if (!tokens.ok()) {
    return tokens.status();
  }
  auto destination = FindSchema(schema, *tokens);
  if (!destination.ok()) {
    return destination.status();
  }
  return destination->get().type();
}

}  // namespace internal_utilities_command_line

CommandLineParser::CommandLineParser(ConfigurationSchema schema,
                                     std::vector<CommandLineOption> options)
    : schema_(std::move(schema)), options_(std::move(options)) {}

Result<CommandLineParser> CommandLineParser::Create(
    ConfigurationSchema schema, std::span<const CommandLineOption> options) {
  std::set<std::string, std::less<>> long_names;
  std::set<char> short_names;
  std::set<std::string, std::less<>> destinations;

  for (const CommandLineOption& option : options) {
    if (!internal_utilities_command_line::IsAsciiName(option.long_name)) {
      return internal_utilities_command_line::CommandLineError(
          "A long option name must contain only ASCII letters, digits, '-' "
          "and '_', without a leading dash");
    }
    if (!long_names.insert(option.long_name).second) {
      return internal_utilities_command_line::CommandLineError(
          "Long option names must be unique");
    }
    if (option.short_name.has_value() &&
        !internal_utilities_command_line::IsAsciiShortName(
            *option.short_name)) {
      return internal_utilities_command_line::CommandLineError(
          "A short option name must be one ASCII letter or digit");
    }
    if (option.short_name.has_value() &&
        !short_names.insert(*option.short_name).second) {
      return internal_utilities_command_line::CommandLineError(
          "Short option names must be unique");
    }
    if (!destinations.insert(option.destination).second) {
      return internal_utilities_command_line::CommandLineError(
          "Command-line destination paths must be unique");
    }

    auto path =
        internal_utilities_command_line::ParsePointer(option.destination);
    if (!path.ok()) {
      return path.status();
    }
    auto destination =
        internal_utilities_command_line::FindSchema(schema, *path);
    if (!destination.ok()) {
      return destination.status();
    }
    if (!internal_utilities_command_line::IsSupportedLeaf(
            destination->get().type())) {
      return Status(
          ErrorCode::kUnsupported,
          "A command-line destination must be a supported scalar schema leaf");
    }
  }

  for (const CommandLineOption& option : options) {
    if (internal_utilities_command_line::FindLongOption(
            options, "no-" + option.long_name) != nullptr) {
      auto option_type =
          internal_utilities_command_line::OptionType(schema, option);
      if (!option_type.ok()) {
        return option_type.status();
      }
      if (*option_type == ConfigurationValueType::kBool) {
        return internal_utilities_command_line::CommandLineError(
            "An explicit long option collides with a boolean negation name");
      }
    }
  }

  return CommandLineParser(
      std::move(schema),
      std::vector<CommandLineOption>(options.begin(), options.end()));
}

// Parsing preserves one state machine so option-order behavior stays explicit.
// NOLINTNEXTLINE(readability-function-size)
Result<CommandLineParseResult> CommandLineParser::Parse(
    std::span<const std::string_view> arguments) const {
  using internal_utilities_command_line::CommandLineError;
  using internal_utilities_command_line::ParsedOptionValue;

  std::vector<ParsedOptionValue> parsed_options;
  std::vector<std::string> positional_arguments;
  std::set<std::string, std::less<>> supplied_destinations;
  bool recognize_options = true;

  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const std::string_view token = arguments[index];
    if (recognize_options && token == "--") {
      recognize_options = false;
      continue;
    }
    if (!recognize_options || token.empty() || token.front() != '-' ||
        token == "-") {
      positional_arguments.emplace_back(token);
      continue;
    }
    const std::size_t option_token_index = index;

    const CommandLineOption* option = nullptr;
    std::optional<std::string_view> inline_value;
    bool negated = false;

    if (token.starts_with("--")) {
      std::string_view name_and_value = token.substr(2);
      if (name_and_value.empty()) {
        return CommandLineError("A long option name cannot be empty");
      }
      const std::size_t equals = name_and_value.find('=');
      const std::string_view name = name_and_value.substr(0, equals);
      if (name.empty()) {
        return CommandLineError("A long option name cannot be empty");
      }
      if (equals != std::string_view::npos) {
        inline_value = name_and_value.substr(equals + 1);
      }

      option = internal_utilities_command_line::FindLongOption(options_, name);
      if (option == nullptr && name.starts_with("no-")) {
        option = internal_utilities_command_line::FindLongOption(
            options_, name.substr(3));
        if (option != nullptr) {
          auto type =
              internal_utilities_command_line::OptionType(schema_, *option);
          if (!type.ok()) {
            return type.status();
          }
          if (*type == ConfigurationValueType::kBool) {
            negated = true;
          } else {
            option = nullptr;
          }
        }
      }
      if (option == nullptr) {
        return CommandLineError("An unknown long option was supplied");
      }
      if (negated && inline_value.has_value()) {
        return CommandLineError(
            "A negated boolean option does not accept an attached value");
      }
    } else {
      if (token.size() != 2) {
        return Status(
            ErrorCode::kUnsupported,
            "Short option clusters and attached short values are unsupported");
      }
      option =
          internal_utilities_command_line::FindShortOption(options_, token[1]);
      if (option == nullptr) {
        return CommandLineError("An unknown short option was supplied");
      }
    }

    if (!supplied_destinations.insert(option->destination).second) {
      return Status(ErrorCode::kConfiguration,
                    "A command-line destination was supplied more than once");
    }

    auto type = internal_utilities_command_line::OptionType(schema_, *option);
    if (!type.ok()) {
      return type.status();
    }

    ConfigurationValue value;
    if (*type == ConfigurationValueType::kBool) {
      if (negated) {
        value = ConfigurationValue(false);
      } else if (inline_value.has_value()) {
        auto converted =
            internal_utilities_command_line::ParseValue(*inline_value, *type);
        if (!converted.ok()) {
          return converted.status();
        }
        value = std::move(*converted);
      } else if (token.starts_with("--") && index + 1 < arguments.size() &&
                 (arguments[index + 1] == "true" ||
                  arguments[index + 1] == "false")) {
        ++index;
        auto converted = internal_utilities_command_line::ParseValue(
            arguments[index], *type);
        if (!converted.ok()) {
          return converted.status();
        }
        value = std::move(*converted);
      } else {
        value = ConfigurationValue(true);
      }
    } else {
      std::string_view text;
      if (inline_value.has_value()) {
        text = *inline_value;
      } else {
        if (index + 1 >= arguments.size()) {
          return CommandLineError("A command-line option is missing its value");
        }
        text = arguments[++index];
      }
      auto converted = internal_utilities_command_line::ParseValue(text, *type);
      if (!converted.ok()) {
        return converted.status();
      }
      value = std::move(*converted);
    }

    parsed_options.push_back(
        ParsedOptionValue{option, std::move(value), option_token_index});
  }

  ConfigurationValue tree =
      schema_.type() == ConfigurationValueType::kObject
          ? ConfigurationValue(ConfigurationValue::Object{})
          : ConfigurationValue();
  ConfigurationOrigins origins;
  for (ParsedOptionValue& parsed : parsed_options) {
    auto path = internal_utilities_command_line::ParsePointer(
        parsed.option->destination);
    if (!path.ok()) {
      return path.status();
    }
    auto inserted = internal_utilities_command_line::InsertValue(
        tree, *path, std::move(parsed.value));
    if (!inserted.ok()) {
      return inserted.status();
    }
    tree = std::move(*inserted);
    origins.insert_or_assign(parsed.option->destination,
                             ConfigurationOrigin::CommandLine(
                                 "argv", std::to_string(parsed.token_index)));
  }

  auto validated = ValidateConfigurationWithOrigins(
      schema_, tree, origins, ConfigurationOrigin::Programmatic());
  if (!validated.ok()) {
    return validated.status();
  }
  return CommandLineParseResult{std::move(*validated),
                                std::move(positional_arguments)};
}

std::string CommandLineParser::RenderHelp() const {
  std::string output;
  for (const CommandLineOption& option : options_) {
    output.append("  ");
    if (option.short_name.has_value()) {
      output.push_back('-');
      output.push_back(*option.short_name);
      output.append(", ");
    } else {
      output.append("    ");
    }
    output.append("--");
    output.append(option.long_name);

    auto type = internal_utilities_command_line::OptionType(schema_, option);
    if (type.ok() && *type != ConfigurationValueType::kBool) {
      output.push_back(' ');
      output.push_back('<');
      output.append(option.value_name.empty() ? "VALUE" : option.value_name);
      output.push_back('>');
    }
    if (!option.help.empty()) {
      output.append("\t");
      output.append(option.help);
    }
    output.push_back('\n');
  }
  return output;
}

}  // namespace asc
