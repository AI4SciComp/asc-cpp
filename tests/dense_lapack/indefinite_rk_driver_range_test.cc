#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_driver_native.h"
#include "indefinite_rk_driver_test_support.h"
#include "indefinite_rk_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_indefinite_rook_test;
using base::TestContext;
template <typename T>
bool Same(T a, T b) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return Same(a.real(), b.real()) && Same(a.imag(), b.imag());
  } else {
    return a == b || (std::isnan(a) && std::isnan(b));
  }
}

template <typename T>
void Fidelity(TestContext& test, bool hermitian,
              asc::DenseBlasTriangle triangle, bool preferred, int nrhs,
              const std::array<T, 3>& original_a,
              const std::array<T, 3>& original_b, const std::array<T, 3>& a,
              const std::array<T, 3>& b,
              const std::array<asc::index_t, 3>& pivots,
              const asc::LapackReport& report) {
  auto native_a = original_a;
  auto native_b = original_b;
  std::array<T, 3> native_e{T{-41}, T{-43}, T{-47}};
  std::array<lapack_int, 3> native_pivots{-23, -29, -31};
  std::array<T, 66> native_work;
  native_work.fill(T{-37});
  lapack_int info = std::numeric_limits<lapack_int>::min();
  const lapack_int entries = preferred ? 64 : 1;
  asc_rk_driver_test::Native(hermitian, triangle == base::kUpper ? 'U' : 'L', 1,
                             nrhs, native_a.data() + 1, 1, native_e.data() + 1,
                             native_pivots.data() + 1, native_b.data() + 1, 1,
                             native_work.data() + 1, entries, info);
  ASC_DENSE_TEST_EQ(test, info, report.native_info.value_or(-1));
  ASC_DENSE_TEST_EQ(test, native_a, a);
  ASC_DENSE_TEST_CHECK(test, Same(native_b[1], b[1]));
  ASC_DENSE_TEST_EQ(test, native_e, (std::array<T, 3>{T{-41}, T{}, T{-47}}));
  ASC_DENSE_TEST_EQ(test, native_pivots[1], pivots[1]);
  ASC_DENSE_TEST_EQ(test, native_pivots.front(), -23);
  ASC_DENSE_TEST_EQ(test, native_pivots.back(), -31);
  ASC_DENSE_TEST_EQ(test, native_b.front(), original_b.front());
  ASC_DENSE_TEST_EQ(test, native_b.back(), original_b.back());
  ASC_DENSE_TEST_EQ(test, native_work[1], T{64});
  ASC_DENSE_TEST_EQ(test, native_work.front(), T{-37});
  for (std::size_t i = static_cast<std::size_t>(entries) + 1;
       i < native_work.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, native_work[i], T{-37});
  }
}

template <typename T>
void Check(TestContext& test, const asc::ReferenceLapackProvider& provider,
           bool hermitian, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout,
           bool preferred, int nrhs, asc::DenseBlasRealType<T> scale,
           bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 3> a{T{-3}, T{scale}, T{-5}};
  std::array<T, 3> b{T{-7}, T{scale}, T{-11}};
  std::array<T, 3> e{T{-41}, T{-43}, T{-47}};
  std::array<asc::index_t, 3> pivots{-13, -17, -19};
  const auto original_a = a;
  const auto original_b = b;
  auto matrix = base::Matrix(a, 1, 1, layout, 1);
  auto rhs = base::Matrix(b, 1, nrhs, rhs_layout, 1);
  auto pivot = base::Pivots(pivots, 1);
  const auto extra = asc_rk_test::OffDiagonal(e, 1);
  const auto plan = base::Take(asc_rk_driver_test::Query(
      provider, triangle, hermitian, matrix, extra, pivot, rhs));
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, preferred ? -1 : 1);
  asc::LapackReport report;
  const auto result = base::WithoutAllocation(test, [&] {
    return asc_rk_driver_test::Driver(provider, triangle, hermitian, matrix,
                                      extra, pivot, rhs, plan, workspace,
                                      report);
  });
  ASC_DENSE_TEST_CHECK(test, result.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_EQ(test, pivots[1], 1);
  ASC_DENSE_TEST_EQ(test, a, original_a);
  ASC_DENSE_TEST_EQ(test, e, (std::array<T, 3>{T{-41}, T{}, T{-47}}));
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  if (fidelity) {
    Fidelity(test, hermitian, triangle, preferred, nrhs, original_a, original_b,
             a, b, pivots, report);
  } else if (nrhs != 0) {
    const auto x = base::ToWide(b[1]);
    ASC_DENSE_TEST_CHECK(test,
                         std::isfinite(x.real()) && std::isfinite(x.imag()));
    ASC_DENSE_TEST_CHECK(test, std::abs(x - base::Wide{1}) <=
                                   64 * std::numeric_limits<Real>::epsilon());
    // A=B=scale is nonsingular and well conditioned. Form the mathematical
    // residual in wider arithmetic; avoid a provider solve as the oracle.
    const long double wide_scale = scale;
    ASC_DENSE_TEST_CHECK(
        test, std::abs(wide_scale * x - wide_scale) <=
                  64 * std::numeric_limits<Real>::epsilon() * wide_scale);
  }
  if (nrhs == 0) {
    ASC_DENSE_TEST_EQ(test, b, original_b);
  }
  ASC_DENSE_TEST_EQ(test, b.front(), original_b.front());
  ASC_DENSE_TEST_EQ(test, b.back(), original_b.back());
  ASC_DENSE_TEST_EQ(test, pivots.front(), -13);
  ASC_DENSE_TEST_EQ(test, pivots.back(), -19);
  scratch.Guards(test, workspace);
  const auto x = base::ToWide(b[1]);
  std::printf(
      "range he=%d uplo=%d A=%d B=%d preferred=%d nrhs=%d scale=%La "
      "X=(%La,%La) INFO=%lld\n",
      hermitian, static_cast<int>(triangle), static_cast<int>(layout),
      static_cast<int>(rhs_layout), preferred, nrhs,
      static_cast<long double>(scale), x.real(), x.imag(),
      static_cast<long long>(report.native_info.value_or(-1)));
}

