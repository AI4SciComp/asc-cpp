#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_DRIVER_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_DRIVER_FAULTS_H_
#include <cstddef>
#include <cstdint>
namespace asc_aasen_driver_fault_test {
enum class Fault : std::uint8_t {
  kPass,
  kOmitInfo,
  kWrite32BitInfo,
  kNegativeInfo,
  kInfoBeyondOrder,
  kFalseSingular,
  kZeroPivot,
  kBackwardPivot,
  kWrongFirstPivot,
  kClobberUpperPivot,
  kConsistentSingular,
  kOmitPivot,
  kWrite32BitPivot,
  kOmitRecommendation,
  kNanRecommendation,
  kNonzeroImagRecommendation,
  kWrongRecommendation,
  kRealOnlyRecommendation,
  kSingularSentinelWitness,
  kSingularNanWitness,
  kSingularNonzeroWitness,
  kSingularRealOnlyWitness,
  kSingularWrongInfo
};
void SetFault(Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
}  // namespace asc_aasen_driver_fault_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_DRIVER_FAULTS_H_
