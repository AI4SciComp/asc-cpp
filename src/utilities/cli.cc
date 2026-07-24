// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include "asc/utilities/cli.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "asc/core/contracts.h"

namespace asc {
namespace {

bool IsAsciiWhitespace(char ch) noexcept {
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

std::string_view TrimView(std::string_view text) noexcept {
  std::size_t first = 0;
  while (first < text.size() && IsAsciiWhitespace(text[first])) {
    ++first;
  }
  std::size_t last = text.size();
  while (last > first && IsAsciiWhitespace(text[last - 1])) {
    --last;
  }
  return text.substr(first, last - first);
}

std::string_view RemoveFileComment(std::string_view text) noexcept {
  bool quoted = false;
  bool escaped = false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (quoted && ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"') {
      quoted = !quoted;
    } else if (!quoted && (ch == '#' || ch == ';')) {
      return text.substr(0, i);
    }
  }
  return text;
}

bool IsValidOptionName(std::string_view name) noexcept {
  if (name.empty()) {
    return true;
  }
  if (name.front() == '-') {
    return false;
  }
  return std::none_of(name.begin(), name.end(), [](char ch) {
    return IsAsciiWhitespace(ch) || ch == '=';
  });
}

Status StatusWithOption(const Status& status, const Option& option) {
  if (status.ok()) {
    return status;
  }
  std::string message = "Invalid value for command-line option '";
  if (!option.GetLongName().empty()) {
    message += "--" + option.GetLongName();
  } else {
    message += "-" + std::string(option.GetShortName());
  }
  message += "': ";
  message += status.message();
  return Status(status.code(), std::move(message));
}

}  // namespace

Option::Option(const char* short_name, const std::string& long_name,
               const std::string& description)
    : short_name_(short_name == nullptr ? "" : short_name),
      long_name_(long_name),
      description_(description) {
  if (short_name == nullptr ||
      (short_name_.empty() && long_name_.empty()) ||
      !IsValidOptionName(short_name_) || !IsValidOptionName(long_name_)) {
    detail::UtilitiesCompatibilityFailure(Status(
        StatusCode::kInvalidArgument,
        "Command-line option names must be non-empty, unprefixed, and may "
        "not contain whitespace or '='"));
  }
}

void Option::SetAttribute(int attribute) {
  if (attribute != kOptional && attribute != kRequired) {
    detail::UtilitiesCompatibilityFailure(
        Status(StatusCode::kInvalidArgument,
               "Command-line option attribute is invalid"));
  }
  attribute_ = attribute;
}

Status Option::ValidateValue(std::string_view) const { return Status::Ok(); }

void OptionParser::AddOptionImpl(const std::shared_ptr<Option>& option) {
  ASC_REQUIRE(option != nullptr,
              "OptionParser requires a non-null option declaration");
  for (const auto& existing : options_) {
    const bool duplicate_short =
        option->GetShortName()[0] != '\0' &&
        std::string_view(existing->GetShortName()) == option->GetShortName();
    const bool duplicate_long =
        !option->GetLongName().empty() &&
        existing->GetLongName() == option->GetLongName();
    if (duplicate_short || duplicate_long) {
      detail::UtilitiesCompatibilityFailure(Status(
          StatusCode::kInvalidArgument,
          "A command-line option with the same short or long name exists"));
    }
  }
  options_.push_back(option);
}

std::shared_ptr<Option> OptionParser::FindOption(
    const char* short_name) const {
  if (short_name == nullptr) {
    return nullptr;
  }
  for (const auto& option : options_) {
    if (std::string_view(option->GetShortName()) == short_name) {
      return option;
    }
  }
  return nullptr;
}

std::shared_ptr<Option> OptionParser::FindOption(
    const std::string& long_name) const {
  for (const auto& option : options_) {
    if (option->GetLongName() == long_name) {
      return option;
    }
  }
  return nullptr;
}

Status OptionParser::TryParse(int argc, const char* const argv[]) {
  ASC_REQUIRE(argc >= 0, "OptionParser argument count must be non-negative");
  ASC_REQUIRE(argc == 0 || argv != nullptr,
              "OptionParser requires an argument vector");

  std::vector<std::pair<std::shared_ptr<Option>, std::string>> candidates;
  std::unordered_set<const Option*> supplied;
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == nullptr) {
      return Status(StatusCode::kInvalidArgument,
                    "Command-line argument must not be null");
    }
    std::string_view token(argv[i]);
    if (token == "--") {
      if (i + 1 < argc) {
        return Status(StatusCode::kInvalidArgument,
                      "Positional command-line arguments are unsupported");
      }
      break;
    }

