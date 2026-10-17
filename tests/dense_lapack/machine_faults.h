#ifndef ASC_TESTS_DENSE_LAPACK_MACHINE_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_MACHINE_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_machine_fault {
enum class Mode : std::uint8_t {
  kPass,
  kSmallBit,
  kLargeBit,
  kBothBits,
  kSwap
};
void Reset(Mode mode);
std::size_t Calls();
std::size_t UnexpectedNativeResults();
}  // namespace asc_machine_fault

#endif  // ASC_TESTS_DENSE_LAPACK_MACHINE_FAULTS_H_
