#ifndef ASC_BENCHMARKS_RANDOM_STORAGE_ALLOCATION_COUNTER_H_
#define ASC_BENCHMARKS_RANDOM_STORAGE_ALLOCATION_COUNTER_H_

#include <cstddef>

namespace asc_random_storage_benchmark {

void ResetAllocationCount() noexcept;
void EnableAllocationCounting() noexcept;
void DisableAllocationCounting() noexcept;
[[nodiscard]] std::size_t AllocationCount() noexcept;

class AllocationCountScope {
 public:
  AllocationCountScope() noexcept {
    ResetAllocationCount();
    EnableAllocationCounting();
  }
  AllocationCountScope(const AllocationCountScope&) = delete;
  AllocationCountScope& operator=(const AllocationCountScope&) = delete;
  ~AllocationCountScope() { DisableAllocationCounting(); }

  [[nodiscard]] std::size_t count() const noexcept { return AllocationCount(); }
};

}  // namespace asc_random_storage_benchmark

#endif  // ASC_BENCHMARKS_RANDOM_STORAGE_ALLOCATION_COUNTER_H_
