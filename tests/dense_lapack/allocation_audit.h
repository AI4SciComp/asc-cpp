#ifndef ASC_TESTS_DENSE_LAPACK_ALLOCATION_AUDIT_H_
#define ASC_TESTS_DENSE_LAPACK_ALLOCATION_AUDIT_H_
#include <cstddef>
namespace asc_lapack_test {
void BeginAllocationAudit();
std::size_t EndAllocationAudit();
}  // namespace asc_lapack_test
#endif  // ASC_TESTS_DENSE_LAPACK_ALLOCATION_AUDIT_H_
