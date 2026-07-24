// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_CORE_STATUS_H_
#define ASC_CORE_STATUS_H_

#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "asc/core/config.h"

namespace asc {

/// @brief Stable, provider-independent operation status code.
enum class StatusCode : std::uint8_t {
  kOk = 0,              ///< Operation completed successfully.
  kInvalidArgument,     ///< An input value is invalid.
  kOutOfRange,          ///< An index or value is outside its valid range.
  kFailedPrecondition,  ///< Required state or accessibility is absent.
  kOverflow,            ///< Checked arithmetic overflowed.
  kAllocationFailed,    ///< Storage allocation or construction failed.
  kUnavailable,         ///< A requested provider is not available.
  kUnsupported,         ///< The provider cannot implement the request.
  kBackendError,        ///< An enabled provider reported a native error.
  kNumericalFailure,    ///< A numerical method failed to produce a result.
  kInternal,            ///< An internal error escaped a provider boundary.
};

/// @brief Value describing operation success or a recoverable failure.
class [[nodiscard]] ASC_EXPORT Status {
 public:
  /// @brief Construct a successful status.
  Status() noexcept = default;

  /// @brief Construct a failed status with an ASC code and diagnostic.
  Status(StatusCode code, std::string message);

  /// @brief Return a successful status value.
  static Status Ok() noexcept { return Status(); }

  /// @brief Construct a failed status with native provider diagnostics.
  static Status FromProvider(StatusCode code, std::string message,
                             std::string provider,
                             std::int64_t provider_code);

  /// @brief Return true when the operation succeeded.
  bool ok() const noexcept { return code_ == StatusCode::kOk; }

  /// @brief Return true when the operation succeeded.
  explicit operator bool() const noexcept { return ok(); }

  /// @brief Return the stable ASC status code.
  StatusCode code() const noexcept { return code_; }

  /// @brief Return the human-readable diagnostic message.
  std::string_view message() const noexcept { return message_; }

  /// @brief Return the optional provider diagnostic name.
  std::string_view provider() const noexcept { return provider_; }

  /// @brief Return the optional signed native provider code.
  std::int64_t provider_code() const noexcept { return provider_code_; }

 private:
  Status(StatusCode code, std::string message, std::string provider,
         std::int64_t provider_code);

  StatusCode code_ = StatusCode::kOk;
  std::string message_;
  std::string provider_;
  std::int64_t provider_code_ = 0;
};

namespace detail {

[[noreturn]] ASC_EXPORT void InvalidResultStatus();
[[noreturn]] ASC_EXPORT void BadResultAccess(const Status& status);

}  // namespace detail

/// @brief Value-or-status result for operations that produce a value.
/// @tparam T Stored non-void, non-reference value type.
template <typename T>
class [[nodiscard]] Result {
  static_assert(!std::is_void_v<T>, "Result<void> is not supported; use Status");
  static_assert(!std::is_reference_v<T>, "Result<T> cannot store a reference");

 public:
  /// @brief Construct a successful result by copying a value.
  Result(const T& value) requires std::copy_constructible<T> : value_(value) {}

  /// @brief Construct a successful result by moving a value.
  Result(T&& value) : value_(std::move(value)) {}

  /// @brief Construct a failed result by copying a non-OK status.
  Result(const Status& status) : value_(ValidateStatus(status)) {}

  /// @brief Construct a failed result by moving a non-OK status.
  Result(Status&& status) : value_(ValidateStatus(std::move(status))) {}

  /// @brief Return true when this result stores a value.
  bool ok() const noexcept { return std::holds_alternative<T>(value_); }

  /// @brief Return true when this result stores a value.
  explicit operator bool() const noexcept { return ok(); }

  /// @brief Return the failure status, or an OK status for a value result.
  const Status& status() const noexcept {
    if (const auto* status = std::get_if<Status>(&value_)) {
      return *status;
    }
    static const Status kOkStatus;
    return kOkStatus;
  }

  /// @brief Return mutable lvalue access to the stored value.
  T& value() & {
    CheckHasValue();
    return std::get<T>(value_);
  }

  /// @brief Return immutable lvalue access to the stored value.
  const T& value() const& {
    CheckHasValue();
    return std::get<T>(value_);
  }

  /// @brief Return rvalue access to the stored value.
  T&& value() && {
    CheckHasValue();
    return std::get<T>(std::move(value_));
  }

 private:
  static Status ValidateStatus(Status status) {
    if (status.ok()) {
      detail::InvalidResultStatus();
    }
    return status;
  }

  void CheckHasValue() const {
    if (!ok()) {
      detail::BadResultAccess(std::get<Status>(value_));
    }
  }

  std::variant<T, Status> value_;
};

}  // namespace asc

#endif  // ASC_CORE_STATUS_H_
