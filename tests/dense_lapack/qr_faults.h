#ifndef ASC_TESTS_DENSE_LAPACK_QR_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_QR_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_qr_test {
enum class Routine : std::uint8_t { kGeqrf, kGeqr2, kGenerate, kApply };
enum class Fault : std::uint8_t {
  kNone,
  kNegative,
  kMinimumInteger,
  kPositive,
  kQueryZero,
  kQueryNan,
  kQueryImaginary
};
void SetFault(Routine routine, Fault fault);
std::size_t ForeignCalls();
std::size_t QueryCalls();
}  // namespace asc_qr_test

#endif  // ASC_TESTS_DENSE_LAPACK_QR_FAULTS_H_
