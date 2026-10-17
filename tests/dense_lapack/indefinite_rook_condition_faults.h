#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_CONDITION_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_CONDITION_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_rook_condition_fault_test {
enum class Fault : std::uint8_t {
  kPass,
  kOmitInfo,
  kWrite32BitZero,
  kNegativeInfo,
  kPositiveInfo,
  kOmitCondition,
  kNegativeCondition,
  kNanCondition,
  kInfCondition
};
void SetFault(Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
long double LastNativeCondition();
}  // namespace asc_rook_condition_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_CONDITION_FAULTS_H_
