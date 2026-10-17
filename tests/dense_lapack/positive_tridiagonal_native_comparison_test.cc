#include <array>
#include <cmath>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <utility>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "installed_lu/normal_return_guard.h"
#include "tridiagonal_test_support.h"

namespace {
namespace support = asc_tridiagonal_test;
using support::Take;
using support::TestContext;
using support::Vector;
template <typename T>
void Direct(asc::DenseBlasRealType<T>* d, T* e, T* b, lapack_int* info) {
  const lapack_int n = 1;
  const lapack_int nrhs = 2;
  const char lower = 'L';
  if constexpr (std::same_as<T, float>) {
    LAPACK_spttrf(&n, d, e, info);
    if (*info == 0) {
      LAPACK_spttrs(&n, &nrhs, d, e, b, &n, info);
    }
  } else if constexpr (std::same_as<T, double>) {
    LAPACK_dpttrf(&n, d, e, info);
    if (*info == 0) {
      LAPACK_dpttrs(&n, &nrhs, d, e, b, &n, info);
    }
  } else if constexpr (std::same_as<T, std::complex<float>>) {
    LAPACK_cpttrf(&n, d, e, info);
    if (*info == 0) {
      LAPACK_cpttrs(&lower, &n, &nrhs, d, e, b, &n, info);
    }
  } else {
    LAPACK_zpttrf(&n, d, e, info);
    if (*info == 0) {
      LAPACK_zpttrs(&lower, &n, &nrhs, d, e, b, &n, info);
    }
  }
}
bool Same(long double a, long double b) {
  return (std::isnan(a) && std::isnan(b)) || a == b;
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const Real a :
       {std::numeric_limits<Real>::denorm_min(),
        2 * std::numeric_limits<Real>::denorm_min(),
        std::numeric_limits<Real>::min(), std::numeric_limits<Real>::max()}) {
    Real direct_d = a;
    T direct_e{};
    std::array<T, 2> direct_b{T{a}, T{a}};
    lapack_int info = -99;
    Direct<T>(&direct_d, &direct_e, direct_b.data(), &info);
    const auto direct = support::ToWide(direct_b[0]);
    std::printf(
        "direct PT real_bytes=%zu scalar_bytes=%zu integer_bytes=%zu a=%La "
        "X=(%La,%La) INFO=%lld exact_X=1\n",
        sizeof(Real), sizeof(T), sizeof(lapack_int),
        static_cast<long double>(a), direct.real(), direct.imag(),
        static_cast<long long>(info));
    ASC_DENSE_TEST_EQ(test, info, 0);
    for (const auto layout : {support::kColumn, support::kRow}) {
      std::array<Real, 3> d{Real{-37}, a, Real{-37}};
      std::array<T, 2> e{};
      const auto matrix =
          Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
              Vector(d, 1), Vector(e, 0)));
      const auto plan = Take(asc::QueryPttrfWorkspace(provider, matrix));
      asc::LapackReport report;
      ASC_DENSE_TEST_CHECK(test,
                           asc::Pttrf(provider, matrix, plan, {}, report).ok());
      const auto factor =
          Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::FromRaw(
              provider, asc::DenseBlasTriangle::kLower,
              Vector(std::as_const(d), 1), Vector(std::as_const(e), 0)));
      support::Rhs<T> rhs(1, 2, layout);
      rhs.At(0, 0) = T{a};
      rhs.At(0, 1) = T{a};
      const auto solve =
          Take(asc::QueryPttrsWorkspace(provider, factor, rhs.View()));
      support::Scratch<T> scratch;
      const auto status = asc::Pttrs(provider, factor, rhs.View(), solve,
                                     scratch.Workspace(solve), report);
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), info);
      const bool finite =
          std::isfinite(direct.real()) && std::isfinite(direct.imag());
      ASC_DENSE_TEST_EQ(test, status.ok(), finite);
      for (int j = 0; j < 2; ++j) {
        const auto observed = support::ToWide(rhs.At(0, j));
        const auto expected =
            support::ToWide(direct_b[static_cast<std::size_t>(j)]);
        ASC_DENSE_TEST_CHECK(test, Same(observed.real(), expected.real()) &&
                                       Same(observed.imag(), expected.imag()));
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  TestContext test;
  Run<float>(test, provider);
  Run<double>(test, provider);
  Run<std::complex<float>>(test, provider);
  Run<std::complex<double>>(test, provider);
  return test.Finish();
}
