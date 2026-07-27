#ifndef ASC_UTILITIES_TIMER_H_
#define ASC_UTILITIES_TIMER_H_

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/utilities/export.h"

namespace asc {

enum class TimerState : std::uint8_t {
  kEmpty = 0,
  kRunning = 1,
  kStopped = 2,
};

class Timer {
 public:
  using Clock = std::chrono::steady_clock;
  using Duration = Clock::duration;

  [[nodiscard]] TimerState state() const noexcept { return state_; }
  [[nodiscard]] std::size_t sample_count() const noexcept {
    return sample_count_;
  }
  [[nodiscard]] Duration total() const noexcept { return total_; }

  ASC_UTILITIES_EXPORT Status Start() noexcept;
  [[nodiscard]] ASC_UTILITIES_EXPORT Result<Duration> Stop() noexcept;
  [[nodiscard]] ASC_UTILITIES_EXPORT Result<Duration> Elapsed() const noexcept;
  [[nodiscard]] ASC_UTILITIES_EXPORT Result<Duration> Last() const noexcept;
  [[nodiscard]] ASC_UTILITIES_EXPORT Result<Duration> Average() const noexcept;
  ASC_UTILITIES_EXPORT void Reset() noexcept;

 private:
  TimerState state_ = TimerState::kEmpty;
  Clock::time_point started_at_{};
  Duration total_ = Duration::zero();
  Duration last_ = Duration::zero();
  std::size_t sample_count_ = 0;
};

}  // namespace asc

#endif  // ASC_UTILITIES_TIMER_H_
