#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#if defined(ASC_SOLVE_SSPTRS_HEADER)
#include <type_traits>
#endif
#include "asc/dense/blas.h"
#include "src/dense/lapack/internal_indefinite.h"
#include "tests/dense/test_support.h"
#include "tests/dense_lapack/indefinite_packed_fixture.h"
#include "tests/dense_lapack/indefinite_packed_native.h"
#include "tests/dense_lapack/indefinite_packed_solve_native.h"
#include "tests/dense_lapack/installed_lu/normal_return_guard.h"

#if defined(ASC_SOLVE_SSPTRS_HEADER)
#define ssptrs_ emitted_ssptrs
#include ASC_SOLVE_SSPTRS_HEADER
#undef ssptrs_
#define dsptrs_ emitted_dsptrs
#include ASC_SOLVE_DSPTRS_HEADER
#undef dsptrs_
#define csptrs_ emitted_csptrs
#include ASC_SOLVE_CSPTRS_HEADER
#undef csptrs_
#define zsptrs_ emitted_zsptrs
#include ASC_SOLVE_ZSPTRS_HEADER
#undef zsptrs_
#define chptrs_ emitted_chptrs
#include ASC_SOLVE_CHPTRS_HEADER
#undef chptrs_
#define zhptrs_ emitted_zhptrs
#include ASC_SOLVE_ZHPTRS_HEADER
#undef zhptrs_

#endif

namespace {
namespace packed = asc_packed_indefinite_test;
#if defined(ASC_SOLVE_SSPTRS_HEADER)
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
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_ssptrs_base)>::Type,
                             typename Sig<decltype(emitted_ssptrs)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_dsptrs_base)>::Type,
                             typename Sig<decltype(emitted_dsptrs)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_csptrs_base)>::Type,
                             typename Sig<decltype(emitted_csptrs)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zsptrs_base)>::Type,
                             typename Sig<decltype(emitted_zsptrs)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_chptrs_base)>::Type,
                             typename Sig<decltype(emitted_chptrs)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zhptrs_base)>::Type,
                             typename Sig<decltype(emitted_zhptrs)>::Type>);

#endif

template <typename T>
T Solution(int i, int j) {
  return packed::Value<T>(static_cast<long double>((i + j) % 3 + 1) / 4,
                          static_cast<long double>((2 * i + j) % 3 - 1) / 8);
}

template <typename T>
void Verify(packed::TestContext& test, const packed::Sample<T>& sample,
            int nrhs, const std::array<T, 128>& rhs,
            const std::array<T, 128>& original) {
  const int n = sample.n;
  const int ldb = n + 1;
  auto expected_padding = original;
  long double norm = 0;
  long double error = 0;
  long double residual = 0;
  for (int j = 0; j < nrhs; ++j) {
    for (int i = 0; i < n; ++i) {
      expected_padding[1 + j * ldb + i] = rhs[1 + j * ldb + i];
      const auto actual = packed::ToWide(rhs[1 + j * ldb + i]);
      const auto difference =
          std::abs(actual - packed::ToWide(Solution<T>(i, j)));
      ASC_DENSE_TEST_CHECK(test, std::isfinite(difference));
      error = std::max(error, difference);
      packed::Wide product{};
      long double row_norm = 0;
      for (int k = 0; k < n; ++k) {
        product +=
            sample.full[i * n + k] * packed::ToWide(rhs[1 + j * ldb + k]);
        row_norm += std::abs(sample.full[i * n + k]);
      }
      const auto difference_rhs =
          std::abs(product - packed::ToWide(original[1 + j * ldb + i]));
      ASC_DENSE_TEST_CHECK(test, std::isfinite(difference_rhs));
      residual = std::max(residual, difference_rhs);
      norm = std::max(norm, row_norm);
    }
  }
  const auto bound = 256 * std::max(n, 1) *
                     std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  ASC_DENSE_TEST_CHECK(test, error <= bound);
  ASC_DENSE_TEST_CHECK(test, residual <= bound * norm);
  ASC_DENSE_TEST_CHECK(
      test,
      packed::EqualBytes(rhs.data(), expected_padding.data(), sizeof(rhs)));
}

template <typename T>
void Case(packed::TestContext& test, bool he, char triangle, int order,
          int columns, int exponent, int kind) {
  packed::Sample<T> sample(order, he,
                           triangle == 'U' ? packed::kUpper : packed::kLower,
                           packed::kColumn, exponent);
  sample.Reset(kind);
  const lapack_int n = order;
  const lapack_int nrhs = columns;
  const lapack_int ldb = order + 1;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 73;
  std::array<lapack_int, 32> pivots;
  pivots.fill(kGuard);
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  packed::Native(he, &triangle, &n, sample.a.data() + 1, pivots.data() + 1,
                 &info[1]);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  for (int i = 0; i < order; ++i) {
    sample.pivots[i + 1] = pivots[i + 1];
  }
  sample.Reconstruction(test);
  const auto factor_before = sample.a;
  const auto pivots_before = pivots;
  std::array<T, 128> rhs;
  rhs.fill(packed::Value<T>(-97, 13));
  for (int j = 0; j < columns; ++j) {
    for (int i = 0; i < order; ++i) {
      packed::Wide sum{};
      for (int k = 0; k < order; ++k) {
        sum += sample.full[i * order + k] * packed::ToWide(Solution<T>(k, j));
      }
      rhs[1 + j * ldb + i] = packed::Value<T>(sum.real(), sum.imag());
    }
  }
  const auto rhs_before = rhs;
  info[1] = std::numeric_limits<lapack_int>::min();
  asc_packed_solve_test::Native(he, &triangle, &n, &nrhs, sample.a.data() + 1,
                                pivots.data() + 1, rhs.data() + 1, &ldb,
                                &info[1]);
  ASC_DENSE_TEST_EQ(test, info[0], kGuard);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  ASC_DENSE_TEST_EQ(test, info[2], kGuard);
  ASC_DENSE_TEST_EQ(test, pivots, pivots_before);
  ASC_DENSE_TEST_CHECK(test,
                       packed::EqualBytes(sample.a.data(), factor_before.data(),
                                          sizeof(sample.a)));
  ASC_DENSE_TEST_EQ(test, pivots.front(), kGuard);
  for (std::size_t i = order + 1; i < pivots.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, pivots[i], kGuard);
  }
  Verify(test, sample, columns, rhs, rhs_before);
}

template <typename T>
int Run(packed::TestContext& test, bool he) {
  int cases = 0;
  for (int n : {0, 1, 2, 5, 17}) {
    for (char triangle : {'U', 'L'}) {
      for (int nrhs : {0, 1, 3}) {
        for (int exponent : {-20, 0, 20}) {
          for (int kind : {0, 3}) {
            Case<T>(test, he, triangle, n, nrhs, exponent, kind);
            ++cases;
          }
        }
      }
    }
  }
  return cases;
}
}  // namespace

int main() {
  asc_lapack_test::NormalReturnGuard normal_return;
  packed::TestContext test;
  int cases = Run<float>(test, false) + Run<double>(test, false);
  cases += Run<std::complex<float>>(test, false) +
           Run<std::complex<double>>(test, false);
  cases += Run<std::complex<float>>(test, true) +
           Run<std::complex<double>>(test, true);
  ASC_DENSE_TEST_EQ(test, cases, 1080);
  const int code = test.Finish();
  std::printf("Packed solve native cases=%d failed=%d\n", cases, code);
  return code;
}
