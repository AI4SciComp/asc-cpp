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
#include "indefinite_rook_driver_native.h"
#include "indefinite_rook_driver_test_support.h"
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
  std::array<lapack_int, 3> native_pivots{-23, -29, -31};
  std::array<T, 66> native_work;
  native_work.fill(T{-37});
  lapack_int info = std::numeric_limits<lapack_int>::min();
  const lapack_int entries = preferred ? 64 : 1;
  asc_rook_driver_test::Native(hermitian, triangle == base::kUpper ? 'U' : 'L',
                               1, nrhs, native_a.data() + 1, 1,
                               native_pivots.data() + 1, native_b.data() + 1, 1,
                               native_work.data() + 1, entries, info);
  ASC_DENSE_TEST_EQ(test, info, report.native_info.value_or(-1));
  ASC_DENSE_TEST_EQ(test, native_a, a);
  ASC_DENSE_TEST_CHECK(test, Same(native_b[1], b[1]));
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
  std::array<asc::index_t, 3> pivots{-13, -17, -19};
  const auto original_a = a;
  const auto original_b = b;
  auto matrix = base::Matrix(a, 1, 1, layout, 1);
  auto rhs = base::Matrix(b, 1, nrhs, rhs_layout, 1);
  auto pivot = base::Pivots(pivots, 1);
  const auto plan = base::Take(asc_rook_driver_test::Query(
      provider, triangle, hermitian, matrix, pivot, rhs));
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, preferred ? -1 : 1);
  asc::LapackReport report;
  const auto result = base::WithoutAllocation(test, [&] {
    return asc_rook_driver_test::Driver(provider, triangle, hermitian, matrix,
                                        pivot, rhs, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, result.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_EQ(test, pivots[1], 1);
  ASC_DENSE_TEST_EQ(test, a, original_a);
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
  std::printf("rook driver range cases=%d fidelity=%d\n", cases, fidelity);
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
