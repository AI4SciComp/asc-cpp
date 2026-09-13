#ifndef ASC_TESTS_DENSE_LAPACK_BAND_CONDITION_FAULT_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_CONDITION_FAULT_SUPPORT_H_

#include <cstddef>
#include <cstdint>

namespace asc_band_condition_test {
void ResetFault(std::int64_t info, double result);
std::size_t FaultCalls();
bool FaultArgumentsValid();
}  // namespace asc_band_condition_test

#endif  // ASC_TESTS_DENSE_LAPACK_BAND_CONDITION_FAULT_SUPPORT_H_
