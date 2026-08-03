#ifndef ASC_CORE_STATUS_H_
#define ASC_CORE_STATUS_H_

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

#include <cstdint>
#include <string>
#include <string_view>

#include "asc/core/export.h"

namespace asc {

// Stable machine-readable error categories. Diagnostic message text is not a
// stable interface.
/**
 * @brief Defines stable machine-readable failure categories.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
// The fixed 32-bit representation is part of the public error-code ABI.
// NOLINTNEXTLINE(performance-enum-size)
enum class ErrorCode : std::uint32_t {
  kOk = 0,               ///< Successful operation with no failure.
  kInvalidArgument = 1,  ///< Selects invalid argument behavior.
  kShape = 2,            ///< Selects shape behavior.
  kIndex = 3,            ///< Selects index behavior.
  kOverflow = 4,         ///< Selects overflow behavior.
  kInvalidState = 5,     ///< Selects invalid state behavior.
  kAllocation = 6,       ///< Selects allocation behavior.
  kMemoryAccess = 7,     ///< Selects memory access behavior.
  kMemoryTransfer = 8,   ///< Selects memory transfer behavior.
  kUnsupported = 9,      ///< Selects unsupported behavior.
  kUnavailable = 10,     ///< Selects unavailable behavior.
  kProvider = 11,        ///< Selects provider behavior.
  kNumerical = 12,       ///< Selects numerical behavior.
  kConfiguration = 13,   ///< Selects configuration behavior.
  kIo = 14,              ///< Selects io behavior.
  kEndOfFile = 15,       ///< Selects end of file behavior.
  kEncoding = 16,        ///< Selects encoding behavior.
  kVersion = 17,         ///< Selects version behavior.
  kInternal = 18,        ///< Selects internal behavior.
};

/**
 * @brief Performs the public ErrorCodeName operation defined by the Core
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] code The code value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT std::string_view ErrorCodeName(ErrorCode code) noexcept;

/**
 * @brief Carries a stable error category and optional diagnostic/provider
 * details.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class [[nodiscard]] Status {
 public:
  /**
   * @brief Constructs a Status with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  Status() noexcept = default;
  /**
   * @brief Constructs a Status with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] code The code value required by this contract.
   * @param[in] message The message value required by this contract.
   * @param[in] provider The provider value required by this contract.
   * @param[in] native_code The native code value required by this contract.
   * @ingroup asc_core
   */
  explicit ASC_CORE_EXPORT Status(ErrorCode code, std::string message = {},
                                  std::string provider = {},
                                  std::int64_t native_code = 0) noexcept;

  /**
   * @brief Performs the public Ok operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_core
   */
  static ASC_CORE_EXPORT Status Ok() noexcept;

  /**
   * @brief Reports whether the documented ok condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] bool ok() const noexcept { return code_ == ErrorCode::kOk; }
  /**
   * @brief Returns the object's code contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] ErrorCode code() const noexcept { return code_; }
  /**
   * @brief Returns the object's message contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] const std::string& message() const noexcept { return message_; }
  /**
   * @brief Returns the object's provider contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] const std::string& provider() const noexcept {
    return provider_;
  }
  /**
   * @brief Returns the object's native code contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] std::int64_t native_code() const noexcept {
    return native_code_;
  }

  /**
   * @brief Performs the public ToString operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT std::string ToString() const;

 private:
  ErrorCode code_ = ErrorCode::kOk;
  std::string message_;
  std::string provider_;
  std::int64_t native_code_ = 0;
};

}  // namespace asc

#endif  // ASC_CORE_STATUS_H_
