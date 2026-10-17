#include "triangular_storage_conversion_faults.h"

#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "asc/dense/blas.h"

namespace asc_triangular_storage_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local bool g_baseline = true;

template <typename T>
T Value(int x) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (std::is_same_v<T, Real>) {
    return static_cast<T>(x);
  } else {
    return {static_cast<Real>(x), static_cast<Real>(x + 11)};
  }
}

template <typename T, typename Function>
bool Baseline(bool pack, char part, Function real) {
  std::array<T, 11> dense;
  std::array<T, 8> packed;
  dense.fill(Value<T>(-7));
  packed.fill(Value<T>(-9));
  if (pack) {
    for (int i = 0; i < 9; ++i) {
      dense[static_cast<std::size_t>(i) + 1] = Value<T>(i + 1);
    }
  } else {
    for (int i = 0; i < 6; ++i) {
      packed[static_cast<std::size_t>(i) + 1] = Value<T>(i + 1);
    }
  }
  auto expected_dense = dense;
  auto expected_packed = packed;
  if (pack) {
    const auto indices = part == 'U' ? std::array{1, 4, 5, 7, 8, 9}
                                     : std::array{1, 2, 3, 5, 6, 9};
    for (std::size_t i = 0; i < indices.size(); ++i) {
      expected_packed[i + 1] = Value<T>(indices[i]);
    }
  } else {
    const auto values = part == 'U' ? std::array{1, -7, -7, 2, 3, -7, 4, 5, 6}
                                    : std::array{1, 2, 3, -7, 4, 5, -7, -7, 6};
    for (std::size_t i = 0; i < values.size(); ++i) {
      expected_dense[i + 1] = Value<T>(values[i]);
    }
  }
  const lapack_int n = 3;
  const lapack_int leading = 3;
  std::array<lapack_int, 3> info{89, std::numeric_limits<lapack_int>::max(),
                                 97};
  if (pack) {
    real(&part, &n, dense.data() + 1, &leading, packed.data() + 1, &info[1],
         std::size_t{1});
  } else {
    real(&part, &n, packed.data() + 1, &leading, dense.data() + 1, &info[1],
         std::size_t{1});
  }
  return dense == expected_dense && packed == expected_packed &&
         info == std::array<lapack_int, 3>{89, 0, 97};
}

