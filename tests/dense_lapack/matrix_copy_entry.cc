#include "matrix_copy_entry.h"

#include <complex>
#include <cstddef>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
namespace asc_copy_entry {
namespace {
thread_local std::size_t g_calls = 0;
thread_local void (*g_entry)() = nullptr;
void Enter() {
  ++g_calls;
  if (g_entry != nullptr) {
    g_entry();
  }
}
}  // namespace
void Reset() {
  g_calls = 0;
  g_entry = nullptr;
}
std::size_t Calls() { return g_calls; }
void OnEntry(void (*callback)()) { g_entry = callback; }
// The existing ELF interception spelling is private to this test translation
// unit. NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_slacpy_base) __real_slacpy_;
extern "C" void __wrap_slacpy_(const char* part, const lapack_int* m,
                               const lapack_int* n, const float* input,
                               const lapack_int* leading, float* output,
                               const lapack_int* output_leading,
                               std::size_t length) {
  Enter();
  __real_slacpy_(part, m, n, input, leading, output, output_leading, length);
}
static_assert(
    std::is_same_v<decltype(LAPACK_slacpy_base), decltype(__wrap_slacpy_)>);
extern "C" decltype(LAPACK_dlacpy_base) __real_dlacpy_;
extern "C" void __wrap_dlacpy_(const char* part, const lapack_int* m,
                               const lapack_int* n, const double* input,
                               const lapack_int* leading, double* output,
                               const lapack_int* output_leading,
                               std::size_t length) {
  Enter();
  __real_dlacpy_(part, m, n, input, leading, output, output_leading, length);
}
static_assert(
    std::is_same_v<decltype(LAPACK_dlacpy_base), decltype(__wrap_dlacpy_)>);
extern "C" decltype(LAPACK_clacpy_base) __real_clacpy_;
extern "C" void __wrap_clacpy_(const char* part, const lapack_int* m,
                               const lapack_int* n,
                               const std::complex<float>* input,
                               const lapack_int* leading,
                               std::complex<float>* output,
                               const lapack_int* output_leading,
                               std::size_t length) {
  Enter();
  __real_clacpy_(part, m, n, input, leading, output, output_leading, length);
}
static_assert(
    std::is_same_v<decltype(LAPACK_clacpy_base), decltype(__wrap_clacpy_)>);
extern "C" decltype(LAPACK_zlacpy_base) __real_zlacpy_;
extern "C" void __wrap_zlacpy_(const char* part, const lapack_int* m,
                               const lapack_int* n,
                               const std::complex<double>* input,
                               const lapack_int* leading,
                               std::complex<double>* output,
                               const lapack_int* output_leading,
                               std::size_t length) {
  Enter();
  __real_zlacpy_(part, m, n, input, leading, output, output_leading, length);
}
static_assert(
    std::is_same_v<decltype(LAPACK_zlacpy_base), decltype(__wrap_zlacpy_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_copy_entry
