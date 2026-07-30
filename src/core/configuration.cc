#include "asc/core/configuration.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "asc/core/contracts.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

namespace internal_core_configuration {

Status TypeMismatch(ConfigurationValueType expected,
                    ConfigurationValueType actual) {
  return Status(ErrorCode::kConfiguration,
                "Configuration value type is " +
                    std::string(ConfigurationValueTypeName(actual)) + ", not " +
                    std::string(ConfigurationValueTypeName(expected)));
}

bool IsContinuationByte(unsigned char byte) noexcept {
  return byte >= 0x80U && byte <= 0xbfU;
}

bool IsValidUtf8(std::string_view value) noexcept {
  std::size_t index = 0;
  while (index < value.size()) {
    const auto first = static_cast<unsigned char>(value[index]);
    if (first <= 0x7fU) {
      ++index;
      continue;
    }

    if (first >= 0xc2U && first <= 0xdfU) {
      if (index + 1 >= value.size() ||
          !IsContinuationByte(static_cast<unsigned char>(value[index + 1]))) {
        return false;
      }
      index += 2;
      continue;
    }

    if (first >= 0xe0U && first <= 0xefU) {
      if (index + 2 >= value.size()) {
        return false;
      }
      const auto second = static_cast<unsigned char>(value[index + 1]);
      const auto third = static_cast<unsigned char>(value[index + 2]);
      const bool valid_second =
          (first == 0xe0U && second >= 0xa0U && second <= 0xbfU) ||
          (first == 0xedU && second >= 0x80U && second <= 0x9fU) ||
          (((first >= 0xe1U && first <= 0xecU) ||
            (first >= 0xeeU && first <= 0xefU)) &&
           IsContinuationByte(second));
      if (!valid_second || !IsContinuationByte(third)) {
        return false;
      }
      index += 3;
      continue;
    }

    if (first >= 0xf0U && first <= 0xf4U) {
      if (index + 3 >= value.size()) {
        return false;
      }
      const auto second = static_cast<unsigned char>(value[index + 1]);
      const auto third = static_cast<unsigned char>(value[index + 2]);
      const auto fourth = static_cast<unsigned char>(value[index + 3]);
      const bool valid_second =
          (first == 0xf0U && second >= 0x90U && second <= 0xbfU) ||
          (first == 0xf4U && second >= 0x80U && second <= 0x8fU) ||
          (first >= 0xf1U && first <= 0xf3U && IsContinuationByte(second));
      if (!valid_second || !IsContinuationByte(third) ||
          !IsContinuationByte(fourth)) {
        return false;
      }
      index += 4;
      continue;
    }
    return false;
  }
  return true;
}

std::string EscapePathToken(std::string_view token) {
  std::string escaped;
  escaped.reserve(token.size());
  for (char character : token) {
    if (character == '~') {
      escaped.append("~0");
    } else if (character == '/') {
      escaped.append("~1");
    } else {
      escaped.push_back(character);
    }
  }
  return escaped;
}

Result<std::string> UnescapePathToken(std::string_view token) {
  std::string unescaped;
  unescaped.reserve(token.size());
  for (std::size_t index = 0; index < token.size(); ++index) {
    if (token[index] != '~') {
      unescaped.push_back(token[index]);
      continue;
    }
    if (index + 1 >= token.size()) {
      return Status(ErrorCode::kInvalidArgument,
                    "Configuration path has an incomplete escape");
    }
    ++index;
    if (token[index] == '0') {
      unescaped.push_back('~');
    } else if (token[index] == '1') {
      unescaped.push_back('/');
    } else {
      return Status(ErrorCode::kInvalidArgument,
                    "Configuration path has an invalid escape");
    }
  }
  return unescaped;
}

std::string ChildPath(std::string_view path, std::string_view key) {
  return std::string(path) + "/" + EscapePathToken(key);
}

std::string ChildPath(std::string_view path, std::size_t index) {
  return std::string(path) + "/" + std::to_string(index);
}

std::string QuoteDiagnosticString(std::string_view value) {
  constexpr char kHexDigits[] = "0123456789abcdef";
  std::string output;
  output.reserve(value.size() + 2U);
  output.push_back('"');
  for (unsigned char byte : value) {
    switch (byte) {
      case '"':
        output.append("\\\"");
        break;
      case '\\':
        output.append("\\\\");
        break;
      case '\b':
        output.append("\\b");
        break;
      case '\f':
        output.append("\\f");
        break;
      case '\n':
        output.append("\\n");
        break;
      case '\r':
        output.append("\\r");
        break;
      case '\t':
        output.append("\\t");
        break;
      default:
        if (byte < 0x20U || byte == 0x7fU) {
          output.append("\\u00");
          output.push_back(kHexDigits[byte >> 4U]);
          output.push_back(kHexDigits[byte & 0x0fU]);
        } else {
          output.push_back(static_cast<char>(byte));
        }
        break;
    }
  }
  output.push_back('"');
  return output;
}

std::string QuoteDiagnosticBytes(std::string_view value) {
  constexpr char kHexDigits[] = "0123456789abcdef";
  std::string output;
  output.reserve(value.size() + 2U);
  output.push_back('"');
  for (unsigned char byte : value) {
    if (byte == '"') {
      output.append("\\\"");
    } else if (byte == '\\') {
      output.append("\\\\");
    } else if (byte >= 0x20U && byte <= 0x7eU) {
      output.push_back(static_cast<char>(byte));
    } else {
      output.append("\\x");
      output.push_back(kHexDigits[byte >> 4U]);
      output.push_back(kHexDigits[byte & 0x0fU]);
    }
  }
  output.push_back('"');
  return output;
}

Status ConfigurationError(std::string_view path, std::string message) {
  const std::string rendered_path =
      path.empty() ? "<root>" : QuoteDiagnosticBytes(path);
  return Status(ErrorCode::kConfiguration,
                rendered_path + ": " + std::move(message));
}

Status ValidateBounds(const ConfigurationSchema& schema,
                      const ConfigurationValue& value, std::string_view path) {
  if (schema.type() == ConfigurationValueType::kSignedInteger) {
    auto number = value.AsSignedInteger();
    if (!number.ok()) {
      return number.status();
    }
    const auto minimum = schema.signed_minimum();
    const auto maximum = schema.signed_maximum();
    if ((minimum.has_value() && *number < *minimum) ||
        (maximum.has_value() && *number > *maximum)) {
      return ConfigurationError(path, "signed integer is outside its bounds");
    }
  } else if (schema.type() == ConfigurationValueType::kUnsignedInteger) {
    auto number = value.AsUnsignedInteger();
    if (!number.ok()) {
      return number.status();
    }
    const auto minimum = schema.unsigned_minimum();
    const auto maximum = schema.unsigned_maximum();
    if ((minimum.has_value() && *number < *minimum) ||
        (maximum.has_value() && *number > *maximum)) {
      return ConfigurationError(path, "unsigned integer is outside its bounds");
    }
  } else if (schema.type() == ConfigurationValueType::kDouble) {
    auto number = value.AsDouble();
    if (!number.ok()) {
      return number.status();
    }
    const auto minimum = schema.double_minimum();
    const auto maximum = schema.double_maximum();
    if ((minimum.has_value() || maximum.has_value()) && std::isnan(*number)) {
      return ConfigurationError(path, "NaN does not satisfy numeric bounds");
    }
    if ((minimum.has_value() && *number < *minimum) ||
        (maximum.has_value() && *number > *maximum)) {
      return ConfigurationError(path, "double is outside its bounds");
    }
  }

  std::optional<std::size_t> size;
  if (schema.type() == ConfigurationValueType::kString) {
    auto string_value = value.AsString();
    if (!string_value.ok()) {
      return string_value.status();
    }
    size = string_value->get().size();
  } else if (schema.type() == ConfigurationValueType::kList) {
    auto list = value.AsList();
    if (!list.ok()) {
      return list.status();
    }
    size = list->get().size();
  }

  const auto minimum_size = schema.size_minimum();
  const auto maximum_size = schema.size_maximum();
  if (size.has_value() &&
      ((minimum_size.has_value() && *size < *minimum_size) ||
       (maximum_size.has_value() && *size > *maximum_size))) {
    return ConfigurationError(path, "value size is outside its bounds");
  }
  return Status::Ok();
}

void RecordDescendantMetadata(
    const ConfigurationValue& value, std::string_view path,
    const ConfigurationMetadata& inherited_metadata,
    const ConfigurationOrigins* origins,
    std::map<std::string, ConfigurationMetadata, std::less<>>& metadata_map) {
  ConfigurationMetadata metadata = inherited_metadata;
  if (origins != nullptr) {
    const auto iterator = origins->find(path);
    if (iterator != origins->end()) {
      metadata.origin = iterator->second;
    }
  }
  metadata_map[std::string(path)] = metadata;
  if (value.type() == ConfigurationValueType::kList) {
    const auto list = value.AsList();
    ASC_DCHECK(list.ok());
    for (std::size_t index = 0; index < list->get().size(); ++index) {
      RecordDescendantMetadata(list->get()[index], ChildPath(path, index),
                               metadata, origins, metadata_map);
    }
  } else if (value.type() == ConfigurationValueType::kObject) {
    const auto object = value.AsObject();
    ASC_DCHECK(object.ok());
    for (const auto& [key, child] : object->get()) {
      RecordDescendantMetadata(child, ChildPath(path, key), metadata, origins,
                               metadata_map);
    }
  }
}

// The recursive schema validation remains cohesive as one operation.
// NOLINTNEXTLINE(readability-function-size)
Result<ConfigurationValue> ValidateNode(
    const ConfigurationSchema& schema, const ConfigurationValue& input,
    std::string_view path, const ConfigurationOrigin& inherited_origin,
    const ConfigurationOrigins* origins, bool inherited_sensitive,
    std::map<std::string, ConfigurationMetadata, std::less<>>& metadata_map) {
  const ConfigurationOrigin* origin = &inherited_origin;
  if (origins != nullptr) {
    const auto iterator = origins->find(path);
    if (iterator != origins->end()) {
      origin = &iterator->second;
    }
  }

  if (input.type() != schema.type()) {
    return ConfigurationError(
        path, "expected " +
                  std::string(ConfigurationValueTypeName(schema.type())) +
                  ", received " +
                  std::string(ConfigurationValueTypeName(input.type())));
  }

  const Status bounds_status = ValidateBounds(schema, input, path);
  if (!bounds_status.ok()) {
    return bounds_status;
  }

  const bool sensitive = inherited_sensitive || schema.sensitive();
  const ConfigurationMetadata metadata{
      .origin = *origin,
      .sensitive = sensitive,
      .deprecated = schema.deprecated(),
  };
  metadata_map[std::string(path)] = metadata;

  if (schema.type() != ConfigurationValueType::kObject) {
    ConfigurationValue output = input;
    if (schema.type() == ConfigurationValueType::kList) {
      const auto list = output.AsList();
      ASC_DCHECK(list.ok());
      for (std::size_t index = 0; index < list->get().size(); ++index) {
        RecordDescendantMetadata(list->get()[index], ChildPath(path, index),
                                 metadata, origins, metadata_map);
      }
    }
    return output;
  }

  const auto input_object_result = input.AsObject();
  ASC_DCHECK(input_object_result.ok());
  const ConfigurationValue::Object& input_object = input_object_result->get();

  for (const auto& [key, unused] : input_object) {
    static_cast<void>(unused);
    if (!schema.fields().contains(key)) {
      return ConfigurationError(ChildPath(path, key), "unknown key");
    }
  }

  ConfigurationValue::Object output_object;
  for (const auto& [key, field_schema] : schema.fields()) {
    const auto input_iterator = input_object.find(key);
    const std::string child_path = ChildPath(path, key);
    if (input_iterator != input_object.end()) {
      auto validated =
          ValidateNode(field_schema, input_iterator->second, child_path,
                       *origin, origins, sensitive, metadata_map);
      if (!validated.ok()) {
        return validated.status();
      }
      output_object.emplace(key, std::move(*validated));
      continue;
    }

    if (field_schema.default_value() != nullptr) {
      auto validated = ValidateNode(field_schema, *field_schema.default_value(),
                                    child_path, ConfigurationOrigin::Default(),
                                    nullptr, sensitive, metadata_map);
      if (!validated.ok()) {
        return ConfigurationError(
            child_path,
            "schema default is invalid: " + validated.status().ToString());
      }
      output_object.emplace(key, std::move(*validated));
    } else if (field_schema.required()) {
      return ConfigurationError(child_path, "required key is missing");
    }
  }
  const auto minimum_size = schema.size_minimum();
  const auto maximum_size = schema.size_maximum();
  if ((minimum_size.has_value() && output_object.size() < *minimum_size) ||
      (maximum_size.has_value() && output_object.size() > *maximum_size)) {
    return ConfigurationError(path, "value size is outside its bounds");
  }
  return ConfigurationValue(std::move(output_object));
}

}  // namespace internal_core_configuration

