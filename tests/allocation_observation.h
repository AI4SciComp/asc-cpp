#ifndef ASC_TESTS_ALLOCATION_OBSERVATION_H_
#define ASC_TESTS_ALLOCATION_OBSERVATION_H_

#include <cstddef>

namespace asc_test {

// A replacement global operator new in a Windows executable does not
// interpose allocations made inside a DLL. Resource allocations made by
// asc_core therefore remain visible to the resource's own counter but not to
// the executable-local process probe. Static Windows and non-Windows builds
// observe those allocations in the process probe as well.
[[nodiscard]] constexpr std::size_t ProcessVisibleResourceAllocationCount(
    std::size_t resource_allocations) noexcept {
#if defined(_WIN32) && !defined(ASC_CORE_STATIC_DEFINE)
  static_cast<void>(resource_allocations);
  return 0;
#else
  return resource_allocations;
#endif
}

// MSVC Debug iterator instrumentation allocates container proxy state when the
// public Status and Result scaffolding constructs standard-library containers.
// Release and non-MSVC builds enforce exact process-allocation counts.
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
