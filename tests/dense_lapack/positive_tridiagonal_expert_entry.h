#ifndef ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_EXPERT_ENTRY_H_
#define ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_EXPERT_ENTRY_H_
#include <cstddef>
#include <cstdint>
namespace asc::internal_ptsvx_test {
enum class Fault : std::uint8_t {
  kNone,
  kWrite,
  kNoInfo,
  kPartialInfo,
  kNegativeInfo,
  kPositiveInfo,
  kBeyondInfo,
  kPivotCondition,
  kConditionWarning,
  kNoCondition,
  kNegativeCondition,
  kNoFerr,
  kNegativeFerr,
  kNoBerr,
  kNegativeBerr,
  kNanCondition,
  kInfiniteCondition,
  kNanSolution,
  kInfiniteSolution,
  kNanFerr,
  kInfiniteBerr,
  kNanDiagonal
};
void SetFault(Fault fault);
void SetEntryHook(void (*hook)());
std::size_t EntryCount();
}  // namespace asc::internal_ptsvx_test
#endif  // ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_EXPERT_ENTRY_H_
