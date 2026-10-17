#ifndef ASC_TESTS_DENSE_LAPACK_LU_EXPERT_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_LU_EXPERT_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_lapack_test {
enum class ExpertFault : std::uint8_t {
  kNone,
  kNegativeInfo,
  kInvalidPivot,
  kExcessInfo,
  kQueryNan,
  kQueryInfinity,
  kQueryNegative,
  kQueryShort
};
void SetExpertFault(ExpertFault fault);
std::size_t InverseQueryCalls();
std::size_t InverseExecutionCalls();
}  // namespace asc_lapack_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_EXPERT_FAULTS_H_
