// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_UTILITIES_CLI_H_
#define ASC_UTILITIES_CLI_H_

/// @file cli.h
/// @brief Transactional typed command-line option parsing.

#include <concepts>
#include <cstddef>
#include <charconv>
#include <cmath>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "asc/core/config.h"
#include "asc/core/status.h"

namespace asc {

/// @brief Whether an option is optional or required in each parse invocation.
enum OptionType { kOptional = 0, kRequired = 1 };

namespace detail {

/// @brief Translate a failed compatibility operation through the legacy path.
[[noreturn]] ASC_EXPORT void UtilitiesCompatibilityFailure(
    const Status& status);

inline bool EqualsCliTextIgnoreCase(std::string_view lhs,
                                    std::string_view rhs) noexcept {
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

template <typename T>
concept CanonicalOptionValue =
    std::same_as<T, int> || std::same_as<T, float> ||
    std::same_as<T, double> || std::same_as<T, bool> ||
    std::same_as<T, std::string>;

template <CanonicalOptionValue T>
Result<T> ParseOptionValue(std::string_view text) {
  if constexpr (std::same_as<T, std::string>) {
    return std::string(text);
  } else if constexpr (std::same_as<T, bool>) {
    if (EqualsCliTextIgnoreCase(text, "true") || text == "1" ||
        EqualsCliTextIgnoreCase(text, "yes") ||
        EqualsCliTextIgnoreCase(text, "on")) {
      return true;
    }
    if (EqualsCliTextIgnoreCase(text, "false") || text == "0" ||
        EqualsCliTextIgnoreCase(text, "no") ||
        EqualsCliTextIgnoreCase(text, "off")) {
      return false;
    }
    return Status(StatusCode::kInvalidArgument,
                  "Command-line Boolean value is invalid");
  } else {
    T value{};
    const char* const first = text.data();
    const char* const last = first + text.size();
    std::from_chars_result parsed;
    if constexpr (std::same_as<T, int>) {
      parsed = std::from_chars(first, last, value, 10);
    } else {
      parsed = std::from_chars(first, last, value,
                               std::chars_format::general);
    }
    if (parsed.ec == std::errc::result_out_of_range) {
      return Status(StatusCode::kOutOfRange,
                    "Command-line numeric value is out of range");
    }
    if (parsed.ec != std::errc() || parsed.ptr != last || text.empty()) {
      return Status(StatusCode::kInvalidArgument,
                    "Command-line numeric value is invalid");
    }
    if constexpr (std::same_as<T, float> || std::same_as<T, double>) {
      if (!std::isfinite(value)) {
        return Status(StatusCode::kInvalidArgument,
                      "Command-line numeric value must be finite");
      }
    }
    return value;
  }
}

template <CanonicalOptionValue T>
std::string FormatOptionValue(const T& value) {
  if constexpr (std::same_as<T, std::string>) {
    return value;
  } else if constexpr (std::same_as<T, bool>) {
    return value ? "true" : "false";
  } else {
    std::ostringstream output;
    output << value;
    return output.str();
  }
}

}  // namespace detail

/// @brief Polymorphic compatibility base for a declared command-line option.
class Option {
 public:
  /// @brief Construct an option from names without leading hyphens.
  Option(const char* short_name, const std::string& long_name,
         const std::string& description);

  virtual ~Option() = default;

  /// @brief Return the owned long option name.
  const std::string& GetLongName() const noexcept { return long_name_; }

  /// @brief Return the owned short option name as a compatibility C string.
  const char* GetShortName() const noexcept { return short_name_.c_str(); }

  /// @brief Return the option description.
  const std::string& GetDescription() const noexcept { return description_; }

  /// @brief Return kOptional or kRequired.
  int GetAttribute() const noexcept { return attribute_; }

  /// @brief Set kOptional or kRequired.
  void SetAttribute(int attribute);

  /// @brief Return whether the option was supplied in the latest parse.
  bool IsSet() const noexcept { return is_set_; }

  /// @brief Parse and commit one value through the compatibility error path.
  virtual void Parse(const std::string& value) = 0;

