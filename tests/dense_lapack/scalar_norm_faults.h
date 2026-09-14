#ifndef ASC_TESTS_DENSE_LAPACK_SCALAR_NORM_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_SCALAR_NORM_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_scalar_norm_fault {
enum class Mode : std::uint8_t {
  kPass,
  kZero,
  kNegativeZero,
  kNegative,
  kTiny,
  kMaximum,
  kInfinity,
  kNegativeInfinity,
  kNan
};
void Reset(Mode mode);
std::size_t Calls();
std::size_t UnexpectedNativeResults();
}  // namespace asc_scalar_norm_fault
#endif  // ASC_TESTS_DENSE_LAPACK_SCALAR_NORM_FAULTS_H_
