#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_CONDITION_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_CONDITION_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_packed_condition_fault_test {
enum class Fault : std::uint8_t {
  kPass,
  kOmitInfo,
  kWrite16BitInfo,
  kWrite32BitInfo,
  kNegativeInfo,
  kPositiveInfo,
  kMaximumInfo,
  kChangedPivot,
  kOmitCondition,
  kNegativeCondition,
  kNanCondition,
  kInfCondition,
  kNegativeInfCondition,
  kZeroCondition,
  kTwoCondition,
  kSubnormalCondition,
  kNegativeZeroCondition
};
void SetFault(Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
std::int64_t LastPublishedInfo();
long double LastNativeCondition();
long double LastPublishedCondition();
bool SeedWasFullWidth();
bool ConditionSeedWasNan();
}  // namespace asc_packed_condition_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_CONDITION_FAULTS_H_
