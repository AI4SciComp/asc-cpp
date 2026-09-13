#ifndef ASC_TESTS_DENSE_LAPACK_LU_BAND_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_BAND_FAULTS_H_

#include <cstdint>

namespace asc_lu_band_faults {
enum class Fault : std::uint8_t {
  kPass,
  kNegative,
  kMinimum,
  kLargePositive,
  kZeroPivot,
  kNegativePivot,
  kOutOfBandPivot,
  kLatePivot,
  kPartialWidth,
  kUnwrittenInfo
};
void Select(Fault fault);
int Calls();
}  // namespace asc_lu_band_faults

#endif  // ASC_TESTS_DENSE_LAPACK_LU_BAND_FAULTS_H_
