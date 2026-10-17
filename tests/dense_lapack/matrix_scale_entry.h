#ifndef ASC_TESTS_DENSE_LAPACK_MATRIX_SCALE_ENTRY_H_
#define ASC_TESTS_DENSE_LAPACK_MATRIX_SCALE_ENTRY_H_
#include <cstddef>
#include <cstdint>
namespace asc_scale_entry {
enum class Mode : std::uint8_t {
  kPass,
  kMissingInfo,
  kPositiveInfo,
  kNegativeInfo,
  kPartialInfo
};
struct Entry {
  char type;
  std::int64_t lower;
  std::int64_t upper;
  std::int64_t rows;
  std::int64_t columns;
  std::int64_t leading;
};
void Reset(Mode mode = Mode::kPass);
std::size_t Calls();
Entry Last();
void OnEntry(void (*callback)());
}  // namespace asc_scale_entry
#endif  // ASC_TESTS_DENSE_LAPACK_MATRIX_SCALE_ENTRY_H_
