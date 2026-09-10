#ifndef ASC_TESTS_DENSE_LAPACK_MATRIX_COPY_ENTRY_H_
#define ASC_TESTS_DENSE_LAPACK_MATRIX_COPY_ENTRY_H_
#include <cstddef>
namespace asc_copy_entry {
void Reset();
std::size_t Calls();
void OnEntry(void (*callback)());
}  // namespace asc_copy_entry
#endif  // ASC_TESTS_DENSE_LAPACK_MATRIX_COPY_ENTRY_H_
