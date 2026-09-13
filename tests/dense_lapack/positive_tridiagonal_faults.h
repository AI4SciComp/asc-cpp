#ifndef ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_pt_fault {
enum class Mode : std::uint8_t {
  kPass,
  kNegative,
  kExcess,
  kUnwritten,
  kPartialWidth,
  kWriteThenNegative
};
void Reset(Mode mode);
std::size_t Calls();
bool LengthsValid();
}  // namespace asc_pt_fault
#endif  // ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_FAULTS_H_
