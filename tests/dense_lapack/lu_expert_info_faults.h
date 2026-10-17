#ifndef ASC_TESTS_DENSE_LAPACK_LU_EXPERT_INFO_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_EXPERT_INFO_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_lapack_test {
enum class LuExpertInfoFault : std::uint8_t { kNone, kWithhold, kShortZero };
struct LuExpertInfoObservation {
  std::size_t calls = 0;
  std::size_t queries = 0;
  std::int64_t incoming = 0;
  std::int64_t actual = 0;
  std::int64_t outgoing = 0;
};
void SetLuExpertInfoFault(LuExpertInfoFault fault);
LuExpertInfoObservation ObserveLuExpertInfo();
}  // namespace asc_lapack_test
#endif  // ASC_TESTS_DENSE_LAPACK_LU_EXPERT_INFO_FAULTS_H_
