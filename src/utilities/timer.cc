// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/timer.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include "asc/utilities/timer.h"

#include <algorithm>

#include "asc/core/contracts.h"

namespace asc {

real_t Timer::Stop() {
  ASC_REQUIRE(is_running_, "Timer::Stop requires an active measurement");
  const TimePoint stop_time = Clock::now();
  const Duration difference = Duration(stop_time - start_time_);
  const real_t elapsed = std::max<real_t>(difference.count(), 0);
  time_deltas_.push_back(elapsed);
  total_time_ += elapsed;
  last_time_ = elapsed;
  ++measurement_count_;
  is_running_ = false;
  return elapsed;
}

real_t Timer::TotalTime(int unit) const {
  ASC_REQUIRE(unit == kSecond || unit == kMilliSecond,
              "Timer time unit is invalid");
  switch (unit) {
    case kSecond:
      return total_time_;
    case kMilliSecond:
      return total_time_ * 1.e3;
  }
  return 0;
}

void Timer::AccumulateTime(real_t* result) const {
  if (time_deltas_.empty()) {
    return;
  }
  ASC_REQUIRE(result != nullptr,
              "Timer accumulation requires a non-null output buffer");
  real_t accumulated = 0;
  for (std::size_t i = 0; i < time_deltas_.size(); ++i) {
    accumulated += time_deltas_[i];
    result[i] = accumulated;
  }
}

void Timer::Compress() {}

void Timer::Reset() noexcept {
  start_time_ = TimePoint{};
  time_deltas_.clear();
  total_time_ = 0;
  last_time_ = 0;
  measurement_count_ = 0;
  is_running_ = false;
}

void Timer::Print(std::ostream& os, std::string msg, int unit) {
  ASC_REQUIRE(unit == kSecond || unit == kMilliSecond,
              "Timer time unit is invalid");
  os << msg << ": ";
  switch (unit) {
    case kSecond:
      os << LastTime() << " s";
      break;
    case kMilliSecond:
      os << LastTime() * 1.e3 << " ms";
      break;
  }
  os << '\n';
}

}  // namespace asc
