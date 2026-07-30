#ifndef ASC_CORE_CONFIGURATION_H_
#define ASC_CORE_CONFIGURATION_H_

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "asc/core/export.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

enum class ConfigurationValueType : std::uint8_t {
  kNull = 0,
  kBool = 1,
  kSignedInteger = 2,
  kUnsignedInteger = 3,
  kDouble = 4,
  kString = 5,
  kList = 6,
  kObject = 7,
};

ASC_CORE_EXPORT std::string_view ConfigurationValueTypeName(
    ConfigurationValueType type) noexcept;

class ConfigurationValue {
 public:
  using List = std::vector<ConfigurationValue>;
  using Object = std::map<std::string, ConfigurationValue, std::less<>>;

  // These conversions provide the variant-style construction contract.
  // NOLINTBEGIN(google-explicit-constructor)
  ASC_CORE_EXPORT ConfigurationValue() noexcept;
  ASC_CORE_EXPORT ConfigurationValue(std::nullptr_t) noexcept;

  template <typename Bool>
    requires std::same_as<std::remove_cvref_t<Bool>, bool>
  ConfigurationValue(Bool value) noexcept : storage_(value) {}

  ASC_CORE_EXPORT ConfigurationValue(std::int64_t value) noexcept;
  ASC_CORE_EXPORT ConfigurationValue(std::uint64_t value) noexcept;

  template <std::integral Integer>
    requires(!std::same_as<std::remove_cv_t<Integer>, bool> &&
             sizeof(Integer) <= sizeof(std::uint64_t) &&
             !std::same_as<std::remove_cv_t<Integer>, std::int64_t> &&
             !std::same_as<std::remove_cv_t<Integer>, std::uint64_t>)
  ConfigurationValue(Integer value) noexcept
      : storage_(IntegerStorage(value)) {}

  template <typename Floating>
    requires std::same_as<std::remove_cvref_t<Floating>, double>
  ConfigurationValue(Floating value) noexcept : storage_(value) {}

  static ASC_CORE_EXPORT Result<ConfigurationValue> Utf8String(
      std::string value);
  ConfigurationValue(const char*) = delete;
  ASC_CORE_EXPORT ConfigurationValue(List value);
  ASC_CORE_EXPORT ConfigurationValue(Object value);
  // NOLINTEND(google-explicit-constructor)

  ASC_CORE_EXPORT ConfigurationValue(const ConfigurationValue&);
  ASC_CORE_EXPORT ConfigurationValue& operator=(const ConfigurationValue&);
  ASC_CORE_EXPORT ConfigurationValue(ConfigurationValue&&) noexcept;
  ASC_CORE_EXPORT ConfigurationValue& operator=(ConfigurationValue&&) noexcept;
  ASC_CORE_EXPORT ~ConfigurationValue();

  [[nodiscard]] ASC_CORE_EXPORT ConfigurationValueType type() const noexcept;
  [[nodiscard]] ASC_CORE_EXPORT bool is_null() const noexcept;

  [[nodiscard]] ASC_CORE_EXPORT Result<bool> AsBool() const;
  [[nodiscard]] ASC_CORE_EXPORT Result<std::int64_t> AsSignedInteger() const;
  [[nodiscard]] ASC_CORE_EXPORT Result<std::uint64_t> AsUnsignedInteger() const;
  [[nodiscard]] ASC_CORE_EXPORT Result<double> AsDouble() const;
  [[nodiscard]] ASC_CORE_EXPORT
      Result<std::reference_wrapper<const std::string>>
      AsString() const;
  [[nodiscard]] ASC_CORE_EXPORT Result<std::reference_wrapper<const List>>
  AsList() const;
  [[nodiscard]] ASC_CORE_EXPORT Result<std::reference_wrapper<const Object>>
  AsObject() const;

 private:
  struct Utf8Tag {};

  using Storage =
      std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double,
                   std::string, List, Object>;

  template <std::integral Integer>
  static Storage IntegerStorage(Integer value) noexcept {
    if constexpr (std::is_signed_v<Integer>) {
      return static_cast<std::int64_t>(value);
    } else {
      return static_cast<std::uint64_t>(value);
    }
  }

  ConfigurationValue(Utf8Tag /*tag*/, std::string value);

  Storage storage_;
};

enum class ConfigurationOriginKind : std::uint8_t {
  kDefault = 0,
  kProgrammatic = 1,
  kCommandLine = 2,
};

struct ConfigurationOrigin {
  ConfigurationOriginKind kind = ConfigurationOriginKind::kProgrammatic;
  std::optional<std::string> source_label;
  std::optional<std::string> location;

  static ASC_CORE_EXPORT ConfigurationOrigin Default();
  static ASC_CORE_EXPORT ConfigurationOrigin
  Programmatic(std::optional<std::string> source_label = std::nullopt,
               std::optional<std::string> location = std::nullopt);
  static ASC_CORE_EXPORT ConfigurationOrigin
  CommandLine(std::optional<std::string> source_label = std::nullopt,
              std::optional<std::string> location = std::nullopt);
};

using ConfigurationOrigins =
    std::map<std::string, ConfigurationOrigin, std::less<>>;

class ConfigurationSchema {
 public:
  using Fields = std::map<std::string, ConfigurationSchema, std::less<>>;

  explicit ASC_CORE_EXPORT ConfigurationSchema(ConfigurationValueType type);

