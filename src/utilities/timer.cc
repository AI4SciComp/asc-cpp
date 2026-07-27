#include "asc/utilities/timer.h"

#include <chrono>
#include <concepts>
#include <cstddef>
#include <limits>

#include "asc/core/status.h"

namespace asc {

Status Timer::Start() noexcept {
  if (state_ == TimerState::kRunning) {
    return Status(ErrorCode::kInvalidState, "Timer is already running");
  }
  started_at_ = Clock::now();
  state_ = TimerState::kRunning;
  return Status::Ok();
}

Result<Timer::Duration> Timer::Stop() noexcept {
  if (state_ != TimerState::kRunning) {
    return Status(ErrorCode::kInvalidState, "Timer is not running");
  }

  const Clock::time_point stopped_at = Clock::now();
  const Duration interval = stopped_at - started_at_;
  if (interval < Duration::zero()) {
    return Status(ErrorCode::kInternal,
                  "Steady clock produced a negative interval");
  }
  if (interval > Duration::max() - total_) {
    return Status(ErrorCode::kOverflow, "Timer duration overflow");
  }
  if (sample_count_ == std::numeric_limits<std::size_t>::max()) {
    return Status(ErrorCode::kOverflow, "Timer sample count overflow");
  }

  total_ += interval;
  last_ = interval;
  ++sample_count_;
  state_ = TimerState::kStopped;
  return interval;
}

Result<Timer::Duration> Timer::Elapsed() const noexcept {
  if (state_ != TimerState::kRunning) {
    return total_;
  }

  const Duration interval = Clock::now() - started_at_;
  if (interval < Duration::zero()) {
    return Status(ErrorCode::kInternal,
                  "Steady clock produced a negative interval");
  }
  if (interval > Duration::max() - total_) {
    return Status(ErrorCode::kOverflow, "Timer duration overflow");
  }
  return total_ + interval;
}

Result<Timer::Duration> Timer::Last() const noexcept {
  if (sample_count_ == 0) {
    return Status(ErrorCode::kInvalidState, "Timer has no completed interval");
  }
  return last_;
}

Result<Timer::Duration> Timer::Average() const noexcept {
  if (sample_count_ == 0) {
    return Status(ErrorCode::kInvalidState, "Timer has no completed interval");
  }
  using Representation = Duration::rep;
  if constexpr (std::integral<Representation>) {
    if (sample_count_ >
        static_cast<std::size_t>(std::numeric_limits<Representation>::max())) {
      return Status(ErrorCode::kOverflow,
                    "Timer sample count does not fit duration arithmetic");
    }
  }
  return total_ / static_cast<Representation>(sample_count_);
}

void Timer::Reset() noexcept {
  state_ = TimerState::kEmpty;
  started_at_ = Clock::time_point{};
  total_ = Duration::zero();
  last_ = Duration::zero();
  sample_count_ = 0;
}

}  // namespace asc