  /// @brief Validate a value without changing option or bound-output state.
  virtual Status ValidateValue(std::string_view value) const;

  /// @brief Return whether this option consumes a value token.
  virtual bool TakesValue() const noexcept { return true; }

  /// @brief Reset this option before a successful parse is committed.
  virtual void ResetForParse() { is_set_ = false; }

  /// @brief Return whether this option has a default value.
  virtual bool HasDefaultValue() const noexcept { return false; }

  /// @brief Append the default value and report whether one exists.
  virtual bool GetDefault(std::stringstream& stream) const = 0;

  /// @brief Return the current option value as text.
  virtual std::string GetValue() const = 0;

 protected:
  std::string short_name_;
  std::string long_name_;
  std::string description_;
  int attribute_ = kOptional;
  bool is_set_ = false;
};

/// @brief Typed option that owns a value and may update a bound variable.
template <detail::CanonicalOptionValue T>
class Variable : public Option {
 public:
  Variable(const char* short_name, const std::string& long_name,
           const std::string& description)
      : Option(short_name, long_name, description) {}

  Variable(const char* short_name, const std::string& long_name,
           const std::string& description, T default_value,
           T* assign_to = nullptr)
      : Option(short_name, long_name, description),
        assign_to_(assign_to),
        default_(std::make_unique<T>(std::move(default_value))) {
    value_ = *default_;
    UpdateReference();
  }

  void Parse(const std::string& value) override {
    Result<T> parsed = detail::ParseOptionValue<T>(value);
    if (!parsed.ok()) {
      detail::UtilitiesCompatibilityFailure(parsed.status());
    }
    is_set_ = true;
    value_ = std::move(parsed).value();
    UpdateReference();
  }

  Status ValidateValue(std::string_view value) const override {
    Result<T> parsed = detail::ParseOptionValue<T>(value);
    return parsed.ok() ? Status::Ok() : parsed.status();
  }

  void ResetForParse() override {
    is_set_ = false;
    value_ = default_ ? *default_ : T{};
    UpdateReference();
  }

  bool HasDefaultValue() const noexcept override { return HasDefault(); }

  /// @brief Bind the current and future committed value to external storage.
  void AssignTo(T* assign_to) {
    assign_to_ = assign_to;
    UpdateReference();
  }

  /// @brief Return whether this option owns a default value.
  bool HasDefault() const noexcept { return default_ != nullptr; }

  bool GetDefault(std::stringstream& stream) const override {
    if (!HasDefault()) {
      return false;
    }
    stream << detail::FormatOptionValue(*default_);
    return true;
  }

  /// @brief Define or replace the default value.
  void SetDefault(const T& value) {
    default_ = std::make_unique<T>(value);
    value_ = value;
    UpdateReference();
  }

  /// @brief Set the current value without changing IsSet().
  void SetValue(const T& value) {
    value_ = value;
    UpdateReference();
  }

  std::string GetValue() const override {
    return detail::FormatOptionValue(value_);
  }

 private:
  void UpdateReference() {
    if (assign_to_ != nullptr) {
      *assign_to_ = value_;
    }
  }

  T* assign_to_ = nullptr;
  std::unique_ptr<T> default_;
  T value_{};
};

/// @brief Boolean flag that consumes no value token.
class Switch : public Variable<bool> {
 public:
  Switch(const char* short_name, const std::string& long_name,
         const std::string& description, bool* assign_to = nullptr)
      : Variable<bool>(short_name, long_name, description, false, assign_to) {}

  void SetDefault(const bool&) = delete;

  void Parse(const std::string&) override {
    is_set_ = true;
    SetValue(true);
  }

  Status ValidateValue(std::string_view value) const override {
    if (!value.empty()) {
      return Status(StatusCode::kInvalidArgument,
                    "A command-line switch does not accept a value");
    }
    return Status::Ok();
  }

  bool TakesValue() const noexcept override { return false; }

  bool HasDefaultValue() const noexcept override { return false; }
};

/// @brief Collection of typed options with transactional parsing.
class ASC_EXPORT OptionParser {
 public:
  /// @brief Construct a parser with optional descriptive help text.
  explicit OptionParser(std::string description = "")
      : description_(std::move(description)) {}

