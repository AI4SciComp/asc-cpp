#include <array>
#include <cstddef>
#include <cstdio>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "scalar_norm_test_support.h"
#if defined(ASC_NORM_SLAPY2_HEADER)
#define slapy2_ emitted_slapy2
#include ASC_NORM_SLAPY2_HEADER
#undef slapy2_
#define dlapy2_ emitted_dlapy2
#include ASC_NORM_DLAPY2_HEADER
#undef dlapy2_
#define slapy3_ emitted_slapy3
#include ASC_NORM_SLAPY3_HEADER
#undef slapy3_
#define dlapy3_ emitted_dlapy3
#include ASC_NORM_DLAPY3_HEADER
#undef dlapy3_
#endif
namespace {
namespace norm = asc_scalar_norm_test;
#if defined(ASC_NORM_SLAPY2_HEADER)
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
static_assert(std::is_same_v<Sig<decltype(LAPACK_slapy2)>::Type,
                             Sig<decltype(emitted_slapy2)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_dlapy2)>::Type,
                             Sig<decltype(emitted_dlapy2)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_slapy3)>::Type,
                             Sig<decltype(emitted_slapy3)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_dlapy3)>::Type,
                             Sig<decltype(emitted_dlapy3)>::Type>);
#endif
template <typename T>
T Native(T* x, T* y, T* z, bool three) {
  if constexpr (std::is_same_v<T, float>) {
    return three ? LAPACK_slapy3(x, y, z) : LAPACK_slapy2(x, y);
  } else {
    return three ? LAPACK_dlapy3(x, y, z) : LAPACK_dlapy2(x, y);
  }
}
template <typename T>
void Case(norm::TestContext& test, T x, T y, T z, bool three) {
  std::array<T, 5> input{T{71}, x, y, z, T{73}};
  const auto before = input;
  const T actual = Native(&input[1], &input[2], &input[3], three);
  for (std::size_t i = 0; i < input.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, norm::Bits(input[i]), norm::Bits(before[i]));
  }
  ASC_DENSE_TEST_CHECK(test,
                       norm::Accurate(actual, norm::Expected(x, y, z, three)));
}
template <typename T>
void Run(norm::TestContext& test) {
  const auto values = norm::Values<T>();
  for (T x : values) {
    for (T y : values) {
      Case(test, x, y, T{}, false);
      for (T z : values) {
        Case(test, x, y, z, true);
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  norm::TestContext test;
  Run<float>(test);
  Run<double>(test);
  std::printf("16800 scalar norm ABI cases; provider integer bytes=%zu\n",
              sizeof(lapack_int));
  return test.Finish();
}