std::string_view ConfigurationValueTypeName(
    ConfigurationValueType type) noexcept {
  switch (type) {
    case ConfigurationValueType::kNull:
      return "null";
    case ConfigurationValueType::kBool:
      return "bool";
    case ConfigurationValueType::kSignedInteger:
      return "signed integer";
    case ConfigurationValueType::kUnsignedInteger:
      return "unsigned integer";
    case ConfigurationValueType::kDouble:
      return "double";
    case ConfigurationValueType::kString:
      return "string";
    case ConfigurationValueType::kList:
      return "list";
    case ConfigurationValueType::kObject:
      return "object";
  }
  return "unknown";
}

ConfigurationValue::ConfigurationValue() noexcept = default;

ConfigurationValue::ConfigurationValue(std::nullptr_t) noexcept
    : ConfigurationValue() {}

ConfigurationValue::ConfigurationValue(std::int64_t value) noexcept
    : storage_(value) {}

ConfigurationValue::ConfigurationValue(std::uint64_t value) noexcept
    : storage_(value) {}

ConfigurationValue::ConfigurationValue(Utf8Tag /*tag*/, std::string value)
    : storage_(std::move(value)) {}

Result<ConfigurationValue> ConfigurationValue::Utf8String(std::string value) {
  if (!internal_core_configuration::IsValidUtf8(value)) {
    return Status(ErrorCode::kEncoding,
                  "Configuration string is not well-formed UTF-8");
  }
  return ConfigurationValue(Utf8Tag{}, std::move(value));
}

