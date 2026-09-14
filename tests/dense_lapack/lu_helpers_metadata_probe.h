#ifndef ASC_TESTS_DENSE_LAPACK_LU_HELPERS_METADATA_PROBE_H_
#define ASC_TESTS_DENSE_LAPACK_LU_HELPERS_METADATA_PROBE_H_
#include <cstddef>
namespace asc_helpers_metadata {
struct Calls {
  std::size_t swap = 0;
  std::size_t scale = 0;
};
void Reset();
Calls Read();
}  // namespace asc_helpers_metadata
#endif  // ASC_TESTS_DENSE_LAPACK_LU_HELPERS_METADATA_PROBE_H_