template <typename T, typename Function>
void Inject(bool pack, const char* part, const lapack_int* n, const T* input,
            const lapack_int* leading, T* output, lapack_int* info,
            std::size_t length, Function real) {
  ++g_calls;
  const auto incoming = *info;
  g_baseline = g_baseline && (*part == 'U' || *part == 'L') && *n == 3 &&
               *leading >= 3 && length == 1 &&
               incoming == std::numeric_limits<lapack_int>::max() &&
               Baseline<T>(pack, *part, real);
  if (g_mode != Mode::kOmitValues) {
    real(part, n, input, leading, output, info, length);
  }
  switch (g_mode) {
    case Mode::kOmitInfo:
      *info = incoming;
      break;
    case Mode::kPositive:
      *info = 1;
      break;
    case Mode::kNegativeOne:
      *info = -1;
      break;
    case Mode::kNegativeTwo:
      *info = -2;
      break;
    case Mode::kNegativeLeading:
      *info = pack ? -4 : -5;
      break;
    case Mode::kMinimum:
      *info = std::numeric_limits<lapack_int>::min();
      break;
    case Mode::kShortZero: {
      *info = incoming;
      const std::int32_t zero = 0;
      std::memcpy(info, &zero, sizeof(zero));
      break;
    }
    case Mode::kOmitValues:
      *info = 0;
      break;
    case Mode::kWrongValues:
      if (pack) {
        std::fill_n(output, *n * (*n + 1) / 2, T{42});
      } else {
        for (lapack_int j = 0; j < *n; ++j) {
          for (lapack_int i = 0; i < *n; ++i) {
            if (*part == 'U' ? i <= j : i >= j) {
              output[j * *leading + i] = T{42};
            }
          }
        }
      }
      break;
    case Mode::kPass:
      break;
  }
}
}  // namespace
void Reset(Mode mode) {
  g_mode = mode;
  g_calls = 0;
  g_baseline = true;
}
std::size_t Calls() { return g_calls; }
bool BaselinePassed() { return g_baseline; }
// Private GNU ELF interception names.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_strttp_base) __real_strttp_;
extern "C" void __wrap_strttp_(const char* part, const lapack_int* n,
                               const float* input, const lapack_int* leading,
                               float* output, lapack_int* info,
                               std::size_t length) {
  Inject(true, part, n, input, leading, output, info, length,
         [](const char* p, const lapack_int* size, const float* in,
            const lapack_int* ld, float* out, lapack_int* result,
            std::size_t len) {
           __real_strttp_(p, size, in, ld, out, result, len);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_strttp_base), decltype(__wrap_strttp_)>);
extern "C" decltype(LAPACK_stpttr_base) __real_stpttr_;
extern "C" void __wrap_stpttr_(const char* part, const lapack_int* n,
                               const float* input, float* output,
                               const lapack_int* leading, lapack_int* info,
                               std::size_t length) {
  Inject(false, part, n, input, leading, output, info, length,
         [](const char* p, const lapack_int* size, const float* in,
            const lapack_int* ld, float* out, lapack_int* result,
            std::size_t len) {
           __real_stpttr_(p, size, in, out, ld, result, len);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_stpttr_base), decltype(__wrap_stpttr_)>);
extern "C" decltype(LAPACK_dtrttp_base) __real_dtrttp_;
extern "C" void __wrap_dtrttp_(const char* part, const lapack_int* n,
                               const double* input, const lapack_int* leading,
                               double* output, lapack_int* info,
                               std::size_t length) {
  Inject(true, part, n, input, leading, output, info, length,
         [](const char* p, const lapack_int* size, const double* in,
            const lapack_int* ld, double* out, lapack_int* result,
            std::size_t len) {
           __real_dtrttp_(p, size, in, ld, out, result, len);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dtrttp_base), decltype(__wrap_dtrttp_)>);
extern "C" decltype(LAPACK_dtpttr_base) __real_dtpttr_;
extern "C" void __wrap_dtpttr_(const char* part, const lapack_int* n,
                               const double* input, double* output,
                               const lapack_int* leading, lapack_int* info,
                               std::size_t length) {
  Inject(false, part, n, input, leading, output, info, length,
         [](const char* p, const lapack_int* size, const double* in,
            const lapack_int* ld, double* out, lapack_int* result,
            std::size_t len) {
           __real_dtpttr_(p, size, in, out, ld, result, len);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dtpttr_base), decltype(__wrap_dtpttr_)>);
extern "C" decltype(LAPACK_ctrttp_base) __real_ctrttp_;
extern "C" void __wrap_ctrttp_(const char* part, const lapack_int* n,
                               const std::complex<float>* input,
                               const lapack_int* leading,
                               std::complex<float>* output, lapack_int* info,
                               std::size_t length) {
  Inject(true, part, n, input, leading, output, info, length,
         [](const char* p, const lapack_int* size,
            const std::complex<float>* in, const lapack_int* ld,
            std::complex<float>* out, lapack_int* result, std::size_t len) {
           __real_ctrttp_(p, size, in, ld, out, result, len);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ctrttp_base), decltype(__wrap_ctrttp_)>);
extern "C" decltype(LAPACK_ctpttr_base) __real_ctpttr_;
extern "C" void __wrap_ctpttr_(const char* part, const lapack_int* n,
                               const std::complex<float>* input,
                               std::complex<float>* output,
                               const lapack_int* leading, lapack_int* info,
                               std::size_t length) {
  Inject(false, part, n, input, leading, output, info, length,
         [](const char* p, const lapack_int* size,
            const std::complex<float>* in, const lapack_int* ld,
            std::complex<float>* out, lapack_int* result, std::size_t len) {
           __real_ctpttr_(p, size, in, out, ld, result, len);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ctpttr_base), decltype(__wrap_ctpttr_)>);
extern "C" decltype(LAPACK_ztrttp_base) __real_ztrttp_;
extern "C" void __wrap_ztrttp_(const char* part, const lapack_int* n,
                               const std::complex<double>* input,
                               const lapack_int* leading,
                               std::complex<double>* output, lapack_int* info,
                               std::size_t length) {
  Inject(true, part, n, input, leading, output, info, length,
         [](const char* p, const lapack_int* size,
            const std::complex<double>* in, const lapack_int* ld,
            std::complex<double>* out, lapack_int* result, std::size_t len) {
           __real_ztrttp_(p, size, in, ld, out, result, len);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ztrttp_base), decltype(__wrap_ztrttp_)>);
extern "C" decltype(LAPACK_ztpttr_base) __real_ztpttr_;
extern "C" void __wrap_ztpttr_(const char* part, const lapack_int* n,
                               const std::complex<double>* input,
                               std::complex<double>* output,
                               const lapack_int* leading, lapack_int* info,
                               std::size_t length) {
  Inject(false, part, n, input, leading, output, info, length,
         [](const char* p, const lapack_int* size,
            const std::complex<double>* in, const lapack_int* ld,
            std::complex<double>* out, lapack_int* result, std::size_t len) {
           __real_ztpttr_(p, size, in, out, ld, result, len);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ztpttr_base), decltype(__wrap_ztpttr_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_triangular_storage_fault
