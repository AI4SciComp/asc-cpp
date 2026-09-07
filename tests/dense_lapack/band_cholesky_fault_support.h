#ifndef ASC_TESTS_DENSE_LAPACK_BAND_CHOLESKY_FAULT_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_CHOLESKY_FAULT_SUPPORT_H_

#include <cstddef>
#include <cstdint>

namespace asc_band_test {
void ResetFault(std::int64_t info, bool require_zero_imaginary = false);
std::size_t FaultCalls();
bool FaultArgumentsValid();
}  // namespace asc_band_test

#endif  // ASC_TESTS_DENSE_LAPACK_BAND_CHOLESKY_FAULT_SUPPORT_H_
