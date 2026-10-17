#include "lu_aux_info_faults.h"

#include <bit>
#include <complex>
#include <cstddef>
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
using Fault = asc_lapack_test::LuAuxInfoFault;
Fault g_fault = Fault::kNone;
asc_lapack_test::LuAuxInfoObservation g_observation;
int g_depth = 0;
class NativeCall {
 public:
  NativeCall() { ++g_depth; }
  NativeCall(const NativeCall&) = delete;
  NativeCall& operator=(const NativeCall&) = delete;
  NativeCall(NativeCall&&) = delete;
  NativeCall& operator=(NativeCall&&) = delete;
  ~NativeCall() { --g_depth; }
};
template <typename Operation>
void Call(lapack_int* info, Operation operation) {
  // GESVX may call other wrapped families. Every nested argument, including
  // INFO itself, is passed unchanged; only the outer ASC output is faulted.
  if (g_depth != 0) {
    operation(info);
    return;
  }
  const NativeCall call;
  lapack_int actual = std::numeric_limits<lapack_int>::min();
  operation(&actual);
  ++g_observation.calls;
  g_observation.incoming = *info;
  g_observation.actual = actual;
  if (g_fault == Fault::kNone) {
    *info = actual;
  } else if (g_fault == Fault::kShortZero) {
    static_assert(std::endian::native == std::endian::little);
    const std::int32_t zero = 0;
    std::memcpy(info, &zero, sizeof(zero));
  }
  g_observation.outgoing = *info;
}
}  // namespace
namespace asc_lapack_test {
void SetLuAuxInfoFault(LuAuxInfoFault fault) {
  g_fault = fault;
  g_observation = {};
}
LuAuxInfoObservation ObserveLuAuxInfo() { return g_observation; }
}  // namespace asc_lapack_test
// Test-only ELF names; all hidden lengths and scalar signatures are checked
// against actual pinned declarations, never an invented raw ABI.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_sgecon_(const char*, const lapack_int*, const float*,
                    const lapack_int*, const float*, float*, float*,
                    lapack_int*, lapack_int*, std::size_t);
