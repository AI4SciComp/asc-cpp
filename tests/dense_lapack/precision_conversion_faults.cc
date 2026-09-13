#include "precision_conversion_faults.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
namespace asc_conversion_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local void (*g_entry)() = nullptr;
bool Enter(lapack_int* info) {
  ++g_calls;
  if (g_entry != nullptr) {
    g_entry();
  }
  if (g_mode == Mode::kUnwrittenOutput) {
    *info = 0;
    return false;
  }
  return true;
}
void Info(lapack_int actual, lapack_int* output) {
  switch (g_mode) {
    case Mode::kNegativeInfo:
      *output = -1;
      break;
    case Mode::kOneInfo:
      *output = 1;
      break;
    case Mode::kExcessInfo:
      *output = 2;
      break;
    case Mode::kUnwrittenInfo:
      break;
    case Mode::kPartialInfo:
      if constexpr (sizeof(lapack_int) > sizeof(std::int32_t)) {
        const std::int32_t zero = 0;
        std::memcpy(output, &zero, sizeof(zero));
      }
      break;
    default:
      *output = actual;
      break;
  }
}
}  // namespace
void Reset(Mode mode) {
  g_mode = mode;
  g_calls = 0;
  g_entry = nullptr;
}
std::size_t Calls() { return g_calls; }
void OnEntry(void (*callback)()) { g_entry = callback; }
// Linker identifiers are confined to this established ELF test mechanism.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_slag2d) __real_slag2d_;
extern "C" void __wrap_slag2d_(const lapack_int* m, const lapack_int* n,
                               const float* input,
                               const lapack_int* input_leading, double* output,
                               const lapack_int* output_leading,
                               lapack_int* info) {
  if (!Enter(info)) {
    return;
  }
  lapack_int actual = -97;
  __real_slag2d_(m, n, input, input_leading, output, output_leading, &actual);
  Info(actual, info);
}
static_assert(
    std::is_same_v<decltype(LAPACK_slag2d), decltype(__wrap_slag2d_)>);
extern "C" decltype(LAPACK_dlag2s) __real_dlag2s_;
extern "C" void __wrap_dlag2s_(const lapack_int* m, const lapack_int* n,
                               const double* input,
                               const lapack_int* input_leading, float* output,
                               const lapack_int* output_leading,
                               lapack_int* info) {
  if (!Enter(info)) {
    return;
  }
  lapack_int actual = -97;
  __real_dlag2s_(m, n, input, input_leading, output, output_leading, &actual);
  Info(actual, info);
}
static_assert(
    std::is_same_v<decltype(LAPACK_dlag2s), decltype(__wrap_dlag2s_)>);
extern "C" decltype(LAPACK_clag2z) __real_clag2z_;
extern "C" void __wrap_clag2z_(const lapack_int* m, const lapack_int* n,
                               const std::complex<float>* input,
                               const lapack_int* input_leading,
                               std::complex<double>* output,
                               const lapack_int* output_leading,
                               lapack_int* info) {
  if (!Enter(info)) {
    return;
  }
  lapack_int actual = -97;
  __real_clag2z_(m, n, input, input_leading, output, output_leading, &actual);
  Info(actual, info);
}
static_assert(
    std::is_same_v<decltype(LAPACK_clag2z), decltype(__wrap_clag2z_)>);
extern "C" decltype(LAPACK_zlag2c) __real_zlag2c_;
extern "C" void __wrap_zlag2c_(const lapack_int* m, const lapack_int* n,
                               const std::complex<double>* input,
                               const lapack_int* input_leading,
                               std::complex<float>* output,
                               const lapack_int* output_leading,
                               lapack_int* info) {
  if (!Enter(info)) {
    return;
  }
  lapack_int actual = -97;
  __real_zlag2c_(m, n, input, input_leading, output, output_leading, &actual);
  Info(actual, info);
}
static_assert(
    std::is_same_v<decltype(LAPACK_zlag2c), decltype(__wrap_zlag2c_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_conversion_fault
