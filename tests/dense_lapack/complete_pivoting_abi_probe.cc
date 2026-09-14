#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#if defined(ASC_COMPLETE_SGETC2_HEADER)
#include <cstdint>  // IWYU pragma: keep; emitted native integer spellings.
#endif
#include <cstdio>
#include <limits>
#include <span>
#include <type_traits>

#include "../../src/dense/lapack/internal_complete_pivoting_prototypes.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_COMPLETE_SGETC2_HEADER)
#define sgetc2_ emitted_sgetc2
#include ASC_COMPLETE_SGETC2_HEADER
#undef sgetc2_
#define sgesc2_ emitted_sgesc2
#include ASC_COMPLETE_SGESC2_HEADER
#undef sgesc2_
#define dgetc2_ emitted_dgetc2
#include ASC_COMPLETE_DGETC2_HEADER
#undef dgetc2_
#define dgesc2_ emitted_dgesc2
#include ASC_COMPLETE_DGESC2_HEADER
#undef dgesc2_
#define cgetc2_ emitted_cgetc2
#include ASC_COMPLETE_CGETC2_HEADER
#undef cgetc2_
#define cgesc2_ emitted_cgesc2
#include ASC_COMPLETE_CGESC2_HEADER
#undef cgesc2_
#define zgetc2_ emitted_zgetc2
#include ASC_COMPLETE_ZGETC2_HEADER
#undef zgetc2_
#define zgesc2_ emitted_zgesc2
#include ASC_COMPLETE_ZGESC2_HEADER
#undef zgesc2_
#endif

