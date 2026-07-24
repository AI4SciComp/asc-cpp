// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_CORE_EVENT_H_
#define ASC_CORE_EVENT_H_

#include <cstdint>
#include <memory>

#include "asc/core/config.h"
#include "asc/core/status.h"

namespace asc {

/// @brief Coarse execution backend selected by an execution context.
enum class BackendKind : std::uint8_t {
  kSerial = 0,
  kOpenMP,
  kCuda,
};

namespace detail {
class EventState;
}  // namespace detail

/// @brief Copyable handle to provider-owned operation completion state.
///
/// Event does not retain operation operands. Callers must keep all referenced
/// storage alive until the event completes. Destroying an event does not wait.
class ASC_EXPORT Event {
 public:
  /// @brief Construct an immediately completed, successful serial event.
  Event();

  /// @brief Share completion state with another event handle.
  Event(const Event&) noexcept = default;

  /// @brief Share completion state by copy assignment.
  Event& operator=(const Event&) noexcept = default;

  /// @brief Move a completion-state handle.
  Event(Event&&) noexcept = default;

  /// @brief Replace this handle with another moved completion-state handle.
  Event& operator=(Event&&) noexcept = default;

  /// @brief Release this handle without waiting for completion.
  ~Event() = default;

  /// @brief Query readiness without blocking.
  bool IsReady() const noexcept;

  /// @brief Wait for completion and return the provider completion status.
  ///
  /// Waiting is idempotent.
  Status Wait() const;

  /// @brief Return the backend that owns the event state.
  BackendKind GetBackend() const noexcept;

 private:
  std::shared_ptr<const detail::EventState> state_;
};

}  // namespace asc

#endif  // ASC_CORE_EVENT_H_
