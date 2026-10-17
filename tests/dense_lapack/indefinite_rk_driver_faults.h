#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_DRIVER_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_DRIVER_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_rk_driver_fault_test {
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
  kUnpairedPivot,
  kOmitWork,
  kNanWork,
  kOmitE,
  kBadE,
  kInconsistentPositiveInfo,
  kIncorrectWork
};
void SetFault(Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
}  // namespace asc_rk_driver_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_DRIVER_FAULTS_H_