void __wrap_sgecon_(const char* norm, const lapack_int* n, const float* a,
                    const lapack_int* lda, const float* anorm, float* rcond,
                    float* work, lapack_int* auxiliary, lapack_int* info,
                    std::size_t norm_length) {
  Call(info, [&](lapack_int* actual_info) {
    __real_sgecon_(norm, n, a, lda, anorm, rcond, work, auxiliary, actual_info,
                   norm_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_sgecon_), decltype(LAPACK_sgecon_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgecon_), decltype(LAPACK_sgecon_base)>);
void __real_sgeequ_(const lapack_int*, const lapack_int*, const float*,
                    const lapack_int*, float*, float*, float*, float*, float*,
                    lapack_int*);
void __wrap_sgeequ_(const lapack_int* m, const lapack_int* n, const float* a,
                    const lapack_int* lda, float* rows, float* columns,
                    float* rowcnd, float* colcnd, float* amax,
                    lapack_int* info) {
  Call(info, [&](lapack_int* actual_info) {
    __real_sgeequ_(m, n, a, lda, rows, columns, rowcnd, colcnd, amax,
                   actual_info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_sgeequ_), decltype(LAPACK_sgeequ)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgeequ_), decltype(LAPACK_sgeequ)>);
void __real_sgeequb_(const lapack_int*, const lapack_int*, const float*,
                     const lapack_int*, float*, float*, float*, float*, float*,
                     lapack_int*);
void __wrap_sgeequb_(const lapack_int* m, const lapack_int* n, const float* a,
                     const lapack_int* lda, float* rows, float* columns,
                     float* rowcnd, float* colcnd, float* amax,
                     lapack_int* info) {
  Call(info, [&](lapack_int* actual_info) {
    __real_sgeequb_(m, n, a, lda, rows, columns, rowcnd, colcnd, amax,
                    actual_info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_sgeequb_), decltype(LAPACK_sgeequb)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgeequb_), decltype(LAPACK_sgeequb)>);
void __real_sgerfs_(const char*, const lapack_int*, const lapack_int*,
                    const float*, const lapack_int*, const float*,
                    const lapack_int*, const lapack_int*, const float*,
                    const lapack_int*, float*, const lapack_int*, float*,
                    float*, float*, lapack_int*, lapack_int*, std::size_t);
void __wrap_sgerfs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const float* a,
                    const lapack_int* lda, const float* af,
                    const lapack_int* ldaf, const lapack_int* pivots,
                    const float* b, const lapack_int* ldb, float* x,
                    const lapack_int* ldx, float* ferr, float* berr,
                    float* work, lapack_int* auxiliary, lapack_int* info,
                    std::size_t trans_length) {
  Call(info, [&](lapack_int* actual_info) {
    __real_sgerfs_(trans, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx,
                   ferr, berr, work, auxiliary, actual_info, trans_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_sgerfs_), decltype(LAPACK_sgerfs_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgerfs_), decltype(LAPACK_sgerfs_base)>);
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
  Call(info, [&](lapack_int* actual_info) {
    __real_sgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, pivots, equed, rows,
                   columns, b, ldb, x, ldx, rcond, ferr, berr, work, auxiliary,
                   actual_info, fact_length, trans_length, equed_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_sgesvx_), decltype(LAPACK_sgesvx_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgesvx_), decltype(LAPACK_sgesvx_base)>);
void __real_dgecon_(const char*, const lapack_int*, const double*,
                    const lapack_int*, const double*, double*, double*,
                    lapack_int*, lapack_int*, std::size_t);
void __wrap_dgecon_(const char* norm, const lapack_int* n, const double* a,
                    const lapack_int* lda, const double* anorm, double* rcond,
                    double* work, lapack_int* auxiliary, lapack_int* info,
                    std::size_t norm_length) {
  Call(info, [&](lapack_int* actual_info) {
    __real_dgecon_(norm, n, a, lda, anorm, rcond, work, auxiliary, actual_info,
                   norm_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_dgecon_), decltype(LAPACK_dgecon_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgecon_), decltype(LAPACK_dgecon_base)>);
void __real_dgeequ_(const lapack_int*, const lapack_int*, const double*,
                    const lapack_int*, double*, double*, double*, double*,
                    double*, lapack_int*);
void __wrap_dgeequ_(const lapack_int* m, const lapack_int* n, const double* a,
                    const lapack_int* lda, double* rows, double* columns,
                    double* rowcnd, double* colcnd, double* amax,
                    lapack_int* info) {
  Call(info, [&](lapack_int* actual_info) {
    __real_dgeequ_(m, n, a, lda, rows, columns, rowcnd, colcnd, amax,
                   actual_info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_dgeequ_), decltype(LAPACK_dgeequ)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgeequ_), decltype(LAPACK_dgeequ)>);
void __real_dgeequb_(const lapack_int*, const lapack_int*, const double*,
                     const lapack_int*, double*, double*, double*, double*,
                     double*, lapack_int*);
void __wrap_dgeequb_(const lapack_int* m, const lapack_int* n, const double* a,
                     const lapack_int* lda, double* rows, double* columns,
                     double* rowcnd, double* colcnd, double* amax,
                     lapack_int* info) {
  Call(info, [&](lapack_int* actual_info) {
    __real_dgeequb_(m, n, a, lda, rows, columns, rowcnd, colcnd, amax,
                    actual_info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_dgeequb_), decltype(LAPACK_dgeequb)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgeequb_), decltype(LAPACK_dgeequb)>);
void __real_dgerfs_(const char*, const lapack_int*, const lapack_int*,
                    const double*, const lapack_int*, const double*,
                    const lapack_int*, const lapack_int*, const double*,
                    const lapack_int*, double*, const lapack_int*, double*,
                    double*, double*, lapack_int*, lapack_int*, std::size_t);
void __wrap_dgerfs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const double* a,
                    const lapack_int* lda, const double* af,
                    const lapack_int* ldaf, const lapack_int* pivots,
                    const double* b, const lapack_int* ldb, double* x,
                    const lapack_int* ldx, double* ferr, double* berr,
                    double* work, lapack_int* auxiliary, lapack_int* info,
                    std::size_t trans_length) {
  Call(info, [&](lapack_int* actual_info) {
    __real_dgerfs_(trans, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx,
                   ferr, berr, work, auxiliary, actual_info, trans_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_dgerfs_), decltype(LAPACK_dgerfs_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgerfs_), decltype(LAPACK_dgerfs_base)>);
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
  Call(info, [&](lapack_int* actual_info) {
    __real_dgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, pivots, equed, rows,
                   columns, b, ldb, x, ldx, rcond, ferr, berr, work, auxiliary,
                   actual_info, fact_length, trans_length, equed_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_dgesvx_), decltype(LAPACK_dgesvx_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgesvx_), decltype(LAPACK_dgesvx_base)>);
void __real_cgecon_(const char*, const lapack_int*, const std::complex<float>*,
                    const lapack_int*, const float*, float*,
                    std::complex<float>*, float*, lapack_int*, std::size_t);
void __wrap_cgecon_(const char* norm, const lapack_int* n,
                    const std::complex<float>* a, const lapack_int* lda,
                    const float* anorm, float* rcond, std::complex<float>* work,
                    float* auxiliary, lapack_int* info,
                    std::size_t norm_length) {
  Call(info, [&](lapack_int* actual_info) {
    __real_cgecon_(norm, n, a, lda, anorm, rcond, work, auxiliary, actual_info,
                   norm_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_cgecon_), decltype(LAPACK_cgecon_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgecon_), decltype(LAPACK_cgecon_base)>);
void __real_cgeequ_(const lapack_int*, const lapack_int*,
                    const std::complex<float>*, const lapack_int*, float*,
                    float*, float*, float*, float*, lapack_int*);
void __wrap_cgeequ_(const lapack_int* m, const lapack_int* n,
                    const std::complex<float>* a, const lapack_int* lda,
                    float* rows, float* columns, float* rowcnd, float* colcnd,
                    float* amax, lapack_int* info) {
  Call(info, [&](lapack_int* actual_info) {
    __real_cgeequ_(m, n, a, lda, rows, columns, rowcnd, colcnd, amax,
                   actual_info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_cgeequ_), decltype(LAPACK_cgeequ)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgeequ_), decltype(LAPACK_cgeequ)>);
void __real_cgeequb_(const lapack_int*, const lapack_int*,
                     const std::complex<float>*, const lapack_int*, float*,
                     float*, float*, float*, float*, lapack_int*);
void __wrap_cgeequb_(const lapack_int* m, const lapack_int* n,
                     const std::complex<float>* a, const lapack_int* lda,
                     float* rows, float* columns, float* rowcnd, float* colcnd,
                     float* amax, lapack_int* info) {
  Call(info, [&](lapack_int* actual_info) {
    __real_cgeequb_(m, n, a, lda, rows, columns, rowcnd, colcnd, amax,
                    actual_info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_cgeequb_), decltype(LAPACK_cgeequb)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgeequb_), decltype(LAPACK_cgeequb)>);
void __real_cgerfs_(const char*, const lapack_int*, const lapack_int*,
                    const std::complex<float>*, const lapack_int*,
                    const std::complex<float>*, const lapack_int*,
                    const lapack_int*, const std::complex<float>*,
                    const lapack_int*, std::complex<float>*, const lapack_int*,
                    float*, float*, std::complex<float>*, float*, lapack_int*,
                    std::size_t);
void __wrap_cgerfs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<float>* a,
                    const lapack_int* lda, const std::complex<float>* af,
                    const lapack_int* ldaf, const lapack_int* pivots,
                    const std::complex<float>* b, const lapack_int* ldb,
                    std::complex<float>* x, const lapack_int* ldx, float* ferr,
                    float* berr, std::complex<float>* work, float* auxiliary,
                    lapack_int* info, std::size_t trans_length) {
  Call(info, [&](lapack_int* actual_info) {
    __real_cgerfs_(trans, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx,
                   ferr, berr, work, auxiliary, actual_info, trans_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_cgerfs_), decltype(LAPACK_cgerfs_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgerfs_), decltype(LAPACK_cgerfs_base)>);
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
  Call(info, [&](lapack_int* actual_info) {
    __real_cgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, pivots, equed, rows,
                   columns, b, ldb, x, ldx, rcond, ferr, berr, work, auxiliary,
                   actual_info, fact_length, trans_length, equed_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_cgesvx_), decltype(LAPACK_cgesvx_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgesvx_), decltype(LAPACK_cgesvx_base)>);
void __real_zgecon_(const char*, const lapack_int*, const std::complex<double>*,
                    const lapack_int*, const double*, double*,
                    std::complex<double>*, double*, lapack_int*, std::size_t);
void __wrap_zgecon_(const char* norm, const lapack_int* n,
                    const std::complex<double>* a, const lapack_int* lda,
                    const double* anorm, double* rcond,
                    std::complex<double>* work, double* auxiliary,
                    lapack_int* info, std::size_t norm_length) {
  Call(info, [&](lapack_int* actual_info) {
    __real_zgecon_(norm, n, a, lda, anorm, rcond, work, auxiliary, actual_info,
                   norm_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_zgecon_), decltype(LAPACK_zgecon_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgecon_), decltype(LAPACK_zgecon_base)>);
void __real_zgeequ_(const lapack_int*, const lapack_int*,
                    const std::complex<double>*, const lapack_int*, double*,
                    double*, double*, double*, double*, lapack_int*);
void __wrap_zgeequ_(const lapack_int* m, const lapack_int* n,
                    const std::complex<double>* a, const lapack_int* lda,
                    double* rows, double* columns, double* rowcnd,
                    double* colcnd, double* amax, lapack_int* info) {
  Call(info, [&](lapack_int* actual_info) {
    __real_zgeequ_(m, n, a, lda, rows, columns, rowcnd, colcnd, amax,
                   actual_info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_zgeequ_), decltype(LAPACK_zgeequ)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgeequ_), decltype(LAPACK_zgeequ)>);
void __real_zgeequb_(const lapack_int*, const lapack_int*,
                     const std::complex<double>*, const lapack_int*, double*,
                     double*, double*, double*, double*, lapack_int*);
void __wrap_zgeequb_(const lapack_int* m, const lapack_int* n,
                     const std::complex<double>* a, const lapack_int* lda,
                     double* rows, double* columns, double* rowcnd,
                     double* colcnd, double* amax, lapack_int* info) {
  Call(info, [&](lapack_int* actual_info) {
    __real_zgeequb_(m, n, a, lda, rows, columns, rowcnd, colcnd, amax,
                    actual_info);
  });
}
static_assert(
    std::is_same_v<decltype(__real_zgeequb_), decltype(LAPACK_zgeequb)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgeequb_), decltype(LAPACK_zgeequb)>);
void __real_zgerfs_(const char*, const lapack_int*, const lapack_int*,
                    const std::complex<double>*, const lapack_int*,
                    const std::complex<double>*, const lapack_int*,
                    const lapack_int*, const std::complex<double>*,
                    const lapack_int*, std::complex<double>*, const lapack_int*,
                    double*, double*, std::complex<double>*, double*,
                    lapack_int*, std::size_t);
void __wrap_zgerfs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<double>* a,
                    const lapack_int* lda, const std::complex<double>* af,
                    const lapack_int* ldaf, const lapack_int* pivots,
                    const std::complex<double>* b, const lapack_int* ldb,
                    std::complex<double>* x, const lapack_int* ldx,
                    double* ferr, double* berr, std::complex<double>* work,
                    double* auxiliary, lapack_int* info,
                    std::size_t trans_length) {
  Call(info, [&](lapack_int* actual_info) {
    __real_zgerfs_(trans, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx,
                   ferr, berr, work, auxiliary, actual_info, trans_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_zgerfs_), decltype(LAPACK_zgerfs_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgerfs_), decltype(LAPACK_zgerfs_base)>);
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
  Call(info, [&](lapack_int* actual_info) {
    __real_zgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, pivots, equed, rows,
                   columns, b, ldb, x, ldx, rcond, ferr, berr, work, auxiliary,
                   actual_info, fact_length, trans_length, equed_length);
  });
}
static_assert(
    std::is_same_v<decltype(__real_zgesvx_), decltype(LAPACK_zgesvx_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgesvx_), decltype(LAPACK_zgesvx_base)>);
}
// NOLINTEND(bugprone-reserved-identifier)