ConfigurationValue::ConfigurationValue(List value)
    : storage_(std::move(value)) {}

ConfigurationValue::ConfigurationValue(Object value)
    : storage_(std::move(value)) {}

ConfigurationValue::ConfigurationValue(const ConfigurationValue&) = default;
ConfigurationValue& ConfigurationValue::operator=(const ConfigurationValue&) =
    default;
ConfigurationValue::ConfigurationValue(ConfigurationValue&&) noexcept = default;
ConfigurationValue& ConfigurationValue::operator=(
    ConfigurationValue&&) noexcept = default;
ConfigurationValue::~ConfigurationValue() = default;

ConfigurationValueType ConfigurationValue::type() const noexcept {
  return static_cast<ConfigurationValueType>(storage_.index());
}

bool ConfigurationValue::is_null() const noexcept {
  return type() == ConfigurationValueType::kNull;
}

Result<bool> ConfigurationValue::AsBool() const {
  if (type() != ConfigurationValueType::kBool) {
    return internal_core_configuration::TypeMismatch(
        ConfigurationValueType::kBool, type());
  }
  return std::get<bool>(storage_);
}

Result<std::int64_t> ConfigurationValue::AsSignedInteger() const {
  if (type() != ConfigurationValueType::kSignedInteger) {
    return internal_core_configuration::TypeMismatch(
        ConfigurationValueType::kSignedInteger, type());
  }
  return std::get<std::int64_t>(storage_);
}

