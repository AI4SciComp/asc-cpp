#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_aasen_two_stage_driver_fault_test {
enum class Fault : std::uint8_t {
  kPass,
  kOmitInfo,
  kWrite32BitInfo,
  kNegativeInfo,
  kOutOfRangeInfo,
  kFalseSingular,
  kRealSingular,
  kOmitOuter,
  kWrite32BitOuter,
  kZeroOuter,
  kNegativeOuter,
  kOutOfRangeOuter,
  kWrongPrefixOuter,
  kBackwardOuter,
  kOmitBand,
  kWrite32BitBand,
  kZeroBand,
  kNegativeBand,
  kOutOfRangeBand,
  kBackwardBand,
  kOutsideBandReach,
  kOmitNb,
  kNanNb,
  kZeroNb,
  kWrongNb,
  kRealOnlyNb,
  kNonzeroWitness,
  kNanWitness,
  kOmitWitness,
  kRealOnlyWitness,
  kOmitWork,
  kNanWork,
  kZeroWork,
  kWrongWork,
  kRealOnlyWork,
};
void SetFault(Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
bool SingularInput(Fault fault);
}  // namespace asc_aasen_two_stage_driver_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_FAULTS_H_
