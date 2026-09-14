#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/core/types.h"
#include "installed_lu/normal_return_guard.h"
#include "triangular_storage_conversion_test_support.h"
#if defined(ASC_STORAGE_STRTTP_HEADER)
#include <type_traits>
#define strttp_ emitted_strttp
#include ASC_STORAGE_STRTTP_HEADER
#undef strttp_
#define stpttr_ emitted_stpttr
#include ASC_STORAGE_STPTTR_HEADER
#undef stpttr_
#define dtrttp_ emitted_dtrttp
#include ASC_STORAGE_DTRTTP_HEADER
#undef dtrttp_
#define dtpttr_ emitted_dtpttr
#include ASC_STORAGE_DTPTTR_HEADER
#undef dtpttr_
#define ctrttp_ emitted_ctrttp
#include ASC_STORAGE_CTRTTP_HEADER
#undef ctrttp_
#define ctpttr_ emitted_ctpttr
#include ASC_STORAGE_CTPTTR_HEADER
#undef ctpttr_
#define ztrttp_ emitted_ztrttp
#include ASC_STORAGE_ZTRTTP_HEADER
#undef ztrttp_
#define ztpttr_ emitted_ztpttr
#include ASC_STORAGE_ZTPTTR_HEADER
#undef ztpttr_
#endif
namespace {
namespace support = asc_triangular_storage_test;
using support::TestContext;
#if defined(ASC_STORAGE_STRTTP_HEADER)
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
static_assert(std::is_same_v<Sig<decltype(LAPACK_strttp_base)>::Type,
                             Sig<decltype(emitted_strttp)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_stpttr_base)>::Type,
                             Sig<decltype(emitted_stpttr)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_dtrttp_base)>::Type,
                             Sig<decltype(emitted_dtrttp)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_dtpttr_base)>::Type,
                             Sig<decltype(emitted_dtpttr)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_ctrttp_base)>::Type,
                             Sig<decltype(emitted_ctrttp)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_ctpttr_base)>::Type,
                             Sig<decltype(emitted_ctpttr)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_ztrttp_base)>::Type,
                             Sig<decltype(emitted_ztrttp)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_ztpttr_base)>::Type,
                             Sig<decltype(emitted_ztpttr)>::Type>);
#endif
template <typename T>
void Case(TestContext& test, bool pack, char uplo, std::size_t n,
          std::size_t padding, std::size_t seed) {
  constexpr std::size_t kDense = 1280;
  constexpr std::size_t kPacked = 564;
  const auto ld = std::max(std::size_t{1}, n) + padding;
  std::array<T, kDense> a;
  std::array<T, kPacked> ap;
  a.fill(T{-97});
  ap.fill(T{-101});
  auto expected_a = a;
  auto expected_ap = ap;
  for (std::size_t j = 0; j < n; ++j) {
    for (std::size_t i = 0; i < n; ++i) {
      const bool selected = uplo == 'U' ? i <= j : i >= j;
      const auto value = support::Fixture<T>(
          static_cast<asc::extent_t>(i), static_cast<asc::extent_t>(j), seed);
      if (pack) {
        a[1 + j * ld + i] = value;
        expected_a[1 + j * ld + i] = value;
      }
      if (selected) {
        // Closed-form documented packed indexing, independent of native K
        // loops.
        const auto index = uplo == 'U' ? j * (j + 1) / 2 + i
                                       : j * (2 * n - j + 1) / 2 + (i - j);
        if (pack) {
          expected_ap[1 + index] = value;
        } else {
          ap[1 + index] = value;
          expected_ap[1 + index] = value;
          expected_a[1 + j * ld + i] = value;
        }
      }
    }
  }
  std::array<lapack_int, 3> size{83, static_cast<lapack_int>(n), 89};
  std::array<lapack_int, 3> leading{97, static_cast<lapack_int>(ld), 101};
  std::array<lapack_int, 3> info{103, std::numeric_limits<lapack_int>::max(),
                                 107};
  std::array<char, 3> part{'a', uplo, 'b'};
  const auto size_before = size;
  const auto leading_before = leading;
  const auto part_before = part;
  support::Native(pack, &part[1], &size[1], a.data() + 1, &leading[1],
                  ap.data() + 1, &info[1]);
  ASC_DENSE_TEST_CHECK(test, size == size_before);
  ASC_DENSE_TEST_CHECK(test, leading == leading_before);
  ASC_DENSE_TEST_CHECK(test, part == part_before);
  ASC_DENSE_TEST_EQ(test, info[0], 103);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  ASC_DENSE_TEST_EQ(test, info[2], 107);
  ASC_DENSE_TEST_CHECK(test, support::Same(a, expected_a));
  ASC_DENSE_TEST_CHECK(test, support::Same(ap, expected_ap));
}
template <typename T>
void Run(TestContext& test) {
  for (auto n : std::array<std::size_t, 8>{0, 1, 2, 3, 5, 8, 17, 33}) {
    for (auto padding : std::array<std::size_t, 3>{0, 1, 5}) {
      for (char uplo : {'U', 'L'}) {
        for (std::size_t seed = 0; seed < 16; ++seed) {
          for (bool pack : {false, true}) {
            Case<T>(test, pack, uplo, n, padding, seed);
          }
        }
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  TestContext test;
  Run<float>(test);
  Run<double>(test);
  Run<std::complex<float>>(test);
  Run<std::complex<double>>(test);
  std::printf("6144 triangular storage ABI cases; integer bytes=%zu\n",
              sizeof(lapack_int));
  return test.Finish();
}
