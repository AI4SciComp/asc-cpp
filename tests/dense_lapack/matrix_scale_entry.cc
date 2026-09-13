#include "matrix_scale_entry.h"

#include <complex>
#include <cstddef>
#include <cstring>
#include <type_traits>

#include "internal_tridiagonal.h"
namespace asc_scale_entry {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local Entry g_last{};
thread_local void (*g_callback)() = nullptr;
template <typename T>
bool Enter(char type, lapack_int lower, lapack_int upper, lapack_int m,
           lapack_int n, T* matrix, lapack_int leading, lapack_int* info) {
  ++g_calls;
  g_last = {type, lower, upper, m, n, leading};
  if (g_callback != nullptr) {
    g_callback();
  }
  if (g_mode == Mode::kPass) {
    return true;
  }
  // The diagonal of column zero is selected in each nonempty tested mode.
  lapack_int diagonal = 0;
  if (type == 'Q') {
    diagonal = upper;
  } else if (type == 'Z') {
    diagonal = lower + upper;
  }
  matrix[diagonal] = T{17};
  if (g_mode == Mode::kPositiveInfo) {
    *info = 1;
  } else if (g_mode == Mode::kNegativeInfo) {
    *info = -4;
  } else if (g_mode == Mode::kPartialInfo) {
    const lapack_int zero = 0;
    // Copy only half the actual integer width; keep the initialized high bytes.
    std::memcpy(info, &zero, sizeof(lapack_int) / 2);
  }
  return false;
}
}  // namespace
void Reset(Mode mode) {
  g_mode = mode;
  g_calls = 0;
  g_last = {};
  g_callback = nullptr;
}
std::size_t Calls() { return g_calls; }
Entry Last() { return g_last; }
void OnEntry(void (*callback)()) { g_callback = callback; }
// GNU ELF interception names are confined to this test translation unit.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_slascl_base) __real_slascl_;
extern "C" void __wrap_slascl_(const char* type, const lapack_int* lower,
                               const lapack_int* upper, const float* cfrom,
                               const float* cto, const lapack_int* m,
                               const lapack_int* n, float* matrix,
                               const lapack_int* leading, lapack_int* info,
                               std::size_t length) {
  if (Enter(*type, *lower, *upper, *m, *n, matrix, *leading, info)) {
    __real_slascl_(type, lower, upper, cfrom, cto, m, n, matrix, leading, info,
                   length);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_slascl_base), decltype(__wrap_slascl_)>);
extern "C" decltype(LAPACK_dlascl_base) __real_dlascl_;
extern "C" void __wrap_dlascl_(const char* type, const lapack_int* lower,
                               const lapack_int* upper, const double* cfrom,
                               const double* cto, const lapack_int* m,
                               const lapack_int* n, double* matrix,
                               const lapack_int* leading, lapack_int* info,
                               std::size_t length) {
  if (Enter(*type, *lower, *upper, *m, *n, matrix, *leading, info)) {
    __real_dlascl_(type, lower, upper, cfrom, cto, m, n, matrix, leading, info,
                   length);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_dlascl_base), decltype(__wrap_dlascl_)>);
extern "C" decltype(LAPACK_clascl_base) __real_clascl_;
extern "C" void __wrap_clascl_(const char* type, const lapack_int* lower,
                               const lapack_int* upper, const float* cfrom,
                               const float* cto, const lapack_int* m,
                               const lapack_int* n, std::complex<float>* matrix,
                               const lapack_int* leading, lapack_int* info,
                               std::size_t length) {
  if (Enter(*type, *lower, *upper, *m, *n, matrix, *leading, info)) {
    __real_clascl_(type, lower, upper, cfrom, cto, m, n, matrix, leading, info,
                   length);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_clascl_base), decltype(__wrap_clascl_)>);
extern "C" decltype(LAPACK_zlascl_base) __real_zlascl_;
extern "C" void __wrap_zlascl_(const char* type, const lapack_int* lower,
                               const lapack_int* upper, const double* cfrom,
                               const double* cto, const lapack_int* m,
                               const lapack_int* n,
                               std::complex<double>* matrix,
                               const lapack_int* leading, lapack_int* info,
                               std::size_t length) {
  if (Enter(*type, *lower, *upper, *m, *n, matrix, *leading, info)) {
    __real_zlascl_(type, lower, upper, cfrom, cto, m, n, matrix, leading, info,
                   length);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_zlascl_base), decltype(__wrap_zlascl_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_scale_entry
