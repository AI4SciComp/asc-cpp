#ifndef ASC_TESTS_DENSE_LAPACK_TRIANGULAR_CONVERSION_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_TRIANGULAR_CONVERSION_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_triangular_conversion_fault {
enum class Mode : std::uint8_t {
  kPass,
  kNegative,
  kRange,
  kExcess,
  kMaximum,
  kMinimum,
  kUnwrittenInfo,
  kShortWidthInfo,
  kUnwrittenOutput,
  kNanOutput,
  kInfiniteOutput,
  kZeroOutput
};
void Reset(Mode mode);
std::size_t Calls();
std::size_t UnexpectedNativeResults();
}  // namespace asc_triangular_conversion_fault
#endif  // ASC_TESTS_DENSE_LAPACK_TRIANGULAR_CONVERSION_FAULTS_H_
