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
#if ASC_LAPACK_INTEGER_BITS == 64
using Integer = long;  // NOLINT(google-runtime-int)
#else
using Integer = int;
#endif

// Hand-reviewed complete PPCON signatures, checked separately against GNU
// compiler emissions. Normalize only C pointee const annotation here.
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

template <typename T, typename Real, typename Auxiliary>
using Condition = void(char*, Integer*, T*, Real*, Real*, T*, Auxiliary*,
                       Integer*, std::size_t);
static_assert(
    kMatches<decltype(LAPACK_sppcon_base), Condition<float, float, Integer>>);
static_assert(
    kMatches<decltype(LAPACK_dppcon_base), Condition<double, double, Integer>>);
static_assert(kMatches<decltype(LAPACK_cppcon_base),
                       Condition<std::complex<float>, float, float>>);
static_assert(kMatches<decltype(LAPACK_zppcon_base),
                       Condition<std::complex<double>, double, double>>);
static_assert(std::is_same_v<lapack_int, Integer>);
static_assert(std::is_signed_v<lapack_int>);
static_assert(sizeof(lapack_int) * CHAR_BIT == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  std::printf(
      "4 complete packed Cholesky condition signatures passed; configured "
      "INTEGER bits=%d, one size_t CHARACTER length.\n",
      ASC_LAPACK_INTEGER_BITS);
  return 0;
}
