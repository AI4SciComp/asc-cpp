#ifndef ASC_TESTS_RANDOM_ALLOCATION_COUNTER_H_
#define ASC_TESTS_RANDOM_ALLOCATION_COUNTER_H_

#include <cstddef>

namespace asc::test {

void BeginRandomAllocationCount() noexcept;
std::size_t EndRandomAllocationCount() noexcept;
void RecordRandomAllocation() noexcept;

}  // namespace asc::test

#endif  // ASC_TESTS_RANDOM_ALLOCATION_COUNTER_H_
