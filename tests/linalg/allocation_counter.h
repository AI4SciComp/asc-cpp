#ifndef ASC_TESTS_LINALG_ALLOCATION_COUNTER_H_
#define ASC_TESTS_LINALG_ALLOCATION_COUNTER_H_

#include <cstddef>

namespace asc::test {

void BeginAllocationCount() noexcept;
std::size_t EndAllocationCount() noexcept;
void RecordAllocation() noexcept;

}  // namespace asc::test

#endif  // ASC_TESTS_LINALG_ALLOCATION_COUNTER_H_
