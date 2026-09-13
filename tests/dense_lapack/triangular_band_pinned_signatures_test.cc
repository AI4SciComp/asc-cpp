#include <climits>
#include <complex>
#include <cstddef>
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

namespace {
// Actual GNU11 compiler emissions use these exact primitive INTEGER types.
#if ASC_LAPACK_INTEGER_BITS == 64
using Integer = long;  // NOLINT(google-runtime-int)
#else
using Integer = int;
#endif

// These hand-reviewed function types describe the four pinned triangular
// band solve routines. They are not compiler emissions.
// Non-BIND(C) Fortran has no C pointee-const annotation: normalize only that
// annotation, retaining every representation type, pointer level, volatile
// qualifier, return type and argument count.
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
using Solve = void(char*, char*, char*, Integer*, Integer*, Integer*, T*,
                   Integer*, T*, Integer*, Integer*, std::size_t, std::size_t,
                   std::size_t);
static_assert(kMatches<decltype(LAPACK_stbtrs_base), Solve<float>>);
static_assert(kMatches<decltype(LAPACK_dtbtrs_base), Solve<double>>);
static_assert(
    kMatches<decltype(LAPACK_ctbtrs_base), Solve<std::complex<float>>>);
static_assert(
    kMatches<decltype(LAPACK_ztbtrs_base), Solve<std::complex<double>>>);
static_assert(std::is_same_v<lapack_int, Integer>);
static_assert(std::is_signed_v<lapack_int>);
static_assert(sizeof(lapack_int) * CHAR_BIT == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  std::printf(
      "4 complete triangular band solve signatures passed; configured "
      "INTEGER bits=%d, exact primitive types and three size_t CHARACTER "
      "lengths.\n",
      ASC_LAPACK_INTEGER_BITS);
  return 0;
}
