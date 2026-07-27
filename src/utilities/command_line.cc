#include "asc/utilities/command_line.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
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

struct PendingValue {
  std::optional<ConfigurationValue> value;
  std::map<std::string, PendingValue, std::less<>> children;
};

bool IsAsciiAlphanumeric(char character) noexcept {
  return (character >= 'a' && character <= 'z') ||
         (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9');
}

bool IsValidLongName(std::string_view name) noexcept {
  if (name.empty() || !IsAsciiAlphanumeric(name.front())) {
    return false;
  }
  for (char character : name) {
    if (!IsAsciiAlphanumeric(character) && character != '-') {
      return false;
    }
  }
  return true;
}

Result<std::vector<std::string>> ParseJsonPointer(std::string_view path) {
  std::vector<std::string> tokens;
  if (path.empty()) {
    return tokens;
  }
  if (path.front() != '/') {
    return Status(ErrorCode::kInvalidArgument,
                  "A configuration path must be a JSON Pointer");
  }

  std::size_t token_start = 1;
  while (true) {
    const std::size_t token_end = path.find('/', token_start);
    const std::string_view escaped =
        path.substr(token_start, token_end - token_start);
    std::string token;
    token.reserve(escaped.size());
    for (std::size_t index = 0; index < escaped.size(); ++index) {
      if (escaped[index] != '~') {
        token.push_back(escaped[index]);
        continue;
      }
      if (index + 1 >= escaped.size()) {
        return Status(ErrorCode::kInvalidArgument,
                      "A configuration path has an incomplete escape");
      }
      ++index;
      if (escaped[index] == '0') {
        token.push_back('~');
      } else if (escaped[index] == '1') {
        token.push_back('/');
      } else {
        return Status(ErrorCode::kInvalidArgument,
                      "A configuration path has an invalid escape");
      }
    }
    tokens.push_back(std::move(token));
    if (token_end == std::string_view::npos) {
      break;
    }
    token_start = token_end + 1;
  }
  return tokens;
}

Result<std::reference_wrapper<const ConfigurationSchema>> FindSchema(
    const ConfigurationSchema& schema,
    std::span<const std::string> path_tokens) {
  const ConfigurationSchema* current = &schema;
  for (const std::string& token : path_tokens) {
    if (current->type() != ConfigurationValueType::kObject) {
      return Status(ErrorCode::kInvalidArgument,
                    "An option path traverses a non-object schema field");
    }
    const auto field = current->fields().find(token);
    if (field == current->fields().end()) {
      return Status(ErrorCode::kInvalidArgument,
                    "An option path is not present in the schema");
    }
    current = &field->second;
  }
  return std::cref(*current);
}

Result<bool> IsSensitiveSchemaPath(const ConfigurationSchema& schema,
                                   std::span<const std::string> path_tokens) {
  const ConfigurationSchema* current = &schema;
  bool sensitive = current->sensitive();
  for (const std::string& token : path_tokens) {
    if (current->type() != ConfigurationValueType::kObject) {
      return Status(ErrorCode::kInvalidArgument,
                    "An option path traverses a non-object schema field");
    }
    const auto field = current->fields().find(token);
    if (field == current->fields().end()) {
      return Status(ErrorCode::kInvalidArgument,
                    "An option path is not present in the schema");
    }
    current = &field->second;
    sensitive = sensitive || current->sensitive();
  }
  return sensitive;
}

bool IsSupportedLeaf(ConfigurationValueType type) noexcept {
  return type == ConfigurationValueType::kBool ||
         type == ConfigurationValueType::kSignedInteger ||
         type == ConfigurationValueType::kUnsignedInteger ||
         type == ConfigurationValueType::kDouble ||
         type == ConfigurationValueType::kString;
}

Result<ConfigurationValue> ParseValue(ConfigurationValueType type,
                                      std::string_view text) {
  if (type == ConfigurationValueType::kBool) {
    if (text == "true") {
      return ConfigurationValue(true);
    }
    if (text == "false") {
      return ConfigurationValue(false);
    }
    return Status(ErrorCode::kInvalidArgument,
                  "A boolean option value must be true or false");
  }

  if (type == ConfigurationValueType::kSignedInteger) {
    std::int64_t value = 0;
    const auto conversion =
        std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (conversion.ec != std::errc() ||
        conversion.ptr != text.data() + text.size()) {
      return Status(ErrorCode::kInvalidArgument,
                    "A signed integer option value is invalid or out of range");
    }
    return ConfigurationValue(value);
  }

  if (type == ConfigurationValueType::kUnsignedInteger) {
    std::uint64_t value = 0;
    const auto conversion =
        std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (conversion.ec != std::errc() ||
        conversion.ptr != text.data() + text.size()) {
      return Status(
          ErrorCode::kInvalidArgument,
          "An unsigned integer option value is invalid or out of range");
    }
    return ConfigurationValue(value);
  }

  if (type == ConfigurationValueType::kDouble) {
    double value = 0.0;
    const auto conversion =
        std::from_chars(text.data(), text.data() + text.size(), value,
                        std::chars_format::general);
    if (conversion.ec != std::errc() ||
        conversion.ptr != text.data() + text.size()) {
      return Status(ErrorCode::kInvalidArgument,
                    "A double option value is invalid or out of range");
    }
    return ConfigurationValue(value);
  }

  if (type == ConfigurationValueType::kString) {
    return ConfigurationValue::Utf8String(std::string(text));
  }

  return Status(ErrorCode::kUnsupported,
                "The option destination type is unsupported");
}

template <typename Integer>
std::string RenderInteger(Integer value) {
  char buffer[32];
  const auto conversion =
      std::to_chars(buffer, buffer + sizeof(buffer), value, 10);
  if (conversion.ec != std::errc()) {
    return "<unrepresentable>";
  }
  return std::string(buffer, conversion.ptr);
}

std::string RenderDouble(double value) {
  char buffer[64];
  const auto conversion = std::to_chars(buffer, buffer + sizeof(buffer), value,
                                        std::chars_format::general);
  if (conversion.ec != std::errc()) {
    return "<unrepresentable>";
  }
  return std::string(buffer, conversion.ptr);
}

std::string AcceptedValueDescription(const ConfigurationSchema& schema) {
  std::string description(ConfigurationValueTypeName(schema.type()));
  if (schema.type() == ConfigurationValueType::kBool) {
    description.append(" (true or false)");
  } else if (schema.type() == ConfigurationValueType::kSignedInteger &&
             (schema.signed_minimum().has_value() ||
              schema.signed_maximum().has_value())) {
    description.append(" in [");
    description.append(RenderInteger(schema.signed_minimum().value_or(
        std::numeric_limits<std::int64_t>::min())));
    description.append(", ");
    description.append(RenderInteger(schema.signed_maximum().value_or(
        std::numeric_limits<std::int64_t>::max())));
    description.push_back(']');
  } else if (schema.type() == ConfigurationValueType::kUnsignedInteger &&
             (schema.unsigned_minimum().has_value() ||
              schema.unsigned_maximum().has_value())) {
    description.append(" in [");
    description.append(RenderInteger(schema.unsigned_minimum().value_or(0)));
    description.append(", ");
    description.append(RenderInteger(schema.unsigned_maximum().value_or(
        std::numeric_limits<std::uint64_t>::max())));
    description.push_back(']');
  } else if (schema.type() == ConfigurationValueType::kDouble &&
             (schema.double_minimum().has_value() ||
              schema.double_maximum().has_value())) {
    description.append(" in [");
    description.append(RenderDouble(schema.double_minimum().value_or(
        -std::numeric_limits<double>::infinity())));
    description.append(", ");
    description.append(RenderDouble(schema.double_maximum().value_or(
        std::numeric_limits<double>::infinity())));
    description.push_back(']');
  } else if (schema.type() == ConfigurationValueType::kString &&
             (schema.size_minimum().has_value() ||
              schema.size_maximum().has_value())) {
    description.append(" with byte length in [");
    description.append(RenderInteger(schema.size_minimum().value_or(0)));
    description.append(", ");
    description.append(RenderInteger(schema.size_maximum().value_or(
        std::numeric_limits<std::size_t>::max())));
    description.push_back(']');
  }
  return description;
}

std::string QuoteDiagnostic(std::string_view value) {
  constexpr char kHex[] = "0123456789abcdef";
  std::string output = "'";
  for (char character : value) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte >= 0x20U && byte <= 0x7eU && byte != '\\' && byte != '\'') {
      output.push_back(static_cast<char>(byte));
    } else if (byte == '\\' || byte == '\'') {
      output.push_back('\\');
      output.push_back(static_cast<char>(byte));
    } else {
      output.append("\\x");
      output.push_back(kHex[byte >> 4U]);
      output.push_back(kHex[byte & 0x0fU]);
    }
  }
  output.push_back('\'');
  return output;
}