    std::shared_ptr<Option> option;
    std::string value;
    bool has_inline_value = false;
    if (token.starts_with("--") && token.size() > 2) {
      token.remove_prefix(2);
      const std::size_t equals = token.find('=');
      const std::string name(token.substr(0, equals));
      option = FindOption(name);
      if (equals != std::string_view::npos) {
        value = std::string(token.substr(equals + 1));
        has_inline_value = true;
      }
    } else if (token.starts_with('-') && token.size() > 1) {
      token.remove_prefix(1);
      option = FindOption(std::string(token).c_str());
    } else {
      return Status(StatusCode::kInvalidArgument,
                    "Positional command-line arguments are unsupported");
    }

    if (!option) {
      return Status(StatusCode::kInvalidArgument,
                    "Unknown command-line option");
    }
    if (option->TakesValue()) {
      if (!has_inline_value) {
        if (i + 1 >= argc || argv[i + 1] == nullptr) {
          return Status(StatusCode::kInvalidArgument,
                        "Command-line option is missing its value");
        }
        value = argv[++i];
      }
    } else if (has_inline_value) {
      return StatusWithOption(
          Status(StatusCode::kInvalidArgument,
                 "A command-line switch does not accept an inline value"),
          *option);
    }

    Status validation = option->ValidateValue(value);
    if (!validation.ok()) {
      return StatusWithOption(validation, *option);
    }
    supplied.insert(option.get());
    candidates.emplace_back(std::move(option), std::move(value));
  }

  for (const auto& option : options_) {
    if (option->GetAttribute() == kRequired &&
        !supplied.contains(option.get())) {
      return Status(StatusCode::kInvalidArgument,
                    "A required command-line option was not provided");
    }
  }
  return CommitCandidates(candidates);
}

Status OptionParser::TryParseFile(const std::string& filename) {
  std::ifstream input(filename);
  if (!input.is_open()) {
    return Status(StatusCode::kUnavailable,
                  "Unable to open command-line option file");
  }

  std::vector<std::pair<std::shared_ptr<Option>, std::string>> candidates;
  std::unordered_set<const Option*> supplied;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    std::string_view text = TrimView(RemoveFileComment(line));
    if (text.empty()) {
      continue;
    }
    const std::size_t equals = text.find('=');
    if (equals == std::string_view::npos) {
      return Status(StatusCode::kInvalidArgument,
                    "Option file entry is missing '=' on line " +
                        std::to_string(line_number));
    }
    std::string name(TrimView(text.substr(0, equals)));
    std::string value(TrimView(text.substr(equals + 1)));
    while (!name.empty() && name.front() == '-') {
      name.erase(name.begin());
    }
    std::shared_ptr<Option> option = FindOption(name);
    if (!option) {
      option = FindOption(name.c_str());
    }
    if (!option) {
      return Status(StatusCode::kInvalidArgument,
                    "Unknown option-file key on line " +
                        std::to_string(line_number));
    }
    if (!option->TakesValue()) {
      if (!value.empty() &&
          !detail::EqualsCliTextIgnoreCase(value, "true")) {
        return Status(StatusCode::kInvalidArgument,
                      "Option-file switch value must be true on line " +
                          std::to_string(line_number));
      }
      value.clear();
    }
    Status validation = option->ValidateValue(value);
    if (!validation.ok()) {
      return StatusWithOption(validation, *option);
    }
    supplied.insert(option.get());
    candidates.emplace_back(std::move(option), std::move(value));
  }
  if (!input.eof() && input.fail()) {
    return Status(StatusCode::kUnavailable,
                  "Failed while reading command-line option file");
  }
  for (const auto& option : options_) {
    if (option->GetAttribute() == kRequired &&
        !supplied.contains(option.get())) {
      return Status(StatusCode::kInvalidArgument,
                    "A required command-line option was not provided");
    }
  }
  return CommitCandidates(candidates);
}

