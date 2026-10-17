#ifndef ASC_TESTS_DENSE_LAPACK_RANK_REVEALING_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_RANK_REVEALING_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_rank_revealing_test {
enum class Routine : std::uint8_t { kGeqp3, kGelsy };
enum class Fault : std::uint8_t {
  kNone,
  kNegativeInfo,
  kMinimumInfo,
  kPositiveInfo,
  kQueryNan,
  kQueryNegative,
  kQueryImaginary,
  kQueryOverflow,
  kPivotZero,
  kPivotLarge,
  kPivotDuplicate,
  kRankNegative,
  kRankLarge
};
void SetFault(Routine routine, Fault fault);
std::size_t ForeignQueries();
std::size_t ForeignExecutions();
std::size_t NonNormalizedFlags();
}  // namespace asc_rank_revealing_test

#endif  // ASC_TESTS_DENSE_LAPACK_RANK_REVEALING_FAULTS_H_