Result<std::uint64_t> ConfigurationValue::AsUnsignedInteger() const {
  if (type() != ConfigurationValueType::kUnsignedInteger) {
    return internal_core_configuration::TypeMismatch(
        ConfigurationValueType::kUnsignedInteger, type());
  }
  return std::get<std::uint64_t>(storage_);
}

Result<double> ConfigurationValue::AsDouble() const {
  if (type() != ConfigurationValueType::kDouble) {
    return internal_core_configuration::TypeMismatch(
        ConfigurationValueType::kDouble, type());
  }
  return std::get<double>(storage_);
}

Result<std::reference_wrapper<const std::string>> ConfigurationValue::AsString()
    const {
  if (type() != ConfigurationValueType::kString) {
    return internal_core_configuration::TypeMismatch(
        ConfigurationValueType::kString, type());
  }
  return std::cref(std::get<std::string>(storage_));
}

Result<std::reference_wrapper<const ConfigurationValue::List>>
ConfigurationValue::AsList() const {
  if (type() != ConfigurationValueType::kList) {
    return internal_core_configuration::TypeMismatch(
        ConfigurationValueType::kList, type());
  }
  return std::cref(std::get<List>(storage_));
}

Result<std::reference_wrapper<const ConfigurationValue::Object>>
ConfigurationValue::AsObject() const {
  if (type() != ConfigurationValueType::kObject) {
    return internal_core_configuration::TypeMismatch(
        ConfigurationValueType::kObject, type());
  }
  return std::cref(std::get<Object>(storage_));
}

