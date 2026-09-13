#include "indefinite_packed_solve_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace {
using asc_packed_solve_fault_test::Fault;
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
bool g_seed = false;
std::int64_t g_native = 0;
std::int64_t g_published = 0;
template <typename Operation>
void Invoke(lapack_int* info, Operation operation) {
  ++g_calls;
  g_seed = *info == std::numeric_limits<lapack_int>::min();
  lapack_int native = std::numeric_limits<lapack_int>::min();
  operation(&native);
  g_native = native;
  switch (g_fault) {
    case Fault::kPass:
      *info = native;
      break;
    case Fault::kOmitInfo:
      break;
    case Fault::kWrite16BitInfo: {
      const auto partial = static_cast<std::int16_t>(native);
      std::memcpy(info, &partial, sizeof(partial));
      break;
    }
    case Fault::kWrite32BitInfo: {
      const auto partial = static_cast<std::int32_t>(native);
      std::memcpy(info, &partial, sizeof(partial));
      break;
    }
    case Fault::kNegativeInfo:
      *info = -3;
      break;
    case Fault::kPositiveInfo:
      *info = 1;
      break;
    case Fault::kMaximumInfo:
      *info = std::numeric_limits<lapack_int>::max();
      break;
  }
  g_published = *info;
}
}  // namespace
namespace asc_packed_solve_fault_test {
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_seed = false;
  g_native = 0;
  g_published = 0;
}
std::size_t Calls() { return g_calls; }
bool SeedWasFullWidth() { return g_seed; }
std::int64_t LastNativeInfo() { return g_native; }
std::int64_t LastPublishedInfo() { return g_published; }
}  // namespace asc_packed_solve_fault_test
// GNU ld requires the exact foreign names. Only the selected solve is wrapped.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssptrs_base) __real_ssptrs_;
void __wrap_ssptrs_(const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, const float* a,
                    const lapack_int* pivots, float* b, const lapack_int* ldb,
                    lapack_int* info, std::size_t length) {
  Invoke(info, [&](lapack_int* native_info) {
    __real_ssptrs_(triangle, n, nrhs, a, pivots, b, ldb, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssptrs_base), decltype(__wrap_ssptrs_)>);
decltype(LAPACK_dsptrs_base) __real_dsptrs_;
void __wrap_dsptrs_(const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, const double* a,
                    const lapack_int* pivots, double* b, const lapack_int* ldb,
                    lapack_int* info, std::size_t length) {
  Invoke(info, [&](lapack_int* native_info) {
    __real_dsptrs_(triangle, n, nrhs, a, pivots, b, ldb, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsptrs_base), decltype(__wrap_dsptrs_)>);
decltype(LAPACK_csptrs_base) __real_csptrs_;
void __wrap_csptrs_(const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_float* a,
                    const lapack_int* pivots, lapack_complex_float* b,
                    const lapack_int* ldb, lapack_int* info,
                    std::size_t length) {
  Invoke(info, [&](lapack_int* native_info) {
    __real_csptrs_(triangle, n, nrhs, a, pivots, b, ldb, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csptrs_base), decltype(__wrap_csptrs_)>);
decltype(LAPACK_zsptrs_base) __real_zsptrs_;
void __wrap_zsptrs_(const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_double* a,
                    const lapack_int* pivots, lapack_complex_double* b,
                    const lapack_int* ldb, lapack_int* info,
                    std::size_t length) {
  Invoke(info, [&](lapack_int* native_info) {
    __real_zsptrs_(triangle, n, nrhs, a, pivots, b, ldb, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsptrs_base), decltype(__wrap_zsptrs_)>);
decltype(LAPACK_chptrs_base) __real_chptrs_;
void __wrap_chptrs_(const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_float* a,
                    const lapack_int* pivots, lapack_complex_float* b,
                    const lapack_int* ldb, lapack_int* info,
                    std::size_t length) {
  Invoke(info, [&](lapack_int* native_info) {
    __real_chptrs_(triangle, n, nrhs, a, pivots, b, ldb, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chptrs_base), decltype(__wrap_chptrs_)>);
decltype(LAPACK_zhptrs_base) __real_zhptrs_;
void __wrap_zhptrs_(const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_double* a,
                    const lapack_int* pivots, lapack_complex_double* b,
                    const lapack_int* ldb, lapack_int* info,
                    std::size_t length) {
  Invoke(info, [&](lapack_int* native_info) {
    __real_zhptrs_(triangle, n, nrhs, a, pivots, b, ldb, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhptrs_base), decltype(__wrap_zhptrs_)>);
}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier)
