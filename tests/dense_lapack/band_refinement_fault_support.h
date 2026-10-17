#ifndef ASC_TESTS_DENSE_LAPACK_BAND_REFINEMENT_FAULT_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_REFINEMENT_FAULT_SUPPORT_H_

#include <cstddef>
#include <cstdint>

namespace asc_band_refinement_test {
void ResetFault(std::int64_t info, double forward_error, double backward_error,
                bool require_zero_original_imaginary);
std::size_t FaultCalls();
bool FaultArgumentsValid();
}  // namespace asc_band_refinement_test

#endif  // ASC_TESTS_DENSE_LAPACK_BAND_REFINEMENT_FAULT_SUPPORT_H_