ConfigurationOrigin ConfigurationOrigin::Default() {
  return ConfigurationOrigin{
      .kind = ConfigurationOriginKind::kDefault,
      .source_label = std::nullopt,
      .location = std::nullopt,
  };
}

ConfigurationOrigin ConfigurationOrigin::Programmatic(
    std::optional<std::string> source_label,
    std::optional<std::string> location) {
  return ConfigurationOrigin{
      .kind = ConfigurationOriginKind::kProgrammatic,
      .source_label = std::move(source_label),
      .location = std::move(location),
  };
}

ConfigurationOrigin ConfigurationOrigin::CommandLine(
    std::optional<std::string> source_label,
    std::optional<std::string> location) {
  return ConfigurationOrigin{
      .kind = ConfigurationOriginKind::kCommandLine,
      .source_label = std::move(source_label),
      .location = std::move(location),
  };
}

ConfigurationSchema::ConfigurationSchema(ConfigurationValueType type)
    : type_(type) {}

const ConfigurationValue* ConfigurationSchema::default_value() const noexcept {
  return default_value_.has_value() ? &*default_value_ : nullptr;
}

ConfigurationSchema& ConfigurationSchema::SetRequired(bool required) noexcept {
  required_ = required;
  return *this;
}

ConfigurationSchema& ConfigurationSchema::SetDeprecated(
    bool deprecated) noexcept {
  deprecated_ = deprecated;
  return *this;
}

ConfigurationSchema& ConfigurationSchema::SetSensitive(
    bool sensitive) noexcept {
  sensitive_ = sensitive;
  return *this;
}

ConfigurationSchema& ConfigurationSchema::SetDefault(ConfigurationValue value) {
  default_value_ = std::move(value);
  return *this;
}

Status ConfigurationSchema::SetSignedBounds(
    std::optional<std::int64_t> minimum, std::optional<std::int64_t> maximum) {
  if (type_ != ConfigurationValueType::kSignedInteger) {
    return Status(ErrorCode::kInvalidArgument,
                  "Signed bounds require a signed-integer schema");
  }
  if (minimum.has_value() && maximum.has_value() && *minimum > *maximum) {
    return Status(ErrorCode::kInvalidArgument,
                  "Signed minimum exceeds maximum");
  }
  signed_minimum_ = minimum;
  signed_maximum_ = maximum;
  return Status::Ok();
}

Status ConfigurationSchema::SetUnsignedBounds(
    std::optional<std::uint64_t> minimum,
    std::optional<std::uint64_t> maximum) {
  if (type_ != ConfigurationValueType::kUnsignedInteger) {
    return Status(ErrorCode::kInvalidArgument,
                  "Unsigned bounds require an unsigned-integer schema");
  }
  if (minimum.has_value() && maximum.has_value() && *minimum > *maximum) {
    return Status(ErrorCode::kInvalidArgument,
                  "Unsigned minimum exceeds maximum");
  }
  unsigned_minimum_ = minimum;
  unsigned_maximum_ = maximum;
  return Status::Ok();
}

