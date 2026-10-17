#include "matrix_set_entry.h"

#include <complex>
#include <cstddef>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
namespace asc_set_entry {
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
// Existing ELF interception spellings are private to this test translation
// unit. NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_slaset_base) __real_slaset_;
extern "C" void __wrap_slaset_(const char* part, const lapack_int* m,
                               const lapack_int* n, const float* alpha,
                               const float* beta, float* output,
                               const lapack_int* leading, std::size_t length) {
  Enter();
  __real_slaset_(part, m, n, alpha, beta, output, leading, length);
}
static_assert(
    std::is_same_v<decltype(LAPACK_slaset_base), decltype(__wrap_slaset_)>);
extern "C" decltype(LAPACK_dlaset_base) __real_dlaset_;
extern "C" void __wrap_dlaset_(const char* part, const lapack_int* m,
                               const lapack_int* n, const double* alpha,
                               const double* beta, double* output,
                               const lapack_int* leading, std::size_t length) {
  Enter();
  __real_dlaset_(part, m, n, alpha, beta, output, leading, length);
}
static_assert(
    std::is_same_v<decltype(LAPACK_dlaset_base), decltype(__wrap_dlaset_)>);
extern "C" decltype(LAPACK_claset_base) __real_claset_;
extern "C" void __wrap_claset_(const char* part, const lapack_int* m,
                               const lapack_int* n,
                               const std::complex<float>* alpha,
                               const std::complex<float>* beta,
                               std::complex<float>* output,
                               const lapack_int* leading, std::size_t length) {
  Enter();
  __real_claset_(part, m, n, alpha, beta, output, leading, length);
}
static_assert(
    std::is_same_v<decltype(LAPACK_claset_base), decltype(__wrap_claset_)>);
extern "C" decltype(LAPACK_zlaset_base) __real_zlaset_;
extern "C" void __wrap_zlaset_(const char* part, const lapack_int* m,
                               const lapack_int* n,
                               const std::complex<double>* alpha,
                               const std::complex<double>* beta,
                               std::complex<double>* output,
                               const lapack_int* leading, std::size_t length) {
  Enter();
  __real_zlaset_(part, m, n, alpha, beta, output, leading, length);
}
static_assert(
    std::is_same_v<decltype(LAPACK_zlaset_base), decltype(__wrap_zlaset_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_set_entry
