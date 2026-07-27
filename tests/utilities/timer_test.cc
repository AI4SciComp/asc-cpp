#include "asc/utilities/timer.h"

#include <chrono>
#include <cstddef>

#include "asc/core/status.h"
#include "test_support.h"

namespace {

void BoundedWork() {
  volatile std::size_t accumulator = 0;
  for (std::size_t index = 0; index < 1000; ++index) {
    accumulator = accumulator + index;
  }
  static_cast<void>(accumulator);
}

void CheckInitialAndInvalidStates(asc_utilities_test::TestContext& context) {
  asc::Timer timer;
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kEmpty);
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{0});
  ASC_UTILITIES_TEST_EQ(context, timer.total(), asc::Timer::Duration::zero());

  const auto elapsed = timer.Elapsed();
  ASC_UTILITIES_TEST_CHECK(context, elapsed.ok());
  ASC_UTILITIES_TEST_EQ(context, *elapsed, asc::Timer::Duration::zero());

  const auto last = timer.Last();
  ASC_UTILITIES_TEST_CHECK(context, !last.ok());
  ASC_UTILITIES_TEST_EQ(context, last.status().code(),
                        asc::ErrorCode::kInvalidState);
  const auto average = timer.Average();
  ASC_UTILITIES_TEST_CHECK(context, !average.ok());
  ASC_UTILITIES_TEST_EQ(context, average.status().code(),
                        asc::ErrorCode::kInvalidState);
  const auto stop = timer.Stop();
  ASC_UTILITIES_TEST_CHECK(context, !stop.ok());
  ASC_UTILITIES_TEST_EQ(context, stop.status().code(),
                        asc::ErrorCode::kInvalidState);
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kEmpty);
}

void CheckSamplesAndAccumulation(asc_utilities_test::TestContext& context) {
  asc::Timer timer;
  ASC_UTILITIES_TEST_CHECK(context, timer.Start().ok());
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kRunning);

  const auto duplicate_start = timer.Start();
  ASC_UTILITIES_TEST_CHECK(context, !duplicate_start.ok());
  ASC_UTILITIES_TEST_EQ(context, duplicate_start.code(),
                        asc::ErrorCode::kInvalidState);
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kRunning);
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{0});

  BoundedWork();
  const auto running_elapsed = timer.Elapsed();
  ASC_UTILITIES_TEST_CHECK(context, running_elapsed.ok());
  ASC_UTILITIES_TEST_CHECK(context,
                           *running_elapsed >= asc::Timer::Duration::zero());

  const auto first = timer.Stop();
  ASC_UTILITIES_TEST_CHECK(context, first.ok());
  ASC_UTILITIES_TEST_CHECK(context, *first >= asc::Timer::Duration::zero());
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kStopped);
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{1});
  ASC_UTILITIES_TEST_EQ(context, timer.total(), *first);
  ASC_UTILITIES_TEST_EQ(context, *timer.Last(), *first);
  ASC_UTILITIES_TEST_EQ(context, *timer.Average(), *first);
  ASC_UTILITIES_TEST_EQ(context, *timer.Elapsed(), *first);

  const auto repeated_stop = timer.Stop();
  ASC_UTILITIES_TEST_CHECK(context, !repeated_stop.ok());
  ASC_UTILITIES_TEST_EQ(context, repeated_stop.status().code(),
                        asc::ErrorCode::kInvalidState);
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{1});
  ASC_UTILITIES_TEST_EQ(context, timer.total(), *first);

  ASC_UTILITIES_TEST_CHECK(context, timer.Start().ok());
  const auto running_after_sample = timer.Elapsed();
  ASC_UTILITIES_TEST_CHECK(context, running_after_sample.ok());
  ASC_UTILITIES_TEST_CHECK(context, *running_after_sample >= *first);
  BoundedWork();
  const auto second = timer.Stop();
  ASC_UTILITIES_TEST_CHECK(context, second.ok());
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{2});
  ASC_UTILITIES_TEST_EQ(context, timer.total(), *first + *second);
  ASC_UTILITIES_TEST_EQ(context, *timer.Last(), *second);
  ASC_UTILITIES_TEST_EQ(context, *timer.Average(), (*first + *second) / 2);
}

void CheckResetFromEveryState(asc_utilities_test::TestContext& context) {
  asc::Timer timer;
  timer.Reset();
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kEmpty);

  ASC_UTILITIES_TEST_CHECK(context, timer.Start().ok());
  BoundedWork();
  timer.Reset();
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kEmpty);
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{0});
  ASC_UTILITIES_TEST_EQ(context, timer.total(), asc::Timer::Duration::zero());
  ASC_UTILITIES_TEST_CHECK(context, !timer.Last().ok());
  ASC_UTILITIES_TEST_CHECK(context, !timer.Average().ok());
  ASC_UTILITIES_TEST_EQ(context, *timer.Elapsed(),
                        asc::Timer::Duration::zero());

  ASC_UTILITIES_TEST_CHECK(context, timer.Start().ok());
  ASC_UTILITIES_TEST_CHECK(context, timer.Stop().ok());
  timer.Reset();
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kEmpty);
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{0});
  ASC_UTILITIES_TEST_EQ(context, timer.total(), asc::Timer::Duration::zero());
}

}  // namespace

int main() {
  asc_utilities_test::TestContext context;
  CheckInitialAndInvalidStates(context);
  CheckSamplesAndAccumulation(context);
  CheckResetFromEveryState(context);
  return context.Finish();
}
