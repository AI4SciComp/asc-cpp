#ifndef ASC_TESTS_DENSE_LAPACK_DMD_QR_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_DMD_QR_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <complex>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack_dmd_qr.h"
#include "dmd_test_support.h"  // IWYU pragma: export

namespace asc_dmd_qr_test {
using asc_dmd_test::kColumn;
using asc_dmd_test::kHost;
using asc_dmd_test::kRow;
using asc_dmd_test::Real;
using asc_dmd_test::Rhs;
using asc_dmd_test::Scratch;
using asc_dmd_test::Take;
using asc_dmd_test::TestContext;
using asc_dmd_test::Value;
using asc_dmd_test::Vector;
using asc_dmd_test::WithoutAllocation;
template <typename T>
struct Problem {
  Rhs<T> f, x, y, z, b, v, s;
  std::array<std::complex<Real<T>>, 66> eigen{};
  std::array<Real<T>, 66> singular{}, residual{};
  std::array<T, 66> tau{};
  asc::index_t rank = -73;
  Problem(asc::extent_t m, asc::extent_t n,
          const std::array<asc::DenseBlasLayout, 7>& layouts)
      : f(m, n, layouts[0]),
        x(n, std::max<asc::extent_t>(0, n - 1), layouts[1]),
        y(n, n, layouts[2]),
        z(m, std::max<asc::extent_t>(0, n - 1), layouts[3]),
        b(n, std::max<asc::extent_t>(0, n - 1), layouts[4]),
        v(std::max<asc::extent_t>(0, n - 1), std::max<asc::extent_t>(0, n - 1),
          layouts[5]),
        s(std::max<asc::extent_t>(0, n - 1), std::max<asc::extent_t>(0, n - 1),
          layouts[6]) {
    eigen.fill({-41, 3});
    singular.fill(-43);
    residual.fill(-47);
    tau.fill(Value<T>(-53, 7));
  }
  asc::LapackDmdQrBuffers<T> Buffers() {
    const auto p = std::max<asc::extent_t>(0, f.columns - 1);
    return {f.View(),
            x.View(),
            y.View(),
            z.View(),
            b.View(),
            v.View(),
            s.View(),
            Vector(eigen, p),
            Vector(singular, p),
            Vector(residual, p),
            Vector(tau, f.columns)};
  }
  void Initialize() {
    for (asc::extent_t j = 0; j < f.columns; ++j) {
      for (asc::extent_t i = 0; i < f.rows; ++i) {
        T value = i < 2 ? T{1} : T{0};
        const auto multiplier = Value<T>(2 + i, i == 0 ? 1 : -1);
        for (asc::extent_t k = 0; k < j; ++k) {
          value *= multiplier;
        }
        f.At(i, j) = value;
      }
    }
  }
  void Guards(TestContext& test) const {
    f.Guards(test);
    x.Guards(test);
    y.Guards(test);
    z.Guards(test);
    b.Guards(test);
    v.Guards(test);
    s.Guards(test);
  }
};
}  // namespace asc_dmd_qr_test
#endif  // ASC_TESTS_DENSE_LAPACK_DMD_QR_TEST_SUPPORT_H_
