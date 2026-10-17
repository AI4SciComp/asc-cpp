#include "lu_pivot_faults.h"

#include <algorithm>
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
using Fault = asc_lapack_test::LuPivotFault;
Fault g_fault = Fault::kNone;
asc_lapack_test::LuPivotObservation g_observation;
int g_depth = 0;
// Redirect only the outer ASC destination. Nested recursive/panel LU and the
// driver's solve see complete native pivots and the original INFO destination.
class NativeCall {
 public:
  NativeCall() { ++g_depth; }
  NativeCall(const NativeCall&) = delete;
  NativeCall& operator=(const NativeCall&) = delete;
  NativeCall(NativeCall&&) = delete;
  NativeCall& operator=(NativeCall&&) = delete;
  ~NativeCall() { --g_depth; }
};
void Publish(const lapack_int* actual, lapack_int* output, lapack_int count,
             lapack_int rows, lapack_int info) {
  static_assert(std::endian::native == std::endian::little);
  ++g_observation.calls;
  g_observation.entries = static_cast<std::size_t>(count);
  g_observation.actual_info = info;
  g_observation.incoming_last = output[count - 1];
  g_observation.actual_last = actual[count - 1];
  for (lapack_int i = 0; i < count; ++i) {
    if (actual[i] >= i + 1 && actual[i] <= rows) {
      ++g_observation.valid_actual;
    }
    if (g_fault == Fault::kOmitAll ||
        (g_fault == Fault::kOmitLast && i == count - 1)) {
      continue;
    }
    if (g_fault == Fault::kShortAll ||
        (g_fault == Fault::kShortLast && i == count - 1)) {
      const auto low = static_cast<std::int32_t>(actual[i]);
      std::memcpy(output + i, &low, sizeof(low));
    } else {
      output[i] = actual[i];
    }
  }
  for (lapack_int i = 0; i < count; ++i) {
    if (output[i] >= i + 1 && output[i] <= rows) {
      ++g_observation.valid_output;
    }
  }
  g_observation.outgoing_last = output[count - 1];
}
template <typename Operation>
void Call(lapack_int rows, lapack_int columns, lapack_int* pivots,
          const lapack_int* info, Operation operation) {
  if (g_depth != 0) {
    operation(pivots);
    return;
  }
  const lapack_int count = std::min(rows, columns);
  // Every fixture is bounded; these are test-owned output destinations.
  if (count <= 0 || count > 128) {
    std::abort();
  }
  std::array<lapack_int, 128> actual;
  actual.fill(std::numeric_limits<lapack_int>::min());
  const NativeCall call;
  operation(actual.data());
  Publish(actual.data(), pivots, count, rows, *info);
}
}  // namespace
namespace asc_lapack_test {
void SetLuPivotFault(LuPivotFault fault) {
  g_fault = fault;
  g_observation = {};
}
LuPivotObservation ObserveLuPivot() { return g_observation; }
}  // namespace asc_lapack_test
// Test-only ELF names, statically checked against the pinned prototypes.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_sgetrf_(const lapack_int*, const lapack_int*, float*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_sgetrf_(const lapack_int* m, const lapack_int* n, float* a,
                    const lapack_int* lda, lapack_int* pivots,
                    lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_sgetrf_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_sgetrf_), decltype(LAPACK_sgetrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgetrf_), decltype(LAPACK_sgetrf)>);
void __real_sgetrf2_(const lapack_int*, const lapack_int*, float*,
                     const lapack_int*, lapack_int*, lapack_int*);
void __wrap_sgetrf2_(const lapack_int* m, const lapack_int* n, float* a,
                     const lapack_int* lda, lapack_int* pivots,
                     lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_sgetrf2_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_sgetrf2_), decltype(LAPACK_sgetrf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgetrf2_), decltype(LAPACK_sgetrf2)>);
void __real_sgetf2_(const lapack_int*, const lapack_int*, float*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_sgetf2_(const lapack_int* m, const lapack_int* n, float* a,
                    const lapack_int* lda, lapack_int* pivots,
                    lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_sgetf2_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_sgetf2_), decltype(LAPACK_sgetf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgetf2_), decltype(LAPACK_sgetf2)>);
void __real_sgesv_(const lapack_int*, const lapack_int*, float*,
                   const lapack_int*, lapack_int*, float*, const lapack_int*,
                   lapack_int*);
void __wrap_sgesv_(const lapack_int* n, const lapack_int* nrhs, float* a,
                   const lapack_int* lda, lapack_int* pivots, float* b,
                   const lapack_int* ldb, lapack_int* info) {
  Call(*n, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_sgesv_(n, nrhs, a, lda, native_pivots, b, ldb, info);
  });
}
static_assert(std::is_same_v<decltype(__real_sgesv_), decltype(LAPACK_sgesv)>);
static_assert(std::is_same_v<decltype(__wrap_sgesv_), decltype(LAPACK_sgesv)>);
void __real_dgetrf_(const lapack_int*, const lapack_int*, double*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_dgetrf_(const lapack_int* m, const lapack_int* n, double* a,
                    const lapack_int* lda, lapack_int* pivots,
                    lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_dgetrf_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_dgetrf_), decltype(LAPACK_dgetrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgetrf_), decltype(LAPACK_dgetrf)>);
void __real_dgetrf2_(const lapack_int*, const lapack_int*, double*,
                     const lapack_int*, lapack_int*, lapack_int*);
