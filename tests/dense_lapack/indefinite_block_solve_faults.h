#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_block_solve_fault_test {
enum class Fault : std::uint8_t {
  kPass,
  kOmitInfo,
  kWrite16BitInfo,
  kWrite32BitInfo,
  kNegativeInfo,
  kPositiveInfo,
  kChangedPivot,
  kChangedFactor,
  kChangedWork
};
void SetFault(Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
std::int64_t LastPublishedInfo();
bool SeedWasFullWidth();
}  // namespace asc_block_solve_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_FAULTS_H_
