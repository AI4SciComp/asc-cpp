#include <array>
#include <cstdio>
#if defined(ASC_ROTATION_S_HEADER)
#include <type_traits>

#include "../../src/dense/lapack/internal_rotation_prototypes.h"
#endif

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "rotation_test_support.h"

#if defined(ASC_ROTATION_S_HEADER)
#define slartg_ emitted_slartg
#include ASC_ROTATION_S_HEADER
#undef slartg_
#define dlartg_ emitted_dlartg
#include ASC_ROTATION_D_HEADER
#undef dlartg_
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_slartg), decltype(emitted_slartg)>);
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_dlartg), decltype(emitted_dlartg)>);
#endif

namespace {
namespace rotation = asc_rotation_test;
namespace numerical = asc_rotation_numerical;

template <typename T>
void Case(rotation::TestContext& test, T f, T g) {
  std::array<T, 3> ff{71, f, 73};
  std::array<T, 3> gg{71, g, 73};
  std::array<T, 3> c{71, -7, 73};
  auto s = c;
  auto r = c;
  rotation::Native(ff[1], gg[1], c[1], s[1], r[1]);
  ASC_DENSE_TEST_EQ(test, numerical::Bits(ff[1]), numerical::Bits(f));
  ASC_DENSE_TEST_EQ(test, numerical::Bits(gg[1]), numerical::Bits(g));
  for (const auto* values : {&ff, &gg, &c, &s, &r}) {
    ASC_DENSE_TEST_EQ(test, (*values)[0], T{71});
    ASC_DENSE_TEST_EQ(test, (*values)[2], T{73});
  }
  ASC_DENSE_TEST_CHECK(test, numerical::Accurate(f, g, c[1], s[1], r[1]));
}

template <typename T>
void Run(rotation::TestContext& test) {
  for (const T f : numerical::Values<T>()) {
    for (const T g : numerical::Values<T>()) {
      Case(test, f, g);
    }
  }
  Case(test, T{3}, T{4});
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  rotation::TestContext test;
  Run<float>(test);
  Run<double>(test);
  std::printf("2050 guarded real LARTG ABI cases; provider integer bytes=%zu\n",
              sizeof(lapack_int));
  return test.Finish();
}
