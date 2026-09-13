#ifndef ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_DRIVER_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_DRIVER_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_packed_cholesky_driver_test {
enum class Fault : std::uint8_t {
  kNone,
  kNoWrite,
  kNegative,
  kImpossible,
  kPositive,
  kLowZero,
  kLowOnes
};
struct Arguments {
  char triangle = '?';
  std::int64_t order = -1;
  std::int64_t rhs = -1;
  std::int64_t leading = -1;
  std::size_t length = 0;
};
[[nodiscard]] Arguments LastArguments(std::size_t routine);
void Arm(std::size_t routine, Fault value);
void Disarm();
[[nodiscard]] std::size_t Calls(std::size_t routine);
}  // namespace asc_packed_cholesky_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_DRIVER_FAULTS_H_
