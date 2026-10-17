#ifndef ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_INVERSE_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_INVERSE_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_packed_cholesky_inverse_test {
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
  char triangle;
  std::int64_t order;
  std::size_t character_length;
};
[[nodiscard]] Arguments LastArguments(std::size_t routine);
void Arm(std::size_t routine, Fault value);
void Disarm();
[[nodiscard]] std::size_t Calls(std::size_t routine);
}  // namespace asc_packed_cholesky_inverse_test
#endif  // ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_INVERSE_FAULTS_H_
