#include "asc/core/types.h"
#include "asc/expression/writable.h"  // NOLINT(misc-include-cleaner): probe
#include "asc/sparse.h"  // NOLINT(misc-include-cleaner): umbrella-header probe

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#error "This Sparse compile contract must disable C++ exceptions."
#endif

int main() {
  static_assert(sizeof(asc::index_t) == 8);
  static_assert(sizeof(asc::nnz_t) == 8);
  return 0;
}
