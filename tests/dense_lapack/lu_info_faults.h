#ifndef ASC_TESTS_DENSE_LAPACK_LU_INFO_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_INFO_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_lapack_test {
// Withheld results are defensive checks, never independent numerical evidence.
enum class LuInfoFault : std::uint8_t { kNone, kWithhold, kShortZero };
struct LuInfoObservation {
  std::size_t calls = 0;
  std::int64_t incoming = 0;
  std::int64_t actual = 0;
  std::int64_t outgoing = 0;
};
void SetLuInfoFault(LuInfoFault fault);
LuInfoObservation ObserveLuInfo();
}  // namespace asc_lapack_test
#endif  // ASC_TESTS_DENSE_LAPACK_LU_INFO_FAULTS_H_
