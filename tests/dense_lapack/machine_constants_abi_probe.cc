#include <array>
#include <cstdio>
#if defined(ASC_MACHINE_CONSTANTS_S_HEADER)
#include <cstddef>
#include <type_traits>
#endif
#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "machine_constants_test_support.h"
#if defined(ASC_MACHINE_CONSTANTS_S_HEADER)
#define slamch_ emitted_slamch
#include ASC_MACHINE_CONSTANTS_S_HEADER
#undef slamch_
#define dlamch_ emitted_dlamch
#include ASC_MACHINE_CONSTANTS_D_HEADER
#undef dlamch_
template <typename T>
struct Argument {
  using Type = T;
};
template <typename T>
struct Argument<T*> {
  using Type = std::remove_const_t<T>*;
};
template <typename T>
struct Signature;
template <typename R, typename... A>
struct Signature<R(A...)> {
  using Type = R(typename Argument<A>::Type...);
};
static_assert(std::is_same_v<Signature<decltype(LAPACK_slamch_base)>::Type,
                             Signature<decltype(emitted_slamch)>::Type>);
static_assert(std::is_same_v<Signature<decltype(LAPACK_dlamch_base)>::Type,
                             Signature<decltype(emitted_dlamch)>::Type>);
static_assert(sizeof(FORTRAN_STRLEN) == sizeof(std::size_t));
#endif
namespace {
namespace constants = asc_machine_constants_test;
namespace numerical = asc_machine_constants_numerical;
template <typename T>
void Run(constants::TestContext& test) {
  for (const auto fixture : numerical::Cases<T>()) {
    for (bool lower : {false, true}) {
      for (int repeat = 0; repeat < 2; ++repeat) {
        const char code =
            lower ? static_cast<char>(fixture.selector + ('a' - 'A'))
                  : fixture.selector;
        std::array<char, 3> input{'#', code, '!'};
        const auto before = input;
        const T result = constants::Native<T>(input[1]);
        ASC_DENSE_TEST_EQ(test, input, before);
        ASC_DENSE_TEST_EQ(test, numerical::Bits(result),
                          numerical::Bits(fixture.expected));
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  constants::TestContext test;
  Run<float>(test);
  Run<double>(test);
  std::printf(
      "80 guarded LAMCH selector ABI cases; provider integer bytes=%zu hidden "
      "length bytes=%zu\n",
      sizeof(lapack_int), sizeof(FORTRAN_STRLEN));
  return test.Finish();
}
