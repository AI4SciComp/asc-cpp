#include <array>
#include <cstdio>
#if defined(ASC_MACHINE_S_HEADER)
#include <type_traits>

#include "../../src/dense/lapack/internal_machine_prototypes.h"
#endif

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "machine_test_support.h"

#if defined(ASC_MACHINE_S_HEADER)
#define slabad_ emitted_slabad
#include ASC_MACHINE_S_HEADER
#undef slabad_
#define dlabad_ emitted_dlabad
#include ASC_MACHINE_D_HEADER
#undef dlabad_
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_slabad), decltype(emitted_slabad)>);
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_dlabad), decltype(emitted_dlabad)>);
#endif

namespace {
namespace machine = asc_machine_test;
namespace numerical = asc_machine_numerical;

template <typename T>
void Case(machine::TestContext& test, T f, T g) {
  std::array<T, 3> ff{71, f, 73};
  std::array<T, 3> gg{71, g, 73};
  machine::Native(ff[1], gg[1]);
  ASC_DENSE_TEST_EQ(test, numerical::Bits(ff[1]), numerical::Bits(f));
  ASC_DENSE_TEST_EQ(test, numerical::Bits(gg[1]), numerical::Bits(g));
  for (const auto* values : {&ff, &gg}) {
    ASC_DENSE_TEST_EQ(test, (*values)[0], T{71});
    ASC_DENSE_TEST_EQ(test, (*values)[2], T{73});
  }
}

template <typename T>
void Run(machine::TestContext& test) {
  for (const T f : numerical::Values<T>()) {
    for (const T g : numerical::Values<T>()) {
      Case(test, f, g);
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  machine::TestContext test;
  Run<float>(test);
  Run<double>(test);
  std::printf("800 guarded LABAD ABI cases; provider integer bytes=%zu\n",
              sizeof(lapack_int));
  return test.Finish();
}
