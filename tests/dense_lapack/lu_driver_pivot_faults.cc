#include "lu_driver_pivot_faults.h"

#include <array>
#include <bit>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapack.h>
#include <lapacke_config.h>
namespace {
using Fault = asc_lapack_test::LuDriverPivotFault;
Fault g_fault = Fault::kNone;
asc_lapack_test::LuDriverPivotObservation g_observation;
void Publish(const lapack_int* actual, lapack_int* output, lapack_int n) {
  static_assert(std::endian::native == std::endian::little);
  for (lapack_int i = 0; i < n; ++i) {
    if (g_fault == Fault::kOmitAll ||
        (g_fault == Fault::kOmitLast && i == n - 1)) {
      continue;
    }
    if (g_fault == Fault::kShortAll ||
        (g_fault == Fault::kShortLast && i == n - 1)) {
      const auto low = static_cast<std::int32_t>(actual[i]);
      std::memcpy(output + i, &low, sizeof(low));
    } else {
      output[i] = actual[i];
    }
  }
}
template <typename Operation>
void Call(char fact, lapack_int n, lapack_int* pivots, const lapack_int* info,
          Operation operation) {
  if (n <= 0 || n > 8) {
    std::abort();
  }
  std::array<lapack_int, 8> actual;
  actual.fill(std::numeric_limits<lapack_int>::min());
  if (fact == 'F') {
    for (lapack_int i = 0; i < n; ++i) {
      actual[i] = pivots[i];
    }
    // FACT=F pivots are inputs. Every actual native argument is preserved.
    operation(pivots);
    for (lapack_int i = 0; i < n; ++i) {
      if (pivots[i] != actual[i]) {
        g_observation.input_preserved = false;
      }
    }
  } else {
    // Only FACT=N/E's output pivot destination is redirected. The real
    // driver and all of its nested calls share these complete native pivots.
    // The original INFO destination and every numerical argument are retained.
    operation(actual.data());
    Publish(actual.data(), pivots, n);
    g_observation.redirected_output = true;
  }
  ++g_observation.calls;
  g_observation.entries = static_cast<std::size_t>(n);
  g_observation.actual_info = *info;
  for (lapack_int i = 0; i < n; ++i) {
    g_observation.actual[i] = actual[i];
    g_observation.valid_actual +=
        static_cast<std::size_t>(actual[i] >= i + 1 && actual[i] <= n);
  }
}
}  // namespace
namespace asc_lapack_test {
void SetLuDriverPivotFault(LuDriverPivotFault fault) {
  g_fault = fault;
  g_observation = {};
}
LuDriverPivotObservation ObserveLuDriverPivot() { return g_observation; }
}  // namespace asc_lapack_test
// Test-only ELF symbols, checked against the exact pinned declarations.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_sgesvx_(const char*, const char*, const lapack_int*,
                    const lapack_int*, float*, const lapack_int*, float*,
                    const lapack_int*, lapack_int*, char*, float*, float*,
                    float*, const lapack_int*, float*, const lapack_int*,
                    float*, float*, float*, float*, lapack_int*, lapack_int*,
                    std::size_t, std::size_t, std::size_t);
void __wrap_sgesvx_(const char* fact, const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, float* a, const lapack_int* lda,
                    float* af, const lapack_int* ldaf, lapack_int* pivots,
                    char* equed, float* rows, float* columns, float* b,
                    const lapack_int* ldb, float* x, const lapack_int* ldx,
                    float* rcond, float* ferr, float* berr, float* work,
                    lapack_int* auxiliary, lapack_int* info,
                    std::size_t fact_length, std::size_t trans_length,
                    std::size_t equed_length) {
  Call(*fact, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_sgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, native_pivots, equed,
                   rows, columns, b, ldb, x, ldx, rcond, ferr, berr, work,
                   auxiliary, info, fact_length, trans_length, equed_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_sgesvx_), decltype(LAPACK_sgesvx_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgesvx_), decltype(LAPACK_sgesvx_base)>);
