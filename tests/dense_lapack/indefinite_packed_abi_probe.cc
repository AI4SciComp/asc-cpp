#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <span>
#if defined(ASC_PACKED_SSPTRF_HEADER)
#include <type_traits>
#endif

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
#include "indefinite_packed_fixture.h"
#include "indefinite_packed_native.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"
#if defined(ASC_PACKED_SSPTRF_HEADER)
#define ssptrf_ emitted_ssptrf
#include ASC_PACKED_SSPTRF_HEADER
#undef ssptrf_
#endif
#if defined(ASC_PACKED_DSPTRF_HEADER)
#define dsptrf_ emitted_dsptrf
#include ASC_PACKED_DSPTRF_HEADER
#undef dsptrf_
#endif
#if defined(ASC_PACKED_CSPTRF_HEADER)
#define csptrf_ emitted_csptrf
#include ASC_PACKED_CSPTRF_HEADER
#undef csptrf_
#endif
#if defined(ASC_PACKED_ZSPTRF_HEADER)
#define zsptrf_ emitted_zsptrf
#include ASC_PACKED_ZSPTRF_HEADER
#undef zsptrf_
#endif
#if defined(ASC_PACKED_CHPTRF_HEADER)
#define chptrf_ emitted_chptrf
#include ASC_PACKED_CHPTRF_HEADER
#undef chptrf_
#endif
#if defined(ASC_PACKED_ZHPTRF_HEADER)
#define zhptrf_ emitted_zhptrf
#include ASC_PACKED_ZHPTRF_HEADER
#undef zhptrf_
#endif
namespace {
#if defined(ASC_PACKED_SSPTRF_HEADER)
template <typename T>
struct Arg {
  using Type = T;
};
template <typename T>
struct Arg<T*> {
  using Type = std::remove_const_t<T>*;
};
template <typename T>
struct Sig;
template <typename R, typename... A>
struct Sig<R(A...)> {
  using Type = R(typename Arg<A>::Type...);
};
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_ssptrf_base)>::Type,
                             typename Sig<decltype(emitted_ssptrf)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_dsptrf_base)>::Type,
                             typename Sig<decltype(emitted_dsptrf)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_csptrf_base)>::Type,
                             typename Sig<decltype(emitted_csptrf)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zsptrf_base)>::Type,
                             typename Sig<decltype(emitted_zsptrf)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_chptrf_base)>::Type,
                             typename Sig<decltype(emitted_chptrf)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zhptrf_base)>::Type,
                             typename Sig<decltype(emitted_zhptrf)>::Type>);
#endif
namespace fixture = asc_packed_indefinite_test;
constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;
template <typename T>
void Probe(fixture::TestContext& test, bool he, char triangle, int order,
           int exponent, int kind) {
  fixture::Sample<T> sample(order, he,
                            triangle == 'U' ? fixture::kUpper : fixture::kLower,
                            fixture::kColumn, exponent);
  sample.Reset(kind);
  std::array<char, 3> uplo{'a', triangle, 'z'};
  std::array<lapack_int, 3> n{kGuard, order, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  std::array<lapack_int, 69> pivots;
  pivots.fill(kGuard);
  for (int i = 0; i < order; ++i) {
    pivots[static_cast<std::size_t>(i) + 1] =
        std::numeric_limits<lapack_int>::min();
  }
  const auto before_uplo = uplo;
  const auto before_n = n;
  auto normalized = sample;
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      for (int i = 0; i < order; ++i) {
        normalized.a[normalized.Offset(i, i)].imag(0);
      }
    }
  }
  auto normalized_pivots = pivots;
  lapack_int normalized_info = std::numeric_limits<lapack_int>::min();
  fixture::Native(he, &uplo[1], &n[1], sample.a.data() + 1, pivots.data() + 1,
                  &info[1]);
  fixture::Native(he, &triangle, &n[1], normalized.a.data() + 1,
                  normalized_pivots.data() + 1, &normalized_info);
  ASC_DENSE_TEST_EQ(test, uplo, before_uplo);
  ASC_DENSE_TEST_EQ(test, n, before_n);
  ASC_DENSE_TEST_EQ(test, info.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, info.back(), kGuard);
  ASC_DENSE_TEST_EQ(test, pivots.front(), kGuard);
  for (std::size_t i = static_cast<std::size_t>(order) + 1; i < pivots.size();
       ++i) {
    ASC_DENSE_TEST_EQ(test, pivots[i], kGuard);
  }
  ASC_DENSE_TEST_EQ(test, info[1], normalized_info);
  ASC_DENSE_TEST_EQ(test, pivots, normalized_pivots);
  ASC_DENSE_TEST_CHECK(test,
                       fixture::EqualBytes(sample.a.data(), normalized.a.data(),
                                           sizeof(sample.a)));
  const bool valid =
      info[1] >= 0 && info[1] <= order &&
      asc::internal_indefinite::Paired(
          std::span<const lapack_int>(pivots.data() + 1,
                                      static_cast<std::size_t>(order)),
          order, sample.triangle)
          .ok();
  ASC_DENSE_TEST_CHECK(test, valid);
  if (valid) {
    if (kind == 1 && order > 0) {
      ASC_DENSE_TEST_EQ(test, info[1], triangle == 'U' ? order : 1);
    } else if (kind == 2 && order > 0) {
      ASC_DENSE_TEST_CHECK(test, info[1] > 0);
    } else {
      ASC_DENSE_TEST_EQ(test, info[1], 0);
    }
    for (int i = 0; i < order; ++i) {
      sample.pivots[static_cast<std::size_t>(i) + 1] =
          pivots[static_cast<std::size_t>(i) + 1];
    }
    sample.Reconstruction(test);
  }
  sample.Guards(test);
  std::printf(
      "Packed native real_bytes=%zu complex=%d he=%d tri=%c n=%d scale=%d "
      "kind=%d INFO=%lld\n",
      sizeof(asc::DenseBlasRealType<T>),
      static_cast<int>(asc::DenseBlasComplex<T>), static_cast<int>(he),
      triangle, order, exponent, kind, static_cast<long long>(info[1]));
}
template <typename T>
void Run(fixture::TestContext& test, bool he, int& cases) {
  for (char tri : {'U', 'L'}) {
    for (int order : {0, 1, 2, 3, 4, 5, 8, 17, 65}) {
      for (int exponent : {-20, 0, 20}) {
        for (int kind : {0, 1, 2, 3}) {
          Probe<T>(test, he, tri, order, exponent, kind);
          ++cases;
        }
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  fixture::TestContext test;
  int cases = 0;
  Run<float>(test, false, cases);
  Run<double>(test, false, cases);
  Run<std::complex<float>>(test, false, cases);
  Run<std::complex<double>>(test, false, cases);
  Run<std::complex<float>>(test, true, cases);
  Run<std::complex<double>>(test, true, cases);
  std::printf("Packed native cases=%d failed=%d\n", cases, test.Finish());
  return test.Finish();
}