  [[nodiscard]] ConfigurationValueType type() const noexcept { return type_; }
  [[nodiscard]] bool required() const noexcept { return required_; }
  [[nodiscard]] bool deprecated() const noexcept { return deprecated_; }
  [[nodiscard]] bool sensitive() const noexcept { return sensitive_; }
  [[nodiscard]] ASC_CORE_EXPORT const ConfigurationValue* default_value()
      const noexcept;
  [[nodiscard]] const Fields& fields() const noexcept { return fields_; }

  ASC_CORE_EXPORT ConfigurationSchema& SetRequired(bool required) noexcept;
  ASC_CORE_EXPORT ConfigurationSchema& SetDeprecated(bool deprecated) noexcept;
  ASC_CORE_EXPORT ConfigurationSchema& SetSensitive(bool sensitive) noexcept;
  ASC_CORE_EXPORT ConfigurationSchema& SetDefault(ConfigurationValue value);

  ASC_CORE_EXPORT Status SetSignedBounds(std::optional<std::int64_t> minimum,
                                         std::optional<std::int64_t> maximum);
  ASC_CORE_EXPORT Status
  SetUnsignedBounds(std::optional<std::uint64_t> minimum,
                    std::optional<std::uint64_t> maximum);
  ASC_CORE_EXPORT Status SetDoubleBounds(std::optional<double> minimum,
                                         std::optional<double> maximum);
  ASC_CORE_EXPORT Status SetSizeBounds(std::optional<std::size_t> minimum,
                                       std::optional<std::size_t> maximum);
  ASC_CORE_EXPORT Status AddField(std::string name, ConfigurationSchema field);

  [[nodiscard]] std::optional<std::int64_t> signed_minimum() const noexcept {
    return signed_minimum_;
  }
  [[nodiscard]] std::optional<std::int64_t> signed_maximum() const noexcept {
    return signed_maximum_;
  }
  [[nodiscard]] std::optional<std::uint64_t> unsigned_minimum() const noexcept {
    return unsigned_minimum_;
  }
  [[nodiscard]] std::optional<std::uint64_t> unsigned_maximum() const noexcept {
    return unsigned_maximum_;
  }
  [[nodiscard]] std::optional<double> double_minimum() const noexcept {
    return double_minimum_;
  }
  [[nodiscard]] std::optional<double> double_maximum() const noexcept {
    return double_maximum_;
  }
  [[nodiscard]] std::optional<std::size_t> size_minimum() const noexcept {
    return size_minimum_;
  }
  [[nodiscard]] std::optional<std::size_t> size_maximum() const noexcept {
    return size_maximum_;
  }

 private:
  ConfigurationValueType type_;
  bool required_ = false;
  bool deprecated_ = false;
  bool sensitive_ = false;
  std::optional<ConfigurationValue> default_value_;
  Fields fields_;
  std::optional<std::int64_t> signed_minimum_;
  std::optional<std::int64_t> signed_maximum_;
  std::optional<std::uint64_t> unsigned_minimum_;
  std::optional<std::uint64_t> unsigned_maximum_;
  std::optional<double> double_minimum_;
  std::optional<double> double_maximum_;
  std::optional<std::size_t> size_minimum_;
  std::optional<std::size_t> size_maximum_;
};

struct ConfigurationMetadata {
  ConfigurationOrigin origin;
  bool sensitive = false;
  bool deprecated = false;
};

class Configuration {
 public:
  [[nodiscard]] const ConfigurationValue& value() const noexcept {
    return value_;
  }

  // Paths use JSON Pointer syntax. The empty path names the root.
  [[nodiscard]] ASC_CORE_EXPORT
      Result<std::reference_wrapper<const ConfigurationValue>>
      Find(std::string_view path) const;
  [[nodiscard]] ASC_CORE_EXPORT Result<ConfigurationOrigin> Origin(
      std::string_view path) const;
  [[nodiscard]] ASC_CORE_EXPORT Result<bool> IsSensitive(
      std::string_view path) const;
  [[nodiscard]] ASC_CORE_EXPORT Result<bool> IsDeprecated(
      std::string_view path) const;

 private:
  friend ASC_CORE_EXPORT Result<Configuration> ValidateConfiguration(
      const ConfigurationSchema& schema, const ConfigurationValue& value,
      ConfigurationOrigin origin);
  friend ASC_CORE_EXPORT Result<Configuration> ValidateConfigurationWithOrigins(
      const ConfigurationSchema& schema, const ConfigurationValue& value,
      const ConfigurationOrigins& origins, ConfigurationOrigin fallback_origin);

  Configuration(
      ConfigurationValue value,
      std::map<std::string, ConfigurationMetadata, std::less<>> metadata);

  ConfigurationValue value_;
  std::map<std::string, ConfigurationMetadata, std::less<>> metadata_;
};

// Validates and copies the complete tree before returning it. The input is
// never modified. Defaults receive kDefault origin; existing values retain
// the supplied programmatic origin.
ASC_CORE_EXPORT Result<Configuration> ValidateConfiguration(
    const ConfigurationSchema& schema, const ConfigurationValue& value,
    ConfigurationOrigin origin = ConfigurationOrigin::Programmatic());

// The origin map uses JSON Pointer paths. An exact path entry overrides the
// inherited origin for that value and its descendants. Defaults always retain
// kDefault origin.
ASC_CORE_EXPORT Result<Configuration> ValidateConfigurationWithOrigins(
    const ConfigurationSchema& schema, const ConfigurationValue& value,
    const ConfigurationOrigins& origins,
    ConfigurationOrigin fallback_origin = ConfigurationOrigin::Programmatic());

ASC_CORE_EXPORT std::string RenderConfigurationValue(
    const ConfigurationValue& value, bool sensitive);

}  // namespace asc

#endif  // ASC_CORE_CONFIGURATION_H_
