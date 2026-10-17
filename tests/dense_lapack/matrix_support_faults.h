#ifndef ASC_TESTS_DENSE_LAPACK_MATRIX_SUPPORT_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_MATRIX_SUPPORT_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_matrix_support_fault {
enum class Mode : std::uint8_t {
  kPass,
  kNegative,
  kTooLarge,
  kChangeM,
  kChangeN,
  kChangeLeading,
  kWrongInRange
};
void Reset(Mode mode);
std::size_t Calls();
std::size_t UnexpectedNativeResults();
}  // namespace asc_matrix_support_fault
#endif  // ASC_TESTS_DENSE_LAPACK_MATRIX_SUPPORT_FAULTS_H_
