#include "lu_expert_info_faults.h"

#include <complex>
#include <cstdint>
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
asc_lapack_test::LuExpertInfoFault g_fault =
    asc_lapack_test::LuExpertInfoFault::kNone;
asc_lapack_test::LuExpertInfoObservation g_observation;
int g_depth = 0;
// GESV and recursive LU may enter another wrapped routine. Interfere only with
// the outer ASC call, preserving every nested provider result unmodified.
class NativeCall {
 public:
  NativeCall() { ++g_depth; }
  NativeCall(const NativeCall&) = delete;
  NativeCall& operator=(const NativeCall&) = delete;
  NativeCall(NativeCall&&) = delete;
  NativeCall& operator=(NativeCall&&) = delete;
  ~NativeCall() { --g_depth; }
};
void Publish(lapack_int actual, lapack_int* destination, bool query = false) {
  if (g_depth != 1) {
    *destination = actual;
    return;
  }
  ++g_observation.calls;
  g_observation.queries += query ? 1 : 0;
  g_observation.incoming = *destination;
  g_observation.actual = actual;
  if (g_fault == asc_lapack_test::LuExpertInfoFault::kNone) {
    *destination = actual;
  } else if (g_fault == asc_lapack_test::LuExpertInfoFault::kShortZero) {
    const std::int32_t zero = 0;
    std::memcpy(destination, &zero, sizeof(zero));
  }
  g_observation.outgoing = *destination;
}
}  // namespace
namespace asc_lapack_test {
void SetLuExpertInfoFault(LuExpertInfoFault fault) {
  g_fault = fault;
  g_observation = {};
}
LuExpertInfoObservation ObserveLuExpertInfo() { return g_observation; }
}  // namespace asc_lapack_test
// Test-only GNU ELF names: all signatures are checked against the pinned
// header. Real calls retain every original argument except the INFO
// destination. NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_sgetrf2_(const lapack_int*, const lapack_int*, float*,
                     const lapack_int*, lapack_int*, lapack_int*);
void __wrap_sgetrf2_(const lapack_int* m, const lapack_int* n, float* a,
                     const lapack_int* lda, lapack_int* pivots,
                     lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_sgetrf2_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
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
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_sgetf2_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
}
static_assert(
    std::is_same_v<decltype(__real_sgetf2_), decltype(LAPACK_sgetf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgetf2_), decltype(LAPACK_sgetf2)>);
void __real_sgetri_(const lapack_int*, float*, const lapack_int*,
                    const lapack_int*, float*, const lapack_int*, lapack_int*);
void __wrap_sgetri_(const lapack_int* n, float* a, const lapack_int* lda,
                    const lapack_int* pivots, float* work,
                    const lapack_int* lwork, lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_sgetri_(n, a, lda, pivots, work, lwork, &actual);
  Publish(actual, info, *lwork == -1);
}
static_assert(
    std::is_same_v<decltype(__real_sgetri_), decltype(LAPACK_sgetri)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgetri_), decltype(LAPACK_sgetri)>);
void __real_sgesv_(const lapack_int*, const lapack_int*, float*,
                   const lapack_int*, lapack_int*, float*, const lapack_int*,
                   lapack_int*);
void __wrap_sgesv_(const lapack_int* n, const lapack_int* nrhs, float* a,
                   const lapack_int* lda, lapack_int* pivots, float* b,
                   const lapack_int* ldb, lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_sgesv_(n, nrhs, a, lda, pivots, b, ldb, &actual);
  Publish(actual, info);
}
static_assert(std::is_same_v<decltype(__real_sgesv_), decltype(LAPACK_sgesv)>);
static_assert(std::is_same_v<decltype(__wrap_sgesv_), decltype(LAPACK_sgesv)>);
void __real_dgetrf2_(const lapack_int*, const lapack_int*, double*,
                     const lapack_int*, lapack_int*, lapack_int*);
void __wrap_dgetrf2_(const lapack_int* m, const lapack_int* n, double* a,
                     const lapack_int* lda, lapack_int* pivots,
                     lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_dgetrf2_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
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
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_dgetf2_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
}
static_assert(
    std::is_same_v<decltype(__real_dgetf2_), decltype(LAPACK_dgetf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgetf2_), decltype(LAPACK_dgetf2)>);
void __real_dgetri_(const lapack_int*, double*, const lapack_int*,
                    const lapack_int*, double*, const lapack_int*, lapack_int*);
void __wrap_dgetri_(const lapack_int* n, double* a, const lapack_int* lda,
                    const lapack_int* pivots, double* work,
                    const lapack_int* lwork, lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_dgetri_(n, a, lda, pivots, work, lwork, &actual);
  Publish(actual, info, *lwork == -1);
}
static_assert(
    std::is_same_v<decltype(__real_dgetri_), decltype(LAPACK_dgetri)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgetri_), decltype(LAPACK_dgetri)>);
