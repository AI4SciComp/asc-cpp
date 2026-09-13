#include "allocation_audit.h"

#include <cstddef>

namespace {
bool g_audit_active = false;
std::size_t g_calls = 0;
void CountCall() {
  if (g_audit_active) {
    ++g_calls;
  }
}
}  // namespace

// ELF static-link audit: --wrap covers calls originating in the linked static
// ASC/LAPACK/BLAS objects. This is not an audit of shared runtime internals.
// Names mandated by GNU ld's --wrap protocol, confined to this test.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void* __real_malloc(std::size_t);
void* __real_calloc(std::size_t, std::size_t);
void* __real_realloc(void*, std::size_t);
void* __real_aligned_alloc(std::size_t, std::size_t);
int __real_posix_memalign(void**, std::size_t, std::size_t);
void __real_free(void*);
void* __wrap_malloc(std::size_t bytes) {
  CountCall();
  return __real_malloc(bytes);
}
void* __wrap_calloc(std::size_t count, std::size_t bytes) {
  CountCall();
  return __real_calloc(count, bytes);
}
void* __wrap_realloc(void* memory, std::size_t bytes) {
  CountCall();
  return __real_realloc(memory, bytes);
}
void* __wrap_aligned_alloc(std::size_t alignment, std::size_t bytes) {
  CountCall();
  return __real_aligned_alloc(alignment, bytes);
}
int __wrap_posix_memalign(void** memory, std::size_t alignment,
                          std::size_t bytes) {
  CountCall();
  return __real_posix_memalign(memory, alignment, bytes);
}
void __wrap_free(void* memory) { __real_free(memory); }
}

// NOLINTEND(bugprone-reserved-identifier)

namespace asc_lapack_test {
void BeginAllocationAudit() {
  g_calls = 0;
  g_audit_active = true;
}
std::size_t EndAllocationAudit() {
  g_audit_active = false;
  return g_calls;
}
}  // namespace asc_lapack_test
