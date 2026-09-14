#ifndef ASC_TESTS_DENSE_LAPACK_TRIANGULAR_STORAGE_CONVERSION_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_TRIANGULAR_STORAGE_CONVERSION_FAULTS_H_
#include <cstddef>
#include <cstdint>

namespace asc_triangular_storage_fault {
enum class Mode : std::uint8_t {
  kPass,
  kOmitInfo,
  kPositive,
  kNegativeOne,
  kNegativeTwo,
  kNegativeLeading,
  kMinimum,
  kShortZero,
  kOmitValues,
  kWrongValues
};
void Reset(Mode mode);
std::size_t Calls();
bool BaselinePassed();
}  // namespace asc_triangular_storage_fault
#endif  // ASC_TESTS_DENSE_LAPACK_TRIANGULAR_STORAGE_CONVERSION_FAULTS_H_
