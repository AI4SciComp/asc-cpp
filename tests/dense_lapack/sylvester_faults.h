#ifndef ASC_TESTS_DENSE_LAPACK_SYLVESTER_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_SYLVESTER_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_sylvester_test {
enum class Fault : std::uint8_t {
  kNone,
  kNegative,
  kMinimumInteger,
  kImpossiblePositive,
  kNegativeScale,
  kLargeScale,
  kNanScale,
  kInfiniteScale,
  kZeroScale,
  kNonfiniteResult,
  kPerturbed,
  kUnwrittenInfo
};
void SetFault(Fault fault);
std::size_t ForeignCalls();
bool CharacterLengthsValid();
std::int64_t ProviderIntegerMinimum();
}  // namespace asc_sylvester_test

#endif  // ASC_TESTS_DENSE_LAPACK_SYLVESTER_FAULTS_H_
