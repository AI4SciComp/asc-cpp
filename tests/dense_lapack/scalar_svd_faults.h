#ifndef ASC_TESTS_DENSE_LAPACK_SCALAR_SVD_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_SCALAR_SVD_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_scalar_svd_fault {
enum class Mode : std::uint8_t {
  kPass,
  kOmitSmall,
  kOmitLarge,
  kOmitRightSine,
  kOmitRightCosine,
  kOmitLeftSine,
  kOmitLeftCosine,
  kNanSmall,
  kInfiniteSmall,
  kNanLarge,
  kPositiveInfiniteLarge,
  kNegativeInfiniteLarge,
  kNanRightSine,
  kLargeRightSine,
  kNanRightCosine,
  kLargeRightCosine,
  kNanLeftSine,
  kLargeLeftSine,
  kNanLeftCosine,
  kLargeLeftCosine,
  kInfiniteCoefficient,
  kAtCoefficientLimit,
  kBeyondCoefficientLimit,
  kChangeF,
  kChangeG,
  kChangeH,
  kUnsorted,
  kWrongFinite,
  kZeroTuple,
};
void Reset(Mode mode);
std::size_t Calls();
std::size_t UnexpectedNativeResults();
}  // namespace asc_scalar_svd_fault
#endif  // ASC_TESTS_DENSE_LAPACK_SCALAR_SVD_FAULTS_H_
