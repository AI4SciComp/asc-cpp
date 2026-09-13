#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_EXPERT_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_EXPERT_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_indefinite_expert_test {
// Test-only ELF interposition. Synthetic results are defensive-path evidence,
// never a substitute for real provider mathematics.
enum class Route : std::uint8_t { kCon, kRfs, kSv, kVx };
enum class Fault : std::uint8_t {
  kNone,
  kNegativeInfo,
  kExcessInfo,
  kPivotMinimum,
  kWorkNan,
  kNegativeDiagnostic,
  kNanDiagnostic,
  kUnexpectedFactorInfo,
};
void SetFault(Route route, Fault fault);
std::size_t ObservedCalls();
}  // namespace asc_indefinite_expert_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_EXPERT_FAULTS_H_
