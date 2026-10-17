#ifndef ASC_TESTS_DENSE_LAPACK_LU_BAND_EXPERT_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_BAND_EXPERT_FAULTS_H_
#include <cstdint>
namespace asc_lu_band_expert_faults {
enum class Routine : std::uint8_t {
  kEquilibrate,
  kCondition,
  kRefine,
  kSolve,
  kDriver
};
enum class Fault : std::uint8_t {
  kPass,
  kNegative,
  kMinimum,
  kLargePositive,
  kNoInfo,
  kPartialInfo,
  kNoPivots,
  kPartialPivot,
  kLatePivot,
  kInvalidEqued,
  kNegativeError,
  kNonfiniteError,
  kMixedError,
  kNegativeStats,
  kNonfiniteStats,
  kAccuracy
};
void Select(Routine routine, Fault fault);
int Calls();
bool SawInfoSentinel();
bool SawPivotSentinels();
}  // namespace asc_lu_band_expert_faults
#endif  // ASC_TESTS_DENSE_LAPACK_LU_BAND_EXPERT_FAULTS_H_
