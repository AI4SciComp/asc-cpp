#ifndef ASC_UTILITIES_TIMER_H_
#define ASC_UTILITIES_TIMER_H_

#include <chrono>
#include <cstddef>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/utilities/export.h"

namespace asc {

namespace internal_utilities_timer {
class TimerAccess;
}  // namespace internal_utilities_timer

enum class TimerState {
  kEmpty,
  kRunning,
  kStopped,
};

class Timer {
 public:
  using Clock = std::chrono::steady_clock;
  using Duration = Clock::duration;

  Timer() noexcept = default;

  [[nodiscard]] TimerState state() const noexcept { return state_; }
  [[nodiscard]] std::size_t sample_count() const noexcept {
    return sample_count_;
  }

  ASC_UTILITIES_EXPORT Status Start() noexcept;
  [[nodiscard]] ASC_UTILITIES_EXPORT Result<Duration> Stop() noexcept;
  [[nodiscard]] ASC_UTILITIES_EXPORT Duration Elapsed() const noexcept;
  [[nodiscard]] ASC_UTILITIES_EXPORT Result<Duration> Last() const noexcept;
  [[nodiscard]] ASC_UTILITIES_EXPORT Result<Duration> Average() const noexcept;
  ASC_UTILITIES_EXPORT void Reset() noexcept;

 private:
  friend class internal_utilities_timer::TimerAccess;

  TimerState state_ = TimerState::kEmpty;
  Clock::time_point start_time_{};
  Duration total_{};
  Duration last_{};
  std::size_t sample_count_ = 0;
};

}  // namespace asc

#endif  // ASC_UTILITIES_TIMER_H_