void __real_dgesvx_(const char*, const char*, const lapack_int*,
                    const lapack_int*, double*, const lapack_int*, double*,
                    const lapack_int*, lapack_int*, char*, double*, double*,
                    double*, const lapack_int*, double*, const lapack_int*,
                    double*, double*, double*, double*, lapack_int*,
                    lapack_int*, std::size_t, std::size_t, std::size_t);
void __wrap_dgesvx_(const char* fact, const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, double* a, const lapack_int* lda,
                    double* af, const lapack_int* ldaf, lapack_int* pivots,
                    char* equed, double* rows, double* columns, double* b,
                    const lapack_int* ldb, double* x, const lapack_int* ldx,
                    double* rcond, double* ferr, double* berr, double* work,
                    lapack_int* auxiliary, lapack_int* info,
                    std::size_t fact_length, std::size_t trans_length,
                    std::size_t equed_length) {
  Call(*fact, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_dgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, native_pivots, equed,
                   rows, columns, b, ldb, x, ldx, rcond, ferr, berr, work,
                   auxiliary, info, fact_length, trans_length, equed_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_dgesvx_), decltype(LAPACK_dgesvx_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgesvx_), decltype(LAPACK_dgesvx_base)>);
void __real_cgesvx_(const char*, const char*, const lapack_int*,
                    const lapack_int*, std::complex<float>*, const lapack_int*,
                    std::complex<float>*, const lapack_int*, lapack_int*, char*,
                    float*, float*, std::complex<float>*, const lapack_int*,
                    std::complex<float>*, const lapack_int*, float*, float*,
                    float*, std::complex<float>*, float*, lapack_int*,
                    std::size_t, std::size_t, std::size_t);
void __wrap_cgesvx_(const char* fact, const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, std::complex<float>* a,
                    const lapack_int* lda, std::complex<float>* af,
                    const lapack_int* ldaf, lapack_int* pivots, char* equed,
                    float* rows, float* columns, std::complex<float>* b,
                    const lapack_int* ldb, std::complex<float>* x,
                    const lapack_int* ldx, float* rcond, float* ferr,
                    float* berr, std::complex<float>* work, float* auxiliary,
                    lapack_int* info, std::size_t fact_length,
                    std::size_t trans_length, std::size_t equed_length) {
  Call(*fact, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_cgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, native_pivots, equed,
                   rows, columns, b, ldb, x, ldx, rcond, ferr, berr, work,
                   auxiliary, info, fact_length, trans_length, equed_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_cgesvx_), decltype(LAPACK_cgesvx_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgesvx_), decltype(LAPACK_cgesvx_base)>);
void __real_zgesvx_(const char*, const char*, const lapack_int*,
                    const lapack_int*, std::complex<double>*, const lapack_int*,
                    std::complex<double>*, const lapack_int*, lapack_int*,
                    char*, double*, double*, std::complex<double>*,
                    const lapack_int*, std::complex<double>*, const lapack_int*,
                    double*, double*, double*, std::complex<double>*, double*,
                    lapack_int*, std::size_t, std::size_t, std::size_t);
void __wrap_zgesvx_(const char* fact, const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, std::complex<double>* a,
                    const lapack_int* lda, std::complex<double>* af,
                    const lapack_int* ldaf, lapack_int* pivots, char* equed,
                    double* rows, double* columns, std::complex<double>* b,
                    const lapack_int* ldb, std::complex<double>* x,
                    const lapack_int* ldx, double* rcond, double* ferr,
                    double* berr, std::complex<double>* work, double* auxiliary,
                    lapack_int* info, std::size_t fact_length,
                    std::size_t trans_length, std::size_t equed_length) {
  Call(*fact, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_zgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, native_pivots, equed,
                   rows, columns, b, ldb, x, ldx, rcond, ferr, berr, work,
                   auxiliary, info, fact_length, trans_length, equed_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_zgesvx_), decltype(LAPACK_zgesvx_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgesvx_), decltype(LAPACK_zgesvx_base)>);
}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier)
