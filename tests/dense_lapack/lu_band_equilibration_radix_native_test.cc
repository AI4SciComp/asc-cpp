#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_expert_test_support.h"
#include "lu_band_test_support.h"
namespace {
namespace support = asc_lu_band_expert_test;
using support::TestContext;
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kCall = LAPACK_sgbequb;
};
template <>
struct Native<double> {
  static constexpr auto kCall = LAPACK_dgbequb;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kCall = LAPACK_cgbequb;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kCall = LAPACK_zgbequb;
};
template <typename T>
void One(TestContext& test, T value, bool mathematical) {
  using Real = asc::DenseBlasRealType<T>;
  const lapack_int n = 1;
  const lapack_int band = 0;
  std::array<T, 3> ab{support::Value<T>(-91), value, support::Value<T>(-93)};
  const auto before = ab;
  std::array<Real, 3> rows{Real{-101}, Real{-103}, Real{-107}};
  std::array<Real, 3> columns{Real{-109}, Real{-113}, Real{-127}};
  std::array<Real, 9> stats{Real{-149}, Real{-131}, Real{-151},
                            Real{-157}, Real{-137}, Real{-163},
                            Real{-167}, Real{-139}, Real{-173}};
  Real& rowcnd = stats[1];
  Real& colcnd = stats[4];
  Real& amax = stats[7];
  lapack_int info = std::numeric_limits<lapack_int>::min();
  support::WithoutAllocation(test, [&] {
    Native<T>::kCall(&n, &n, &band, &band, ab.data() + 1, &n, rows.data() + 1,
                     columns.data() + 1, &rowcnd, &colcnd, &amax, &info);
    return true;
  });
  for (std::size_t i = 0; i < ab.size(); ++i) {
    ASC_DENSE_TEST_CHECK(test,
                         asc_lu_band_test::SameScalarBytes(ab[i], before[i]));
  }
  ASC_DENSE_TEST_EQ(test, stats[0], Real{-149});
  ASC_DENSE_TEST_EQ(test, stats[2], Real{-151});
  ASC_DENSE_TEST_EQ(test, stats[3], Real{-157});
  ASC_DENSE_TEST_EQ(test, stats[5], Real{-163});
  ASC_DENSE_TEST_EQ(test, stats[6], Real{-167});
  ASC_DENSE_TEST_EQ(test, stats[8], Real{-173});
  ASC_DENSE_TEST_EQ(test, rows.front(), Real{-101});
  ASC_DENSE_TEST_EQ(test, rows.back(), Real{-107});
  ASC_DENSE_TEST_EQ(test, columns.front(), Real{-109});
  ASC_DENSE_TEST_EQ(test, columns.back(), Real{-127});
  ASC_DENSE_TEST_CHECK(test, info >= 0 && info <= 2);
  const auto wide = support::ToWide(value);
  const auto magnitude = std::abs(wide.real()) + std::abs(wide.imag());
  std::printf(
      "raw GBEQUB value=(%.18Lg,%.18Lg) INFO=%lld R=%.18Lg C=%.18Lg "
      "ROWCND=%.18Lg COLCND=%.18Lg AMAX=%.18Lg original=%.18Lg\n",
      wide.real(), wide.imag(), static_cast<long long>(info),
      static_cast<long double>(rows[1]), static_cast<long double>(columns[1]),
      static_cast<long double>(rowcnd), static_cast<long double>(colcnd),
      static_cast<long double>(amax), magnitude);
  if (mathematical) {
    ASC_DENSE_TEST_EQ(test, info, 0);
    ASC_DENSE_TEST_EQ(test, rowcnd, Real{1});
    ASC_DENSE_TEST_EQ(test, colcnd, Real{1});
    ASC_DENSE_TEST_CHECK(test, std::isfinite(amax));
    ASC_DENSE_TEST_CHECK(
        test, std::abs(static_cast<long double>(amax) - magnitude) <=
                  64 * std::numeric_limits<Real>::epsilon() * magnitude);
    ASC_DENSE_TEST_CHECK(test, rows[1] > 0 && std::isfinite(rows[1]) &&
                                   columns[1] > 0 && std::isfinite(columns[1]));
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  TestContext test;
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "math" && mode != "nonfinite") {
    return 2;
  }
  const auto run = [&]<typename T>() {
    using Real = asc::DenseBlasRealType<T>;
    if (mode == "math") {
      for (const Real value :
           {Real{1}, Real{3}, std::numeric_limits<Real>::min(),
            std::numeric_limits<Real>::min() / Real{8},
            std::numeric_limits<Real>::max(),
            std::numeric_limits<Real>::denorm_min()}) {
        One(test, support::Value<T>(value), true);
      }
    } else {
      const auto nan = std::numeric_limits<Real>::quiet_NaN();
      const auto infinity = std::numeric_limits<Real>::infinity();
      One(test, support::Value<T>(nan), false);
      One(test, support::Value<T>(infinity), false);
      One(test, support::Value<T>(-infinity), false);
      if constexpr (!std::is_same_v<T, Real>) {
        One(test,
            support::Value<T>(std::numeric_limits<Real>::max(),
                              std::numeric_limits<Real>::max()),
            false);
        One(test, support::Value<T>(Real{1}, nan), false);
      }
    }
  };
  if (scalar == "s") {
    run.template operator()<float>();
  } else if (scalar == "d") {
    run.template operator()<double>();
  } else if (scalar == "c") {
    run.template operator()<std::complex<float>>();
  } else if (scalar == "z") {
    run.template operator()<std::complex<double>>();
  } else {
    return 2;
  }
  return test.Finish();
}
