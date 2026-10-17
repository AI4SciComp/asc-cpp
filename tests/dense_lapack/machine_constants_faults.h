#ifndef ASC_TESTS_DENSE_LAPACK_MACHINE_CONSTANTS_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_MACHINE_CONSTANTS_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_machine_constants_fault {
enum class Mode : std::uint8_t {
  kPass,
  kZero,
  kWrongSign,
  kNan,
  kInfinite,
  kChangeSelector,
  kWrongFinite
};
void Reset(Mode mode);
std::size_t Calls();
std::size_t UnexpectedNativeResults();
}  // namespace asc_machine_constants_fault

#endif  // ASC_TESTS_DENSE_LAPACK_MACHINE_CONSTANTS_FAULTS_H_
