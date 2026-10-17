#ifndef ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_CONDITION_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_CONDITION_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_packed_cholesky_condition_test {
enum class Fault : std::uint8_t {
  kNone,
  kNoWrite,
  kNegative,
  kPositive,
  kLowZero,
  kLowOnes,
  kNoOutput,
  kNegativeOutput,
  kNanOutput,
  kInfiniteOutput
};
struct Arguments {
  char triangle;
  std::int64_t order;
  std::size_t character_length;
  const void* factor;
  const void* condition;
  const void* scalar_work;
  const void* auxiliary_work;
  long double norm;
};
void Arm(std::size_t routine, Fault value);
void Disarm();
[[nodiscard]] std::size_t Calls(std::size_t routine);
[[nodiscard]] Arguments LastArguments(std::size_t routine);
}  // namespace asc_packed_cholesky_condition_test

#endif  // ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_CONDITION_FAULTS_H_
