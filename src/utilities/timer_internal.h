#ifndef ASC_SRC_UTILITIES_TIMER_INTERNAL_H_
#define ASC_SRC_UTILITIES_TIMER_INTERNAL_H_

#include <chrono>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/utilities/timer.h"

namespace asc::internal_utilities_timer {

inline Result<Timer::Duration> CheckedDurationAdd(Timer::Duration left,
                                                  Timer::Duration right) {
  using Rep = Timer::Duration::rep;
  const Rep left_count = left.count();
  const Rep right_count = right.count();
  constexpr Rep kLowest = std::numeric_limits<Rep>::lowest();
  constexpr Rep kMaximum = std::numeric_limits<Rep>::max();

  if constexpr (std::is_unsigned_v<Rep>) {
    if (left_count > kMaximum - right_count) {
      return Status(ErrorCode::kOverflow, "Timer duration addition overflow");
    }
  } else {
    if ((right_count > Rep{0} && left_count > kMaximum - right_count) ||
        (right_count < Rep{0} && left_count < kLowest - right_count)) {
      return Status(ErrorCode::kOverflow, "Timer duration addition overflow");
    }
  }
  return Timer::Duration(left_count + right_count);
}

inline Result<Timer::Duration> CheckedElapsedDuration(
    Timer::Clock::time_point start, Timer::Clock::time_point stop) {
  if (stop < start) {
    return Status(ErrorCode::kInvalidState,
                  "The steady timer clock moved backwards");
  }

  using Rep = Timer::Duration::rep;
  const Rep start_count = start.time_since_epoch().count();
  const Rep stop_count = stop.time_since_epoch().count();
  constexpr Rep kMaximum = std::numeric_limits<Rep>::max();
  if constexpr (!std::is_unsigned_v<Rep>) {
    if (start_count < Rep{0} && stop_count > kMaximum + start_count) {
      return Status(ErrorCode::kOverflow, "Timer interval duration overflow");
    }
  }
  return Timer::Duration(stop_count - start_count);
}

inline Result<std::size_t> CheckedNextSampleCount(std::size_t sample_count) {
  if (sample_count == std::numeric_limits<std::size_t>::max()) {
    return Status(ErrorCode::kOverflow, "Timer sample count overflow");
  }
  const std::size_t next = sample_count + 1;
  using Rep = Timer::Duration::rep;
  if constexpr (std::integral<Rep>) {
    auto converted = CheckedCast<Rep>(next);
    if (!converted.ok()) {
      return Status(ErrorCode::kOverflow,
                    "Timer sample count exceeds the duration range");
    }
  }
  return next;
}

inline Result<Timer::Duration::rep> SampleCountAsDurationRep(
    std::size_t sample_count) {
  using Rep = Timer::Duration::rep;
  if constexpr (std::integral<Rep>) {
    auto converted = CheckedCast<Rep>(sample_count);
    if (!converted.ok()) {
      return Status(ErrorCode::kOverflow,
                    "Timer sample count exceeds the duration range");
    }
    return *converted;
  } else {
    const Rep converted = static_cast<Rep>(sample_count);
    if (converted > std::numeric_limits<Rep>::max()) {
      return Status(ErrorCode::kOverflow,
                    "Timer sample count exceeds the duration range");
    }
    return converted;
  }
}

// Private test access for deterministic boundary coverage. This header is a
// source-tree implementation detail and is not installed.
class TimerAccess {
 public:
  static void Set(Timer& timer, TimerState state,
                  Timer::Clock::time_point start_time, Timer::Duration total,
                  Timer::Duration last, std::size_t sample_count) noexcept {
    timer.state_ = state;
    timer.start_time_ = start_time;
    timer.total_ = total;
    timer.last_ = last;
    timer.sample_count_ = sample_count;
  }
};

}  // namespace asc::internal_utilities_timer

#endif  // ASC_SRC_UTILITIES_TIMER_INTERNAL_H_
