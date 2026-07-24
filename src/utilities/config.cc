// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include "asc/utilities/config.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "asc/core/contracts.h"

namespace asc {
namespace detail {

[[noreturn]] void UtilitiesCompatibilityFailure(const Status& status);

}  // namespace detail
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

bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    char left = lhs[i];
    char right = rhs[i];
    if (left >= 'A' && left <= 'Z') {
      left = static_cast<char>(left - 'A' + 'a');
    }
    if (right >= 'A' && right <= 'Z') {
      right = static_cast<char>(right - 'A' + 'a');
    }
    if (left != right) {
      return false;
    }
  }
  return true;
}

bool IsValidConfigName(std::string_view name) noexcept {
  if (name.empty()) {
    return false;
  }
  const auto is_letter = [](char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
  };
  if (!is_letter(name.front()) && name.front() != '_') {
    return false;
  }
  return std::all_of(name.begin() + 1, name.end(), [&](char ch) {
    return is_letter(ch) || (ch >= '0' && ch <= '9') || ch == '_' ||
           ch == '.' || ch == '-';
  });
}

std::string_view RemoveComment(std::string_view text) noexcept {
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
    } else if (!quoted && ch == '#') {
      return text.substr(0, i);
    }
  }
  return text;
}

Status ValidateRepresentable(const ConfigValue& value) {
  if (value.IsDouble()) {
    if (!std::isfinite(value.AsDouble())) {
      return Status(StatusCode::kInvalidArgument,
                    "Configuration doubles must be finite");
    }
  } else if (value.IsVector()) {
    for (const double item : value.AsVector()) {
      if (!std::isfinite(item)) {
        return Status(StatusCode::kInvalidArgument,
                      "Configuration vector values must be finite");
      }
    }
  } else if (value.IsMatrix()) {
    const ConfigMatrix& matrix = value.AsMatrix();
    const std::size_t columns = matrix.empty() ? 0 : matrix.front().size();
    for (const ConfigVector& row : matrix) {
      if (row.size() != columns) {
        return Status(StatusCode::kInvalidArgument,
                      "Configuration matrices must be rectangular");
      }
      for (const double item : row) {
        if (!std::isfinite(item)) {
          return Status(StatusCode::kInvalidArgument,
                        "Configuration matrix values must be finite");
        }
      }
    }
  }
  return Status::Ok();
}

Result<double> ParseFiniteDouble(std::string_view text) {
  text = TrimView(text);
  if (text.empty()) {
    return Status(StatusCode::kInvalidArgument,
                  "Configuration floating-point value is empty");
  }
  double value = 0;
  const char* const first = text.data();
  const char* const last = first + text.size();
  const std::from_chars_result parsed =
      std::from_chars(first, last, value, std::chars_format::general);
  if (parsed.ec == std::errc::result_out_of_range) {
    return Status(StatusCode::kOutOfRange,
                  "Configuration floating-point value is out of range");
  }
  if (parsed.ec != std::errc() || parsed.ptr != last ||
      !std::isfinite(value)) {
    return Status(StatusCode::kInvalidArgument,
                  "Configuration floating-point value is invalid");
  }
  return value;
}

Result<std::string> ParseQuotedString(std::string_view text) {
  if (text.size() < 2 || text.front() != '"' || text.back() != '"') {
    return Status(StatusCode::kInvalidArgument,
                  "Configuration string must be quoted");
  }
  std::string result;
  result.reserve(text.size() - 2);
  for (std::size_t i = 1; i + 1 < text.size(); ++i) {
    char ch = text[i];
    if (ch != '\\') {
      if (ch == '"') {
        return Status(StatusCode::kInvalidArgument,
                      "Configuration string contains an unescaped quote");
      }
      if (static_cast<unsigned char>(ch) < 0x20) {
        return Status(StatusCode::kInvalidArgument,
                      "Configuration string contains an unescaped control");
      }
      result.push_back(ch);
      continue;
    }
    ++i;
    if (i + 1 >= text.size()) {
      return Status(StatusCode::kInvalidArgument,
                    "Configuration string ends in an escape");
    }
    switch (text[i]) {
      case '"':
        result.push_back('"');
        break;
      case '\\':
        result.push_back('\\');
        break;
      case 'n':
        result.push_back('\n');
        break;
      case 'r':
        result.push_back('\r');
        break;
      case 't':
        result.push_back('\t');
        break;
      default:
        return Status(StatusCode::kInvalidArgument,
                      "Configuration string escape is invalid");
    }
  }
  return result;
}

