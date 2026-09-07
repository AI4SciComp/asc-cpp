#include "asc/utilities/timer.h"

#include <chrono>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

#include "../../src/utilities/timer_internal.h"
#include "asc/core/status.h"
#include "test_support.h"

namespace {

static_assert(std::is_same_v<asc::Timer::Clock, std::chrono::steady_clock>);
static_assert(asc::Timer::Clock::is_steady);
static_assert(noexcept(std::declval<asc::Timer&>().Start()));
static_assert(noexcept(std::declval<asc::Timer&>().Stop()));
static_assert(noexcept(std::declval<const asc::Timer&>().Elapsed()));
static_assert(noexcept(std::declval<const asc::Timer&>().Last()));
static_assert(noexcept(std::declval<const asc::Timer&>().Average()));
static_assert(noexcept(std::declval<asc::Timer&>().Reset()));

void CheckInitialAndInvalidStates(asc_utilities_test::TestContext& context) {
  asc::Timer timer;
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kEmpty);
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{0});
  ASC_UTILITIES_TEST_EQ(context, timer.Elapsed(), asc::Timer::Duration::zero());
  ASC_UTILITIES_TEST_EQ(context, timer.Last().status().code(),
                        asc::ErrorCode::kInvalidState);
  ASC_UTILITIES_TEST_EQ(context, timer.Average().status().code(),
                        asc::ErrorCode::kInvalidState);
  ASC_UTILITIES_TEST_EQ(context, timer.Stop().status().code(),
                        asc::ErrorCode::kInvalidState);
}

void CheckSamplesAndArithmetic(asc_utilities_test::TestContext& context) {
  asc::Timer timer;
  ASC_UTILITIES_TEST_CHECK(context, timer.Start().ok());
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kRunning);
  ASC_UTILITIES_TEST_EQ(context, timer.Start().code(),
                        asc::ErrorCode::kInvalidState);
  ASC_UTILITIES_TEST_CHECK(context,
                           timer.Elapsed() >= asc::Timer::Duration::zero());

  const auto first = timer.Stop();
  ASC_UTILITIES_TEST_CHECK(context, first.ok());
  ASC_UTILITIES_TEST_CHECK(context, *first >= asc::Timer::Duration::zero());
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kStopped);
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{1});
  ASC_UTILITIES_TEST_EQ(context, *timer.Last(), *first);
  ASC_UTILITIES_TEST_EQ(context, *timer.Average(), *first);
  ASC_UTILITIES_TEST_EQ(context, timer.Elapsed(), *first);

  ASC_UTILITIES_TEST_CHECK(context, timer.Start().ok());
  ASC_UTILITIES_TEST_CHECK(context, timer.Elapsed() >= *first);
  const auto second = timer.Stop();
  ASC_UTILITIES_TEST_CHECK(context, second.ok());
  ASC_UTILITIES_TEST_CHECK(context, *second >= asc::Timer::Duration::zero());
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{2});
  ASC_UTILITIES_TEST_EQ(context, *timer.Last(), *second);
  ASC_UTILITIES_TEST_EQ(context, timer.Elapsed(), *first + *second);
  ASC_UTILITIES_TEST_EQ(context, *timer.Average(), (*first + *second) / 2);
}

void CheckResetFromEveryState(asc_utilities_test::TestContext& context) {
  asc::Timer timer;
  timer.Reset();
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kEmpty);

  ASC_UTILITIES_TEST_CHECK(context, timer.Start().ok());
  timer.Reset();
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kEmpty);
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{0});
  ASC_UTILITIES_TEST_EQ(context, timer.Elapsed(), asc::Timer::Duration::zero());
  ASC_UTILITIES_TEST_CHECK(context, !timer.Last().ok());

  ASC_UTILITIES_TEST_CHECK(context, timer.Start().ok());
  ASC_UTILITIES_TEST_CHECK(context, timer.Stop().ok());
  timer.Reset();
  ASC_UTILITIES_TEST_EQ(context, timer.state(), asc::TimerState::kEmpty);
  ASC_UTILITIES_TEST_EQ(context, timer.sample_count(), std::size_t{0});
  ASC_UTILITIES_TEST_EQ(context, timer.Elapsed(), asc::Timer::Duration::zero());
  ASC_UTILITIES_TEST_CHECK(context, !timer.Average().ok());
}

