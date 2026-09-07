#ifndef ASC_TESTS_DENSE_LAPACK_LU_DRIVER_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_DRIVER_FAULTS_H_

#include <cstdint>

namespace asc_lapack_test {
enum class DriverFault : std::uint8_t {
  kNone,
  kNegative,
  kExcessiveInfo,
  kBadPivot,
  kInvalidEqued,
  kChangedEqued,
  kNanRcond,
  kNanFerr,
  kNegativeBerr,
  kInfiniteGrowth
};
void SetDriverFault(DriverFault fault);
}  // namespace asc_lapack_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_DRIVER_FAULTS_H_
