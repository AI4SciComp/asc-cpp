#ifndef ASC_TESTS_DENSE_LAPACK_LU_EQUILIBRATION_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_EQUILIBRATION_FAULTS_H_

#include <cstdint>

namespace asc_lapack_test {
enum class EquilibrationFault : std::uint8_t { kNone, kNegative, kExcess };
void SetEquilibrationFault(EquilibrationFault fault);
}  // namespace asc_lapack_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_EQUILIBRATION_FAULTS_H_
