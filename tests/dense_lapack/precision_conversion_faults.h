#ifndef ASC_TESTS_DENSE_LAPACK_PRECISION_CONVERSION_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_PRECISION_CONVERSION_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_conversion_fault {
enum class Mode : std::uint8_t {
  kPass,
  kNegativeInfo,
  kOneInfo,
  kExcessInfo,
  kUnwrittenInfo,
  kPartialInfo,
  kUnwrittenOutput
};
void Reset(Mode mode);
std::size_t Calls();
void OnEntry(void (*callback)());
}  // namespace asc_conversion_fault
#endif  // ASC_TESTS_DENSE_LAPACK_PRECISION_CONVERSION_FAULTS_H_
