#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_DRIVER_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_DRIVER_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_rook_driver_fault_test {
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
  kNanWork
};
void SetFault(Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
}  // namespace asc_rook_driver_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_DRIVER_FAULTS_H_