void __real_dgesv_(const lapack_int*, const lapack_int*, double*,
                   const lapack_int*, lapack_int*, double*, const lapack_int*,
                   lapack_int*);
void __wrap_dgesv_(const lapack_int* n, const lapack_int* nrhs, double* a,
                   const lapack_int* lda, lapack_int* pivots, double* b,
                   const lapack_int* ldb, lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_dgesv_(n, nrhs, a, lda, pivots, b, ldb, &actual);
  Publish(actual, info);
}
static_assert(std::is_same_v<decltype(__real_dgesv_), decltype(LAPACK_dgesv)>);
static_assert(std::is_same_v<decltype(__wrap_dgesv_), decltype(LAPACK_dgesv)>);
void __real_cgetrf2_(const lapack_int*, const lapack_int*, std::complex<float>*,
                     const lapack_int*, lapack_int*, lapack_int*);
void __wrap_cgetrf2_(const lapack_int* m, const lapack_int* n,
                     std::complex<float>* a, const lapack_int* lda,
                     lapack_int* pivots, lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_cgetrf2_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
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
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_cgetf2_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
}
static_assert(
    std::is_same_v<decltype(__real_cgetf2_), decltype(LAPACK_cgetf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgetf2_), decltype(LAPACK_cgetf2)>);
void __real_cgetri_(const lapack_int*, std::complex<float>*, const lapack_int*,
                    const lapack_int*, std::complex<float>*, const lapack_int*,
                    lapack_int*);
void __wrap_cgetri_(const lapack_int* n, std::complex<float>* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    std::complex<float>* work, const lapack_int* lwork,
                    lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_cgetri_(n, a, lda, pivots, work, lwork, &actual);
  Publish(actual, info, *lwork == -1);
}
static_assert(
    std::is_same_v<decltype(__real_cgetri_), decltype(LAPACK_cgetri)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgetri_), decltype(LAPACK_cgetri)>);
void __real_cgesv_(const lapack_int*, const lapack_int*, std::complex<float>*,
                   const lapack_int*, lapack_int*, std::complex<float>*,
                   const lapack_int*, lapack_int*);
void __wrap_cgesv_(const lapack_int* n, const lapack_int* nrhs,
                   std::complex<float>* a, const lapack_int* lda,
                   lapack_int* pivots, std::complex<float>* b,
                   const lapack_int* ldb, lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_cgesv_(n, nrhs, a, lda, pivots, b, ldb, &actual);
  Publish(actual, info);
}
static_assert(std::is_same_v<decltype(__real_cgesv_), decltype(LAPACK_cgesv)>);
static_assert(std::is_same_v<decltype(__wrap_cgesv_), decltype(LAPACK_cgesv)>);
void __real_zgetrf2_(const lapack_int*, const lapack_int*,
                     std::complex<double>*, const lapack_int*, lapack_int*,
                     lapack_int*);
void __wrap_zgetrf2_(const lapack_int* m, const lapack_int* n,
                     std::complex<double>* a, const lapack_int* lda,
                     lapack_int* pivots, lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_zgetrf2_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
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
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_zgetf2_(m, n, a, lda, pivots, &actual);
  Publish(actual, info);
}
static_assert(
    std::is_same_v<decltype(__real_zgetf2_), decltype(LAPACK_zgetf2)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgetf2_), decltype(LAPACK_zgetf2)>);
void __real_zgetri_(const lapack_int*, std::complex<double>*, const lapack_int*,
                    const lapack_int*, std::complex<double>*, const lapack_int*,
                    lapack_int*);
void __wrap_zgetri_(const lapack_int* n, std::complex<double>* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    std::complex<double>* work, const lapack_int* lwork,
                    lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_zgetri_(n, a, lda, pivots, work, lwork, &actual);
  Publish(actual, info, *lwork == -1);
}
static_assert(
    std::is_same_v<decltype(__real_zgetri_), decltype(LAPACK_zgetri)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgetri_), decltype(LAPACK_zgetri)>);
void __real_zgesv_(const lapack_int*, const lapack_int*, std::complex<double>*,
                   const lapack_int*, lapack_int*, std::complex<double>*,
                   const lapack_int*, lapack_int*);
void __wrap_zgesv_(const lapack_int* n, const lapack_int* nrhs,
                   std::complex<double>* a, const lapack_int* lda,
                   lapack_int* pivots, std::complex<double>* b,
                   const lapack_int* ldb, lapack_int* info) {
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  __real_zgesv_(n, nrhs, a, lda, pivots, b, ldb, &actual);
  Publish(actual, info);
}
static_assert(std::is_same_v<decltype(__real_zgesv_), decltype(LAPACK_zgesv)>);
static_assert(std::is_same_v<decltype(__wrap_zgesv_), decltype(LAPACK_zgesv)>);
}
// NOLINTEND(bugprone-reserved-identifier)
