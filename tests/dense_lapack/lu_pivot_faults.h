#ifndef ASC_TESTS_DENSE_LAPACK_LU_PIVOT_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_PIVOT_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_lapack_test {
enum class LuPivotFault : std::uint8_t {
  kNone,
  kOmitAll,
  kOmitLast,
  kShortAll,
  kShortLast
};
struct LuPivotObservation {
  std::size_t calls = 0;
  std::size_t entries = 0;
  std::size_t valid_actual = 0;
  std::size_t valid_output = 0;
  std::int64_t actual_info = 0;
  std::int64_t incoming_last = 0;
  std::int64_t actual_last = 0;
  std::int64_t outgoing_last = 0;
};
void SetLuPivotFault(LuPivotFault fault);
LuPivotObservation ObserveLuPivot();
}  // namespace asc_lapack_test
#endif  // ASC_TESTS_DENSE_LAPACK_LU_PIVOT_FAULTS_H_
