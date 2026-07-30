#ifndef ASC_CORE_RESULT_H_
#define ASC_CORE_RESULT_H_

#include <concepts>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "asc/core/contracts.h"
#include "asc/core/status.h"

namespace asc {

// Stores either one T or one non-OK Status. Result supports move-only values.
template <typename T>
class [[nodiscard]] Result {
  static_assert(!std::is_reference_v<T>, "Result<T> does not store references");
  static_assert(!std::is_void_v<T>, "Use Status for operations without values");

 public:
  // Value and failure conversions enable direct propagation from functions
  // returning Result<T>.
  // NOLINTBEGIN(google-explicit-constructor)
  Result(const T& value)
    requires std::copy_constructible<T>
      : value_(value) {}

  Result(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
      : value_(std::move(value)) {}

  Result(Status status)
    requires(!std::same_as<T, Status>)
      : status_(std::move(status)) {
    ASC_CHECK_MESSAGE(!status_.ok(),
                      "A failed Result requires a non-OK Status");
  }
  // NOLINTEND(google-explicit-constructor)

  static Result Failure(Status status) {
    ASC_CHECK_MESSAGE(!status.ok(), "A failed Result requires a non-OK Status");
    return Result(FailureTag{}, std::move(status));
  }

  Result(const Result&) = default;
  Result& operator=(const Result&) = default;
  Result(Result&&) noexcept(std::is_nothrow_move_constructible_v<T>) = default;
  Result& operator=(Result&&) noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_move_assignable_v<T>) = default;
  ~Result() = default;

  [[nodiscard]] bool ok() const noexcept { return value_.has_value(); }
  [[nodiscard]] const Status& status() const noexcept { return status_; }

  [[nodiscard]] T& value() & { return CheckedValue(); }

  [[nodiscard]] const T& value() const& { return CheckedValue(); }

  [[nodiscard]] T&& value() && { return std::move(CheckedValue()); }

  T& operator*() & { return value(); }
  const T& operator*() const& { return value(); }
  T&& operator*() && { return std::move(*this).value(); }

  T* operator->() { return &CheckedValue(); }

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