std::string ArgumentContext(std::size_t index,
                            std::string_view option_spelling) {
  return "argv[" + std::to_string(index) + "] option " +
         QuoteDiagnostic(option_spelling);
}

Status SetPendingValue(PendingValue& root,
                       std::span<const std::string> path_tokens,
                       ConfigurationValue value) {
  PendingValue* current = &root;
  for (const std::string& token : path_tokens) {
    if (current->value.has_value()) {
      return Status(ErrorCode::kInternal,
                    "An option path conflicts with a scalar destination");
    }
    current = &current->children[token];
  }
  if (current->value.has_value() || !current->children.empty()) {
    return Status(ErrorCode::kInternal,
                  "An option destination was populated more than once");
  }
  current->value = std::move(value);
  return Status::Ok();
}

Result<ConfigurationValue> BuildConfigurationValue(PendingValue node) {
  if (node.value.has_value()) {
    return std::move(*node.value);
  }

  ConfigurationValue::Object object;
  for (auto& [key, child] : node.children) {
    auto child_value = BuildConfigurationValue(std::move(child));
    if (!child_value.ok()) {
      return child_value.status();
    }
    object.emplace(std::move(key), std::move(*child_value));
  }
  return ConfigurationValue(std::move(object));
}

std::string OptionDisplay(const CommandLineOption& option,
                          ConfigurationValueType type) {
  std::string display = "  ";
  if (option.short_name.has_value()) {
    display.push_back('-');
    display.push_back(*option.short_name);
    display.append(", ");
  } else {
    display.append("    ");
  }
  if (type == ConfigurationValueType::kBool) {
    display.append("--[no-]");
    display.append(option.long_name);
  } else {
    display.append("--");
    display.append(option.long_name);
    display.append(" <");
    display.append(option.value_name.empty() ? "VALUE" : option.value_name);
    display.push_back('>');
  }
  return display;
}

}  // namespace internal_utilities_command_line

