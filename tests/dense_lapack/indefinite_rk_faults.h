#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_rk_fault_test {
enum class Routine : std::uint8_t { kTf2, kTrf };
enum class Fault : std::uint8_t {
  kPass,
  kOmitInfo,
  kWrite16BitInfo,
  kWrite32BitInfo,
  kNegativeInfo,
  kLargeInfo,
  kZeroPivot,
  kBrokenPair,
  kWrongDirection,
  kChangedE,
  kChangedFactorSlot,
  kChangedWork
};
void SetFault(Routine routine, Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
std::int64_t LastPublishedInfo();
bool SeedWasFullWidth();
}  // namespace asc_rk_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_FAULTS_H_
