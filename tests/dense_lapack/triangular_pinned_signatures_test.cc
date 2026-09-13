#include <climits>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"

#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#elif ASC_LAPACK_INTEGER_BITS != 32
#error "The pinned-signature probe requires the configured integer ABI."
#endif
#include <lapack.h>
#include <lapacke_config.h>

#include "internal_triangular_prototypes.h"

namespace {
using Integer = std::conditional_t<ASC_LAPACK_INTEGER_BITS == 64, std::int64_t,
                                   std::int32_t>;

// These hand-reviewed function types describe the 12 pinned triangular
// routines. They are not compiler emissions. Non-BIND(C) Fortran has no C
// pointee-const annotation: normalize only that annotation, retaining every
// representation type, pointer level, volatile qualifier, return type and
// argument count.
template <typename T>
struct PointeeUnqualified {
  using type = T;
};
template <typename T>
struct PointeeUnqualified<T*> {
  using type = std::remove_const_t<T>*;
};
template <typename T>
struct Signature;
template <typename Return, typename... Args>
struct Signature<Return(Args...)> {
  using type = Return(typename PointeeUnqualified<Args>::type...);
};
template <typename Actual, typename Expected>
constexpr bool kMatches =
    std::is_same_v<typename Signature<Actual>::type, Expected>;

template <typename T>
using Inverse = void(char*, char*, Integer*, T*, Integer*, Integer*,
                     std::size_t, std::size_t);
template <typename T>
using Solve = void(char*, char*, char*, Integer*, Integer*, T*, Integer*, T*,
                   Integer*, Integer*, std::size_t, std::size_t, std::size_t);
static_assert(kMatches<decltype(LAPACK_strtri_base), Inverse<float>>);
static_assert(kMatches<decltype(LAPACK_strti2_base), Inverse<float>>);
static_assert(kMatches<decltype(LAPACK_strtrs_base), Solve<float>>);
static_assert(kMatches<decltype(LAPACK_dtrtri_base), Inverse<double>>);
static_assert(kMatches<decltype(LAPACK_dtrti2_base), Inverse<double>>);
static_assert(kMatches<decltype(LAPACK_dtrtrs_base), Solve<double>>);
static_assert(
    kMatches<decltype(LAPACK_ctrtri_base), Inverse<std::complex<float>>>);
static_assert(
    kMatches<decltype(LAPACK_ctrti2_base), Inverse<std::complex<float>>>);
static_assert(
    kMatches<decltype(LAPACK_ctrtrs_base), Solve<std::complex<float>>>);
static_assert(
    kMatches<decltype(LAPACK_ztrtri_base), Inverse<std::complex<double>>>);
static_assert(
    kMatches<decltype(LAPACK_ztrti2_base), Inverse<std::complex<double>>>);
static_assert(
    kMatches<decltype(LAPACK_ztrtrs_base), Solve<std::complex<double>>>);
static_assert(std::is_same_v<lapack_int, Integer>);
static_assert(std::is_signed_v<lapack_int>);
static_assert(sizeof(lapack_int) * CHAR_BIT == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  std::printf(
      "12 complete triangular pinned-signature checks passed; configured "
      "INTEGER bits=%d, two/three size_t CHARACTER lengths; no WORK.\n",
      ASC_LAPACK_INTEGER_BITS);
  return 0;
}
