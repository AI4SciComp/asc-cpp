#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_aasen_fault_test {
enum class Fault : std::uint8_t {
  kPass,
  kOmitInfo,
  kWrite32BitInfo,
  kNegativeInfo,
  kPositiveInfo,
  kOmitPivots,
  kWrite32BitPivots,
  kZeroPivot,
  kOutOfBoundsPivot,
  kWrongFirstPivot,
  kBackwardPivot,
  kNegativePivot,
  kOmitWork,
  kNanWork,
  kIncorrectWork,
  kRealOnlyWork
};
void SetFault(Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
}  // namespace asc_aasen_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_FAULTS_H_