CommandLineParser::CommandLineParser(ConfigurationSchema schema,
                                     std::vector<CommandLineOption> options)
    : schema_(std::move(schema)), options_(std::move(options)) {}

Result<CommandLineParser> CommandLineParser::Create(
    ConfigurationSchema schema, std::vector<CommandLineOption> options) {
  if (schema.type() != ConfigurationValueType::kObject) {
    return Status(ErrorCode::kInvalidArgument,
                  "A command-line configuration schema root must be an object");
  }

  std::set<std::string, std::less<>> long_names;
  std::set<char> short_names;
  std::set<std::string, std::less<>> paths;
  std::map<std::string, bool, std::less<>> boolean_long_names;

  for (const CommandLineOption& option : options) {
    if (!internal_utilities_command_line::IsValidLongName(option.long_name)) {
      return Status(
          ErrorCode::kInvalidArgument,
          "A long option name must contain only ASCII letters, digits, and "
          "hyphens, and must start with a letter or digit");
    }
    if (!long_names.insert(option.long_name).second) {
      return Status(ErrorCode::kInvalidArgument,
                    "Long option names must be unique");
    }
    if (option.short_name.has_value() &&
        (!internal_utilities_command_line::IsAsciiAlphanumeric(
             *option.short_name) ||
         !short_names.insert(*option.short_name).second)) {
      return Status(ErrorCode::kInvalidArgument,
                    "A short option must be a unique ASCII letter or digit");
    }
    if (!paths.insert(option.configuration_path).second) {
      return Status(ErrorCode::kInvalidArgument,
                    "Option destination paths must be unique");
    }

    auto path_tokens = internal_utilities_command_line::ParseJsonPointer(
        option.configuration_path);
    if (!path_tokens.ok()) {
      return path_tokens.status();
    }
    auto destination =
        internal_utilities_command_line::FindSchema(schema, *path_tokens);
    if (!destination.ok()) {
      return destination.status();
    }
    const ConfigurationValueType type = destination->get().type();
    if (!internal_utilities_command_line::IsSupportedLeaf(type)) {
      return Status(ErrorCode::kUnsupported,
                    "An option must target a supported scalar schema field");
    }
    boolean_long_names.emplace(option.long_name,
                               type == ConfigurationValueType::kBool);
  }

  for (const auto& [long_name, is_boolean] : boolean_long_names) {
    if (is_boolean && long_names.contains("no-" + long_name)) {
      return Status(
          ErrorCode::kInvalidArgument,
          "A long option conflicts with a generated boolean negation");
    }
  }
  return CommandLineParser(std::move(schema), std::move(options));
}

