#include <asc/core/contracts.h>
#include <asc/utilities/timer.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <sstream>
#include <string>

namespace asc {
namespace {

TEST(TimerTest, EmptyQueriesAndAccumulationAreSafe) {
  const Timer timer;

  EXPECT_FALSE(timer.IsRunning());
  EXPECT_EQ(timer.GetMeasurementCount(), 0U);
  EXPECT_EQ(timer.LastTime(), real_t{0});
  EXPECT_EQ(timer.TotalTime(), real_t{0});
  EXPECT_EQ(timer.TotalTime(kMilliSecond), real_t{0});
  EXPECT_EQ(timer.AverageTime(), real_t{0});
  EXPECT_NO_THROW(timer.AccumulateTime(nullptr));
}

TEST(TimerTest, TracksIdleRunningAndRepeatedStart) {
  Timer timer;

  timer.Start();
  EXPECT_TRUE(timer.IsRunning());
  timer.Start();
  EXPECT_TRUE(timer.IsRunning());

  const real_t elapsed = timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
  EXPECT_GE(elapsed, real_t{0});
  EXPECT_EQ(timer.GetMeasurementCount(), 1U);
  EXPECT_EQ(timer.LastTime(), elapsed);
  EXPECT_EQ(timer.TotalTime(), elapsed);
  EXPECT_EQ(timer.AverageTime(), elapsed);
}

TEST(TimerTest, MaintainsStatisticsBeyondHistoricalCapacity) {
  Timer timer;
  constexpr std::size_t kMeasurements = 257;
  real_t expected_total = 0;
  real_t expected_last = 0;

  for (std::size_t i = 0; i < kMeasurements; ++i) {
    timer.Start();
    expected_last = timer.Stop();
    expected_total += expected_last;
  }

  const real_t scale =
      std::max(real_t{1}, expected_total) *
      std::numeric_limits<real_t>::epsilon() * real_t{kMeasurements};
  EXPECT_EQ(timer.GetMeasurementCount(), kMeasurements);
  EXPECT_EQ(timer.LastTime(), expected_last);
  EXPECT_NEAR(timer.TotalTime(), expected_total, scale);
  EXPECT_NEAR(timer.AverageTime(),
              expected_total / static_cast<real_t>(kMeasurements), scale);
  EXPECT_NEAR(timer.TotalTime(kMilliSecond),
              timer.TotalTime(kSecond) * real_t{1000}, scale * real_t{1000});
}

TEST(TimerTest, AccumulationReturnsRecordedPrefixSums) {
  Timer timer;
  std::array<real_t, 3> elapsed{};
  std::array<real_t, 3> accumulated{};

  for (std::size_t i = 0; i < elapsed.size(); ++i) {
    timer.Start();
    elapsed[i] = timer.Stop();
  }
  timer.AccumulateTime(accumulated.data());

  EXPECT_EQ(accumulated[0], elapsed[0]);
  EXPECT_EQ(accumulated[1], elapsed[0] + elapsed[1]);
  EXPECT_EQ(accumulated[2], elapsed[0] + elapsed[1] + elapsed[2]);
}

TEST(TimerTest, CompressPreservesObservableStatistics) {
  Timer timer;
  for (int i = 0; i < 7; ++i) {
    timer.Start();
    static_cast<void>(timer.Stop());
  }

  const std::size_t count = timer.GetMeasurementCount();
  const real_t last = timer.LastTime();
  const real_t total = timer.TotalTime();
  const real_t average = timer.AverageTime();

  timer.Compress();

  EXPECT_EQ(timer.GetMeasurementCount(), count);
  EXPECT_EQ(timer.LastTime(), last);
  EXPECT_EQ(timer.TotalTime(), total);
  EXPECT_EQ(timer.AverageTime(), average);
}

TEST(TimerTest, ResetRestoresEmptyIdleState) {
  Timer timer;
  timer.Start();
  static_cast<void>(timer.Stop());
  timer.Start();

  timer.Reset();

  EXPECT_FALSE(timer.IsRunning());
  EXPECT_EQ(timer.GetMeasurementCount(), 0U);
  EXPECT_EQ(timer.LastTime(), real_t{0});
  EXPECT_EQ(timer.TotalTime(), real_t{0});
  EXPECT_EQ(timer.AverageTime(), real_t{0});
  EXPECT_NO_THROW(timer.AccumulateTime(nullptr));
}

TEST(TimerTest, PrintUsesLastMeasurementUnitAndOneNewline) {
  Timer timer;
  timer.Start();
  static_cast<void>(timer.Stop());

  std::ostringstream seconds;
  timer.Print(seconds, "work", kSecond);
  EXPECT_TRUE(seconds.str().starts_with("work: "));
  EXPECT_TRUE(seconds.str().ends_with(" s\n"));
  EXPECT_EQ(seconds.str().find('\n'), seconds.str().size() - 1);

  std::ostringstream milliseconds;
  timer.Print(milliseconds, "work", kMilliSecond);
  EXPECT_TRUE(milliseconds.str().starts_with("work: "));
  EXPECT_TRUE(milliseconds.str().ends_with(" ms\n"));
  EXPECT_EQ(milliseconds.str().find('\n'), milliseconds.str().size() - 1);
}

#ifdef ASC_USE_EXCEPTION
TEST(TimerTest, RejectsInvalidStateUnitsAndOutputPointer) {
  Timer timer;
  EXPECT_THROW(static_cast<void>(timer.Stop()), ContractException);
  EXPECT_THROW(static_cast<void>(timer.TotalTime(73)), ContractException);

  timer.Start();
  static_cast<void>(timer.Stop());
  EXPECT_THROW(timer.AccumulateTime(nullptr), ContractException);

  std::ostringstream output;
  EXPECT_THROW(timer.Print(output, "work", 73), ContractException);
}
#endif

}  // namespace
}  // namespace asc
