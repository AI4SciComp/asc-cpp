#ifndef ASC_TESTS_DENSE_LAPACK_LU_REFINEMENT_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_REFINEMENT_FAULTS_H_

#include <cstdint>

namespace asc_lapack_test {
enum class RefinementFault : std::uint8_t {
  kNone,
  kNegative,
  kPositive,
  kNanForward,
  kInfiniteBackward,
  kNegativeForward,
  kNegativeBackward
};
void SetRefinementFault(RefinementFault fault);
}  // namespace asc_lapack_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_REFINEMENT_FAULTS_H_