class StructuredValueParser {
 public:
  explicit StructuredValueParser(std::string_view text) : text_(text) {}

  Result<ConfigVector> ParseVector() {
    SkipWhitespace();
    if (!Consume('[')) {
      return Status(StatusCode::kInvalidArgument,
                    "Configuration vector is missing '['");
    }
    ConfigVector result;
    SkipWhitespace();
    if (Consume(']')) {
      return result;
    }
    while (true) {
      Result<double> item = ParseNumber();
      if (!item.ok()) {
        return item.status();
      }
      result.push_back(item.value());
      SkipWhitespace();
      if (Consume(']')) {
        return result;
      }
      if (!Consume(',')) {
        return Status(StatusCode::kInvalidArgument,
                      "Configuration vector requires ',' or ']'");
      }
      SkipWhitespace();
      if (Peek() == ']') {
        return Status(StatusCode::kInvalidArgument,
                      "Configuration vector has a trailing comma");
      }
    }
  }

  Result<ConfigMatrix> ParseMatrix() {
    SkipWhitespace();
    if (!Consume('[')) {
      return Status(StatusCode::kInvalidArgument,
                    "Configuration matrix is missing '['");
    }
    ConfigMatrix result;
    SkipWhitespace();
    if (Consume(']')) {
      return result;
    }
    std::size_t columns = 0;
    bool first_row = true;
    while (true) {
      Result<ConfigVector> row = ParseVector();
      if (!row.ok()) {
        return row.status();
      }
      if (first_row) {
        columns = row.value().size();
        first_row = false;
      } else if (row.value().size() != columns) {
        return Status(StatusCode::kInvalidArgument,
                      "Configuration matrices must be rectangular");
      }
      result.push_back(std::move(row).value());
      SkipWhitespace();
      if (Consume(']')) {
        return result;
      }
      if (!Consume(',')) {
        return Status(StatusCode::kInvalidArgument,
                      "Configuration matrix requires ',' or ']'");
      }
      SkipWhitespace();
      if (Peek() == ']') {
        return Status(StatusCode::kInvalidArgument,
                      "Configuration matrix has a trailing comma");
      }
    }
  }

  void SkipWhitespace() noexcept {
    while (position_ < text_.size() &&
           IsAsciiWhitespace(text_[position_])) {
      ++position_;
    }
  }

  bool AtEnd() {
    SkipWhitespace();
    return position_ == text_.size();
  }

 private:
  bool Consume(char expected) noexcept {
    if (position_ < text_.size() && text_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  char Peek() const noexcept {
    return position_ < text_.size() ? text_[position_] : '\0';
  }

  Result<double> ParseNumber() {
    SkipWhitespace();
    const std::size_t first = position_;
    while (position_ < text_.size() && text_[position_] != ',' &&
           text_[position_] != ']') {
      ++position_;
    }
    return ParseFiniteDouble(text_.substr(first, position_ - first));
  }

  std::string_view text_;
  std::size_t position_ = 0;
};

Result<std::string> FormatDouble(double value) {
  if (!std::isfinite(value)) {
    return Status(StatusCode::kInvalidArgument,
                  "Configuration doubles must be finite");
  }
  char buffer[128];
  const std::to_chars_result formatted = std::to_chars(
      buffer, buffer + sizeof(buffer), value, std::chars_format::general,
      std::numeric_limits<double>::max_digits10);
  if (formatted.ec != std::errc()) {
    return Status(StatusCode::kInternal,
                  "Configuration double formatting failed");
  }
  std::string result(buffer, formatted.ptr);
  if (result.find_first_of(".eE") == std::string::npos) {
    result += ".0";
  }
  return result;
}

Result<std::string> QuoteString(std::string_view value) {
  std::string result = "\"";
  for (const char ch : value) {
    switch (ch) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          return Status(StatusCode::kInvalidArgument,
                        "Configuration string contains an unsupported control");
        }
        result.push_back(ch);
        break;
    }
  }
  result.push_back('"');
  return result;
}

