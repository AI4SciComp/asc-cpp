#ifndef ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_DRIVER_ENTRY_H_
#define ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_DRIVER_ENTRY_H_
#include <cstddef>
#include <cstdint>
namespace asc::internal_ptsv_test {
enum class Fault : std::uint8_t {
  kNone,
  kWrite,
  kNoInfo,
  kPartialInfo,
  kNegativeInfo,
  kPositiveInfo,
  kBeyondInfo,
  kNanSolution,
  kInfiniteSolution,
  kNanDiagonal,
  kInfiniteDiagonal
};
void SetFault(Fault fault);
void SetEntryHook(void (*hook)());
std::size_t EntryCount();
}  // namespace asc::internal_ptsv_test
#endif  // ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_DRIVER_ENTRY_H_
