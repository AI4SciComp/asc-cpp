#ifndef ASC_TESTS_DENSE_LAPACK_POSITIVE_ROTATION_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_POSITIVE_ROTATION_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_positive_rotation_fault {
enum class Mode : std::uint8_t {
  kPass,
  kOmitCosine,
  kOmitSine,
  kOmitRotated,
  kNegativeCosine,
  kLargeCosine,
  kNanCosine,
  kInfiniteCosine,
  kLargeSine,
  kNanSine,
  kInfiniteSine,
  kNanRotated,
  kNegativeRotated,
  kInfiniteRotated,
  kNegativeInfiniteRotated,
  kChangeF,
  kChangeG,
  kWrongFinite,
  kZeroTuple
};
void Reset(Mode mode);
std::size_t Calls();
std::size_t UnexpectedNativeResults();
}  // namespace asc_positive_rotation_fault

#endif  // ASC_TESTS_DENSE_LAPACK_POSITIVE_ROTATION_FAULTS_H_
