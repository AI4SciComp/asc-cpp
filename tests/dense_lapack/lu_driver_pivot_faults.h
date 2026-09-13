#ifndef ASC_TESTS_DENSE_LAPACK_LU_DRIVER_PIVOT_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_DRIVER_PIVOT_FAULTS_H_
#include <array>
#include <cstddef>
#include <cstdint>
namespace asc_lapack_test {
enum class LuDriverPivotFault : std::uint8_t {
  kNone,
  kOmitAll,
  kOmitLast,
  kShortAll,
  kShortLast
};
struct LuDriverPivotObservation {
  std::size_t calls = 0;
  std::size_t entries = 0;
  std::size_t valid_actual = 0;
  std::int64_t actual_info = 0;
  bool redirected_output = false;
  bool input_preserved = true;
  std::array<std::int64_t, 8> actual{};
};
void SetLuDriverPivotFault(LuDriverPivotFault fault);
LuDriverPivotObservation ObserveLuDriverPivot();
}  // namespace asc_lapack_test
#endif  // ASC_TESTS_DENSE_LAPACK_LU_DRIVER_PIVOT_FAULTS_H_
