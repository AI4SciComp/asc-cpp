#ifndef ASC_TESTS_DENSE_LAPACK_SCALAR_SORT_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_SCALAR_SORT_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_scalar_sort_fault {
enum class Mode : std::uint8_t {
  kPass,
  kOmitInfo,
  kPositive,
  kNegativeOne,
  kNegativeTwo,
  kMinimum,
  kShortZero,
  kUnsorted,
  kSortedWrong,
  kNan,
  kOmitValues
};
void Reset(Mode mode);
std::size_t Calls();
std::size_t UnexpectedNativeResults();
}  // namespace asc_scalar_sort_fault
#endif  // ASC_TESTS_DENSE_LAPACK_SCALAR_SORT_FAULTS_H_
