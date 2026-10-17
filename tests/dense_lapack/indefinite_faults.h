#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_FAULTS_H_

#include <cstdint>

namespace asc_indefinite_test {
enum class Fault : std::uint8_t {
  kNone,
  kNegativeInfo,
  kExcessInfo,
  kPivotMinimum,
  kBrokenPair,
  kWorkNan
};
enum class ForeignRoutine : std::uint8_t { kTrf, kTf2, kTrs };
void SetFault(ForeignRoutine routine, Fault fault);
}  // namespace asc_indefinite_test

#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_FAULTS_H_
