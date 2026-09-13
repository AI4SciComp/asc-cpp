#ifndef ASC_TESTS_DENSE_LAPACK_LU_HELPERS_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_HELPERS_FAULTS_H_

#include <cstddef>

namespace asc_lapack_test {
struct HelperProbe {
  std::size_t swap_calls = 0;
  std::size_t scale_calls = 0;
  std::size_t equed_length = 0;
};
void ResetHelperProbe(char returned_equed = '\0');
HelperProbe ReadHelperProbe();
}  // namespace asc_lapack_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_HELPERS_FAULTS_H_
