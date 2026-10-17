#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "src/dense/lapack/internal_indefinite.h"
#include "tests/dense/test_support.h"
#include "tests/dense_lapack/indefinite_packed_fixture.h"
#include "tests/dense_lapack/indefinite_packed_native.h"
#include "tests/dense_lapack/installed_lu/normal_return_guard.h"
#if defined(ASC_INVERSE_SSPTRI_HEADER)
#include <type_traits>
#endif
#include "asc/dense/blas.h"
#if defined(ASC_INVERSE_SSPTRI_HEADER)
#define ssptri_ emitted_ssptri
#include ASC_INVERSE_SSPTRI_HEADER
#undef ssptri_
#define dsptri_ emitted_dsptri
#include ASC_INVERSE_DSPTRI_HEADER
#undef dsptri_
#define csptri_ emitted_csptri
#include ASC_INVERSE_CSPTRI_HEADER
#undef csptri_
#define zsptri_ emitted_zsptri
#include ASC_INVERSE_ZSPTRI_HEADER
#undef zsptri_
#define chptri_ emitted_chptri
#include ASC_INVERSE_CHPTRI_HEADER
#undef chptri_
#define zhptri_ emitted_zhptri
#include ASC_INVERSE_ZHPTRI_HEADER
#undef zhptri_
#endif
namespace {
namespace packed = asc_packed_indefinite_test;

#if defined(ASC_INVERSE_SSPTRI_HEADER)
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
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_ssptri_base)>::Type,
                             typename Sig<decltype(emitted_ssptri)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_dsptri_base)>::Type,
                             typename Sig<decltype(emitted_dsptri)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_csptri_base)>::Type,
                             typename Sig<decltype(emitted_csptri)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zsptri_base)>::Type,
                             typename Sig<decltype(emitted_zsptri)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_chptri_base)>::Type,
                             typename Sig<decltype(emitted_chptri)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zhptri_base)>::Type,
                             typename Sig<decltype(emitted_zhptri)>::Type>);

#endif

template <typename T>
void Inverse(bool he, const char* triangle, const lapack_int* n, T* a,
             const lapack_int* p, T* work, lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssptri(triangle, n, a, p, work, info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsptri(triangle, n, a, p, work, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chptri(triangle, n, a, p, work, info);
    } else {
      LAPACK_csptri(triangle, n, a, p, work, info);
    }
  } else {
    if (he) {
      LAPACK_zhptri(triangle, n, a, p, work, info);
    } else {
      LAPACK_zsptri(triangle, n, a, p, work, info);
    }
  }
}
template <typename T>
void Case(packed::TestContext& test, bool he, char triangle, int order,
          int exponent, int kind) {
  packed::Sample<T> sample(order, he,
                           triangle == 'U' ? packed::kUpper : packed::kLower,
                           packed::kColumn, exponent);
  sample.Reset(kind);
  const lapack_int n = order;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 79;
  std::array<lapack_int, 68> p;
  p.fill(kGuard);
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  packed::Native(he, &triangle, &n, sample.a.data() + 1, p.data() + 1,
                 &info[1]);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  for (int i = 0; i < order; ++i) {
    sample.pivots[i + 1] = p[i + 1];
  }
  sample.Reconstruction(test);
  const auto old_p = p;
  std::array<T, 68> work;
  work.fill(packed::Value<T>(-103, 11));
  info[1] = std::numeric_limits<lapack_int>::min();
  Inverse(he, &triangle, &n, sample.a.data() + 1, p.data() + 1, work.data() + 1,
          &info[1]);
  ASC_DENSE_TEST_EQ(test, info[0], kGuard);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  ASC_DENSE_TEST_EQ(test, info[2], kGuard);
  ASC_DENSE_TEST_EQ(test, p, old_p);
  ASC_DENSE_TEST_EQ(test, work[0], packed::Value<T>(-103, 11));
  for (std::size_t i = order + 1; i < work.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, work[i], packed::Value<T>(-103, 11));
  }
  ASC_DENSE_TEST_EQ(test, sample.a[0], sample.original[0]);
  for (std::size_t i = 1 + order * (order + 1) / 2; i < sample.a.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, sample.a[i], sample.original[i]);
  }
  std::array<packed::Wide, 65 * 65> inverse{};
  for (int j = 0; j < order; ++j) {
    for (int i = 0; i < order; ++i) {
      if (sample.Selected(i, j)) {
        auto value = packed::ToWide(sample.a[sample.Offset(i, j)]);
        inverse[i * order + j] = value;
        inverse[j * order + i] = packed::Adjoint(value, he);
      }
    }
  }
  long double error = 0;
  long double norm_a = 0;
  long double norm_inverse = 0;
  for (int i = 0; i < order; ++i) {
    long double row_a = 0;
    long double row_inverse = 0;
    for (int j = 0; j < order; ++j) {
      row_a += std::abs(sample.full[i * order + j]);
      row_inverse += std::abs(inverse[i * order + j]);
      packed::Wide sum{};
      for (int k = 0; k < order; ++k) {
        sum += sample.full[i * order + k] * inverse[k * order + j];
      }
      const auto difference =
          std::abs(sum - packed::Wide{i == j ? 1.0L : 0.0L});
      ASC_DENSE_TEST_CHECK(test, std::isfinite(difference));
      error = std::max(error, difference);
    }
    norm_a = std::max(norm_a, row_a);
    norm_inverse = std::max(norm_inverse, row_inverse);
  }
  const long double bound =
      256 * std::max(order, 1) *
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() *
      std::max(1.0L, norm_a * norm_inverse);
  ASC_DENSE_TEST_CHECK(test, std::isfinite(norm_inverse) && error <= bound);
}
template <typename T>
int Run(packed::TestContext& test, bool he) {
  int count = 0;
  for (int n : {0, 1, 2, 5, 17, 65}) {
    for (char triangle : {'U', 'L'}) {
      for (int exponent : {-20, 0, 20}) {
        for (int kind : {0, 3}) {
          Case<T>(test, he, triangle, n, exponent, kind);
          ++count;
        }
      }
    }
  }
  return count;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  packed::TestContext test;
  int cases = Run<float>(test, false) + Run<double>(test, false);
  cases += Run<std::complex<float>>(test, false) +
           Run<std::complex<double>>(test, false);
  cases += Run<std::complex<float>>(test, true) +
           Run<std::complex<double>>(test, true);
  ASC_DENSE_TEST_EQ(test, cases, 432);
  const auto code = test.Finish();
  std::printf("Packed inverse native cases=%d failed=%d\n", cases, code);
  return code;
}
