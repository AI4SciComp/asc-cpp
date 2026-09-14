#ifndef ASC_TESTS_DENSE_LAPACK_MATRIX_PERMUTATION_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_MATRIX_PERMUTATION_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_matrix_permutation_fault {
enum class Mode : std::uint8_t {
  kPass,
  kNegativeIndex,
  kChangedPermutation,
  kMaximumIndex,
  kOmitValues,
  kWrongValues
};
void Reset(Mode mode);
std::size_t Calls();
bool BaselinePassed();
}  // namespace asc_matrix_permutation_fault
#endif  // ASC_TESTS_DENSE_LAPACK_MATRIX_PERMUTATION_FAULTS_H_
