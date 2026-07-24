// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/timer.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_TIMER_H_
#define ASC_TIMER_H_

/// @file timer.h
/// @brief High-resolution performance timing utilities
///
/// This module provides a Timer class for accurate performance measurement
/// using C++11 chrono steady_clock. Features include:
/// - High-resolution start/stop timing
/// - Multiple time-delta recording with lossless summary statistics
/// - Average, total, and last time queries
/// - Time compression for accumulated measurements
/// - Configurable output units (seconds or milliseconds)
///
/// @par Example - Basic timing:
/// @code
/// asc::Timer timer;
/// timer.Start();
/// // ... code to measure ...
/// real_t elapsed = timer.Stop();
/// std::cout << "Elapsed: " << elapsed << " seconds\n";
/// @endcode
///
/// @par Example - Multiple measurements:
/// @code
/// asc::Timer timer;
/// for (int i = 0; i < 100; ++i) {
///   timer.Start();
///   // ... operation ...
///   timer.Stop();
/// }
/// std::cout << "Average: " << timer.AverageTime() << " seconds\n";
/// std::cout << "Total: " << timer.TotalTime() << " seconds\n";
/// @endcode
///
/// @par Example - Pretty printing with units:
/// @code
/// timer.Print(std::cout, "Operation completed", asc::kMilliSecond);
/// // Output: "Operation completed: 123.45 ms"
/// @endcode

#include <chrono>
#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

#include "asc/core/config.h"

namespace asc {

/// @brief Time unit for timer output
///
/// Specifies the unit for time reporting in Print() and TotalTime().
/// - kSecond: Report time in seconds
/// - kMilliSecond: Report time in milliseconds
enum TimeUnit { kSecond = 0, kMilliSecond = 1 };

/// @brief High-resolution timer for performance measurement
///
/// Timer uses std::chrono::steady_clock for monotonic time measurement,
/// ensuring accurate performance profiling even across system time changes.
/// It records time deltas between Start/Stop pairs and provides statistics
/// such as average, total, and last measurement.
///
/// @par Thread Safety:
/// Timer is NOT thread-safe. Use separate Timer instances per thread.
///
/// @par Example - Benchmarking a function:
/// @code
/// void benchmark() {
///   asc::Timer timer;
///   const int N = 1000;
///   for (int i = 0; i < N; ++i) {
///     timer.Start();
///     expensive_operation();
///     timer.Stop();
///   }
///   timer.Print(std::cout, "Operation stats");
///   std::cout << "Last iteration: " << timer.LastTime() << " s\n";
/// }
/// @endcode
class Timer {
 private:
  using Duration = std::chrono::duration<real_t>;
  using Clock = std::chrono::steady_clock;
  using TimePoint = std::chrono::steady_clock::time_point;

 public:
  /// @brief Construct an idle timer with no completed measurements.
  ///
  /// Creates a new Timer ready for Start/Stop measurements.
  /// The internal time delta buffer is initialized but empty.
  Timer() = default;

  /// @brief Start timing measurement
  ///
  /// Records the current time point. Call Stop() to compute elapsed time.
  /// Repeating Start() while running restarts the active measurement.
  ///
  /// @par Example:
  /// @code
  /// timer.Start();
  /// compute();
  /// timer.Stop();
  /// @endcode
  void Start() noexcept {
    start_time_ = Clock::now();
    is_running_ = true;
  }

  /// @brief Stop timing and record elapsed time
  ///
  /// Computes the time elapsed since the last Start() call and stores
  /// it in the internal buffer. Returns the elapsed time in seconds.
  ///
  /// @return Elapsed time in seconds since last Start()
  ///
  /// @par Example:
  /// @code
  /// timer.Start();
  /// do_work();
  /// real_t t = timer.Stop();  // t = elapsed time in seconds
  /// @endcode
  real_t Stop();

  /// @brief Compute average of all recorded time deltas
  ///
  /// Returns the mean of all Start/Stop measurements recorded so far, or zero
  /// when no measurement has completed.
  ///
  /// @return Average time in seconds across all measurements
  ///
  /// @par Example:
  /// @code
  /// for (int i = 0; i < 100; ++i) {
  ///   timer.Start(); work(); timer.Stop();
  /// }
  /// real_t avg = timer.AverageTime();  // Mean of 100 measurements
  /// @endcode
  real_t AverageTime() const noexcept {
    if (measurement_count_ == 0) {
      return 0;
    }
    return total_time_ / static_cast<real_t>(measurement_count_);
  }

  /// @brief Compute total of all recorded time deltas
  ///
  /// Sums all Start/Stop measurements and converts to the specified unit.
  ///
  /// @param unit Time unit for output (kSecond or kMilliSecond)
  /// @return Total accumulated time in the specified unit
  ///
  /// @par Example:
  /// @code
  /// real_t total_sec = timer.TotalTime(asc::kSecond);
  /// real_t total_ms = timer.TotalTime(asc::kMilliSecond);
  /// @endcode
  real_t TotalTime(int unit = kSecond) const;

  /// @brief Get the most recent time delta
  ///
  /// Returns the elapsed time from the last Stop() call.
  ///
  /// @return Last recorded time delta in seconds
  ///
  /// @par Example:
  /// @code
  /// timer.Start(); work(); timer.Stop();
  /// real_t last = timer.LastTime();  // Time of most recent measurement
  /// @endcode
  real_t LastTime() const noexcept { return last_time_; }

  /// @brief Copy all time deltas to an external array
  ///
  /// Copies the internal time delta buffer to the provided array.
  /// The caller must ensure the array has capacity for GetMeasurementCount()
  /// deltas. An empty timer performs no write and accepts a null pointer.
  ///
  /// @param result Pointer to an array receiving every recorded prefix sum.
  ///
  /// @par Example:
  /// @code
  /// std::vector<real_t> deltas(timer.GetMeasurementCount());
  /// timer.AccumulateTime(deltas.data());
  /// // deltas now contains cumulative recorded times.
  /// @endcode
  void AccumulateTime(real_t* result) const;

  /// @brief Compress all time deltas into a single sum
  ///
  /// Preserve every observable statistic and recorded prefix. Utilities M1
  /// keeps this compatibility operation as a semantic no-op.
  ///
  /// @par Example:
  /// @code
  /// // After many measurements:
  /// timer.Compress();
  /// @endcode
  void Compress();

  /// @brief Reset the timer to its initial idle, empty state.
  void Reset() noexcept;

  /// @brief Return whether a measurement is currently active.
  bool IsRunning() const noexcept { return is_running_; }

  /// @brief Return the number of completed measurements.
  std::size_t GetMeasurementCount() const noexcept {
    return measurement_count_;
  }

  /// @brief Print formatted timing message
  ///
  /// Outputs a message with the last completed interval in the specified unit.
  /// Format: "msg: X.XX [unit]"
  ///
  /// @param os Output stream
  /// @param msg Message prefix
  /// @param unit Time unit (kSecond or kMilliSecond)
  ///
  /// @par Example:
  /// @code
  /// timer.Print(std::cout, "Computation time", asc::kMilliSecond);
  /// // Output: "Computation time: 123.45 ms"
  /// @endcode
  void Print(std::ostream& os, std::string msg, int unit = kSecond);

 private:
  TimePoint start_time_{};
  std::vector<real_t> time_deltas_;
  real_t total_time_ = 0;
  real_t last_time_ = 0;
  std::size_t measurement_count_ = 0;
  bool is_running_ = false;
};

}  // namespace asc

#endif  // ASC_TIMER_H_
