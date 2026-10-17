#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_REFINEMENT_FIDELITY_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_REFINEMENT_FIDELITY_H_
#include <array>
#include <cstddef>
#include <limits>

#include "asc/dense/blas.h"
#include "indefinite_packed_refinement_native.h"
#include "indefinite_packed_refinement_test_support.h"
#include "src/dense/lapack/internal_indefinite.h"
#include "tests/dense/test_support.h"

namespace asc_packed_refinement_test {
template <typename T>
struct NativeCase {
  using Real = asc::DenseBlasRealType<T>;
  static constexpr lapack_int kGuard =
      std::numeric_limits<lapack_int>::max() - 79;
  std::array<T, 2200> original;
  std::array<T, 2200> factors;
  std::array<T, 220> b;
  std::array<T, 220> x;
  std::array<T, 197> work;
  std::array<Real, 67> rwork;
  std::array<lapack_int, 68> pivots;
  std::array<lapack_int, 68> iwork;
  std::array<Real, 5> ferr;
  std::array<Real, 5> berr;
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  lapack_int n;
  lapack_int nrhs;
  lapack_int leading;
  char triangle;
  bool hermitian;
  explicit NativeCase(const Fixture<T>& sample)
      : n(sample.system.a.n),
        nrhs(sample.system.nrhs),
        leading(n + 2),
        triangle(sample.system.a.triangle == kUpper ? 'U' : 'L'),
        hermitian(sample.system.a.hermitian) {
    original.fill(Value<T>(-317, 37));
    factors.fill(Value<T>(-331, 41));
    b.fill(Value<T>(-337, 43));
    x.fill(Value<T>(-347, 47));
    work.fill(Value<T>(-349, 53));
    rwork.fill(Real{-353});
    pivots.fill(kGuard);
    iwork.fill(kGuard);
    ferr.fill(Real{-359});
    berr.fill(Real{-367});
    FillPacked(sample);
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        b[1 + i + j * leading] =
            sample.system.original_rhs[sample.system.Offset(i, j)];
        x[1 + i + j * leading] = sample.initial_x[sample.Offset(i, j)];
      }
    }
  }
  void FillPacked(const Fixture<T>& sample) {
    std::size_t slot = 1;
    for (int j = 0; j < n; ++j) {
      pivots[j + 1] = static_cast<lapack_int>(sample.system.a.pivots[j + 1]);
      for (int i = 0; i < n; ++i) {
        if (sample.system.a.Selected(i, j)) {
          factors[slot++] = sample.system.a.a[sample.system.a.Offset(i, j)];
        }
      }
    }
    slot = 1;
    for (int major = 0; major < n; ++major) {
      for (int minor = 0; minor < n; ++minor) {
        const int i = sample.original_layout == kColumn ? minor : major;
        const int j = sample.original_layout == kColumn ? major : minor;
        if (sample.system.a.Selected(i, j)) {
          const int target = triangle == 'U' ? j * (j + 1) / 2 + i
                                             : j * n - j * (j - 1) / 2 + i - j;
          original[1 + target] = sample.original[slot++];
        }
      }
    }
  }
  void Execute() {
    Native(hermitian, &triangle, &n, &nrhs, original.data() + 1,
           factors.data() + 1, pivots.data() + 1, b.data() + 1, &leading,
           x.data() + 1, &leading, ferr.data() + 1, berr.data() + 1,
           work.data() + 1, rwork.data() + 1, iwork.data() + 1, &info[1]);
  }
  void Guards(TestContext& test, const NativeCase<T>& before) const {
    ASC_DENSE_TEST_CHECK(
        test,
        EqualBytes(original.data(), before.original.data(), sizeof(original)));
    ASC_DENSE_TEST_CHECK(test, EqualBytes(factors.data(), before.factors.data(),
                                          sizeof(factors)));
    ASC_DENSE_TEST_EQ(test, b, before.b);
    ASC_DENSE_TEST_EQ(test, pivots, before.pivots);
    ASC_DENSE_TEST_EQ(test, info[0], kGuard);
    ASC_DENSE_TEST_EQ(test, info[1], 0);
    ASC_DENSE_TEST_EQ(test, info[2], kGuard);
    for (std::size_t i = 0; i < ferr.size(); ++i) {
      if (i == 0 || i > static_cast<std::size_t>(nrhs)) {
        ASC_DENSE_TEST_EQ(test, ferr[i], before.ferr[i]);
        ASC_DENSE_TEST_EQ(test, berr[i], before.berr[i]);
      }
    }
    for (std::size_t i = 0; i < x.size(); ++i) {
      bool active = false;
      for (int j = 0; j < nrhs; ++j) {
        active = active || (i >= 1 + (static_cast<std::size_t>(j) *
                                      static_cast<std::size_t>(leading)) &&
                            i < 1 + (static_cast<std::size_t>(j) *
                                         static_cast<std::size_t>(leading) +
                                     static_cast<std::size_t>(n)));
      }
      if (!active) {
        ASC_DENSE_TEST_EQ(test, x[i], before.x[i]);
      }
    }
    const auto active_n = n != 0 && nrhs != 0 ? static_cast<std::size_t>(n) : 0;
    const auto scalar_count = (asc::DenseBlasComplex<T> ? 2 : 3) * active_n;
    for (std::size_t i = 0; i < work.size(); ++i) {
      if (i == 0 || i > scalar_count) {
        ASC_DENSE_TEST_EQ(test, work[i], before.work[i]);
      }
    }
    for (std::size_t i = 0; i < rwork.size(); ++i) {
      if (!asc::DenseBlasComplex<T> || i == 0 || i > active_n) {
        ASC_DENSE_TEST_EQ(test, rwork[i], before.rwork[i]);
      }
    }
    for (std::size_t i = 0; i < iwork.size(); ++i) {
      if (asc::DenseBlasComplex<T> || i == 0 || i > active_n) {
        ASC_DENSE_TEST_EQ(test, iwork[i], before.iwork[i]);
      }
    }
  }
};

template <typename T>
void Fidelity(TestContext& test, const Fixture<T>& sample) {
  NativeCase<T> raw(sample);
  const auto before = raw;
  raw.Execute();
  raw.Guards(test, before);
  for (int j = 0; j < raw.nrhs; ++j) {
    ASC_DENSE_TEST_CHECK(test, EqualBytes(&sample.ferr[j + 1], &raw.ferr[j + 1],
                                          sizeof(raw.ferr[0])));
    ASC_DENSE_TEST_CHECK(test, EqualBytes(&sample.berr[j + 1], &raw.berr[j + 1],
                                          sizeof(raw.berr[0])));
    for (int i = 0; i < raw.n; ++i) {
      ASC_DENSE_TEST_CHECK(
          test, EqualBytes(&sample.x[sample.Offset(i, j)],
                           &raw.x[1 + i + j * raw.leading], sizeof(T)));
    }
  }
}
}  // namespace asc_packed_refinement_test
#endif
