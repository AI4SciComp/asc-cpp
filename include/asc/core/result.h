#ifndef ASC_CORE_RESULT_H_
#define ASC_CORE_RESULT_H_

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
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "asc/core/contracts.h"
#include "asc/core/status.h"

namespace asc {

// Stores either one T or one non-OK Status. Result supports move-only values.
/**
 * @brief Stores either one value or one non-OK Status.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
template <typename T>
class [[nodiscard]] Result {
  static_assert(!std::is_reference_v<T>, "Result<T> does not store references");
  static_assert(!std::is_void_v<T>, "Use Status for operations without values");

 public:
  // Value and failure conversions enable direct propagation from functions
  // returning Result<T>.
  // NOLINTBEGIN(google-explicit-constructor)
  /**
   * @brief Constructs a Result with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] value Value read or written by the operation.
   * @ingroup asc_core
   */
  Result(const T& value)
    requires std::copy_constructible<T>
      : value_(value) {}

  /**
   * @brief Constructs a Result with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] value Value read or written by the operation.
   * @ingroup asc_core
   */
  Result(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
      : value_(std::move(value)) {}

  /**
   * @brief Constructs a Result with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] status The status value required by this contract.
   * @ingroup asc_core
   */
  Result(Status status)
    requires(!std::same_as<T, Status>)
      : status_(std::move(status)) {
    ASC_CHECK_MESSAGE(!status_.ok(),
                      "A failed Result requires a non-OK Status");
  }
  // NOLINTEND(google-explicit-constructor)

  /**
   * @brief Performs the public Failure operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] status The status value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  static Result Failure(Status status) {
    ASC_CHECK_MESSAGE(!status.ok(), "A failed Result requires a non-OK Status");
    return Result(FailureTag{}, std::move(status));
  }

  /**
   * @brief Constructs a Result with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  Result(const Result&) = default;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  Result& operator=(const Result&) = default;
  /**
   * @brief Constructs a Result with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  Result(Result&&) noexcept(std::is_nothrow_move_constructible_v<T>) = default;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  Result& operator=(Result&&) noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_move_assignable_v<T>) = default;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  ~Result() = default;

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
  [[nodiscard]] bool ok() const noexcept { return value_.has_value(); }
  /**
   * @brief Returns the object's status contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_core
   */
  [[nodiscard]] const Status& status() const noexcept { return status_; }

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
  [[nodiscard]] T& value() & { return CheckedValue(); }

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
  [[nodiscard]] const T& value() const& { return CheckedValue(); }

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
  [[nodiscard]] T&& value() && { return std::move(CheckedValue()); }

  /**
   * @brief Performs the public operator* operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  T& operator*() & { return value(); }
  /**
   * @brief Performs the public operator* operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  const T& operator*() const& { return value(); }
  /**
   * @brief Performs the public operator* operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  T&& operator*() && { return std::move(*this).value(); }

  /**
   * @brief Performs the public operator-> operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  T* operator->() { return &CheckedValue(); }

  /**
   * @brief Performs the public operator-> operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  const T* operator->() const { return &CheckedValue(); }

 private:
  struct FailureTag {};

  Result(FailureTag /*tag*/, Status status) : status_(std::move(status)) {}

  [[noreturn]] void FailMissingValue() const {
    const std::string_view diagnostic =
        status_.message().empty() ? ErrorCodeName(status_.code())
                                  : std::string_view(status_.message());
    FatalContract("value_.has_value()", __FILE__, __LINE__, diagnostic);
  }

  [[nodiscard]] T& CheckedValue() {
    if (!value_.has_value()) {
      FailMissingValue();
    }
    return *value_;
  }

  [[nodiscard]] const T& CheckedValue() const {
    if (!value_.has_value()) {
      FailMissingValue();
    }
    return *value_;
  }

  std::optional<T> value_;
  Status status_;
};

}  // namespace asc

#endif  // ASC_CORE_RESULT_H_
