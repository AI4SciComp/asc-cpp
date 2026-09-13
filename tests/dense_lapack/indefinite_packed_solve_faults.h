#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_SOLVE_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_SOLVE_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_packed_solve_fault_test {
enum class Fault : std::uint8_t {
  kPass,
  kOmitInfo,
  kWrite16BitInfo,
  kWrite32BitInfo,
  kNegativeInfo,
  kPositiveInfo,
  kMaximumInfo
};
void SetFault(Fault fault);
std::size_t Calls();
bool SeedWasFullWidth();
std::int64_t LastNativeInfo();
std::int64_t LastPublishedInfo();
}  // namespace asc_packed_solve_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_SOLVE_FAULTS_H_
