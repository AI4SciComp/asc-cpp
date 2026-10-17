#ifndef ASC_TESTS_DENSE_LAPACK_COMPLETE_PIVOTING_FAULTS_H_
#define ASC_TESTS_DENSE_LAPACK_COMPLETE_PIVOTING_FAULTS_H_

#include <cstddef>
#include <cstdint>

namespace asc_complete_pivoting_fault {
enum class Mode : std::uint8_t {
  kPass,
  kOmitInfo,
  kNegativeInfo,
  kMaximumInfo,
  kMissingRow,
  kInvalidColumn,
  kNonfiniteValues,
  kOmitValues,
  kWrongValues,
  kOmitScale,
  kNegativeScale,
  kLargeScale,
  kZeroScale,
  kChangedFactor,
  kChangedRow,
  kChangedColumn
};
void Reset(Mode mode);
std::size_t Calls();
bool BaselinePassed();
}  // namespace asc_complete_pivoting_fault
#endif  // ASC_TESTS_DENSE_LAPACK_COMPLETE_PIVOTING_FAULTS_H_
