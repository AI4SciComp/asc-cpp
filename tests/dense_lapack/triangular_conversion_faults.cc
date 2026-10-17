#include "triangular_conversion_faults.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_triangular_conversion_prototypes.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
namespace asc_triangular_conversion_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local std::size_t g_unexpected = 0;
template <typename T>
T Value(float value) {
  if constexpr (std::is_same_v<T, float>) {
    return value;
  } else {
    return {value, 0};
  }
}
template <typename Input, typename Output, typename Function>
void Invoke(Function native, const char* triangle, const lapack_int* n,
            const Input* input, const lapack_int* lda, Output* output,
            const lapack_int* ldout, lapack_int* info, std::size_t length) {
  ++g_calls;
  if (*info != 0) {
    ++g_unexpected;
  }
  if (g_mode == Mode::kUnwrittenOutput) {
    return;
  }
  lapack_int actual = 0;
  native(triangle, n, input, lda, output, ldout, &actual, length);
  if (actual != 0) {
    ++g_unexpected;
  }
  switch (g_mode) {
    case Mode::kNegative:
      *info = -1;
      break;
    case Mode::kRange:
      *info = 1;
      break;
    case Mode::kExcess:
      *info = 2;
      break;
    case Mode::kMaximum:
      *info = std::numeric_limits<lapack_int>::max();
      break;
    case Mode::kMinimum:
      *info = std::numeric_limits<lapack_int>::min();
      break;
    case Mode::kUnwrittenInfo:
      break;
    case Mode::kShortWidthInfo: {
      const std::int32_t one = 1;
      std::memcpy(info, &one, sizeof(one));
      break;
    }
    case Mode::kNanOutput:
      output[0] = Value<Output>(std::numeric_limits<float>::quiet_NaN());
      break;
    case Mode::kInfiniteOutput:
      output[0] = Value<Output>(std::numeric_limits<float>::infinity());
      break;
    case Mode::kZeroOutput:
      output[0] = Value<Output>(-0.0F);
      break;
    default:
      *info = actual;
      break;
  }
}
}  // namespace
void Reset(Mode mode) {
  g_mode = mode;
  g_calls = 0;
  g_unexpected = 0;
}
std::size_t Calls() { return g_calls; }
std::size_t UnexpectedNativeResults() { return g_unexpected; }
// These reserved identifiers implement the established GNU linker test
// boundary. NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_dlat2s_base) __real_dlat2s_;
extern "C" void __wrap_dlat2s_(const char* triangle, const lapack_int* n,
                               const double* input, const lapack_int* lda,
                               float* output, const lapack_int* ldout,
                               lapack_int* info, std::size_t length) {
  Invoke(__real_dlat2s_, triangle, n, input, lda, output, ldout, info, length);
}
static_assert(
    std::is_same_v<decltype(__wrap_dlat2s_), decltype(LAPACK_dlat2s_base)>);
extern "C" decltype(LAPACK_zlat2c_base) __real_zlat2c_;
extern "C" void __wrap_zlat2c_(const char* triangle, const lapack_int* n,
                               const std::complex<double>* input,
                               const lapack_int* lda,
                               std::complex<float>* output,
                               const lapack_int* ldout, lapack_int* info,
                               std::size_t length) {
  Invoke(__real_zlat2c_, triangle, n, input, lda, output, ldout, info, length);
}
static_assert(
    std::is_same_v<decltype(__wrap_zlat2c_), decltype(LAPACK_zlat2c_base)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_triangular_conversion_fault
