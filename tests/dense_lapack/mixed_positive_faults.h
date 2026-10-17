#ifndef ASC_TESTS_DENSE_LAPACK_MIXED_POSITIVE_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_MIXED_POSITIVE_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_mixed_positive_fault {
enum class Mode : std::uint8_t {
  kPass,
  kNegativeInfo,
  kExcessInfo,
  kUnwrittenInfo,
  kPartialInfo,
  kInvalidIter,
  kUnwrittenIter,
  kPartialIter,
  kUnwrittenX,
  kImplementationFallback,
  kWriteThenNegative
};
void Reset(Mode mode);
std::size_t Calls();
// Test-local boundary callback: invoked at the wrapped foreign entry before
// forwarding to the real provider. No signal handler or provider replacement.
void OnEntry(void (*callback)());
}  // namespace asc_mixed_positive_fault
#endif  // ASC_TESTS_DENSE_LAPACK_MIXED_POSITIVE_FAULTS_H_