namespace {
using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
#if defined(ASC_COMPLETE_SGETC2_HEADER)
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
static_assert(std::is_same_v<Sig<decltype(sgetc2_)>::Type,
                             Sig<decltype(emitted_sgetc2)>::Type>);
static_assert(std::is_same_v<Sig<decltype(sgesc2_)>::Type,
                             Sig<decltype(emitted_sgesc2)>::Type>);
static_assert(std::is_same_v<Sig<decltype(dgetc2_)>::Type,
                             Sig<decltype(emitted_dgetc2)>::Type>);
static_assert(std::is_same_v<Sig<decltype(dgesc2_)>::Type,
                             Sig<decltype(emitted_dgesc2)>::Type>);
static_assert(std::is_same_v<Sig<decltype(cgetc2_)>::Type,
                             Sig<decltype(emitted_cgetc2)>::Type>);
static_assert(std::is_same_v<Sig<decltype(cgesc2_)>::Type,
                             Sig<decltype(emitted_cgesc2)>::Type>);
static_assert(std::is_same_v<Sig<decltype(zgetc2_)>::Type,
                             Sig<decltype(emitted_zgetc2)>::Type>);
static_assert(std::is_same_v<Sig<decltype(zgesc2_)>::Type,
                             Sig<decltype(emitted_zgesc2)>::Type>);
#endif
template <typename T>
bool Same(const T& a, const T& b) {
  const auto x = std::as_bytes(std::span(&a, 1));
  const auto y = std::as_bytes(std::span(&b, 1));
  return std::equal(x.begin(), x.end(), y.begin());
}
template <typename T>
T Value(long double real, long double imaginary = 0) {
  using R = asc::DenseBlasRealType<T>;
  if constexpr (std::is_same_v<T, R>) {
    return static_cast<T>(real);
  } else {
    return {static_cast<R>(real), static_cast<R>(imaginary)};
  }
}
template <typename T>
Wide ToWide(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}
template <typename T>
bool Finite(T value) {
  const auto w = ToWide(value);
  return std::isfinite(w.real()) && std::isfinite(w.imag());
}
template <typename T>
void Factor(const lapack_int* n, T* a, const lapack_int* ld, lapack_int* ip,
            lapack_int* jp, lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    sgetc2_(n, a, ld, ip, jp, info);
  } else if constexpr (std::is_same_v<T, double>) {
    dgetc2_(n, a, ld, ip, jp, info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    cgetc2_(n, a, ld, ip, jp, info);
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    zgetc2_(n, a, ld, ip, jp, info);
  }
}
template <typename T>
void Solve(const lapack_int* n, const T* a, const lapack_int* ld, T* rhs,
           const lapack_int* ip, const lapack_int* jp,
           asc::DenseBlasRealType<T>* scale) {
  if constexpr (std::is_same_v<T, float>) {
    sgesc2_(n, a, ld, rhs, ip, jp, scale);
  } else if constexpr (std::is_same_v<T, double>) {
    dgesc2_(n, a, ld, rhs, ip, jp, scale);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    cgesc2_(n, a, ld, rhs, ip, jp, scale);
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    zgesc2_(n, a, ld, rhs, ip, jp, scale);
  }
}
template <typename T>
struct Fixture {
  std::array<T, 160> a;
  std::array<T, 10> rhs;
  std::array<lapack_int, 10> ip;
  std::array<lapack_int, 10> jp;
  std::array<lapack_int, 3> n;
  std::array<lapack_int, 3> ld;
  std::array<lapack_int, 3> info;
  std::array<asc::DenseBlasRealType<T>, 3> scale;
  Fixture(std::size_t order, std::size_t padding)
      : n{79, static_cast<lapack_int>(order), 83},
        ld{89,
           static_cast<lapack_int>(std::max(std::size_t{1}, order) + padding),
           97},
        info{101, std::numeric_limits<lapack_int>::max(), 103},
        scale{107, -79, 109} {
    a.fill(Value<T>(-71, 23));
    rhs.fill(Value<T>(-73, 29));
    ip.fill(-101);
    jp.fill(-103);
  }
  [[nodiscard]] std::size_t Index(std::size_t i, std::size_t j) const {
    return 1 + j * static_cast<std::size_t>(ld[1]) + i;
  }
};
template <typename T>
auto Input(std::size_t n, std::size_t padding, std::size_t seed, int exponent) {
  Fixture<T> data(n, padding);
  for (std::size_t j = 0; j < n; ++j) {
    for (std::size_t i = 0; i < n; ++i) {
      const auto row = (i + seed) % n;
      const auto column = (n - j + seed) % n;
      const auto real =
          row == column
              ? static_cast<long double>(4 * n + row + 1)
              : static_cast<long double>(
                    static_cast<int>((row + 2 * column + seed) % 7) - 3) /
                    8;
      const auto imaginary =
          static_cast<long double>(
              static_cast<int>((2 * row + column + seed) % 5) - 2) /
          16;
      data.a[data.Index(i, j)] = Value<T>(std::scalbn(real, exponent),
                                          std::scalbn(imaginary, exponent));
    }
  }
  for (std::size_t i = 0; i < n; ++i) {
    Wide b{};
    for (std::size_t j = 0; j < n; ++j) {
      b += ToWide(data.a[data.Index(i, j)]) *
           ToWide(Value<T>(static_cast<long double>(j + 1) / 8,
                           static_cast<long double>(j + 2) / 16));
    }
    data.rhs[i + 1] = Value<T>(b.real(), b.imag());
  }
  return data;
}
template <typename T>
void Reconstruct(TestContext& test, const Fixture<T>& data,
                 const Fixture<T>& before, std::size_t n,
                 std::array<std::size_t, 8> rows,
                 std::array<std::size_t, 8> columns) {
  using R = asc::DenseBlasRealType<T>;
  for (std::size_t i = 0; i < n; ++i) {
    std::swap(rows[i], rows[static_cast<std::size_t>(data.ip[i + 1] - 1)]);
    std::swap(columns[i],
              columns[static_cast<std::size_t>(data.jp[i + 1] - 1)]);
  }
  const long double eps = std::numeric_limits<R>::epsilon();
  for (std::size_t j = 0; j < n; ++j) {
    for (std::size_t i = 0; i < n; ++i) {
      Wide value{};
      long double bound = 0;
      for (std::size_t k = 0; k < n; ++k) {
        Wide l{};
        if (i == k) {
          l = Wide{1, 0};
        } else if (i > k) {
          l = ToWide(data.a[data.Index(i, k)]);
        }
        const auto u = k <= j ? ToWide(data.a[data.Index(k, j)]) : Wide{};
        value += l * u;
        bound += std::abs(l) * std::abs(u);
      }
      const auto original = ToWide(before.a[before.Index(rows[i], columns[j])]);
      ASC_DENSE_TEST_CHECK(test, std::abs(value - original) <=
                                     64 * static_cast<long double>(n) * eps *
                                         (bound + std::abs(original)));
    }
  }
}
template <typename T>
void Case(TestContext& test, std::size_t n, std::size_t padding,
          std::size_t seed, int exponent) {
  using R = asc::DenseBlasRealType<T>;
  auto data = Input<T>(n, padding, seed, exponent);
  const auto before = data;
  Factor(&data.n[1], data.a.data() + 1, &data.ld[1], data.ip.data() + 1,
         data.jp.data() + 1, &data.info[1]);
  ASC_DENSE_TEST_EQ(test, data.n, before.n);
  ASC_DENSE_TEST_EQ(test, data.ld, before.ld);
  ASC_DENSE_TEST_EQ(test, data.info, (std::array<lapack_int, 3>{101, 0, 103}));
  ASC_DENSE_TEST_CHECK(test, Same(data.rhs, before.rhs));
  for (std::size_t j = 0; j < data.a.size(); ++j) {
    const auto column =
        j == 0 ? 160 : (j - 1) / static_cast<std::size_t>(data.ld[1]);
    const auto row =
        j == 0 ? 160 : (j - 1) % static_cast<std::size_t>(data.ld[1]);
    if (column >= n || row >= n) {
      ASC_DENSE_TEST_CHECK(test, Same(data.a[j], before.a[j]));
    }
  }
  bool valid = true;
  std::array<std::size_t, 8> rows{};
  std::array<std::size_t, 8> columns{};
  for (std::size_t i = 0; i < n; ++i) {
    rows[i] = i;
    columns[i] = i;
    valid = valid && data.ip[i + 1] >= static_cast<lapack_int>(i + 1) &&
            data.ip[i + 1] <= static_cast<lapack_int>(n) &&
            data.jp[i + 1] >= static_cast<lapack_int>(i + 1) &&
            data.jp[i + 1] <= static_cast<lapack_int>(n);
  }
  for (std::size_t i = 0; i < data.ip.size(); ++i) {
    if (i == 0 || i > n) {
      ASC_DENSE_TEST_EQ(test, data.ip[i], before.ip[i]);
      ASC_DENSE_TEST_EQ(test, data.jp[i], before.jp[i]);
    }
  }
  ASC_DENSE_TEST_CHECK(test, valid);
  if (!valid) {
    return;
  }
  if (n == 0) {
    return;  // GESC2 has no valid native empty path; never call it at N=0.
  }
  Reconstruct(test, data, before, n, rows, columns);
  const long double eps = std::numeric_limits<R>::epsilon();
  const auto factors = data;
  Solve(&data.n[1], data.a.data() + 1, &data.ld[1], data.rhs.data() + 1,
        data.ip.data() + 1, data.jp.data() + 1, &data.scale[1]);
  ASC_DENSE_TEST_CHECK(test, Same(data.a, factors.a));
  ASC_DENSE_TEST_EQ(test, data.ip, factors.ip);
  ASC_DENSE_TEST_EQ(test, data.jp, factors.jp);
  ASC_DENSE_TEST_EQ(test, data.n, before.n);
  ASC_DENSE_TEST_EQ(test, data.ld, before.ld);
  ASC_DENSE_TEST_EQ(test, data.scale, (std::array<R, 3>{107, 1, 109}));
  for (std::size_t i = 0; i < data.rhs.size(); ++i) {
    if (i == 0 || i > n) {
      ASC_DENSE_TEST_CHECK(test, Same(data.rhs[i], before.rhs[i]));
    }
  }
  for (std::size_t i = 0; i < n; ++i) {
    Wide ax{};
    long double bound = 0;
    for (std::size_t j = 0; j < n; ++j) {
      const auto a = ToWide(before.a[before.Index(i, j)]);
      const auto x = ToWide(data.rhs[j + 1]);
      ax += a * x;
      bound += std::abs(a) * std::abs(x);
    }
    const auto rhs = ToWide(before.rhs[i + 1]);
    ASC_DENSE_TEST_CHECK(test, Finite(data.rhs[i + 1]));
    ASC_DENSE_TEST_CHECK(
        test, std::abs(ax - rhs) <= 64 * static_cast<long double>(n) * eps *
                                        (bound + std::abs(rhs)));
  }
}
template <typename T>
void Scaling(TestContext& test) {
  using R = asc::DenseBlasRealType<T>;
  Fixture<T> data(1, 2);
  const R small =
      4 * (std::numeric_limits<R>::min() / std::numeric_limits<R>::epsilon());
  data.a[1] = Value<T>(small, small / 4);
  data.rhs[1] = Value<T>(std::numeric_limits<R>::max() / 8,
                         std::numeric_limits<R>::max() / 16);
  const auto before = data;
  Factor(&data.n[1], data.a.data() + 1, &data.ld[1], data.ip.data() + 1,
         data.jp.data() + 1, &data.info[1]);
  ASC_DENSE_TEST_EQ(test, data.info[1], 0);
  ASC_DENSE_TEST_EQ(test, data.ip[1], 1);
  ASC_DENSE_TEST_EQ(test, data.jp[1], 1);
  const auto factors = data;
  Solve(&data.n[1], data.a.data() + 1, &data.ld[1], data.rhs.data() + 1,
        data.ip.data() + 1, data.jp.data() + 1, &data.scale[1]);
  ASC_DENSE_TEST_CHECK(test, Same(data.a, factors.a));
  ASC_DENSE_TEST_EQ(test, data.ip, factors.ip);
  ASC_DENSE_TEST_EQ(test, data.jp, factors.jp);
  ASC_DENSE_TEST_CHECK(test, std::isfinite(data.scale[1]) &&
                                 data.scale[1] > 0 && data.scale[1] < 1);
  ASC_DENSE_TEST_CHECK(test, Finite(data.rhs[1]));
  const auto ax = ToWide(before.a[1]) * ToWide(data.rhs[1]);
  const auto b =
      static_cast<long double>(data.scale[1]) * ToWide(before.rhs[1]);
  ASC_DENSE_TEST_CHECK(
      test, std::abs(ax - b) <= 64 * std::numeric_limits<R>::epsilon() *
                                    (std::abs(ax) + std::abs(b)));
  ASC_DENSE_TEST_EQ(test, data.n, before.n);
  ASC_DENSE_TEST_EQ(test, data.ld, before.ld);
  ASC_DENSE_TEST_EQ(test, data.scale[0], before.scale[0]);
  ASC_DENSE_TEST_EQ(test, data.scale[2], before.scale[2]);
  for (std::size_t i = 0; i < data.rhs.size(); ++i) {
    if (i != 1) {
      ASC_DENSE_TEST_CHECK(test, Same(data.rhs[i], before.rhs[i]));
    }
  }
}
template <typename T>
auto ZeroInput(std::size_t n, std::size_t padding, int rhs_kind) {
  using R = asc::DenseBlasRealType<T>;
  Fixture<T> data(n, padding);
  const R small =
      std::numeric_limits<R>::min() / std::numeric_limits<R>::epsilon();
  for (std::size_t j = 0; j < n; ++j) {
    for (std::size_t i = 0; i < n; ++i) {
      data.a[data.Index(i, j)] = T{};
    }
  }
  for (std::size_t i = 0; i < n; ++i) {
    R base = R{1};
    if (rhs_kind == 0) {
      base = small;
    } else if (rhs_kind != 1) {
      base = std::numeric_limits<R>::max() / R{32};
    }
    const R real = base * static_cast<R>(i + 1);
    data.rhs[i + 1] = Value<T>(real, real / R{2});
  }
  return data;
}
template <typename T>
void ZeroCase(TestContext& test, std::size_t n, std::size_t padding,
              int rhs_kind) {
  using R = asc::DenseBlasRealType<T>;
  auto data = ZeroInput<T>(n, padding, rhs_kind);
  const R small =
      std::numeric_limits<R>::min() / std::numeric_limits<R>::epsilon();
  const auto before = data;
  Factor(&data.n[1], data.a.data() + 1, &data.ld[1], data.ip.data() + 1,
         data.jp.data() + 1, &data.info[1]);
  ASC_DENSE_TEST_EQ(test, data.n, before.n);
  ASC_DENSE_TEST_EQ(test, data.ld, before.ld);
  ASC_DENSE_TEST_EQ(
      test, data.info,
      (std::array<lapack_int, 3>{101, static_cast<lapack_int>(n), 103}));
  ASC_DENSE_TEST_CHECK(test, Same(data.rhs, before.rhs));
  ASC_DENSE_TEST_CHECK(test, Same(data.scale, before.scale));
  bool valid = true;
  for (std::size_t i = 0; i < data.ip.size(); ++i) {
    if (i == 0 || i > n) {
      ASC_DENSE_TEST_EQ(test, data.ip[i], before.ip[i]);
      ASC_DENSE_TEST_EQ(test, data.jp[i], before.jp[i]);
    } else {
      // >= ties select the last remaining element. On a zero matrix both
      // sequential permutations are identical and every pivot is N.
      ASC_DENSE_TEST_EQ(test, data.ip[i], static_cast<lapack_int>(n));
      ASC_DENSE_TEST_EQ(test, data.jp[i], static_cast<lapack_int>(n));
      valid = valid && data.ip[i] >= static_cast<lapack_int>(i) &&
              data.ip[i] <= static_cast<lapack_int>(n) &&
              data.jp[i] >= static_cast<lapack_int>(i) &&
              data.jp[i] <= static_cast<lapack_int>(n);
    }
  }
  for (std::size_t k = 0; k < data.a.size(); ++k) {
    const auto column =
        k == 0 ? 160 : (k - 1) / static_cast<std::size_t>(data.ld[1]);
    const auto row =
        k == 0 ? 160 : (k - 1) % static_cast<std::size_t>(data.ld[1]);
    if (row < n && column < n) {
      // A_tilde = SMLNUM * I: both permutations are the same, L is identity,
      // and every U diagonal is independently known, including the last.
      ASC_DENSE_TEST_EQ(test, data.a[k], row == column ? Value<T>(small) : T{});
    } else {
      ASC_DENSE_TEST_CHECK(test, Same(data.a[k], before.a[k]));
    }
  }
  if (!valid || n == 0) {
    return;
  }
  const auto factors = data;
  Solve(&data.n[1], data.a.data() + 1, &data.ld[1], data.rhs.data() + 1,
        data.ip.data() + 1, data.jp.data() + 1, &data.scale[1]);
  ASC_DENSE_TEST_CHECK(test, Same(data.a, factors.a));
  ASC_DENSE_TEST_EQ(test, data.ip, factors.ip);
  ASC_DENSE_TEST_EQ(test, data.jp, factors.jp);
  ASC_DENSE_TEST_EQ(test, data.info, factors.info);
  ASC_DENSE_TEST_EQ(test, data.n, factors.n);
  ASC_DENSE_TEST_EQ(test, data.ld, factors.ld);
  ASC_DENSE_TEST_EQ(test, data.scale.front(), before.scale.front());
  ASC_DENSE_TEST_EQ(test, data.scale.back(), before.scale.back());
  ASC_DENSE_TEST_CHECK(test, std::isfinite(data.scale[1]) &&
                                 data.scale[1] > 0 && data.scale[1] <= 1);
  if (rhs_kind == 0) {
    ASC_DENSE_TEST_EQ(test, data.scale[1], R{1});
  } else {
    ASC_DENSE_TEST_CHECK(test, data.scale[1] < 1);
  }
  for (std::size_t i = 0; i < data.rhs.size(); ++i) {
    if (i == 0 || i > n) {
      ASC_DENSE_TEST_CHECK(test, Same(data.rhs[i], before.rhs[i]));
      continue;
    }
    ASC_DENSE_TEST_CHECK(test, Finite(data.rhs[i]));
    const Wide lhs = static_cast<long double>(small) * ToWide(data.rhs[i]);
    const Wide rhs =
        static_cast<long double>(data.scale[1]) * ToWide(before.rhs[i]);
    const long double bound = std::abs(lhs) + std::abs(rhs);
    ASC_DENSE_TEST_CHECK(
        test,
        std::abs(lhs - rhs) <= 32 * std::numeric_limits<R>::epsilon() * bound);
  }
}
template <typename T>
std::size_t Run(TestContext& test) {
  std::size_t cases = 0;
  for (std::size_t n : std::array<std::size_t, 6>{0, 1, 2, 3, 5, 8}) {
    for (std::size_t pad : std::array<std::size_t, 3>{0, 1, 3}) {
      for (std::size_t seed = 0; seed < 12; ++seed) {
        for (int exponent : {-32, 0, 32}) {
          Case<T>(test, n, pad, seed, exponent);
          ++cases;
        }
      }
    }
  }
  Scaling<T>(test);
  ++cases;
  for (std::size_t padding : {0U, 1U, 3U}) {
    ZeroCase<T>(test, 0, padding, 0);
    ++cases;
    for (std::size_t n = 1; n <= 8; ++n) {
      for (int rhs = 0; rhs < 3; ++rhs) {
        ZeroCase<T>(test, n, padding, rhs);
        ++cases;
      }
    }
  }
  return cases;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  TestContext test;
  std::size_t cases = 0;
  cases += Run<float>(test);
  cases += Run<double>(test);
  cases += Run<std::complex<float>>(test);
  cases += Run<std::complex<double>>(test);
  ASC_DENSE_TEST_EQ(test, cases, std::size_t{2896});
  std::printf("%zu bounded complete-pivoting ABI cases; INTEGER=%zu bytes\n",
              cases, sizeof(lapack_int));
  return test.Finish();
}
