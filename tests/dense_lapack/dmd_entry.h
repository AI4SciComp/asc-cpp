#ifndef ASC_TESTS_DENSE_LAPACK_DMD_ENTRY_H_
#define ASC_TESTS_DENSE_LAPACK_DMD_ENTRY_H_

#include <cstddef>
#include <cstdint>

namespace asc_dmd_entry {
enum class Mode : std::uint8_t {
  kReal,
  kOmitInfo,
  kOmitRank,
  kSvdFailure,
  kEigenFailure,
  kNegativeInfo,
  kImpossibleInfo,
  kWarning,
  kNarrowInfo,
  kNarrowRank
};
void Reset(Mode mode = Mode::kReal);
std::size_t Calls();
void OnEntry(void (*callback)());
}  // namespace asc_dmd_entry
#endif  // ASC_TESTS_DENSE_LAPACK_DMD_ENTRY_H_
