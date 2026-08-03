#ifndef ASC_CORE_CONFIGURATION_H_
#define ASC_CORE_CONFIGURATION_H_

/**
 * @file
 * @brief Public Core declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_core
 */

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

/**
 * @brief Identifies the active configuration value type.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
enum class ConfigurationValueType : std::uint8_t {
  kNull = 0,             ///< Selects null behavior.
  kBool = 1,             ///< Selects bool behavior.
  kSignedInteger = 2,    ///< Selects signed integer behavior.
  kUnsignedInteger = 3,  ///< Selects unsigned integer behavior.
  kDouble = 4,           ///< Selects double behavior.
  kString = 5,           ///< Selects string behavior.
  kList = 6,             ///< Selects list behavior.
  kObject = 7,           ///< Selects object behavior.
};

/**
 * @brief Performs the public ConfigurationValueTypeName operation defined by
 * the Core contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] type The type value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT std::string_view ConfigurationValueTypeName(
    ConfigurationValueType type) noexcept;

/**
 * @brief Stores one validated configuration value variant.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class ConfigurationValue {
 public:
  /**
   * @brief Defines the public List type used by this Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  using List = std::vector<ConfigurationValue>;
  /**
   * @brief Defines the public Object type used by this Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  using Object = std::map<std::string, ConfigurationValue, std::less<>>;

  // These conversions provide the variant-style construction contract.
  // NOLINTBEGIN(google-explicit-constructor)
  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationValue() noexcept;
  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationValue(std::nullptr_t) noexcept;

  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] value Value read or written by the operation.
   * @ingroup asc_core
   */
  template <typename Bool>
    requires std::same_as<std::remove_cvref_t<Bool>, bool>
  ConfigurationValue(Bool value) noexcept : storage_(value) {}

  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] value Value read or written by the operation.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationValue(std::int64_t value) noexcept;
  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] value Value read or written by the operation.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationValue(std::uint64_t value) noexcept;

  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @tparam Integer Type or non-type argument satisfying the declaration's
   * constraints.
   * @param[in] value Value read or written by the operation.
   * @ingroup asc_core
   */
  template <std::integral Integer>
    requires(!std::same_as<std::remove_cv_t<Integer>, bool> &&
             sizeof(Integer) <= sizeof(std::uint64_t) &&
             !std::same_as<std::remove_cv_t<Integer>, std::int64_t> &&
             !std::same_as<std::remove_cv_t<Integer>, std::uint64_t>)
  ConfigurationValue(Integer value) noexcept
      : storage_(IntegerStorage(value)) {}

  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] value Value read or written by the operation.
   * @ingroup asc_core
   */
  template <typename Floating>
    requires std::same_as<std::remove_cvref_t<Floating>, double>
  ConfigurationValue(Floating value) noexcept : storage_(value) {}

  /**
   * @brief Performs the public Utf8String operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] value Value read or written by the operation.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  static ASC_CORE_EXPORT Result<ConfigurationValue> Utf8String(
      std::string value);
  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  ConfigurationValue(const char*) = delete;
  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] value Value read or written by the operation.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationValue(List value);
  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] value Value read or written by the operation.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationValue(Object value);
  // NOLINTEND(google-explicit-constructor)

  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationValue(const ConfigurationValue&);
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationValue& operator=(const ConfigurationValue&);
  /**
   * @brief Constructs a ConfigurationValue with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationValue(ConfigurationValue&&) noexcept;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationValue& operator=(ConfigurationValue&&) noexcept;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ~ConfigurationValue();

  /**
   * @brief Returns the object's type contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT ConfigurationValueType type() const noexcept;
  /**
   * @brief Reports whether the documented is_null condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT bool is_null() const noexcept;

  /**
   * @brief Performs the public AsBool operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT Result<bool> AsBool() const;
  /**
   * @brief Computes the AsSignedInteger operation defined by the Core numerical
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT Result<std::int64_t> AsSignedInteger() const;
  /**
   * @brief Computes the AsUnsignedInteger operation defined by the Core
   * numerical contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT Result<std::uint64_t> AsUnsignedInteger() const;
  /**
   * @brief Performs the public AsDouble operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT Result<double> AsDouble() const;
  /**
   * @brief Performs the public AsString operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT
      Result<std::reference_wrapper<const std::string>>
      AsString() const;
  /**
   * @brief Performs the public AsList operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT Result<std::reference_wrapper<const List>>
  AsList() const;
  /**
   * @brief Performs the public AsObject operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
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

/**
 * @brief Identifies configuration precedence provenance.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
enum class ConfigurationOriginKind : std::uint8_t {
  kDefault = 0,       ///< Selects default behavior.
  kProgrammatic = 1,  ///< Selects programmatic behavior.
  kCommandLine = 2,   ///< Selects command line behavior.
};

/**
 * @brief Identifies how and where a configuration value was supplied.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
struct ConfigurationOrigin {
  /**
   * @brief Stores the ind value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  ConfigurationOriginKind kind = ConfigurationOriginKind::kProgrammatic;
  /**
   * @brief Stores the source label value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  std::optional<std::string> source_label;
  /**
   * @brief Stores the location value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  std::optional<std::string> location;

  /**
   * @brief Performs the public Default operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  static ASC_CORE_EXPORT ConfigurationOrigin Default();
  /**
   * @brief Performs the public Programmatic operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] source_label The source label value required by this contract.
   * @param[in] location The location value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  static ASC_CORE_EXPORT ConfigurationOrigin
  Programmatic(std::optional<std::string> source_label = std::nullopt,
               std::optional<std::string> location = std::nullopt);
  /**
   * @brief Performs the public CommandLine operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] source_label The source label value required by this contract.
   * @param[in] location The location value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  static ASC_CORE_EXPORT ConfigurationOrigin
  CommandLine(std::optional<std::string> source_label = std::nullopt,
              std::optional<std::string> location = std::nullopt);
};

/**
 * @brief Defines the public ConfigurationOrigins type used by this Core
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
using ConfigurationOrigins =
    std::map<std::string, ConfigurationOrigin, std::less<>>;

/**
 * @brief Defines recursive configuration type and validation constraints.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class ConfigurationSchema {
 public:
  /**
   * @brief Defines the public Fields type used by this Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  using Fields = std::map<std::string, ConfigurationSchema, std::less<>>;

  /**
   * @brief Constructs a ConfigurationSchema with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] type The type value required by this contract.
   * @ingroup asc_core
   */
  explicit ASC_CORE_EXPORT ConfigurationSchema(ConfigurationValueType type);

  /**
   * @brief Returns the object's type contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] ConfigurationValueType type() const noexcept { return type_; }
  /**
   * @brief Reports whether the documented required condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] bool required() const noexcept { return required_; }
  /**
   * @brief Reports whether the documented deprecated condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] bool deprecated() const noexcept { return deprecated_; }
  /**
   * @brief Reports whether the documented sensitive condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] bool sensitive() const noexcept { return sensitive_; }
  /**
   * @brief Performs the public default_value operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT const ConfigurationValue* default_value()
      const noexcept;
  /**
   * @brief Performs the public fields operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] const Fields& fields() const noexcept { return fields_; }

  /**
   * @brief Updates Core state after validating the requested value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] required The required value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationSchema& SetRequired(bool required) noexcept;
  /**
   * @brief Updates Core state after validating the requested value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] deprecated The deprecated value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationSchema& SetDeprecated(bool deprecated) noexcept;
  /**
   * @brief Updates Core state after validating the requested value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] sensitive The sensitive value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationSchema& SetSensitive(bool sensitive) noexcept;
  /**
   * @brief Updates Core state after validating the requested value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] value Value read or written by the operation.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ConfigurationSchema& SetDefault(ConfigurationValue value);

  /**
   * @brief Updates Core state after validating the requested value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] minimum The minimum value required by this contract.
   * @param[in] maximum The maximum value required by this contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT Status SetSignedBounds(std::optional<std::int64_t> minimum,
                                         std::optional<std::int64_t> maximum);
  /**
   * @brief Updates Core state after validating the requested value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] minimum The minimum value required by this contract.
   * @param[in] maximum The maximum value required by this contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT Status
  SetUnsignedBounds(std::optional<std::uint64_t> minimum,
                    std::optional<std::uint64_t> maximum);
  /**
   * @brief Updates Core state after validating the requested value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] minimum The minimum value required by this contract.
   * @param[in] maximum The maximum value required by this contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT Status SetDoubleBounds(std::optional<double> minimum,
                                         std::optional<double> maximum);
  /**
   * @brief Updates Core state after validating the requested value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] minimum The minimum value required by this contract.
   * @param[in] maximum The maximum value required by this contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT Status SetSizeBounds(std::optional<std::size_t> minimum,
                                       std::optional<std::size_t> maximum);
  /**
   * @brief Updates Core state after validating the requested value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] name The name value required by this contract.
   * @param[in] field The field value required by this contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT Status AddField(std::string name, ConfigurationSchema field);

  /**
   * @brief Returns the object's signed minimum contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] std::optional<std::int64_t> signed_minimum() const noexcept {
    return signed_minimum_;
  }
  /**
   * @brief Returns the object's signed maximum contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] std::optional<std::int64_t> signed_maximum() const noexcept {
    return signed_maximum_;
  }
  /**
   * @brief Returns the object's unsigned minimum contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] std::optional<std::uint64_t> unsigned_minimum() const noexcept {
    return unsigned_minimum_;
  }
  /**
   * @brief Returns the object's unsigned maximum contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] std::optional<std::uint64_t> unsigned_maximum() const noexcept {
    return unsigned_maximum_;
  }
  /**
   * @brief Returns the object's double minimum contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] std::optional<double> double_minimum() const noexcept {
    return double_minimum_;
  }
  /**
   * @brief Returns the object's double maximum contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] std::optional<double> double_maximum() const noexcept {
    return double_maximum_;
  }
  /**
   * @brief Returns the object's size minimum contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] std::optional<std::size_t> size_minimum() const noexcept {
    return size_minimum_;
  }
  /**
   * @brief Returns the object's size maximum contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
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

/**
 * @brief Records origin, sensitivity, and deprecation metadata.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
struct ConfigurationMetadata {
  /**
   * @brief Stores the origin value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  ConfigurationOrigin origin;
  /**
   * @brief Stores the sensitive value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  bool sensitive = false;
  /**
   * @brief Stores the deprecated value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  bool deprecated = false;
};

/**
 * @brief Stores a validated configuration and per-path metadata.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class Configuration {
 public:
  /**
   * @brief Performs the public value operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] const ConfigurationValue& value() const noexcept {
    return value_;
  }

  // Paths use JSON Pointer syntax. The empty path names the root.
  /**
   * @brief Performs the public Find operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] path Filesystem path interpreted by the synchronous I/O
   * operation.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT
      Result<std::reference_wrapper<const ConfigurationValue>>
      Find(std::string_view path) const;
  /**
   * @brief Performs the public Origin operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] path Filesystem path interpreted by the synchronous I/O
   * operation.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT Result<ConfigurationOrigin> Origin(
      std::string_view path) const;
  /**
   * @brief Reports whether the documented IsSensitive condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] path Filesystem path interpreted by the synchronous I/O
   * operation.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT Result<bool> IsSensitive(
      std::string_view path) const;
  /**
   * @brief Reports whether the documented IsDeprecated condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] path Filesystem path interpreted by the synchronous I/O
   * operation.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
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
/**
 * @brief Validates the documented shape, access, ownership, and provider
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] schema The schema value required by this contract.
 * @param[in] value Value read or written by the operation.
 * @param[in] origin The origin value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT Result<Configuration> ValidateConfiguration(
    const ConfigurationSchema& schema, const ConfigurationValue& value,
    ConfigurationOrigin origin = ConfigurationOrigin::Programmatic());

// The origin map uses JSON Pointer paths. An exact path entry overrides the
// inherited origin for that value and its descendants. Defaults always retain
// kDefault origin.
/**
 * @brief Validates the documented shape, access, ownership, and provider
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] schema The schema value required by this contract.
 * @param[in] value Value read or written by the operation.
 * @param[in] origins The origins value required by this contract.
 * @param[in] fallback_origin The fallback origin value required by this
 * contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT Result<Configuration> ValidateConfigurationWithOrigins(
    const ConfigurationSchema& schema, const ConfigurationValue& value,
    const ConfigurationOrigins& origins,
    ConfigurationOrigin fallback_origin = ConfigurationOrigin::Programmatic());

/**
 * @brief Performs the public RenderConfigurationValue operation defined by the
 * Core contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] value Value read or written by the operation.
 * @param[in] sensitive The sensitive value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT std::string RenderConfigurationValue(
    const ConfigurationValue& value, bool sensitive);

}  // namespace asc

#endif  // ASC_CORE_CONFIGURATION_H_