template <typename T, typename Offset>
void BlockFidelity(TestContext& test, bool hermitian,
                   asc::DenseBlasTriangle triangle, bool preferred, T upper,
                   T lower, int second, const Offset& offset,
                   const std::array<T, 8>& a, const std::array<T, 8>& b,
                   const std::array<T, 4>& e,
                   const std::array<asc::index_t, 4>& pivots) {
  std::array<T, 4> native_a{T{}, lower, upper, T{}};
  std::array<T, 2> native_b{upper, lower};
  std::array<T, 2> native_e{};
  std::array<lapack_int, 2> native_pivots{};
  std::array<T, 128> work{};
  lapack_int info = std::numeric_limits<lapack_int>::min();
  asc_rk_driver_test::Native(hermitian, triangle == base::kUpper ? 'U' : 'L', 2,
                             1, native_a.data(), 2, native_e.data(),
                             native_pivots.data(), native_b.data(), 2,
                             work.data(), preferred ? 128 : 1, info);
  ASC_DENSE_TEST_EQ(test, info, 0);
  ASC_DENSE_TEST_CHECK(test,
                       Same(b[1], native_b[0]) && Same(b[second], native_b[1]));
  for (int j = 0; j < 2; ++j) {
    ASC_DENSE_TEST_EQ(test, pivots[j + 1], native_pivots[j]);
    ASC_DENSE_TEST_CHECK(test, Same(e[j + 1], native_e[j]));
    for (int i = 0; i < 2; ++i) {
      if ((triangle == base::kUpper && i <= j) ||
          (triangle == base::kLower && i >= j)) {
        ASC_DENSE_TEST_CHECK(test, Same(a[offset(i, j)], native_a[j * 2 + i]));
      }
    }
  }
}

