#ifndef ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_EQUILIBRATION_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_EQUILIBRATION_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_packed_cholesky_equilibration_test {
enum class Fault : std::uint8_t {
  kNone,
  kNoWrite,
  kNegative,
  kImpossible,
  kPositive,
  kLowZero,
  kLowOnes,
  kBadScale,
  kBadCondition,
  kBadMaximum
};
struct Arguments {
  char triangle = '?';
  std::int64_t order = -1;
  std::size_t length = 0;
  const void* matrix = nullptr;
  void* scales = nullptr;
  void* condition = nullptr;
  void* maximum = nullptr;
};
[[nodiscard]] Arguments LastArguments(std::size_t routine);
void Arm(std::size_t routine, Fault value);
void Disarm();
[[nodiscard]] std::size_t Calls(std::size_t routine);
}  // namespace asc_packed_cholesky_equilibration_test
#endif  // ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_EQUILIBRATION_FAULTS_H_
