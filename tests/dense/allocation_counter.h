#ifndef ASC_TESTS_DENSE_ALLOCATION_COUNTER_H_
#define ASC_TESTS_DENSE_ALLOCATION_COUNTER_H_

#include <cstddef>

namespace asc_dense_test {

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

}  // namespace asc_dense_test

#endif  // ASC_TESTS_DENSE_ALLOCATION_COUNTER_H_
