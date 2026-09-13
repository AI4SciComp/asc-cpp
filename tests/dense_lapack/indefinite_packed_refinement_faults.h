#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_REFINEMENT_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_REFINEMENT_FAULTS_H_
#include <complex>
#include <cstddef>
#include <cstdint>
namespace asc_packed_refinement_fault_test {
enum class Fault : std::uint8_t {
  kPass,
  kOmitInfo,
  kWrite16BitInfo,
  kWrite32BitInfo,
  kNegativeInfo,
  kPositiveInfo,
  kMaximumInfo,
  kChangedPivot,
  kOmitErrors,
  kOmitFirstForward,
  kOmitLastBackward,
  kNegativeForward,
  kNegativeBackward,
  kNanForward,
  kInfBackward,
  kNegativeInfForward,
  kZeroErrors,
  kTwoErrors,
  kSubnormalErrors,
  kNegativeZeroErrors,
  kNanX,
  kInfX,
  kInvalidInfoNegativeError,
  kChangedPivotNanError
};
void SetFault(Fault fault);
std::size_t Calls();
std::int64_t LastNativeInfo();
std::int64_t LastPublishedInfo();
long double LastNativeForward(int j);
long double LastNativeBackward(int j);
long double LastPublishedForward(int j);
long double LastPublishedBackward(int j);
std::complex<long double> LastNativeX(int i, int j);
std::complex<long double> LastPublishedX(int i, int j);
bool SeedWasFullWidth();
bool ErrorSeedsWereNan();
bool NativeGuardsPass();
}  // namespace asc_packed_refinement_fault_test
#endif