Status ConfigurationSchema::SetDoubleBounds(std::optional<double> minimum,
                                            std::optional<double> maximum) {
  if (type_ != ConfigurationValueType::kDouble) {
    return Status(ErrorCode::kInvalidArgument,
                  "Double bounds require a double schema");
  }
  if ((minimum.has_value() && std::isnan(*minimum)) ||
      (maximum.has_value() && std::isnan(*maximum))) {
    return Status(ErrorCode::kInvalidArgument, "Double bounds cannot be NaN");
  }
  if (minimum.has_value() && maximum.has_value() && *minimum > *maximum) {
    return Status(ErrorCode::kInvalidArgument,
                  "Double minimum exceeds maximum");
  }
  double_minimum_ = minimum;
  double_maximum_ = maximum;
  return Status::Ok();
}

Status ConfigurationSchema::SetSizeBounds(std::optional<std::size_t> minimum,
                                          std::optional<std::size_t> maximum) {
  if (type_ != ConfigurationValueType::kString &&
      type_ != ConfigurationValueType::kList &&
      type_ != ConfigurationValueType::kObject) {
    return Status(ErrorCode::kInvalidArgument,
                  "Size bounds require a string, list, or object schema");
  }
  if (minimum.has_value() && maximum.has_value() && *minimum > *maximum) {
    return Status(ErrorCode::kInvalidArgument, "Size minimum exceeds maximum");
  }
  size_minimum_ = minimum;
  size_maximum_ = maximum;
  return Status::Ok();
}

Status ConfigurationSchema::AddField(std::string name,
                                     ConfigurationSchema field) {
  if (type_ != ConfigurationValueType::kObject) {
    return Status(ErrorCode::kInvalidArgument,
                  "Nested fields require an object schema");
  }
  if (fields_.contains(name)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Configuration schema field already exists");
  }
  fields_.emplace(std::move(name), std::move(field));
  return Status::Ok();
}

Configuration::Configuration(
    ConfigurationValue value,
    std::map<std::string, ConfigurationMetadata, std::less<>> metadata)
    : value_(std::move(value)), metadata_(std::move(metadata)) {}

Result<std::reference_wrapper<const ConfigurationValue>> Configuration::Find(
    std::string_view path) const {
  if (path.empty()) {
    return std::cref(value_);
  }
  if (path.front() != '/') {
    return Status(ErrorCode::kInvalidArgument,
                  "A non-empty configuration path must begin with '/'");
  }

  const ConfigurationValue* current = &value_;
  std::size_t token_start = 1;
  while (true) {
    const std::size_t separator = path.find('/', token_start);
    const std::string_view encoded_token =
        path.substr(token_start, separator - token_start);
    auto token = internal_core_configuration::UnescapePathToken(encoded_token);
    if (!token.ok()) {
      return token.status();
    }

    if (current->type() == ConfigurationValueType::kObject) {
      auto object = current->AsObject();
      ASC_DCHECK(object.ok());
      const auto iterator = object->get().find(*token);
      if (iterator == object->get().end()) {
        return Status(ErrorCode::kConfiguration,
                      "Configuration path does not exist");
      }
      current = &iterator->second;
    } else if (current->type() == ConfigurationValueType::kList) {
      auto list = current->AsList();
      ASC_DCHECK(list.ok());
      if (token->empty()) {
        return Status(ErrorCode::kInvalidArgument,
                      "A list path token must be an index");
      }
      std::size_t index = 0;
      for (char character : *token) {
        if (character < '0' || character > '9') {
          return Status(ErrorCode::kInvalidArgument,
                        "A list path token must be a decimal index");
        }
        const std::size_t digit = static_cast<std::size_t>(character - '0');
        if (index > (std::numeric_limits<std::size_t>::max() - digit) / 10U) {
          return Status(ErrorCode::kOverflow,
                        "Configuration list index overflows size_t");
        }
        index = index * 10U + digit;
      }
      if (index >= list->get().size()) {
        return Status(ErrorCode::kIndex,
                      "Configuration list index is out of range");
      }
      current = &list->get()[index];
    } else {
      return Status(ErrorCode::kConfiguration,
                    "Configuration path traverses a scalar value");
    }

    if (separator == std::string_view::npos) {
      break;
    }
    token_start = separator + 1;
  }
  return std::cref(*current);
}

