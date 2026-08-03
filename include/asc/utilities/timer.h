#ifndef ASC_UTILITIES_TIMER_H_
#define ASC_UTILITIES_TIMER_H_

/**
 * @file
 * @brief Public Utilities declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_utilities
 */

#include <chrono>
#include <cstddef>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/utilities/export.h"

namespace asc {

namespace internal_utilities_timer {
class TimerAccess;
}  // namespace internal_utilities_timer

/**
 * @brief Identifies whether a timer is stopped or running.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Utilities module contract.
 * @ingroup asc_utilities
 */
// Preserve the established public enum representation.
// NOLINTNEXTLINE(performance-enum-size)
enum class TimerState {
  kEmpty,    ///< Selects empty behavior.
  kRunning,  ///< Selects running behavior.
  kStopped,  ///< Selects stopped behavior.
};

/**
 * @brief Accumulates monotonic wall-clock samples without internal
 * synchronization.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Utilities module contract.
 * @ingroup asc_utilities
 */
class Timer {
 public:
  /**
   * @brief Defines the public Clock type used by this Utilities contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @ingroup asc_utilities
   */
  using Clock = std::chrono::steady_clock;
  /**
   * @brief Defines the public Duration type used by this Utilities contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @ingroup asc_utilities
   */
  using Duration = Clock::duration;

  /**
   * @brief Constructs a Timer with the documented ownership and validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   * @ingroup asc_utilities
   */
  Timer() noexcept = default;

  /**
   * @brief Returns the object's state contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_utilities
   */
  [[nodiscard]] TimerState state() const noexcept { return state_; }
  /**
   * @brief Returns the object's sample count contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_utilities
   */
  [[nodiscard]] std::size_t sample_count() const noexcept {
    return sample_count_;
  }

  /**
   * @brief Performs the start state transition defined by this Utilities
   * object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_utilities
   */
  ASC_UTILITIES_EXPORT Status Start() noexcept;
  /**
   * @brief Performs the stop state transition defined by this Utilities object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_utilities
   */
  [[nodiscard]] ASC_UTILITIES_EXPORT Result<Duration> Stop() noexcept;
  /**
   * @brief Performs the public Elapsed operation defined by the Utilities
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_utilities
   */
  [[nodiscard]] ASC_UTILITIES_EXPORT Duration Elapsed() const noexcept;
  /**
   * @brief Performs the public Last operation defined by the Utilities
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_utilities
   */
  [[nodiscard]] ASC_UTILITIES_EXPORT Result<Duration> Last() const noexcept;
  /**
   * @brief Performs the public Average operation defined by the Utilities
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_utilities
   */
  [[nodiscard]] ASC_UTILITIES_EXPORT Result<Duration> Average() const noexcept;
  /**
   * @brief Performs the reset state transition defined by this Utilities
   * object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @ingroup asc_utilities
   */
  ASC_UTILITIES_EXPORT void Reset() noexcept;

 private:
  /**
   * @brief Performs the public TimerAccess operation defined by the Utilities
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Utilities module contract.
   *
   * @ingroup asc_utilities
   */
  friend class internal_utilities_timer::TimerAccess;

  TimerState state_ = TimerState::kEmpty;
  Clock::time_point start_time_;
  Duration total_{};
  Duration last_{};
  std::size_t sample_count_ = 0;
};

}  // namespace asc

#endif  // ASC_UTILITIES_TIMER_H_
