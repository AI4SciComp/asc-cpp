#include "asc/core.h"

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#error "This compile contract must disable C++ exceptions."
#endif

int main() {
  static_assert(sizeof(asc::index_t) == 8);
  return 0;
}
