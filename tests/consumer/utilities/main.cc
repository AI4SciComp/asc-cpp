#include <cstddef>

#include "asc/utilities.h"

int main() {
  asc::Timer timer;
  if (!timer.Start().ok()) {
    return 1;
  }
  const auto interval = timer.Stop();
  if (!interval.ok() || *interval < asc::Timer::Duration::zero()) {
    return 2;
  }
  if (timer.state() != asc::TimerState::kStopped ||
      timer.sample_count() != std::size_t{1}) {
    return 3;
  }
  return 0;
}