Result<std::string> FormatValue(const ConfigValue& value) {
  Status representable = ValidateRepresentable(value);
  if (!representable.ok()) {
    return representable;
  }
  if (value.IsEmpty()) {
    return std::string("null");
  }
  if (value.IsBool()) {
    return std::string(value.AsBool() ? "true" : "false");
  }
  if (value.IsInt()) {
    return std::to_string(value.AsInt());
  }
  if (value.IsDouble()) {
    return FormatDouble(value.AsDouble());
  }
  if (value.IsString()) {
    return QuoteString(value.AsString());
  }
  if (value.IsVector()) {
    std::string result = "vector[";
    for (std::size_t i = 0; i < value.AsVector().size(); ++i) {
      Result<std::string> item = FormatDouble(value.AsVector()[i]);
      if (!item.ok()) {
        return item.status();
      }
      if (i != 0) {
        result += ", ";
      }
      result += item.value();
    }
    result.push_back(']');
    return result;
  }

  std::string result = "matrix[";
  const ConfigMatrix& matrix = value.AsMatrix();
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    if (row != 0) {
      result += ", ";
    }
    result.push_back('[');
    for (std::size_t column = 0; column < matrix[row].size(); ++column) {
      Result<std::string> item = FormatDouble(matrix[row][column]);
      if (!item.ok()) {
        return item.status();
      }
      if (column != 0) {
        result += ", ";
      }
      result += item.value();
    }
    result.push_back(']');
  }
  result.push_back(']');
  return result;
}

bool LooksNumeric(std::string_view text) noexcept {
  if (text.empty()) {
    return false;
  }
  bool has_digit = false;
  for (const char ch : text) {
    if (ch >= '0' && ch <= '9') {
      has_digit = true;
      continue;
    }
    if (ch != '+' && ch != '-' && ch != '.' && ch != 'e' && ch != 'E') {
      return false;
    }
  }
  return has_digit;
}

}  // namespace

ConfigValue::ConfigValue() = default;

ConfigValue::ConfigValue(bool value) : value_(value) {}

ConfigValue::ConfigValue(int value) : value_(value) {}

ConfigValue::ConfigValue(double value) : value_(value) {}

ConfigValue::ConfigValue(const char* value) {
  ASC_REQUIRE(value != nullptr,
              "Configuration string construction requires a value");
  value_ = std::string(value);
}

ConfigValue::ConfigValue(std::string value) : value_(std::move(value)) {}

ConfigValue::ConfigValue(ConfigVector value) : value_(std::move(value)) {}

ConfigValue::ConfigValue(ConfigMatrix value) : value_(std::move(value)) {}

bool ConfigValue::AsBool() const {
  ASC_REQUIRE(IsBool(), "Configuration value is not bool");
  return std::get<bool>(value_);
}

int ConfigValue::AsInt() const {
  if (IsInt()) {
    return std::get<int>(value_);
  }
  ASC_REQUIRE(IsDouble(), "Configuration value is not numeric");
  const double value = std::get<double>(value_);
  const double minimum =
      static_cast<double>(std::numeric_limits<int>::min());
  const double maximum =
      static_cast<double>(std::numeric_limits<int>::max());
  ASC_REQUIRE(std::isfinite(value) && value == std::trunc(value) &&
                  value >= minimum && value <= maximum,
              "Configuration double cannot be represented exactly as int");
  return static_cast<int>(value);
}

double ConfigValue::AsDouble() const {
  if (IsDouble()) {
    return std::get<double>(value_);
  }
  ASC_REQUIRE(IsInt(), "Configuration value is not numeric");
  return static_cast<double>(std::get<int>(value_));
}

const std::string& ConfigValue::AsString() const {
  ASC_REQUIRE(IsString(), "Configuration value is not string");
  return std::get<std::string>(value_);
}

const ConfigVector& ConfigValue::AsVector() const {
  ASC_REQUIRE(IsVector(), "Configuration value is not vector");
  return std::get<ConfigVector>(value_);
}

