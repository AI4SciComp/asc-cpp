#ifndef ASC_TESTS_DENSE_LAPACK_SVD_LEAST_SQUARES_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_SVD_LEAST_SQUARES_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_svd_least_squares_test {
enum class Routine : std::uint8_t { kGelss, kGelsd };
enum class Fault : std::uint8_t {
  kNone,
  kNegative,
  kMinimumInteger,
  kPositive,
  kImpossiblePositive,
  kRankNegative,
  kRankTooLarge,
  kQueryZero,
  kQueryNan,
  kQueryInfinity,
  kQueryImaginary,
  kRealQueryZero,
  kRealQueryNan,
  kIntegerQueryZero
};
void SetFault(Routine routine, Fault fault);
std::size_t ForeignCalls();
std::size_t QueryCalls();
std::size_t RoutineCalls(Routine routine);
}  // namespace asc_svd_least_squares_test

#endif  // ASC_TESTS_DENSE_LAPACK_SVD_LEAST_SQUARES_FAULTS_H_
