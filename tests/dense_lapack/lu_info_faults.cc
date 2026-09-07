#include "lu_info_faults.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapacke_config.h>
namespace {
asc_lapack_test::LuInfoFault g_fault = asc_lapack_test::LuInfoFault::kNone;
asc_lapack_test::LuInfoObservation g_observation;
void Publish(lapack_int actual, lapack_int* destination) {
  ++g_observation.calls;
  g_observation.incoming = *destination;
  g_observation.actual = actual;
  if (g_fault == asc_lapack_test::LuInfoFault::kNone) {
    *destination = actual;
  } else if (g_fault == asc_lapack_test::LuInfoFault::kShortZero) {
    // A deliberate wrong-width output is tested only on actual ILP64.
    const std::int32_t zero = 0;
    std::memcpy(destination, &zero, sizeof(zero));
  }
  g_observation.outgoing = *destination;
}
}  // namespace
namespace asc_lapack_test {
void SetLuInfoFault(LuInfoFault fault) {
  g_fault = fault;
  g_observation = {};
}
LuInfoObservation ObserveLuInfo() { return g_observation; }
}  // namespace asc_lapack_test
// Exact audited GNU ELF symbols/hidden size_t length. The unmodified pinned
// function always runs with all original arguments except its INFO destination.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_sgetrf_(const lapack_int*, const lapack_int*, float*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_sgetrf_(const lapack_int* m, const lapack_int* n, float* a,
                    const lapack_int* lda, lapack_int* pivots,
                    lapack_int* info) {
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_sgetrf_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
}
void __real_sgetrs_(const char*, const lapack_int*, const lapack_int*,
                    const float*, const lapack_int*, const lapack_int*, float*,
                    const lapack_int*, lapack_int*, std::size_t);
void __wrap_sgetrs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const float* a,
                    const lapack_int* lda, const lapack_int* pivots, float* b,
                    const lapack_int* ldb, lapack_int* info,
                    std::size_t trans_length) {
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_sgetrs_(trans, n, nrhs, a, lda, pivots, b, ldb, &actual, trans_length);
  Publish(actual, info);
}
void __real_dgetrf_(const lapack_int*, const lapack_int*, double*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_dgetrf_(const lapack_int* m, const lapack_int* n, double* a,
                    const lapack_int* lda, lapack_int* pivots,
                    lapack_int* info) {
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_dgetrf_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
}
void __real_dgetrs_(const char*, const lapack_int*, const lapack_int*,
                    const double*, const lapack_int*, const lapack_int*,
                    double*, const lapack_int*, lapack_int*, std::size_t);
void __wrap_dgetrs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const double* a,
                    const lapack_int* lda, const lapack_int* pivots, double* b,
                    const lapack_int* ldb, lapack_int* info,
                    std::size_t trans_length) {
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_dgetrs_(trans, n, nrhs, a, lda, pivots, b, ldb, &actual, trans_length);
  Publish(actual, info);
}
void __real_cgetrf_(const lapack_int*, const lapack_int*, std::complex<float>*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_cgetrf_(const lapack_int* m, const lapack_int* n,
                    std::complex<float>* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_int* info) {
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_cgetrf_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
}
void __real_cgetrs_(const char*, const lapack_int*, const lapack_int*,
                    const std::complex<float>*, const lapack_int*,
                    const lapack_int*, std::complex<float>*, const lapack_int*,
                    lapack_int*, std::size_t);
void __wrap_cgetrs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<float>* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    std::complex<float>* b, const lapack_int* ldb,
                    lapack_int* info, std::size_t trans_length) {
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_cgetrs_(trans, n, nrhs, a, lda, pivots, b, ldb, &actual, trans_length);
  Publish(actual, info);
}
void __real_zgetrf_(const lapack_int*, const lapack_int*, std::complex<double>*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_zgetrf_(const lapack_int* m, const lapack_int* n,
                    std::complex<double>* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_int* info) {
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_zgetrf_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
}
void __real_zgetrs_(const char*, const lapack_int*, const lapack_int*,
                    const std::complex<double>*, const lapack_int*,
                    const lapack_int*, std::complex<double>*, const lapack_int*,
                    lapack_int*, std::size_t);
void __wrap_zgetrs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<double>* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    std::complex<double>* b, const lapack_int* ldb,
                    lapack_int* info, std::size_t trans_length) {
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_zgetrs_(trans, n, nrhs, a, lda, pivots, b, ldb, &actual, trans_length);
  Publish(actual, info);
}
}
// NOLINTEND(bugprone-reserved-identifier)
