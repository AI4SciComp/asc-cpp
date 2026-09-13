#ifndef ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_REFINEMENT_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_REFINEMENT_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_packed_cholesky_refinement_test {
enum class Fault : std::uint8_t {
  kNone,
  kNoWrite,
  kNegative,
  kPositive,
  kLowZero,
  kLowOnes,
  kNoForward,
  kNoBackward,
  kNegativeForward,
  kNegativeBackward,
  kNanForward,
  kNanBackward,
  kInfiniteForward,
  kInfiniteBackward
};
struct Arguments {
  char triangle;
  std::int64_t order;
  std::int64_t right_hand_sides;
  std::int64_t ldb;
  std::int64_t ldx;
  std::size_t character_length;
  const void* original;
  const void* factor;
  const void* rhs;
  const void* solution;
  const void* forward;
  const void* backward;
  const void* scalar_work;
  const void* auxiliary_work;
  std::int64_t incoming_info;
};
void Arm(std::size_t routine, Fault value);
void Disarm();
[[nodiscard]] std::size_t Calls(std::size_t routine);
[[nodiscard]] Arguments LastArguments(std::size_t routine);
}  // namespace asc_packed_cholesky_refinement_test

#endif  // ASC_TESTS_DENSE_LAPACK_PACKED_CHOLESKY_REFINEMENT_FAULTS_H_
