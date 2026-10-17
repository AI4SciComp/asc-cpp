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

namespace {

using Integer = std::conditional_t<ASC_LAPACK_INTEGER_BITS == 64, std::int64_t,
                                   std::int32_t>;

// These manually reviewed shapes describe the twenty pinned Fortran routines.
// They are not compiler emissions. Non-BIND(C) Fortran has no C pointee-const
// annotation; normalize only that annotation, retaining every representation
// type, pointer level, volatile qualifier, return type and trailing length.
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

template <typename T, typename Real>
using Gbequ = void(Integer* m, Integer* n, Integer* kl, Integer* ku, T* ab,
                   Integer* ldab, Real* rows, Real* columns,
                   Real* row_condition, Real* column_condition, Real* maximum,
                   Integer* info);

template <typename T, typename Real, typename Extra>
using Gbcon = void(char* norm, Integer* n, Integer* kl, Integer* ku, T* ab,
                   Integer* ldab, Integer* pivots, Real* a_norm,
                   Real* reciprocal_condition, T* work, Extra* extra,
                   Integer* info, std::size_t norm_length);

template <typename T, typename Real, typename Extra>
using Gbrfs = void(char* trans, Integer* n, Integer* kl, Integer* ku,
                   Integer* rhs_count, T* ab, Integer* ldab, T* factors,
                   Integer* factor_ld, Integer* pivots, T* b, Integer* ldb,
                   T* x, Integer* ldx, Real* ferr, Real* berr, T* work,
                   Extra* extra, Integer* info, std::size_t trans_length);

template <typename T>
using Gbsv = void(Integer* n, Integer* kl, Integer* ku, Integer* rhs_count,
                  T* ab, Integer* ldab, Integer* pivots, T* b, Integer* ldb,
                  Integer* info);

template <typename T, typename Real, typename Extra>
using Gbsvx = void(char* fact, char* trans, Integer* n, Integer* kl,
                   Integer* ku, Integer* rhs_count, T* ab, Integer* ldab,
                   T* factors, Integer* factor_ld, Integer* pivots, char* equed,
                   Real* rows, Real* columns, T* b, Integer* ldb, T* x,
                   Integer* ldx, Real* reciprocal_condition, Real* ferr,
                   Real* berr, T* work, Extra* extra, Integer* info,
                   std::size_t fact_length, std::size_t trans_length,
                   std::size_t equed_length);

static_assert(kMatches<decltype(LAPACK_sgbequ), Gbequ<float, float>>);
static_assert(kMatches<decltype(LAPACK_dgbequ), Gbequ<double, double>>);
static_assert(
    kMatches<decltype(LAPACK_cgbequ), Gbequ<std::complex<float>, float>>);
static_assert(
    kMatches<decltype(LAPACK_zgbequ), Gbequ<std::complex<double>, double>>);

static_assert(
    kMatches<decltype(LAPACK_sgbcon_base), Gbcon<float, float, Integer>>);
static_assert(
    kMatches<decltype(LAPACK_dgbcon_base), Gbcon<double, double, Integer>>);
static_assert(kMatches<decltype(LAPACK_cgbcon_base),
                       Gbcon<std::complex<float>, float, float>>);
static_assert(kMatches<decltype(LAPACK_zgbcon_base),
                       Gbcon<std::complex<double>, double, double>>);

static_assert(
    kMatches<decltype(LAPACK_sgbrfs_base), Gbrfs<float, float, Integer>>);
static_assert(
    kMatches<decltype(LAPACK_dgbrfs_base), Gbrfs<double, double, Integer>>);
static_assert(kMatches<decltype(LAPACK_cgbrfs_base),
                       Gbrfs<std::complex<float>, float, float>>);
static_assert(kMatches<decltype(LAPACK_zgbrfs_base),
                       Gbrfs<std::complex<double>, double, double>>);

static_assert(kMatches<decltype(LAPACK_sgbsv), Gbsv<float>>);
static_assert(kMatches<decltype(LAPACK_dgbsv), Gbsv<double>>);
static_assert(kMatches<decltype(LAPACK_cgbsv), Gbsv<std::complex<float>>>);
static_assert(kMatches<decltype(LAPACK_zgbsv), Gbsv<std::complex<double>>>);

static_assert(
    kMatches<decltype(LAPACK_sgbsvx_base), Gbsvx<float, float, Integer>>);
static_assert(
    kMatches<decltype(LAPACK_dgbsvx_base), Gbsvx<double, double, Integer>>);
static_assert(kMatches<decltype(LAPACK_cgbsvx_base),
                       Gbsvx<std::complex<float>, float, float>>);
static_assert(kMatches<decltype(LAPACK_zgbsvx_base),
                       Gbsvx<std::complex<double>, double, double>>);

static_assert(std::is_same_v<lapack_int, Integer>);
static_assert(std::is_signed_v<lapack_int>);
static_assert(sizeof(lapack_int) * CHAR_BIT == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);
static_assert(std::is_same_v<FORTRAN_STRLEN, std::size_t>);
static_assert(std::is_unsigned_v<FORTRAN_STRLEN>);

}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  std::printf(
      "20 complete GB20 pinned-signature checks passed; configured INTEGER "
      "bits=%d, trailing CHARACTER length bytes=%zu.\n",
      ASC_LAPACK_INTEGER_BITS, sizeof(FORTRAN_STRLEN));
  return 0;
}