Status OptionParser::CommitCandidates(
    const std::vector<std::pair<std::shared_ptr<Option>, std::string>>&
        candidates) {
  for (const auto& option : options_) {
    option->ResetForParse();
  }
  for (const auto& [option, value] : candidates) {
    option->Parse(value);
  }
  return Status::Ok();
}

std::string OptionParser::GetHelp() const { return Message(true); }

std::string OptionParser::GetUsage() const { return Message(false); }

void OptionParser::PrintHelp(std::ostream& stream) const {
  stream << GetHelp();
}

void OptionParser::PrintUsage(std::ostream& stream) const {
  stream << GetUsage();
}

std::string OptionParser::Message(bool help) const {
  std::ostringstream output;
  if (!description_.empty()) {
    output << description_ << ":\n";
  }

  std::size_t margin = 40;
  for (const auto& option : options_) {
    const std::string label =
        help ? OptionToHelp(option) : OptionToUsage(option);
    margin = std::max(margin, std::min<std::size_t>(78, label.size() + 2));
  }

  for (const auto& option : options_) {
    std::string label = help ? OptionToHelp(option) : OptionToUsage(option);
    if (label.size() < margin) {
      label.resize(margin, ' ');
    } else {
      label += '\n';
      label.append(margin, ' ');
    }
    output << label;

    std::istringstream description(option->GetDescription());
    std::string line;
    bool first = true;
    while (std::getline(description, line)) {
      if (!first) {
        output << '\n' << std::string(margin, ' ');
      }
      output << line;
      first = false;
    }
    output << '\n';
  }
  return output.str();
}

std::string OptionParser::OptionToHelp(
    const std::shared_ptr<Option>& option) const {
  std::ostringstream output;
  output << "  ";
  if (option->GetShortName()[0] != '\0') {
    output << '-' << option->GetShortName();
    if (!option->GetLongName().empty()) {
      output << ", ";
    }
  }
  if (!option->GetLongName().empty()) {
    output << "--" << option->GetLongName();
  }
  if (option->GetAttribute() == kRequired) {
    output << " (required)";
  } else {
    output << " (optional";
    std::stringstream default_value;
    if (option->GetDefault(default_value)) {
      output << ", default=" << default_value.str();
    }
    output << ')';
  }
  return output.str();
}

std::string OptionParser::OptionToUsage(
    const std::shared_ptr<Option>& option) const {
  std::ostringstream output;
  output << "  " << option->GetLongName() << ' ';
  if (std::dynamic_pointer_cast<Variable<int>>(option)) {
    output << "(int)";
  } else if (std::dynamic_pointer_cast<Variable<float>>(option)) {
    output << "(float)";
  } else if (std::dynamic_pointer_cast<Variable<double>>(option)) {
    output << "(double)";
  } else if (std::dynamic_pointer_cast<Variable<std::string>>(option)) {
    output << "(string)";
  } else if (std::dynamic_pointer_cast<Variable<bool>>(option)) {
    output << "(bool)";
  }

  std::stringstream value;
  if (option->IsSet()) {
    value << option->GetValue();
  } else if (!option->GetDefault(value)) {
    value << "n/a";
  }
  output << " = " << value.str();
  return output.str();
}

}  // namespace asc
