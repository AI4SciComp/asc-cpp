#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#if defined(ASC_LAT2_DLAT2S_HEADER)
#include <type_traits>

#include "../../src/dense/lapack/internal_triangular_conversion_prototypes.h"
#endif

#include "installed_lu/normal_return_guard.h"
#include "triangular_conversion_test_support.h"
#if defined(ASC_LAT2_DLAT2S_HEADER)
#define dlat2s_ emitted_dlat2s
#include ASC_LAT2_DLAT2S_HEADER
#undef dlat2s_
#define zlat2c_ emitted_zlat2c
#include ASC_LAT2_ZLAT2C_HEADER
#undef zlat2c_
#endif
namespace {
namespace tc = asc_triangular_conversion_test;
#if defined(ASC_LAT2_DLAT2S_HEADER)
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
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_dlat2s_base)>::Type,
                             typename Sig<decltype(emitted_dlat2s)>::Type>);
static_assert(std::is_same_v<typename Sig<decltype(LAPACK_zlat2c_base)>::Type,
                             typename Sig<decltype(emitted_zlat2c)>::Type>);
#endif
template <typename Input>
void Case(tc::TestContext& test, asc::extent_t n,
          asc::DenseBlasTriangle triangle, std::size_t shift, int kind) {
  tc::Problem<Input> p(n, triangle, tc::kColumn, tc::kColumn);
  tc::Initialize(p, shift);
  if (n > 0 && kind != 0) {
    const double value = kind == 1 ? std::numeric_limits<double>::max()
                                   : std::numeric_limits<double>::quiet_NaN();
    p.input.At(n - 1, n - 1) = tc::Value<Input>(value, 0);
  }
  const auto before = p;
  const auto high = std::numeric_limits<lapack_int>::max();
  std::array<lapack_int, 3> info{71, high, 73};
  tc::Native(triangle == tc::kUpper ? 'U' : 'L', static_cast<lapack_int>(n),
             p.input.View().data(), static_cast<lapack_int>(p.input.ld),
             p.output.View().data(), static_cast<lapack_int>(p.output.ld),
             info[1]);
  // The success path intentionally preserves the entire incoming native INFO.
  ASC_DENSE_TEST_EQ(test, info[1], n > 0 && kind == 1 ? 1 : high);
  ASC_DENSE_TEST_EQ(test, info[0], 71);
  ASC_DENSE_TEST_EQ(test, info[2], 73);
  if (kind != 1 || n == 0) {
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t i = 0; i < n; ++i) {
        auto expected = before.output.At(i, j);
        if (tc::Selected(triangle, i, j)) {
          const auto value = tc::ToWide(before.input.At(i, j));
          expected = tc::Value<tc::Output<Input>>(value.real(), value.imag());
        }
        ASC_DENSE_TEST_CHECK(test, tc::Same(p.output.At(i, j), expected));
      }
    }
  }
  for (std::size_t i = 0; i < p.input.data.size(); ++i) {
    ASC_DENSE_TEST_CHECK(test, tc::Same(p.input.data[i], before.input.data[i]));
  }
  p.output.Guards(test);
}
template <typename Input>
void Run(tc::TestContext& test) {
  for (asc::extent_t n : {0, 1, 2, 5, 13}) {
    for (auto triangle : {tc::kUpper, tc::kLower}) {
      for (std::size_t shift = 0; shift < 16; ++shift) {
        for (int kind = 0; kind < 3; ++kind) {
          Case<Input>(test, n, triangle, shift, kind);
        }
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  tc::TestContext test;
  Run<double>(test);
  Run<std::complex<double>>(test);
  std::printf("960 guarded LAT2 ABI cases, integer bytes=%zu\n",
              sizeof(lapack_int));
  return test.Finish();
}
