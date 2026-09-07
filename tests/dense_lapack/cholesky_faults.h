#ifndef ASC_TESTS_DENSE_LAPACK_CHOLESKY_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_CHOLESKY_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_cholesky_test {
enum class Routine : std::uint8_t {
  kPotrf,
  kPotrf2,
  kPotf2,
  kPotri,
  kPotrs,
  kPosv
};
enum class Fault : std::uint8_t { kNone, kNegative, kMinimum, kExcess };
void SetFault(Routine routine, Fault fault);
std::size_t Calls();
}  // namespace asc_cholesky_test

#endif  // ASC_TESTS_DENSE_LAPACK_CHOLESKY_FAULTS_H_
