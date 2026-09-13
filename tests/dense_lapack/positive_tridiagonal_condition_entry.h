#ifndef ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_CONDITION_ENTRY_H_
#define ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_CONDITION_ENTRY_H_
#include <cstddef>
#include <cstdint>
namespace asc_ptcon_entry {
enum class Fault : std::uint8_t {
  kNone,
  kNoInfo,
  kPartialInfo,
  kNegativeInfo,
  kPositiveInfo,
  kNoOutput,
  kNegativeOutput,
  kNanOutput,
  kInfiniteOutput
};
void Reset();
void SetFault(Fault fault);
std::size_t Calls();
void OnEntry(void (*callback)(void*), void* argument);
}  // namespace asc_ptcon_entry
#endif  // ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_CONDITION_ENTRY_H_
