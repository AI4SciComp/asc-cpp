#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_aasen_two_stage_solve_fault_test {
enum class Fault : std::uint8_t {
  kPass,
  kOmitInfo,
  kWrite32BitInfo,
  kNegativeInfo,
  kPositiveInfo,
  kInfoAtOrder,
  kInfoBeyondOrder,
  kZeroOuterPivot,
  kZeroBandPivot,
  kBackwardOuterPivot,
  kBackwardBandPivot,
  kUpperOuterPivot,
  kUpperBandPivot,
  kChangedOuterPivot,
  kChangedBandPivot
};
void SetFault(Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
}  // namespace asc_aasen_two_stage_solve_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_FAULTS_H_
