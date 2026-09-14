#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <span>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_PERM_SLAPMR_HEADER)
#include <cstdint>  // IWYU pragma: keep; emitted Fortran LOGICAL declarations.
#define slapmr_ emitted_slapmr
#include ASC_PERM_SLAPMR_HEADER
#undef slapmr_
#define slapmt_ emitted_slapmt
#include ASC_PERM_SLAPMT_HEADER
#undef slapmt_
#define dlapmr_ emitted_dlapmr
#include ASC_PERM_DLAPMR_HEADER
#undef dlapmr_
#define dlapmt_ emitted_dlapmt
#include ASC_PERM_DLAPMT_HEADER
#undef dlapmt_
#define clapmr_ emitted_clapmr
#include ASC_PERM_CLAPMR_HEADER
#undef clapmr_
#define clapmt_ emitted_clapmt
#include ASC_PERM_CLAPMT_HEADER
#undef clapmt_
#define zlapmr_ emitted_zlapmr
#include ASC_PERM_ZLAPMR_HEADER
#undef zlapmr_
#define zlapmt_ emitted_zlapmt
#include ASC_PERM_ZLAPMT_HEADER
#undef zlapmt_
#endif
namespace {
using asc_dense_test::TestContext;
#if defined(ASC_PERM_SLAPMR_HEADER)
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
static_assert(std::is_same_v<Sig<decltype(LAPACK_slapmr)>::Type,
                             Sig<decltype(emitted_slapmr)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_slapmt)>::Type,
                             Sig<decltype(emitted_slapmt)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_dlapmr)>::Type,
                             Sig<decltype(emitted_dlapmr)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_dlapmt)>::Type,
                             Sig<decltype(emitted_dlapmt)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_clapmr)>::Type,
                             Sig<decltype(emitted_clapmr)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_clapmt)>::Type,
                             Sig<decltype(emitted_clapmt)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_zlapmr)>::Type,
                             Sig<decltype(emitted_zlapmr)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_zlapmt)>::Type,
                             Sig<decltype(emitted_zlapmt)>::Type>);
