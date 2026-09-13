#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_CONDITION_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_CONDITION_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_rk_condition_fault_test {
enum class Fault : std::uint8_t {
  kPass,
  kOmitInfo,
  kWrite16BitInfo,
  kWrite32BitInfo,
  kNegativeInfo,
  kPositiveInfo,
  kOutOfRangeInfo,
  kChangedPivot,
  kOmitCondition,
  kWrite16BitCondition,
  kWrite32BitCondition,
  kNegativeCondition,
  kNanCondition,
  kInfCondition,
  kZeroCondition,
  kAboveOneCondition
};
void SetFault(Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
std::int64_t LastPublishedInfo();
long double LastNativeCondition();
bool SeedWasFullWidth();
}  // namespace asc_rk_condition_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_CONDITION_FAULTS_H_
