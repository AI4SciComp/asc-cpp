#ifndef ASC_TESTS_DENSE_LAPACK_LU_BAND_EQUILIBRATION_RADIX_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_BAND_EQUILIBRATION_RADIX_FAULTS_H_
#include <cstdint>
namespace asc_gb_equb_faults {
enum class Fault : std::uint8_t {
  kPass,
  kNegative,
  kMinimum,
  kLargePositive,
  kNoInfo,
  kPartialInfo,
  kOne
};
void Select(Fault fault);
int Calls();
bool SawInfoSentinel();
}  // namespace asc_gb_equb_faults
#endif  // ASC_TESTS_DENSE_LAPACK_LU_BAND_EQUILIBRATION_RADIX_FAULTS_H_