const ConfigMatrix& ConfigValue::AsMatrix() const {
  ASC_REQUIRE(IsMatrix(), "Configuration value is not matrix");
  return std::get<ConfigMatrix>(value_);
}

ConfigParser::ConfigParser() = default;

void ConfigParser::AddConfig(std::string_view name,
                             ConfigValue default_value,
                             Validator validator) {
  const Status status =
      TryAddConfig(name, std::move(default_value), std::move(validator));
  if (!status.ok()) {
    detail::UtilitiesCompatibilityFailure(status);
  }
}

Status ConfigParser::TryAddConfig(std::string_view name,
                                  ConfigValue default_value,
                                  Validator validator) {
  if (!IsValidConfigName(name)) {
    return Status(StatusCode::kInvalidArgument,
                  "Configuration key is invalid: " + std::string(name));
  }
  Entry candidate{std::string(name), std::move(default_value),
                  std::move(validator)};
  Status validation = Validate(candidate);
  if (!validation.ok()) {
    return validation;
  }
  const int index = Find(name);
  if (index < 0) {
    entries_.push_back(std::move(candidate));
  } else {
    entries_[static_cast<std::size_t>(index)] = std::move(candidate);
  }
  return Status::Ok();
}

const ConfigValue& ConfigParser::GetConfig(std::string_view name) const {
  const int index = Find(name);
  if (index < 0) {
    detail::UtilitiesCompatibilityFailure(
        Status(StatusCode::kOutOfRange,
               "Configuration option not found: " + std::string(name)));
  }
  return entries_[static_cast<std::size_t>(index)].value;
}

void ConfigParser::SetConfig(std::string_view name, ConfigValue value) {
  Status status = TrySetConfig(name, value);
  if (status.code() == StatusCode::kOutOfRange) {
    status = TryAddConfig(name, std::move(value));
  }
  if (!status.ok()) {
    detail::UtilitiesCompatibilityFailure(status);
  }
}

Status ConfigParser::TrySetConfig(std::string_view name, ConfigValue value) {
  const int index = Find(name);
  if (index < 0) {
    return Status(StatusCode::kOutOfRange,
                  "Configuration option not found: " + std::string(name));
  }
  Entry candidate = entries_[static_cast<std::size_t>(index)];
  candidate.value = std::move(value);
  Status validation = Validate(candidate);
  if (!validation.ok()) {
    return validation;
  }
  entries_[static_cast<std::size_t>(index)].value =
      std::move(candidate.value);
  return Status::Ok();
}

Result<ConfigValue> ConfigParser::FindConfig(std::string_view name) const {
  const int index = Find(name);
  if (index < 0) {
    return Status(StatusCode::kOutOfRange,
                  "Configuration option not found: " + std::string(name));
  }
  return entries_[static_cast<std::size_t>(index)].value;
}

void ConfigParser::LoadFromFile(const std::string& filename) {
  std::ifstream input(filename);
  if (!input.is_open()) {
    detail::UtilitiesCompatibilityFailure(
        Status(StatusCode::kUnavailable,
               "Cannot open configuration file: " + filename));
  }
  std::string text((std::istreambuf_iterator<char>(input)),
                   std::istreambuf_iterator<char>());
  if (!input.eof() && input.fail()) {
    detail::UtilitiesCompatibilityFailure(
        Status(StatusCode::kUnavailable,
               "Failed while reading configuration file: " + filename));
  }
  const Status status = TryLoadFromStringImpl(
      text, UnknownConfigKeyPolicy::kAdd, true);
  if (!status.ok()) {
    detail::UtilitiesCompatibilityFailure(status);
  }
}

Status ConfigParser::TryLoadFromString(std::string_view text,
                                       UnknownConfigKeyPolicy policy) {
  return TryLoadFromStringImpl(text, policy, false);
}

Status ConfigParser::TryLoadFromFile(const std::string& filename,
                                     UnknownConfigKeyPolicy policy) {
  std::ifstream input(filename);
  if (!input.is_open()) {
    return Status(StatusCode::kUnavailable,
                  "Cannot open configuration file: " + filename);
  }
  std::string text((std::istreambuf_iterator<char>(input)),
                   std::istreambuf_iterator<char>());
  if (!input.eof() && input.fail()) {
    return Status(StatusCode::kUnavailable,
                  "Failed while reading configuration file: " + filename);
  }
  return TryLoadFromString(text, policy);
}

