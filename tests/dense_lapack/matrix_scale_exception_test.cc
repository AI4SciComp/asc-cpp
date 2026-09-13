#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <type_traits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "internal_tridiagonal.h"
#include "matrix_scale_test_support.h"
#include "tridiagonal_test_support.h"

namespace {
namespace support = asc_tridiagonal_test;
using support::Take;
using support::TestContext;

template <typename T>
T Direct(TestContext& test, T value, asc::DenseBlasRealType<T> from,
         asc::DenseBlasRealType<T> to) {
  const char type = 'G';
  const lapack_int zero = 0;
  const lapack_int one = 1;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_slascl(&type, &zero, &zero, &from, &to, &one, &one, &value, &one,
                  &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dlascl(&type, &zero, &zero, &from, &to, &one, &one, &value, &one,
                  &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_clascl(&type, &zero, &zero, &from, &to, &one, &one, &value, &one,
                  &info);
  } else {
    LAPACK_zlascl(&type, &zero, &zero, &from, &to, &one, &one, &value, &one,
                  &info);
  }
  ASC_DENSE_TEST_EQ(test, info, 0);
  return value;
}

template <typename Real>
bool SameComponent(Real observed, Real expected) {
  if (std::isnan(expected)) {
    return std::isnan(observed);
  }
  return observed == expected &&
         std::signbit(observed) == std::signbit(expected);
}

template <typename T>
void Compare(TestContext& test, T observed, T expected) {
  const auto a = support::ToWide(observed);
  const auto b = support::ToWide(expected);
  ASC_DENSE_TEST_CHECK(test, SameComponent(a.real(), b.real()));
  ASC_DENSE_TEST_CHECK(test, SameComponent(a.imag(), b.imag()));
}

template <typename T>
int Run() {
  using Real = asc::DenseBlasRealType<T>;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
  constexpr Real kNaN = std::numeric_limits<Real>::quiet_NaN();
  // These exercise the provider's exceptional branches, outside the separate
  // finite-final-result numerical assertions. Agreement here is fidelity only.
  const std::array scales{std::array<Real, 2>{kInfinity, 1},
                          std::array<Real, 2>{-kInfinity, 1},
                          std::array<Real, 2>{kInfinity, -kInfinity},
                          std::array<Real, 2>{kInfinity, kInfinity},
                          std::array<Real, 2>{1, kInfinity},
                          std::array<Real, 2>{-1, kInfinity},
                          std::array<Real, 2>{-1, -kInfinity},
                          std::array<Real, 2>{1, 0},
                          std::array<Real, 2>{1, 2}};
  for (auto layout : {support::kColumn, support::kRow}) {
    asc_scale_test::Problem<T> problem('G', 1, 1, 0, 0, layout);
    const auto plan = Take(problem.Query(provider));
    const auto workspace = problem.Workspace(plan);
    for (const auto& scale : scales) {
      for (Real component :
           {Real{1}, Real{-1}, Real{0}, -Real{0}, kInfinity, kNaN}) {
        const T input = support::Value<T>(component, component);
        const auto expected = Direct(test, input, scale[0], scale[1]);
        const auto before = problem.data;
        problem.data[1] = input;
        asc::LapackReport report;
        ASC_DENSE_TEST_CHECK(
            test,
            problem.Run(provider, scale[0], scale[1], plan, workspace, report)
                .ok());
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
        Compare(test, problem.data[1], expected);
        problem.data[1] = before[1];
        ASC_DENSE_TEST_CHECK(test,
                             asc_scale_test::SameBytes(problem.data, before));
      }
    }
  }
  const auto signed_limit = support::ToWide(
      Direct(test, support::Value<T>(1, 1), Real{-1}, kInfinity));
  std::printf(
      "Exceptional fidelity: 108 adapter/direct comparisons; "
      "CFROM=-1 CTO=+Inf A=1(+i): native=(%La,%La), INFO=0; "
      "no finite-result acceptance credit\n",
      signed_limit.real(), signed_limit.imag());
  return test.Finish();
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>();
  }
  if (scalar == "d") {
    return Run<double>();
  }
  if (scalar == "c") {
    return Run<std::complex<float>>();
  }
  if (scalar == "z") {
    return Run<std::complex<double>>();
  }
  return 2;
}
