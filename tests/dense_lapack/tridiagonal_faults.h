#ifndef ASC_TESTS_DENSE_LAPACK_TRIDIAGONAL_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_TRIDIAGONAL_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_tridiagonal_fault {
enum class Operation : std::uint8_t {
  kFactor,
  kSolve,
  kDriver,
  kCondition,
  kRefinement,
  kExpert
};
enum class Mode : std::uint8_t {
  kNone,
  kNegativeInfo,
  kExcessInfo,
  kInvalidPivot,
  kNegativeDiagnostic,
  kNonfiniteDiagnostic,
  kWarningNegativeError,
  kUnwrittenPivot,
  kPartialPivotWidth,
  kUnwrittenInfo
};
void Reset(Operation operation, Mode mode = Mode::kNone);
std::size_t Calls();
std::size_t FocusCalls();
bool IntegerSegmentsValid();
bool CharacterLengthsValid();
bool ZeroRhsObserved();
}  // namespace asc_tridiagonal_fault

#endif  // ASC_TESTS_DENSE_LAPACK_TRIDIAGONAL_FAULTS_H_