// A pure 2-block with finite entries has the exact solution (1,1). Wide
// residuals avoid overflowing the test oracle at either large input scale.
template <typename T>
void LargeBlock(TestContext& test, const asc::ReferenceLapackProvider& provider,
                bool hermitian, asc::DenseBlasTriangle triangle,
                asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout,
                bool preferred, asc::DenseBlasRealType<T> scale,
                bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  const T upper{scale, scale};
  const T lower = hermitian ? std::conj(upper) : upper;
  std::array<T, 8> a;
  a.fill(T{-53});
  const auto offset = [layout](int i, int j) {
    return 1 + (layout == base::kColumn ? j * 3 + i : i * 3 + j);
  };
  a[offset(0, 0)] = T{};
  a[offset(1, 1)] = T{};
  if (triangle == base::kUpper) {
    a[offset(0, 1)] = upper;
  } else {
    a[offset(1, 0)] = lower;
  }
  std::array<T, 8> b;
  b.fill(T{-59});
  const int second = rhs_layout == base::kColumn ? 2 : 3;
  b[1] = upper;
  b[second] = lower;
  const auto original_a = a;
  const auto original_b = b;
  std::array<T, 4> e{T{-61}, T{-67}, T{-67}, T{-71}};
  std::array<asc::index_t, 4> pivots{-73, -79, -79, -83};
  const auto matrix = base::Matrix(a, 2, 2, layout, 3);
  const auto rhs =
      base::Matrix(b, 2, 1, rhs_layout, rhs_layout == base::kColumn ? 3 : 2);
  const auto extra = asc_rk_test::OffDiagonal(e, 2);
  const auto pivot = base::Pivots(pivots, 2);
  const auto plan = base::Take(asc_rk_driver_test::Query(
      provider, triangle, hermitian, matrix, extra, pivot, rhs));
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, preferred ? -1 : 1);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, base::WithoutAllocation(test, [&] {
                               return asc_rk_driver_test::Driver(
                                   provider, triangle, hermitian, matrix, extra,
                                   pivot, rhs, plan, workspace, report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  if (fidelity) {
    BlockFidelity(test, hermitian, triangle, preferred, upper, lower, second,
                  offset, a, b, e, pivots);
  } else {
    const base::Wide x0 = base::ToWide(b[1]);
    const base::Wide x1 = base::ToWide(b[second]);
    const long double tolerance = 64 * std::numeric_limits<Real>::epsilon();
    for (const auto x : {x0, x1}) {
      ASC_DENSE_TEST_CHECK(test,
                           std::isfinite(x.real()) && std::isfinite(x.imag()));
      ASC_DENSE_TEST_CHECK(test, std::abs(x - base::Wide{1}) <= tolerance);
    }
    ASC_DENSE_TEST_CHECK(test,
                         std::abs(base::ToWide(upper) * (x1 - base::Wide{1})) <=
                             tolerance * std::abs(base::ToWide(upper)));
    ASC_DENSE_TEST_CHECK(test,
                         std::abs(base::ToWide(lower) * (x0 - base::Wide{1})) <=
                             tolerance * std::abs(base::ToWide(lower)));
  }
  for (int k = 0; k < 8; ++k) {
    if (k != offset(0, 0) && k != offset(1, 1) &&
        k != (triangle == base::kUpper ? offset(0, 1) : offset(1, 0))) {
      ASC_DENSE_TEST_EQ(test, a[k], original_a[k]);
    }
    if (k != 1 && k != second) {
      ASC_DENSE_TEST_EQ(test, b[k], original_b[k]);
    }
  }
  ASC_DENSE_TEST_EQ(test, e.front(), T{-61});
  ASC_DENSE_TEST_EQ(test, e.back(), T{-71});
  ASC_DENSE_TEST_EQ(test, pivots.front(), -73);
  ASC_DENSE_TEST_EQ(test, pivots.back(), -83);
  scratch.Guards(test, workspace);
  std::printf(
      "RK driver large block scale=%La he=%d tri=%d A=%d B=%d preferred=%d\n",
      static_cast<long double>(scale), hermitian, static_cast<int>(triangle),
      static_cast<int>(layout), static_cast<int>(rhs_layout), preferred);
}

template <typename T>
int Run(bool hermitian, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const auto rhs_layout : {base::kColumn, base::kRow}) {
        for (const bool preferred : {false, true}) {
          if constexpr (asc::DenseBlasComplex<T>) {
            for (const Real fraction : {Real{0.25}, Real{0.75}}) {
              LargeBlock<T>(test, provider, hermitian, triangle, layout,
                            rhs_layout, preferred,
                            fraction * std::numeric_limits<Real>::max(),
                            fidelity);
              ++cases;
            }
          }
          for (const int nrhs : {0, 1}) {
            for (const Real scale :
                 {2 * std::numeric_limits<Real>::min(),
                  std::numeric_limits<Real>::min() / 1024,
                  2 * std::numeric_limits<Real>::denorm_min(),
                  std::numeric_limits<Real>::max()}) {
              Check<T>(test, provider, hermitian, triangle, layout, rhs_layout,
                       preferred, nrhs, scale, fidelity);
              ++cases;
            }
          }
        }
      }
    }
  }
  std::printf("RK driver range cases=%d fidelity=%d\n", cases, fidelity);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity";
  if (scalar == "s") {
    return Run<float>(false, fidelity);
  }
  if (scalar == "d") {
    return Run<double>(false, fidelity);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false, fidelity);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false, fidelity);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true, fidelity);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true, fidelity);
  }
  return 2;
}