Result<ConfigurationOrigin> Configuration::Origin(std::string_view path) const {
  const auto iterator = metadata_.find(path);
  if (iterator == metadata_.end()) {
    return Status(ErrorCode::kConfiguration,
                  "Configuration metadata path does not exist");
  }
  return iterator->second.origin;
}

Result<bool> Configuration::IsSensitive(std::string_view path) const {
  const auto iterator = metadata_.find(path);
  if (iterator == metadata_.end()) {
    return Status(ErrorCode::kConfiguration,
                  "Configuration metadata path does not exist");
  }
  return iterator->second.sensitive;
}

Result<bool> Configuration::IsDeprecated(std::string_view path) const {
  const auto iterator = metadata_.find(path);
  if (iterator == metadata_.end()) {
    return Status(ErrorCode::kConfiguration,
                  "Configuration metadata path does not exist");
  }
  return iterator->second.deprecated;
}

// Preserve the established public by-value origin ABI.
// NOLINTBEGIN(performance-unnecessary-value-param)
Result<Configuration> ValidateConfiguration(const ConfigurationSchema& schema,
                                            const ConfigurationValue& value,
                                            ConfigurationOrigin origin) {
  std::map<std::string, ConfigurationMetadata, std::less<>> metadata;
  auto validated = internal_core_configuration::ValidateNode(
      schema, value, "", origin, nullptr, false, metadata);
  if (!validated.ok()) {
    return validated.status();
  }
  return Configuration(std::move(*validated), std::move(metadata));
}

Result<Configuration> ValidateConfigurationWithOrigins(
    const ConfigurationSchema& schema, const ConfigurationValue& value,
    const ConfigurationOrigins& origins, ConfigurationOrigin fallback_origin) {
  std::map<std::string, ConfigurationMetadata, std::less<>> metadata;
  auto validated = internal_core_configuration::ValidateNode(
      schema, value, "", fallback_origin, &origins, false, metadata);
  if (!validated.ok()) {
    return validated.status();
  }
  for (const auto& [path, unused_origin] : origins) {
    static_cast<void>(unused_origin);
    if (!metadata.contains(path)) {
      return Status(ErrorCode::kConfiguration,
                    "Configuration origin path does not exist");
    }
  }
  return Configuration(std::move(*validated), std::move(metadata));
}
// NOLINTEND(performance-unnecessary-value-param)

std::string RenderConfigurationValue(const ConfigurationValue& value,
                                     bool sensitive) {
  if (sensitive) {
    return "<redacted>";
  }
  switch (value.type()) {
    case ConfigurationValueType::kNull:
      return "null";
    case ConfigurationValueType::kBool:
      return *value.AsBool() ? "true" : "false";
    case ConfigurationValueType::kSignedInteger:
      return std::to_string(*value.AsSignedInteger());
    case ConfigurationValueType::kUnsignedInteger:
      return std::to_string(*value.AsUnsignedInteger());
    case ConfigurationValueType::kDouble: {
      std::array<char, 64> buffer{};
      const auto result =
          std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                        *value.AsDouble(), std::chars_format::general);
      if (result.ec != std::errc{}) {
        return "<unrenderable-double>";
      }
      return {buffer.data(), result.ptr};
    }
    case ConfigurationValueType::kString:
      return internal_core_configuration::QuoteDiagnosticString(
          value.AsString()->get());
    case ConfigurationValueType::kList:
      return "<list size=" + std::to_string(value.AsList()->get().size()) + ">";
    case ConfigurationValueType::kObject:
      return "<object size=" + std::to_string(value.AsObject()->get().size()) +
             ">";
  }
  return "<unknown>";
}

}  // namespace asc