Status ConfigParser::TryLoadFromStringImpl(
    std::string_view text, UnknownConfigKeyPolicy policy,
    bool allow_legacy_option) {
  if (policy != UnknownConfigKeyPolicy::kReject &&
      policy != UnknownConfigKeyPolicy::kAdd) {
    return Status(StatusCode::kInvalidArgument,
                  "Unknown configuration-key policy is invalid");
  }
  ConfigParser candidate = *this;
  std::unordered_set<std::string> supplied;
  std::size_t line_number = 0;
  while (!text.empty()) {
    ++line_number;
    const std::size_t newline = text.find('\n');
    std::string_view line = text.substr(0, newline);
    text = newline == std::string_view::npos
               ? std::string_view{}
               : text.substr(newline + 1);
    line = TrimView(RemoveComment(line));
    if (line.empty()) {
      continue;
    }
    const std::size_t equals = line.find('=');
    if (equals == std::string_view::npos) {
      return Status(StatusCode::kInvalidArgument,
                    "Configuration entry is missing '=' on line " +
                        std::to_string(line_number));
    }
    std::string key(TrimView(line.substr(0, equals)));
    std::string_view value = TrimView(line.substr(equals + 1));
    if (allow_legacy_option && key == "OPTION") {
      const std::size_t nested_equals = value.find('=');
      if (nested_equals == std::string_view::npos) {
        return Status(StatusCode::kInvalidArgument,
                      "Legacy OPTION entry is invalid on line " +
                          std::to_string(line_number));
      }
      key = std::string(TrimView(value.substr(0, nested_equals)));
      value = TrimView(value.substr(nested_equals + 1));
    }
    if (!IsValidConfigName(key)) {
      return Status(StatusCode::kInvalidArgument,
                    "Configuration key is invalid on line " +
                        std::to_string(line_number));
    }
    if (!supplied.insert(key).second) {
      return Status(StatusCode::kInvalidArgument,
                    "Duplicate configuration key on line " +
                        std::to_string(line_number));
    }
    Result<ConfigValue> parsed = TryParseValue(value);
    if (!parsed.ok()) {
      return Status(parsed.status().code(),
                    "Invalid configuration value for '" + key +
                        "' on line " + std::to_string(line_number) + ": " +
                        std::string(parsed.status().message()));
    }
    Status status = candidate.TrySetConfig(key, parsed.value());
    if (status.code() == StatusCode::kOutOfRange) {
      if (policy == UnknownConfigKeyPolicy::kReject) {
        return Status(StatusCode::kInvalidArgument,
                      "Unknown configuration key on line " +
                          std::to_string(line_number) + ": " + key);
      }
      status = candidate.TryAddConfig(key, std::move(parsed).value());
    }
    if (!status.ok()) {
      return status;
    }
  }
  entries_ = std::move(candidate.entries_);
  return Status::Ok();
}

Result<std::string> ConfigParser::Serialize() const {
  std::vector<const Entry*> sorted;
  sorted.reserve(entries_.size());
  for (const Entry& entry : entries_) {
    sorted.push_back(&entry);
  }
  std::sort(sorted.begin(), sorted.end(), [](const Entry* lhs,
                                             const Entry* rhs) {
    return lhs->name < rhs->name;
  });

  std::string result;
  for (const Entry* entry : sorted) {
    Status validation = Validate(*entry);
    if (!validation.ok()) {
      return validation;
    }
    Result<std::string> value = FormatValue(entry->value);
    if (!value.ok()) {
      return value.status();
    }
    result += entry->name + " = " + value.value() + '\n';
  }
  return result;
}

const std::string& ConfigParser::GetConfigName(int index) const {
  if (index < 0 || index >= GetNConfigs()) {
    detail::UtilitiesCompatibilityFailure(
        Status(StatusCode::kOutOfRange,
               "Configuration index is out of range"));
  }
  return entries_[static_cast<std::size_t>(index)].name;
}

ConfigValue ConfigParser::ParseValue(const std::string& text) {
  Result<ConfigValue> result = TryParseValue(text);
  if (!result.ok()) {
    detail::UtilitiesCompatibilityFailure(result.status());
  }
  return std::move(result).value();
}

