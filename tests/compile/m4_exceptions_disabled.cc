#include "asc/expression/writable.h"
#include "asc/sparse.h"

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#error "This Sparse compile contract must disable C++ exceptions."
#endif

int main() {
  static_assert(sizeof(asc::index_t) == 8);
  static_assert(sizeof(asc::nnz_t) == 8);
  return 0;
}