  /// @brief Add an option with an explicit requirement attribute.
  template <typename T, int Attribute, typename... Args>
  std::shared_ptr<T> AddOption(Args&&... args) {
    static_assert(Attribute == kOptional || Attribute == kRequired,
                  "Option attribute must be kOptional or kRequired");
    auto option = std::make_shared<T>(std::forward<Args>(args)...);
    option->SetAttribute(Attribute);
    if (Attribute == kRequired && option->HasDefaultValue()) {
      detail::UtilitiesCompatibilityFailure(Status(
          StatusCode::kInvalidArgument,
          "A required command-line option cannot have a default value"));
    }
    AddOptionImpl(option);
    return option;
  }

  /// @brief Add an optional option.
  template <typename T, typename... Args>
  std::shared_ptr<T> AddOption(Args&&... args) {
    auto option = std::make_shared<T>(std::forward<Args>(args)...);
    AddOptionImpl(option);
    return option;
  }

  /// @brief Parse argc/argv transactionally; argv[0] is the program name.
  Status TryParse(int argc, const char* const argv[]);

  /// @brief Parse the migration-era key=value option-file syntax.
  Status TryParseFile(const std::string& filename);

  /// @brief Compatibility wrapper for TryParse().
  void Parse(int argc, const char* argv[]);

  /// @brief Compatibility wrapper for TryParseFile().
  void Parse(const std::string& filename);

  /// @brief Return an option by short name through the compatibility path.
  template <typename T>
  std::shared_ptr<T> GetOption(const char* short_name) const {
    std::shared_ptr<Option> option = FindOption(short_name);
    if (!option) {
      detail::UtilitiesCompatibilityFailure(
          Status(StatusCode::kInvalidArgument, "Command-line option not found"));
    }
    auto result = std::dynamic_pointer_cast<T>(option);
    if (!result) {
      detail::UtilitiesCompatibilityFailure(Status(
          StatusCode::kInvalidArgument,
          "Command-line option has an incompatible requested type"));
    }
    return result;
  }

  /// @brief Return an option by long name through the compatibility path.
  template <typename T>
  std::shared_ptr<T> GetOption(const std::string& long_name) const {
    std::shared_ptr<Option> option = FindOption(long_name);
    if (!option) {
      detail::UtilitiesCompatibilityFailure(
          Status(StatusCode::kInvalidArgument, "Command-line option not found"));
    }
    auto result = std::dynamic_pointer_cast<T>(option);
    if (!result) {
      detail::UtilitiesCompatibilityFailure(Status(
          StatusCode::kInvalidArgument,
          "Command-line option has an incompatible requested type"));
    }
    return result;
  }

  /// @brief Return formatted help text without using a global stream.
  std::string GetHelp() const;

  /// @brief Return formatted usage/value text without using a global stream.
  std::string GetUsage() const;

  /// @brief Write formatted help text to an explicit stream.
  void PrintHelp(std::ostream& stream) const;

  /// @brief Write formatted usage text to an explicit stream.
  void PrintUsage(std::ostream& stream) const;

  /// @brief Compatibility overload that writes help to asc::mout.
  void PrintHelp() const;

  /// @brief Compatibility overload that writes usage to asc::mout.
  void PrintUsage() const;

 private:
  std::vector<std::shared_ptr<Option>> options_;
  std::string description_;

  void AddOptionImpl(const std::shared_ptr<Option>& option);
  std::shared_ptr<Option> FindOption(const char* short_name) const;
  std::shared_ptr<Option> FindOption(const std::string& long_name) const;
  Status CommitCandidates(
      const std::vector<std::pair<std::shared_ptr<Option>, std::string>>&
          candidates);
  std::string Message(bool help) const;
  std::string OptionToHelp(const std::shared_ptr<Option>& option) const;
  std::string OptionToUsage(const std::shared_ptr<Option>& option) const;
};

}  // namespace asc

#endif  // ASC_UTILITIES_CLI_H_