void __wrap_dgetrf2_(const lapack_int* m, const lapack_int* n, double* a,
                     const lapack_int* lda, lapack_int* pivots,
                     lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_dgetrf2_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_dgetrf2_), decltype(LAPACK_dgetrf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgetrf2_), decltype(LAPACK_dgetrf2)>);
void __real_dgetf2_(const lapack_int*, const lapack_int*, double*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_dgetf2_(const lapack_int* m, const lapack_int* n, double* a,
                    const lapack_int* lda, lapack_int* pivots,
                    lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_dgetf2_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_dgetf2_), decltype(LAPACK_dgetf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgetf2_), decltype(LAPACK_dgetf2)>);
void __real_dgesv_(const lapack_int*, const lapack_int*, double*,
                   const lapack_int*, lapack_int*, double*, const lapack_int*,
                   lapack_int*);
void __wrap_dgesv_(const lapack_int* n, const lapack_int* nrhs, double* a,
                   const lapack_int* lda, lapack_int* pivots, double* b,
                   const lapack_int* ldb, lapack_int* info) {
  Call(*n, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_dgesv_(n, nrhs, a, lda, native_pivots, b, ldb, info);
  });
}
static_assert(std::is_same_v<decltype(__real_dgesv_), decltype(LAPACK_dgesv)>);
static_assert(std::is_same_v<decltype(__wrap_dgesv_), decltype(LAPACK_dgesv)>);
void __real_cgetrf_(const lapack_int*, const lapack_int*, std::complex<float>*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_cgetrf_(const lapack_int* m, const lapack_int* n,
                    std::complex<float>* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_cgetrf_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_cgetrf_), decltype(LAPACK_cgetrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgetrf_), decltype(LAPACK_cgetrf)>);
void __real_cgetrf2_(const lapack_int*, const lapack_int*, std::complex<float>*,
                     const lapack_int*, lapack_int*, lapack_int*);
void __wrap_cgetrf2_(const lapack_int* m, const lapack_int* n,
                     std::complex<float>* a, const lapack_int* lda,
                     lapack_int* pivots, lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_cgetrf2_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_cgetrf2_), decltype(LAPACK_cgetrf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgetrf2_), decltype(LAPACK_cgetrf2)>);
void __real_cgetf2_(const lapack_int*, const lapack_int*, std::complex<float>*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_cgetf2_(const lapack_int* m, const lapack_int* n,
                    std::complex<float>* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_cgetf2_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_cgetf2_), decltype(LAPACK_cgetf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgetf2_), decltype(LAPACK_cgetf2)>);
void __real_cgesv_(const lapack_int*, const lapack_int*, std::complex<float>*,
                   const lapack_int*, lapack_int*, std::complex<float>*,
                   const lapack_int*, lapack_int*);
void __wrap_cgesv_(const lapack_int* n, const lapack_int* nrhs,
                   std::complex<float>* a, const lapack_int* lda,
                   lapack_int* pivots, std::complex<float>* b,
                   const lapack_int* ldb, lapack_int* info) {
  Call(*n, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_cgesv_(n, nrhs, a, lda, native_pivots, b, ldb, info);
  });
}
static_assert(std::is_same_v<decltype(__real_cgesv_), decltype(LAPACK_cgesv)>);
static_assert(std::is_same_v<decltype(__wrap_cgesv_), decltype(LAPACK_cgesv)>);
void __real_zgetrf_(const lapack_int*, const lapack_int*, std::complex<double>*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_zgetrf_(const lapack_int* m, const lapack_int* n,
                    std::complex<double>* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_zgetrf_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_zgetrf_), decltype(LAPACK_zgetrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgetrf_), decltype(LAPACK_zgetrf)>);
void __real_zgetrf2_(const lapack_int*, const lapack_int*,
                     std::complex<double>*, const lapack_int*, lapack_int*,
                     lapack_int*);
void __wrap_zgetrf2_(const lapack_int* m, const lapack_int* n,
                     std::complex<double>* a, const lapack_int* lda,
                     lapack_int* pivots, lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_zgetrf2_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_zgetrf2_), decltype(LAPACK_zgetrf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgetrf2_), decltype(LAPACK_zgetrf2)>);
void __real_zgetf2_(const lapack_int*, const lapack_int*, std::complex<double>*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_zgetf2_(const lapack_int* m, const lapack_int* n,
                    std::complex<double>* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_int* info) {
  Call(*m, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_zgetf2_(m, n, a, lda, native_pivots, info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_zgetf2_), decltype(LAPACK_zgetf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgetf2_), decltype(LAPACK_zgetf2)>);
void __real_zgesv_(const lapack_int*, const lapack_int*, std::complex<double>*,
                   const lapack_int*, lapack_int*, std::complex<double>*,
                   const lapack_int*, lapack_int*);
void __wrap_zgesv_(const lapack_int* n, const lapack_int* nrhs,
                   std::complex<double>* a, const lapack_int* lda,
                   lapack_int* pivots, std::complex<double>* b,
                   const lapack_int* ldb, lapack_int* info) {
  Call(*n, *n, pivots, info, [&](lapack_int* native_pivots) {
    __real_zgesv_(n, nrhs, a, lda, native_pivots, b, ldb, info);
  });
}
static_assert(std::is_same_v<decltype(__real_zgesv_), decltype(LAPACK_zgesv)>);
static_assert(std::is_same_v<decltype(__wrap_zgesv_), decltype(LAPACK_zgesv)>);
}
// NOLINTEND(bugprone-reserved-identifier)