Result<CommandLineParseResult> CommandLineParser::Parse(
    std::span<const std::string_view> arguments) const {
  std::map<std::string_view, std::size_t, std::less<>> long_options;
  std::map<char, std::size_t> short_options;
  std::vector<ConfigurationValueType> option_types;
  std::vector<bool> option_sensitive;
  std::vector<const ConfigurationSchema*> option_schemas;
  option_types.reserve(options_.size());
  option_sensitive.reserve(options_.size());
  option_schemas.reserve(options_.size());
  for (std::size_t index = 0; index < options_.size(); ++index) {
    const CommandLineOption& option = options_[index];
    long_options.emplace(option.long_name, index);
    if (option.short_name.has_value()) {
      short_options.emplace(*option.short_name, index);
    }
    auto path_tokens = internal_utilities_command_line::ParseJsonPointer(
        option.configuration_path);
    if (!path_tokens.ok()) {
      return path_tokens.status();
    }
    auto destination =
        internal_utilities_command_line::FindSchema(schema_, *path_tokens);
    if (!destination.ok()) {
      return destination.status();
    }
    option_types.push_back(destination->get().type());
    option_schemas.push_back(&destination->get());
    auto sensitive = internal_utilities_command_line::IsSensitiveSchemaPath(
        schema_, *path_tokens);
    if (!sensitive.ok()) {
      return sensitive.status();
    }
    option_sensitive.push_back(*sensitive);
  }

  internal_utilities_command_line::PendingValue pending;
  ConfigurationOrigins origins;
  std::set<std::string, std::less<>> seen_paths;
  std::vector<std::string> positional_arguments;
  bool recognize_options = true;

  for (std::size_t argument_index = 0; argument_index < arguments.size();
       ++argument_index) {
    const std::size_t option_token_index = argument_index;
    const std::string_view token = arguments[argument_index];
    if (!recognize_options) {
      positional_arguments.emplace_back(token);
      continue;
    }
    if (token == "--") {
      recognize_options = false;
      continue;
    }
    if (token == "-" || token.empty() || token.front() != '-') {
      positional_arguments.emplace_back(token);
      continue;
    }

    std::size_t option_index = 0;
    bool negated_boolean = false;
    std::optional<std::string_view> inline_value;

    if (token.starts_with("--")) {
      std::string_view name_and_value = token.substr(2);
      if (name_and_value.empty()) {
        return Status(ErrorCode::kInvalidArgument,
                      "argv[" + std::to_string(option_token_index) +
                          "] has an empty long option name");
      }
      const std::size_t equals = name_and_value.find('=');
      const std::string_view name = name_and_value.substr(0, equals);
      if (equals != std::string_view::npos) {
        inline_value = name_and_value.substr(equals + 1);
      }

      auto option = long_options.find(name);
      if (option == long_options.end() && name.starts_with("no-")) {
        option = long_options.find(name.substr(3));
        if (option != long_options.end() &&
            option_types[option->second] == ConfigurationValueType::kBool) {
          negated_boolean = true;
        } else {
          option = long_options.end();
        }
      }
      if (option == long_options.end()) {
        std::string safe_name(name);
        for (std::size_t index = 0; index < options_.size(); ++index) {
          if (!option_sensitive[index]) {
            continue;
          }
          const std::string_view sensitive_name = options_[index].long_name;
          if (name.size() > sensitive_name.size() &&
              name.starts_with(sensitive_name)) {
            safe_name = std::string(sensitive_name) + "<redacted>";
            break;
          }
          const std::string negated_name = "no-" + std::string(sensitive_name);
          if (name.size() > negated_name.size() &&
              name.starts_with(negated_name)) {
            safe_name = negated_name + "<redacted>";
            break;
          }
        }
        return Status(ErrorCode::kInvalidArgument,
                      "argv[" + std::to_string(option_token_index) +
                          "] contains unknown option " +
                          internal_utilities_command_line::QuoteDiagnostic(
                              "--" + safe_name));
      }
      option_index = option->second;
      if (negated_boolean && inline_value.has_value()) {
        return Status(
            ErrorCode::kInvalidArgument,
            internal_utilities_command_line::ArgumentContext(
                option_token_index, "--" + options_[option_index].long_name) +
                " is a negated boolean and cannot have a value");
      }
    } else {
      if (token.size() != 2 ||
          !internal_utilities_command_line::IsAsciiAlphanumeric(token[1])) {
        std::string safe_spelling = "-";
        if (token.size() >= 2 &&
            internal_utilities_command_line::IsAsciiAlphanumeric(token[1])) {
          safe_spelling.push_back(token[1]);
        } else {
          safe_spelling.append("<malformed>");
        }
        return Status(ErrorCode::kInvalidArgument,
                      "argv[" + std::to_string(option_token_index) +
                          "] has malformed short option " +
                          internal_utilities_command_line::QuoteDiagnostic(
                              safe_spelling));
      }
      const auto option = short_options.find(token[1]);
      if (option == short_options.end()) {
        return Status(
            ErrorCode::kInvalidArgument,
            "argv[" + std::to_string(option_token_index) +
                "] contains unknown option " +
                internal_utilities_command_line::QuoteDiagnostic(token));
      }
      option_index = option->second;
    }

    const CommandLineOption& option = options_[option_index];
    const ConfigurationValueType type = option_types[option_index];
    std::string option_spelling = "--" + option.long_name;
    if (!token.starts_with("--") && option.short_name.has_value()) {
      option_spelling = "-";
      option_spelling.push_back(*option.short_name);
    }
    if (!seen_paths.insert(option.configuration_path).second) {
      return Status(ErrorCode::kInvalidArgument,
                    internal_utilities_command_line::ArgumentContext(
                        option_token_index, option_spelling) +
                        " supplies a destination more than once");
    }

    Result<ConfigurationValue> parsed_value =
        Status(ErrorCode::kInternal, "Option value was not parsed");
    if (type == ConfigurationValueType::kBool && !inline_value.has_value()) {
      parsed_value = ConfigurationValue(!negated_boolean);
    } else {
      if (negated_boolean) {
        return Status(ErrorCode::kInvalidArgument,
                      internal_utilities_command_line::ArgumentContext(
                          option_token_index, option_spelling) +
                          " is a negated boolean and cannot have a value");
      }
      std::string_view value;
      if (inline_value.has_value()) {
        value = *inline_value;
      } else {
        if (argument_index + 1 >= arguments.size()) {
          return Status(
              ErrorCode::kInvalidArgument,
              internal_utilities_command_line::ArgumentContext(
                  option_token_index, option_spelling) +
                  " expects " +
                  internal_utilities_command_line::AcceptedValueDescription(
                      *option_schemas[option_index]) +
                  " but its value is missing");
        }
        value = arguments[++argument_index];
      }
      parsed_value = internal_utilities_command_line::ParseValue(type, value);
    }
    if (!parsed_value.ok()) {
      std::string message =
          internal_utilities_command_line::ArgumentContext(option_token_index,
                                                           option_spelling) +
          " expects " +
          internal_utilities_command_line::AcceptedValueDescription(
              *option_schemas[option_index]) +
          "; received ";
      if (option_sensitive[option_index]) {
        message.append("<redacted>");
      } else {
        std::string_view received;
        if (inline_value.has_value()) {
          received = *inline_value;
        } else {
          received = arguments[argument_index];
        }
        message.append(
            internal_utilities_command_line::QuoteDiagnostic(received));
      }
      message.append(": ");
      message.append(parsed_value.status().message());
      return Status(parsed_value.status().code(), std::move(message));
    }

    const auto leaf_validation =
        ValidateConfiguration(*option_schemas[option_index], *parsed_value,
                              ConfigurationOrigin::CommandLine(
                                  "argv", std::to_string(option_token_index)));
    if (!leaf_validation.ok()) {
      std::string message =
          internal_utilities_command_line::ArgumentContext(option_token_index,
                                                           option_spelling) +
          " expects " +
          internal_utilities_command_line::AcceptedValueDescription(
              *option_schemas[option_index]) +
          "; received ";
      if (option_sensitive[option_index]) {
        message.append("<redacted>");
      } else {
        std::string_view received;
        if (inline_value.has_value()) {
          received = *inline_value;
        } else {
          received = arguments[argument_index];
        }
        message.append(
            internal_utilities_command_line::QuoteDiagnostic(received));
      }
      message.append(": ");
      message.append(leaf_validation.status().message());
      return Status(leaf_validation.status().code(), std::move(message));
    }

    auto path_tokens = internal_utilities_command_line::ParseJsonPointer(
        option.configuration_path);
    if (!path_tokens.ok()) {
      return path_tokens.status();
    }
    const Status set_status = internal_utilities_command_line::SetPendingValue(
        pending, *path_tokens, std::move(*parsed_value));
    if (!set_status.ok()) {
      return set_status;
    }
    origins.emplace(option.configuration_path,
                    ConfigurationOrigin::CommandLine(
                        "argv", std::to_string(option_token_index)));
  }

  auto configuration_value =
      internal_utilities_command_line::BuildConfigurationValue(
          std::move(pending));
  if (!configuration_value.ok()) {
    return configuration_value.status();
  }
  auto configuration = ValidateConfigurationWithOrigins(
      schema_, *configuration_value, origins,
      ConfigurationOrigin::CommandLine("argv"));
  if (!configuration.ok()) {
    return configuration.status();
  }
  return CommandLineParseResult{
      .configuration = std::move(*configuration),
      .positional_arguments = std::move(positional_arguments),
  };
}

std::string CommandLineParser::RenderHelp(std::string_view program_name) const {
  std::vector<std::string> displays;
  displays.reserve(options_.size());
  std::size_t display_width = 0;
  for (const CommandLineOption& option : options_) {
    auto path_tokens = internal_utilities_command_line::ParseJsonPointer(
        option.configuration_path);
    auto destination =
        internal_utilities_command_line::FindSchema(schema_, *path_tokens);
    displays.push_back(internal_utilities_command_line::OptionDisplay(
        option, destination->get().type()));
    display_width = std::max(display_width, displays.back().size());
  }

  std::string output = "Usage: ";
  output.append(program_name);
  output.append(" [options] [--] [arguments...]\n\nOptions:\n");
  for (std::size_t index = 0; index < options_.size(); ++index) {
    output.append(displays[index]);
    output.append(display_width - displays[index].size() + 2U, ' ');
    output.append(options_[index].help);
    output.push_back('\n');
  }
  return output;
}

}  // namespace asc
