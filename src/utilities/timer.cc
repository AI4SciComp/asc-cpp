#include "asc/utilities/timer.h"

#include <chrono>
#include <cstddef>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "timer_internal.h"

namespace asc {

Status Timer::Start() noexcept {
  if (state_ == TimerState::kRunning) {
    return Status(ErrorCode::kInvalidState, "The timer is already running");
  }
  start_time_ = Clock::now();
  state_ = TimerState::kRunning;
  return Status::Ok();
}

Result<Timer::Duration> Timer::Stop() noexcept {
  if (state_ != TimerState::kRunning) {
    return Status(ErrorCode::kInvalidState, "The timer is not running");
  }
  const Clock::time_point stop_time = Clock::now();
  auto interval =
      internal_utilities_timer::CheckedElapsedDuration(start_time_, stop_time);
  if (!interval.ok()) {
    return interval.status();
  }
  auto next_total =
      internal_utilities_timer::CheckedDurationAdd(total_, *interval);
  if (!next_total.ok()) {
    return next_total.status();
  }
  auto next_sample_count =
      internal_utilities_timer::CheckedNextSampleCount(sample_count_);
  if (!next_sample_count.ok()) {
    return next_sample_count.status();
  }

  last_ = *interval;
  total_ = *next_total;
  sample_count_ = *next_sample_count;
  state_ = TimerState::kStopped;
  return last_;
}

Timer::Duration Timer::Elapsed() const noexcept {
  if (state_ == TimerState::kRunning) {
    auto interval = internal_utilities_timer::CheckedElapsedDuration(
        start_time_, Clock::now());
    if (!interval.ok()) {
      return Duration::max();
    }
    auto elapsed =
        internal_utilities_timer::CheckedDurationAdd(total_, *interval);
    return elapsed.ok() ? *elapsed : Duration::max();
  }
  return total_;
}

Result<Timer::Duration> Timer::Last() const noexcept {
  if (sample_count_ == 0) {
    return Status(ErrorCode::kInvalidState,
                  "The timer has no completed interval");
  }
  return last_;
}

Result<Timer::Duration> Timer::Average() const noexcept {
  if (sample_count_ == 0) {
    return Status(ErrorCode::kInvalidState,
                  "The timer has no completed interval");
  }
  auto divisor =
      internal_utilities_timer::SampleCountAsDurationRep(sample_count_);
  if (!divisor.ok()) {
    return divisor.status();
  }
  return total_ / *divisor;
}

void Timer::Reset() noexcept {
  state_ = TimerState::kEmpty;
  start_time_ = Clock::time_point{};
  total_ = Duration{};
  last_ = Duration{};
  sample_count_ = 0;
}

}  // namespace asc
