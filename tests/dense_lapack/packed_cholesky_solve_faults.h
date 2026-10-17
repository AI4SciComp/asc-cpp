#ifndef ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_SOLVE_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_SOLVE_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_packed_cholesky_solve_test {
enum class Fault : std::uint8_t {
  kNone,
  kNoWrite,
  kNegative,
  kImpossible,
  kPositive,
  kLowZero,
  kLowOnes
};
void Arm(std::size_t routine, Fault value);
void Disarm();
[[nodiscard]] std::size_t Calls(std::size_t routine);
}  // namespace asc_packed_cholesky_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_SOLVE_FAULTS_H_