void CheckArithmeticBoundaries(asc_utilities_test::TestContext& context) {
  using asc::internal_utilities_timer::CheckedDurationAdd;
  using asc::internal_utilities_timer::CheckedElapsedDuration;
  using asc::internal_utilities_timer::CheckedNextSampleCount;
  using asc::internal_utilities_timer::SampleCountAsDurationRep;
  using Duration = asc::Timer::Duration;
  using Rep = Duration::rep;

  const auto positive_overflow =
      CheckedDurationAdd(Duration::max(), Duration{1});
  ASC_UTILITIES_TEST_EQ(context, positive_overflow.status().code(),
                        asc::ErrorCode::kOverflow);
  if constexpr (std::is_signed_v<Rep>) {
    const auto negative_overflow =
        CheckedDurationAdd(Duration::min(), Duration{-1});
    ASC_UTILITIES_TEST_EQ(context, negative_overflow.status().code(),
                          asc::ErrorCode::kOverflow);

    const asc::Timer::Clock::time_point earliest(Duration::min());
    const asc::Timer::Clock::time_point latest(Duration::max());
    const auto interval_overflow = CheckedElapsedDuration(earliest, latest);
    ASC_UTILITIES_TEST_EQ(context, interval_overflow.status().code(),
                          asc::ErrorCode::kOverflow);
  }

  const auto count_overflow =
      CheckedNextSampleCount(std::numeric_limits<std::size_t>::max());
  ASC_UTILITIES_TEST_EQ(context, count_overflow.status().code(),
                        asc::ErrorCode::kOverflow);

  if constexpr (std::numeric_limits<std::size_t>::digits >
                std::numeric_limits<Rep>::digits) {
    const auto divisor_overflow =
        SampleCountAsDurationRep(std::numeric_limits<std::size_t>::max());
    ASC_UTILITIES_TEST_EQ(context, divisor_overflow.status().code(),
                          asc::ErrorCode::kOverflow);
  }
}

void CheckPublicOverflowTransactions(asc_utilities_test::TestContext& context) {
  using Access = asc::internal_utilities_timer::TimerAccess;
  using Duration = asc::Timer::Duration;

  asc::Timer total_overflow;
  constexpr Duration kOldLast{37};
  constexpr std::size_t kOldCount = 4;
  Access::Set(total_overflow, asc::TimerState::kRunning,
              asc::Timer::Clock::now() - Duration{1}, Duration::max(), kOldLast,
              kOldCount);
  const auto failed_total = total_overflow.Stop();
  ASC_UTILITIES_TEST_EQ(context, failed_total.status().code(),
                        asc::ErrorCode::kOverflow);
  ASC_UTILITIES_TEST_EQ(context, total_overflow.state(),
                        asc::TimerState::kRunning);
  ASC_UTILITIES_TEST_EQ(context, total_overflow.sample_count(), kOldCount);
  ASC_UTILITIES_TEST_EQ(context, *total_overflow.Last(), kOldLast);
  ASC_UTILITIES_TEST_EQ(context, total_overflow.Elapsed(), Duration::max());

  asc::Timer count_overflow;
  const std::size_t maximum_count = std::numeric_limits<std::size_t>::max();
  Access::Set(count_overflow, asc::TimerState::kRunning,
              asc::Timer::Clock::now() - Duration{1}, Duration{11}, Duration{7},
              maximum_count);
  const auto failed_count = count_overflow.Stop();
  ASC_UTILITIES_TEST_EQ(context, failed_count.status().code(),
                        asc::ErrorCode::kOverflow);
  ASC_UTILITIES_TEST_EQ(context, count_overflow.state(),
                        asc::TimerState::kRunning);
  ASC_UTILITIES_TEST_EQ(context, count_overflow.sample_count(), maximum_count);
  ASC_UTILITIES_TEST_EQ(context, *count_overflow.Last(), Duration{7});

  Access::Set(count_overflow, asc::TimerState::kStopped,
              asc::Timer::Clock::time_point{}, Duration{11}, Duration{7},
              maximum_count);
  const auto failed_average = count_overflow.Average();
  ASC_UTILITIES_TEST_EQ(context, failed_average.status().code(),
                        asc::ErrorCode::kOverflow);
  ASC_UTILITIES_TEST_EQ(context, count_overflow.state(),
                        asc::TimerState::kStopped);
  ASC_UTILITIES_TEST_EQ(context, count_overflow.sample_count(), maximum_count);
  ASC_UTILITIES_TEST_EQ(context, *count_overflow.Last(), Duration{7});
}

}  // namespace

int main() {
  asc_utilities_test::TestContext context;
  CheckInitialAndInvalidStates(context);
  CheckSamplesAndArithmetic(context);
  CheckResetFromEveryState(context);
  CheckArithmeticBoundaries(context);
  CheckPublicOverflowTransactions(context);
  return context.Finish();
}