Result<ConfigValue> ConfigParser::TryParseValue(std::string_view text) {
  text = TrimView(RemoveComment(text));
  if (text.empty()) {
    return Status(StatusCode::kInvalidArgument,
                  "Configuration value is empty");
  }
  if (text == "null") {
    return ConfigValue();
  }
  if (EqualsIgnoreCase(text, "true")) {
    return ConfigValue(true);
  }
  if (EqualsIgnoreCase(text, "false")) {
    return ConfigValue(false);
  }
  if (text.front() == '"') {
    Result<std::string> parsed = ParseQuotedString(text);
    if (!parsed.ok()) {
      return parsed.status();
    }
    return ConfigValue(std::move(parsed).value());
  }
  if (text.starts_with("vector[")) {
    StructuredValueParser parser(text.substr(6));
    Result<ConfigVector> parsed = parser.ParseVector();
    if (!parsed.ok()) {
      return parsed.status();
    }
    if (!parser.AtEnd()) {
      return Status(StatusCode::kInvalidArgument,
                    "Configuration vector has trailing input");
    }
    return ConfigValue(std::move(parsed).value());
  }
  if (text.starts_with("matrix[")) {
    StructuredValueParser parser(text.substr(6));
    Result<ConfigMatrix> parsed = parser.ParseMatrix();
    if (!parsed.ok()) {
      return parsed.status();
    }
    if (!parser.AtEnd()) {
      return Status(StatusCode::kInvalidArgument,
                    "Configuration matrix has trailing input");
    }
    return ConfigValue(std::move(parsed).value());
  }

  int integer = 0;
  const char* const first = text.data();
  const char* const last = first + text.size();
  const std::from_chars_result integer_result =
      std::from_chars(first, last, integer, 10);
  if (integer_result.ec == std::errc() && integer_result.ptr == last) {
    return ConfigValue(integer);
  }
  if (integer_result.ec == std::errc::result_out_of_range &&
      LooksNumeric(text) && text.find_first_of(".eE") == std::string_view::npos) {
    return Status(StatusCode::kOutOfRange,
                  "Configuration integer is out of int range");
  }
  if (LooksNumeric(text)) {
    Result<double> real = ParseFiniteDouble(text);
    if (!real.ok()) {
      return real.status();
    }
    return ConfigValue(real.value());
  }
  if (EqualsIgnoreCase(text, "nan") || EqualsIgnoreCase(text, "+nan") ||
      EqualsIgnoreCase(text, "-nan") || EqualsIgnoreCase(text, "inf") ||
      EqualsIgnoreCase(text, "+inf") || EqualsIgnoreCase(text, "-inf") ||
      EqualsIgnoreCase(text, "infinity") ||
      EqualsIgnoreCase(text, "+infinity") ||
      EqualsIgnoreCase(text, "-infinity")) {
    return Status(StatusCode::kInvalidArgument,
                  "Configuration doubles must be finite");
  }
  if (std::any_of(text.begin(), text.end(), [](char ch) {
        return IsAsciiWhitespace(ch) || ch == '[' || ch == ']' || ch == ',' ||
               ch == '"';
      })) {
    return Status(StatusCode::kInvalidArgument,
                  "Unquoted configuration string token is invalid");
  }
  return ConfigValue(std::string(text));
}

int ConfigParser::Find(std::string_view name) const {
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].name == name) {
      ASC_REQUIRE(i <= static_cast<std::size_t>(std::numeric_limits<int>::max()),
                  "Configuration collection exceeds its compatibility index");
      return static_cast<int>(i);
    }
  }
  return -1;
}

Status ConfigParser::Validate(const Entry& entry) {
  Status representable = ValidateRepresentable(entry.value);
  if (!representable.ok()) {
    return Status(representable.code(), "Configuration '" + entry.name +
                                            "': " +
                                            std::string(representable.message()));
  }
  if (entry.validator && !entry.validator(entry.value)) {
    return Status(StatusCode::kInvalidArgument,
                  "Configuration validator rejected '" + entry.name + "'");
  }
  return Status::Ok();
}

}  // namespace asc
