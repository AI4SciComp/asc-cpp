#ifndef ASC_TESTS_DENSE_LAPACK_BAND_EXPERT_DRIVER_FAULT_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_EXPERT_DRIVER_FAULT_SUPPORT_H_

#include <cstddef>
#include <cstdint>

namespace asc_band_driver_test {
struct FaultConfig {
  std::int64_t info = 0;
  double reciprocal_condition = 0.5;
  double forward_error = 0.25;
  double backward_error = 0.125;
  char equilibration = 'N';
  bool packed_factor_output = false;
  bool packed_solution_output = false;
  bool check_raw_factor = true;
};
void ResetFault(FaultConfig config = {});
std::size_t FaultCalls();
bool FaultArgumentsValid();
}  // namespace asc_band_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_BAND_EXPERT_DRIVER_FAULT_SUPPORT_H_
