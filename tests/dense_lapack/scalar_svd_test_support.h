#ifndef ASC_TESTS_DENSE_LAPACK_SCALAR_SVD_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_SCALAR_SVD_TEST_SUPPORT_H_
#include <array>
#include <cstddef>
#include <type_traits>

#include "../../src/dense/lapack/internal_scalar_svd_prototypes.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_scalar_svd.h"
#include "installed_lu/scalar_svd_numerical.h"  // IWYU pragma: export
#include "tridiagonal_test_support.h"           // IWYU pragma: export
namespace asc_scalar_svd_test {
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::WithoutAllocation;
constexpr auto kHost = asc::MemorySpace::kHost;
template <typename T>
struct Output {
  std::array<std::array<T, 3>, 6> values{};
  Output() {
    for (auto& item : values) {
      item = {T{71}, T{-19}, T{73}};
    }
  }
  auto View(std::size_t i) {
    return Take(asc::DenseBlasVectorView<T>::Create(
        values[i].data() + 1, 1, 1,
        {values[i].data(), sizeof(values[i]), kHost}));
  }
  auto Views() {
    return std::array{View(0), View(1), View(2), View(3), View(4), View(5)};
  }
};
template <typename T>
asc::Status Execute(const asc::ReferenceLapackProvider& provider, T f, T g, T h,
                    const std::array<asc::DenseBlasVectorView<T>, 6>& v,
                    asc::LapackReport& report) {
  return asc::Lasv2(provider, f, g, h, v[0], v[1], v[2], v[3], v[4], v[5],
                    report);
}
template <typename T>
void Native(T& f, T& g, T& h, T& small, T& large, T& sr, T& cr, T& sl, T& cl) {
  if constexpr (std::is_same_v<T, float>) {
    ASC_LAPACK_slasv2(&f, &g, &h, &small, &large, &sr, &cr, &sl, &cl);
  } else {
    ASC_LAPACK_dlasv2(&f, &g, &h, &small, &large, &sr, &cr, &sl, &cl);
  }
}
}  // namespace asc_scalar_svd_test
#endif  // ASC_TESTS_DENSE_LAPACK_SCALAR_SVD_TEST_SUPPORT_H_
