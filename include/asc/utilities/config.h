// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/utilities/config.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_UTILITIES_CONFIG_H_
#define ASC_UTILITIES_CONFIG_H_

/// @file config.h
/// @brief Array-independent typed configuration values and file parsing.

#include <functional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "asc/core/status.h"

namespace asc {

/// @brief Standard-library representation of a configuration vector.
using ConfigVector = std::vector<double>;

/// @brief Standard-library representation of a configuration matrix.
///
/// Each inner vector represents one row. Configuration storage deliberately
/// does not depend on ASC's numerical array ownership or execution machinery.
using ConfigMatrix = std::vector<ConfigVector>;

/// @brief Policy for keys in input that were not previously declared.
enum class UnknownConfigKeyPolicy {
  kReject,  ///< Return a failure status for an unknown key.
  kAdd,     ///< Add an unknown key without a validator.
};

/// @brief Type-safe value stored by ConfigParser.
class ConfigValue {
 public:
  /// @brief Complete set of supported configuration storage types.
  using Storage =
      std::variant<std::monostate, bool, int, double, std::string,
                   ConfigVector, ConfigMatrix>;

  /// @brief Construct an empty configuration value.
  ConfigValue();

  /// @brief Construct a Boolean configuration value.
  explicit ConfigValue(bool value);

  /// @brief Construct an integer configuration value.
  explicit ConfigValue(int value);

  /// @brief Construct a floating-point configuration value.
  explicit ConfigValue(double value);

  /// @brief Construct a string configuration value.
  explicit ConfigValue(const char* value);

  /// @brief Construct a string configuration value.
  explicit ConfigValue(std::string value);

  /// @brief Construct a vector configuration value.
  explicit ConfigValue(ConfigVector value);

  /// @brief Construct a matrix configuration value.
  explicit ConfigValue(ConfigMatrix value);

  /// @brief Return whether the value is empty.
  bool IsEmpty() const {
    return std::holds_alternative<std::monostate>(value_);
  }

  /// @brief Return whether the value stores a Boolean.
  bool IsBool() const { return std::holds_alternative<bool>(value_); }

  /// @brief Return whether the value stores an integer.
  bool IsInt() const { return std::holds_alternative<int>(value_); }

  /// @brief Return whether the value stores a floating-point number.
  bool IsDouble() const { return std::holds_alternative<double>(value_); }

  /// @brief Return whether the value stores a string.
  bool IsString() const {
    return std::holds_alternative<std::string>(value_);
  }

  /// @brief Return whether the value stores a standard vector.
  bool IsVector() const {
    return std::holds_alternative<ConfigVector>(value_);
  }

  /// @brief Return whether the value stores a standard nested vector.
  bool IsMatrix() const {
    return std::holds_alternative<ConfigMatrix>(value_);
  }

  /// @brief Return whether the value stores an integer or double.
  bool IsNumeric() const { return IsInt() || IsDouble(); }

  /// @brief Read the value as a Boolean.
  /// @throws ContractException when the stored type is not Boolean and
  /// contract exceptions are enabled.
  bool AsBool() const;

  /// @brief Read the value as an integer.
  /// @throws ContractException when the stored value is not an integer or an
  /// exactly integral, in-range double and contract exceptions are enabled.
  int AsInt() const;

  /// @brief Read the value as a double.
  /// @throws ContractException when the stored type is not numeric and
  /// contract exceptions are enabled.
  double AsDouble() const;

  /// @brief Read the stored string.
  const std::string& AsString() const;

  /// @brief Read the stored standard vector.
  const ConfigVector& AsVector() const;

  /// @brief Read the stored standard nested vector.
  const ConfigMatrix& AsMatrix() const;

 private:
  Storage value_;
};

/// @brief Named typed configuration collection with optional validators.
class ConfigParser {
 public:
  /// @brief Validator called whenever a configuration entry changes.
  using Validator = std::function<bool(const ConfigValue&)>;

  /// @brief Construct an empty parser.
  ConfigParser();

  /// @brief Add or replace a named configuration entry.
  void AddConfig(std::string_view name, ConfigValue default_value = {},
                 Validator validator = {});

  /// @brief Validate and transactionally add or replace a named entry.
  Status TryAddConfig(std::string_view name, ConfigValue default_value = {},
                      Validator validator = {});

  /// @brief Return whether a named entry exists.
  bool HasConfig(std::string_view name) const { return Find(name) >= 0; }

  /// @brief Return a named configuration value.
  const ConfigValue& GetConfig(std::string_view name) const;

  /// @brief Set a named value, preserving its existing validator.
  void SetConfig(std::string_view name, ConfigValue value);

  /// @brief Set an already declared value transactionally.
  Status TrySetConfig(std::string_view name, ConfigValue value);

  /// @brief Return a copy of a named value or an out-of-range status.
  Result<ConfigValue> FindConfig(std::string_view name) const;

  /// @brief Load `name = value` entries from a text file.
  void LoadFromFile(const std::string& filename);

  /// @brief Parse and apply canonical text transactionally.
  Status TryLoadFromString(
      std::string_view text,
      UnknownConfigKeyPolicy policy = UnknownConfigKeyPolicy::kReject);

  /// @brief Load and apply a canonical configuration file transactionally.
  Status TryLoadFromFile(
      const std::string& filename,
      UnknownConfigKeyPolicy policy = UnknownConfigKeyPolicy::kReject);

  /// @brief Serialize all entries in canonical lexical key order.
  Result<std::string> Serialize() const;

  /// @brief Return the number of entries.
  int GetNConfigs() const { return static_cast<int>(entries_.size()); }

  /// @brief Return the name at the given insertion-order index.
  const std::string& GetConfigName(int index) const;

  /// @brief Parse one canonical value through the compatibility failure path.
  static ConfigValue ParseValue(const std::string& text);

  /// @brief Parse one canonical or compatible unquoted value.
  static Result<ConfigValue> TryParseValue(std::string_view text);

 private:
  struct Entry {
    std::string name;
    ConfigValue value;
    Validator validator;
  };

  std::vector<Entry> entries_;

  int Find(std::string_view name) const;
  static Status Validate(const Entry& entry);
  Status TryLoadFromStringImpl(std::string_view text,
                               UnknownConfigKeyPolicy policy,
                               bool allow_legacy_option);
};

}  // namespace asc

#endif  // ASC_UTILITIES_CONFIG_H_
