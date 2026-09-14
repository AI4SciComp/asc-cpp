#include "lu_helpers_metadata_probe.h"

#include <complex>
#include <cstddef>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
namespace asc_helpers_metadata {
namespace {
thread_local Calls g_calls;
}  // namespace
void Reset() { g_calls = {}; }
Calls Read() { return g_calls; }
// Exact private GNU ELF entry observation; no return-value or numeric mutation.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_slaswp) __real_slaswp_;
extern "C" void __wrap_slaswp_(const lapack_int* n, float* a,
                               const lapack_int* ld, const lapack_int* first,
                               const lapack_int* last, const lapack_int* pivots,
                               const lapack_int* increment) {
  ++g_calls.swap;
  __real_slaswp_(n, a, ld, first, last, pivots, increment);
}
static_assert(
    std::is_same_v<decltype(LAPACK_slaswp), decltype(__wrap_slaswp_)>);
// LAQGE signatures retain the existing compiler-emitted scalar and hidden
// length ABI.
extern "C" void __real_slaqge_(lapack_int* m, lapack_int* n, float* a,
                               lapack_int* ld, float* rows, float* columns,
                               float* row_condition, float* column_condition,
                               float* maximum, char* applied,
                               std::size_t length);
extern "C" void __wrap_slaqge_(lapack_int* m, lapack_int* n, float* a,
                               lapack_int* ld, float* rows, float* columns,
                               float* row_condition, float* column_condition,
                               float* maximum, char* applied,
                               std::size_t length) {
  ++g_calls.scale;
  __real_slaqge_(m, n, a, ld, rows, columns, row_condition, column_condition,
                 maximum, applied, length);
}
extern "C" decltype(LAPACK_dlaswp) __real_dlaswp_;
extern "C" void __wrap_dlaswp_(const lapack_int* n, double* a,
                               const lapack_int* ld, const lapack_int* first,
                               const lapack_int* last, const lapack_int* pivots,
                               const lapack_int* increment) {
  ++g_calls.swap;
  __real_dlaswp_(n, a, ld, first, last, pivots, increment);
}
static_assert(
    std::is_same_v<decltype(LAPACK_dlaswp), decltype(__wrap_dlaswp_)>);
// LAQGE signatures retain the existing compiler-emitted scalar and hidden
// length ABI.
extern "C" void __real_dlaqge_(lapack_int* m, lapack_int* n, double* a,
                               lapack_int* ld, double* rows, double* columns,
                               double* row_condition, double* column_condition,
                               double* maximum, char* applied,
                               std::size_t length);
extern "C" void __wrap_dlaqge_(lapack_int* m, lapack_int* n, double* a,
                               lapack_int* ld, double* rows, double* columns,
                               double* row_condition, double* column_condition,
                               double* maximum, char* applied,
                               std::size_t length) {
  ++g_calls.scale;
  __real_dlaqge_(m, n, a, ld, rows, columns, row_condition, column_condition,
                 maximum, applied, length);
}
extern "C" decltype(LAPACK_claswp) __real_claswp_;
extern "C" void __wrap_claswp_(const lapack_int* n, std::complex<float>* a,
                               const lapack_int* ld, const lapack_int* first,
                               const lapack_int* last, const lapack_int* pivots,
                               const lapack_int* increment) {
  ++g_calls.swap;
  __real_claswp_(n, a, ld, first, last, pivots, increment);
}
static_assert(
    std::is_same_v<decltype(LAPACK_claswp), decltype(__wrap_claswp_)>);
// LAQGE signatures retain the existing compiler-emitted scalar and hidden
// length ABI.
extern "C" void __real_claqge_(lapack_int* m, lapack_int* n,
                               std::complex<float>* a, lapack_int* ld,
                               float* rows, float* columns,
                               float* row_condition, float* column_condition,
                               float* maximum, char* applied,
                               std::size_t length);
extern "C" void __wrap_claqge_(lapack_int* m, lapack_int* n,
                               std::complex<float>* a, lapack_int* ld,
                               float* rows, float* columns,
                               float* row_condition, float* column_condition,
                               float* maximum, char* applied,
                               std::size_t length) {
  ++g_calls.scale;
  __real_claqge_(m, n, a, ld, rows, columns, row_condition, column_condition,
                 maximum, applied, length);
}
extern "C" decltype(LAPACK_zlaswp) __real_zlaswp_;
extern "C" void __wrap_zlaswp_(const lapack_int* n, std::complex<double>* a,
                               const lapack_int* ld, const lapack_int* first,
                               const lapack_int* last, const lapack_int* pivots,
                               const lapack_int* increment) {
  ++g_calls.swap;
  __real_zlaswp_(n, a, ld, first, last, pivots, increment);
}
static_assert(
    std::is_same_v<decltype(LAPACK_zlaswp), decltype(__wrap_zlaswp_)>);
// LAQGE signatures retain the existing compiler-emitted scalar and hidden
// length ABI.
extern "C" void __real_zlaqge_(lapack_int* m, lapack_int* n,
                               std::complex<double>* a, lapack_int* ld,
                               double* rows, double* columns,
                               double* row_condition, double* column_condition,
                               double* maximum, char* applied,
                               std::size_t length);
extern "C" void __wrap_zlaqge_(lapack_int* m, lapack_int* n,
                               std::complex<double>* a, lapack_int* ld,
                               double* rows, double* columns,
                               double* row_condition, double* column_condition,
                               double* maximum, char* applied,
                               std::size_t length) {
  ++g_calls.scale;
  __real_zlaqge_(m, n, a, ld, rows, columns, row_condition, column_condition,
                 maximum, applied, length);
}
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_helpers_metadata