#endif
template <typename T>
bool Same(const T& a, const T& b) {
  const auto x = std::as_bytes(std::span(&a, 1));
  const auto y = std::as_bytes(std::span(&b, 1));
  return std::equal(x.begin(), x.end(), y.begin());
}
template <typename T>
T Value(std::size_t i, std::size_t j, std::size_t seed) {
  using R = asc::DenseBlasRealType<T>;
  const std::array<R, 12> values{R{0},
                                 -R{0},
                                 R{1},
                                 R{-3},
                                 R{0.25},
                                 R{-0.125},
                                 std::numeric_limits<R>::denorm_min(),
                                 std::numeric_limits<R>::min(),
                                 std::numeric_limits<R>::max(),
                                 std::numeric_limits<R>::infinity(),
                                 -std::numeric_limits<R>::infinity(),
                                 std::numeric_limits<R>::quiet_NaN()};
  const auto real = values[(i + 3 * j + seed) % values.size()];
  if constexpr (std::is_same_v<T, R>) {
    return real;
  } else {
    return {real, values[(2 * i + j + seed + 5) % values.size()]};
  }
}
template <typename T>
void Native(bool rows, const lapack_logical* forward, const lapack_int* m,
            const lapack_int* n, T* x, const lapack_int* ld, lapack_int* k) {
  if constexpr (std::is_same_v<T, float>) {
    if (rows) {
      LAPACK_slapmr(forward, m, n, x, ld, k);
    } else {
      LAPACK_slapmt(forward, m, n, x, ld, k);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    if (rows) {
      LAPACK_dlapmr(forward, m, n, x, ld, k);
    } else {
      LAPACK_dlapmt(forward, m, n, x, ld, k);
    }
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (rows) {
      LAPACK_clapmr(forward, m, n, x, ld, k);
    } else {
      LAPACK_clapmt(forward, m, n, x, ld, k);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (rows) {
      LAPACK_zlapmr(forward, m, n, x, ld, k);
    } else {
      LAPACK_zlapmt(forward, m, n, x, ld, k);
    }
  }
}
template <typename T>
void Case(TestContext& test, bool rows, bool forward, std::size_t d,
          std::size_t other, std::size_t padding, std::size_t seed,
          const std::array<lapack_int, 33>& permutation) {
  const auto m = rows ? d : other;
  const auto n = rows ? other : d;
  const auto ld = std::max(std::size_t{1}, m) + padding;
  std::array<T, 1400> x;
  x.fill(T{-97});
  for (std::size_t j = 0; j < n; ++j) {
    for (std::size_t i = 0; i < m; ++i) {
      x[1 + j * ld + i] = Value<T>(i, j, seed);
    }
  }
  const auto before = x;
  auto expected = x;
  for (std::size_t j = 0; j < n; ++j) {
    for (std::size_t i = 0; i < m; ++i) {
      const auto pi = rows ? static_cast<std::size_t>(permutation[i] - 1) : i;
      const auto pj = rows ? j : static_cast<std::size_t>(permutation[j] - 1);
      if (forward) {
        expected[1 + j * ld + i] = before[1 + pj * ld + pi];
      } else {
        expected[1 + pj * ld + pi] = before[1 + j * ld + i];
      }
    }
  }
  std::array<lapack_int, 35> k;
  k.fill(101);
  std::copy_n(permutation.begin(), d, k.begin() + 1);
  const auto k_before = k;
  std::array<lapack_int, 3> mm{103, static_cast<lapack_int>(m), 107};
  std::array<lapack_int, 3> nn{109, static_cast<lapack_int>(n), 113};
  std::array<lapack_int, 3> leading{127, static_cast<lapack_int>(ld), 131};
  std::array<lapack_logical, 3> ff{137, forward ? 1 : 0, 139};
  const auto mm_before = mm;
  const auto nn_before = nn;
  const auto ld_before = leading;
  const auto ff_before = ff;
  Native(rows, &ff[1], &mm[1], &nn[1], x.data() + 1, &leading[1], k.data() + 1);
  ASC_DENSE_TEST_CHECK(test, Same(x, expected));
  ASC_DENSE_TEST_CHECK(test, k == k_before);
  ASC_DENSE_TEST_CHECK(test, mm == mm_before);
  ASC_DENSE_TEST_CHECK(test, nn == nn_before);
  ASC_DENSE_TEST_CHECK(test, leading == ld_before);
  ASC_DENSE_TEST_CHECK(test, ff == ff_before);
}
template <typename T>
std::size_t Run(TestContext& test) {
  std::size_t cases = 0;
  for (std::size_t d :
       std::array<std::size_t, 9>{0, 1, 2, 3, 4, 5, 8, 17, 33}) {
    std::array<lapack_int, 33> k{};
    for (std::size_t i = 0; i < d; ++i) {
      k[i] = static_cast<lapack_int>(i + 1);
    }
    std::size_t variant = 0;
    do {
      for (std::size_t other : std::array<std::size_t, 4>{0, 1, 3, 7}) {
        for (std::size_t padding : std::array<std::size_t, 2>{0, 2}) {
          for (std::size_t seed = 0; seed < 12; ++seed) {
            for (bool rows : {false, true}) {
              for (bool forward : {false, true}) {
                Case<T>(test, rows, forward, d, other, padding, seed, k);
                ++cases;
              }
            }
          }
        }
      }
      ++variant;
      if (d <= 5) {
        if (!std::next_permutation(
                k.begin(), k.begin() + static_cast<std::ptrdiff_t>(d))) {
          break;
        }
      } else {
        if (variant == 4) {
          break;
        }
        if (variant == 1) {
          std::reverse(k.begin(), k.begin() + static_cast<std::ptrdiff_t>(d));
        }
        if (variant == 2) {
          for (std::size_t i = 0; i < d; ++i) {
            k[i] = static_cast<lapack_int>((i + 1) % d + 1);
          }
        }
        if (variant == 3) {
          for (std::size_t i = 0; i < d; ++i) {
            k[i] = static_cast<lapack_int>(i + 1);
          }
          for (std::size_t i = 0; i + 2 < d; i += 3) {
            std::rotate(k.begin() + static_cast<std::ptrdiff_t>(i),
                        k.begin() + static_cast<std::ptrdiff_t>(i + 1),
                        k.begin() + static_cast<std::ptrdiff_t>(i + 3));
          }
        }
      }
    } while (true);
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
  std::printf("%zu valid permutation cases; INTEGER=%zu LOGICAL=%zu bytes\n",
              cases, sizeof(lapack_int), sizeof(lapack_logical));
  return test.Finish();
}
