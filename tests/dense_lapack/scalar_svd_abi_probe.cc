#include <array>
#include <cstdio>
#if defined(ASC_SCALAR_SVD_S_HEADER)
#include <type_traits>

#include "../../src/dense/lapack/internal_scalar_svd_prototypes.h"

#endif

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "scalar_svd_test_support.h"

#if defined(ASC_SCALAR_SVD_S_HEADER)
#define slasv2_ emitted_slasv2
#include ASC_SCALAR_SVD_S_HEADER
#undef slasv2_
#define dlasv2_ emitted_dlasv2
#include ASC_SCALAR_SVD_D_HEADER
#undef dlasv2_
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_slasv2), decltype(emitted_slasv2)>);
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_dlasv2), decltype(emitted_dlasv2)>);
#endif

namespace {
namespace svd = asc_scalar_svd_test;
namespace numerical = asc_scalar_svd_numerical;

template <typename T>
void Case(svd::TestContext& test, T f, T g, T h) {
  std::array<T, 3> ff{71, f, 73};
  std::array<T, 3> gg{71, g, 73};
  std::array<T, 3> hh{71, h, 73};
  svd::Output<T> output;
  auto& v = output.values;
  svd::Native(ff[1], gg[1], hh[1], v[0][1], v[1][1], v[2][1], v[3][1], v[4][1],
              v[5][1]);
  ASC_DENSE_TEST_EQ(test, numerical::Bits(ff[1]), numerical::Bits(f));
  ASC_DENSE_TEST_EQ(test, numerical::Bits(gg[1]), numerical::Bits(g));
  ASC_DENSE_TEST_EQ(test, numerical::Bits(hh[1]), numerical::Bits(h));
  for (const auto* values : {&ff, &gg, &hh}) {
    ASC_DENSE_TEST_EQ(test, (*values)[0], T{71});
    ASC_DENSE_TEST_EQ(test, (*values)[2], T{73});
  }
  for (const auto& values : v) {
    ASC_DENSE_TEST_EQ(test, values[0], T{71});
    ASC_DENSE_TEST_EQ(test, values[2], T{73});
  }
  ASC_DENSE_TEST_CHECK(
      test, numerical::Accurate(f, g, h, v[0][1], v[1][1], v[2][1], v[3][1],
                                v[4][1], v[5][1]));
}
template <typename T>
void Run(svd::TestContext& test) {
  numerical::ForEach<T>([&](T f, T g, T h) { Case(test, f, g, h); });
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  svd::TestContext test;
  Run<float>(test);
  Run<double>(test);
  std::printf("77824 guarded LASV2 ABI cases; provider integer bytes=%zu\n",
              sizeof(lapack_int));
  return test.Finish();
}
