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

  T& value() & {
    CheckHasValue();
    return *value_;
  }

  const T& value() const& {
    CheckHasValue();
    return *value_;
  }

  T&& value() && {
    CheckHasValue();
    return std::move(*value_);
  }

  T& operator*() & { return value(); }
  const T& operator*() const& { return value(); }
  T&& operator*() && { return std::move(*this).value(); }

  T* operator->() {
    CheckHasValue();
    return &*value_;
  }

  const T* operator->() const {
    CheckHasValue();
    return &*value_;
  }

 private:
  struct FailureTag {};

  Result(FailureTag, Status status) : status_(std::move(status)) {}

  void CheckHasValue() const {
    if (!value_.has_value()) {
      const std::string_view diagnostic =
          status_.message().empty() ? ErrorCodeName(status_.code())
                                    : std::string_view(status_.message());
      FatalContract("value_.has_value()", __FILE__, __LINE__, diagnostic);
    }
  }

  std::optional<T> value_;
  Status status_;
};

}  // namespace asc

#endif  // ASC_CORE_RESULT_H_
