#ifndef ASC_TESTS_ALLOCATION_OBSERVATION_H_
#define ASC_TESTS_ALLOCATION_OBSERVATION_H_

#include <cstddef>

namespace asc_test {

// MSVC Debug iterator instrumentation allocates container proxy state when the
// public Status and Result scaffolding constructs standard-library containers.
// The Windows Release matrix enforces exact process-allocation counts.
#if defined(_MSC_VER) && defined(_DEBUG)
inline constexpr bool kHasExactProcessAllocationObservation = false;
#else
inline constexpr bool kHasExactProcessAllocationObservation = true;
#endif

[[nodiscard]] constexpr bool ProcessAllocationCountMatches(
    std::size_t observed, std::size_t expected) noexcept {
  if constexpr (kHasExactProcessAllocationObservation) {
    return observed == expected;
  } else {
    // Preserve the operation's required allocations while allowing the
    // documented instrumentation overhead. The required Windows Release job
    // enforces the exact count.
    return observed >= expected;
  }
}

}  // namespace asc_test

#endif  // ASC_TESTS_ALLOCATION_OBSERVATION_H_
