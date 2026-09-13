#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_tridiagonal.h"
#include "asc/dense/providers/lapack_tridiagonal_condition.h"
#include "asc/dense/providers/lapack_tridiagonal_driver.h"
#include "asc/dense/providers/lapack_tridiagonal_refinement.h"
#include "installed_lu/normal_return_guard.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_tridiagonal_test::kCapacity;
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kNone;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::Pivots;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::Scratch;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Tri;
using asc_tridiagonal_test::Value;
using asc_tridiagonal_test::Vector;

template <typename T>
void ExpertMath(TestContext& test, const asc::ReferenceLapackProvider& provider,
                const Tri<T>& original, Tri<T>& factors,
                std::array<asc::index_t, kCapacity + 2>& pivots,
                const Rhs<T>& rhs, Rhs<T>& solution,
                asc::DenseBlasRealType<T> value,
                asc::DenseBlasRealType<T>& rcond,
                std::array<asc::DenseBlasRealType<T>, 8>& ferr,
                std::array<asc::DenseBlasRealType<T>, 8>& berr) {
  using Real = asc::DenseBlasRealType<T>;
  asc::LapackReport report;
  for (const bool factored : {false, true}) {
    const auto query = [&] {
      if (factored) {
        return asc::QueryGtsvxFactoredWorkspace(
            provider, kNone, std::as_const(original).View(),
            std::as_const(factors).Factors(), Pivots(pivots, 1),
            std::as_const(rhs).View(), solution.View(), rcond, Vector(ferr, 1),
            Vector(berr, 1));
      }
      return asc::QueryGtsvxWorkspace(
          provider, kNone, std::as_const(original).View(), factors.Factors(),
          Vector(pivots, 1), std::as_const(rhs).View(), solution.View(), rcond,
          Vector(ferr, 1), Vector(berr, 1));
    };
    const auto plan = Take(query());
    Scratch<T> driver_scratch;
    const auto workspace = driver_scratch.Workspace(plan);
    const auto execute = [&] {
      if (factored) {
        return asc::GtsvxFactored(
            provider, kNone, std::as_const(original).View(),
            std::as_const(factors).Factors(), Pivots(pivots, 1),
            std::as_const(rhs).View(), solution.View(), rcond, Vector(ferr, 1),
            Vector(berr, 1), plan, workspace, report);
      }
      return asc::Gtsvx(
          provider, kNone, std::as_const(original).View(), factors.Factors(),
          Vector(pivots, 1), std::as_const(rhs).View(), solution.View(), rcond,
          Vector(ferr, 1), Vector(berr, 1), plan, workspace, report);
    };
    const auto status = execute();
    std::printf(
        "GTSVX FACT=%c A=%a status=%u called=%d INFO=%jd "
        "RCOND=%a FERR=%a BERR=%a X=%a\n",
        factored ? 'F' : 'N', static_cast<double>(value),
        static_cast<unsigned>(status.code()), report.called_provider,
        static_cast<std::intmax_t>(report.native_info.value_or(-999)),
        static_cast<double>(rcond), static_cast<double>(ferr[1]),
        static_cast<double>(berr[1]),
        static_cast<double>(ToWide(solution.At(0, 0)).real()));
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_NEAR(test, rcond, Real{1}, Real{0},
                        Real{128} * std::numeric_limits<Real>::epsilon());
    ASC_DENSE_TEST_CHECK(test,
                         std::isfinite(ferr[1]) && std::isfinite(berr[1]));
    ASC_DENSE_TEST_EQ(test, solution.At(0, 0), T{1});
  }
}

template <typename T>
void Check(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const Real value : {Real{1}, std::numeric_limits<Real>::min(),
                           std::numeric_limits<Real>::min() / Real{8},
                           std::numeric_limits<Real>::max() / Real{8}}) {
    Tri<T> original(1);
    original.Initialize(2, value);
    auto factors = original;
    std::array<asc::index_t, kCapacity + 2> pivots{};
    auto plan = Take(asc::QueryGttrfWorkspace(provider, factors.Factors(),
                                              Vector(pivots, 1)));
    Scratch<T> factor_scratch;
    auto workspace = factor_scratch.Workspace(plan);
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(
        test, asc::Gttrf(provider, factors.Factors(), Vector(pivots, 1), plan,
                         workspace, report)
                  .ok());
    Real rcond = -1;
    plan = Take(asc::QueryGtconWorkspace(
        provider, asc::LapackConditionNorm::kOne,
        std::as_const(factors).Factors(), Pivots(pivots, 1), value, rcond));
    Scratch<T> condition_scratch;
    workspace = condition_scratch.Workspace(plan);
    const auto condition_status =
        asc::Gtcon(provider, asc::LapackConditionNorm::kOne,
                   std::as_const(factors).Factors(), Pivots(pivots, 1), value,
                   rcond, plan, workspace, report);
    std::printf("GTCON A=%a status=%u called=%d INFO=%jd RCOND=%a expected=1\n",
                static_cast<double>(value),
                static_cast<unsigned>(condition_status.code()),
                report.called_provider,
                static_cast<std::intmax_t>(report.native_info.value_or(-999)),
                static_cast<double>(rcond));
    ASC_DENSE_TEST_CHECK(test, condition_status.ok());
    ASC_DENSE_TEST_NEAR(test, rcond, Real{1}, Real{0},
                        Real{128} * std::numeric_limits<Real>::epsilon());
    Rhs<T> rhs(1, 1, kColumn);
    Rhs<T> solution(1, 1, kRow);
    rhs.At(0, 0) = Value<T>(value);
    solution.At(0, 0) = T{1};
    std::array<Real, 8> ferr{};
    std::array<Real, 8> berr{};
    plan = Take(asc::QueryGtrfsWorkspace(
        provider, kNone, std::as_const(original).View(),
        std::as_const(factors).Factors(), Pivots(pivots, 1),
        std::as_const(rhs).View(), solution.View(), Vector(ferr, 1),
        Vector(berr, 1)));
    Scratch<T> refine_scratch;
    workspace = refine_scratch.Workspace(plan);
    const auto refine_status =
        asc::Gtrfs(provider, kNone, std::as_const(original).View(),
                   std::as_const(factors).Factors(), Pivots(pivots, 1),
                   std::as_const(rhs).View(), solution.View(), Vector(ferr, 1),
                   Vector(berr, 1), plan, workspace, report);
    std::printf(
        "GTRFS A=%a status=%u called=%d INFO=%jd FERR=%a BERR=%a X=%a\n",
        static_cast<double>(value), static_cast<unsigned>(refine_status.code()),
        report.called_provider,
        static_cast<std::intmax_t>(report.native_info.value_or(-999)),
        static_cast<double>(ferr[1]), static_cast<double>(berr[1]),
        static_cast<double>(ToWide(solution.At(0, 0)).real()));
    ASC_DENSE_TEST_CHECK(test, refine_status.ok());
    ASC_DENSE_TEST_CHECK(test,
                         std::isfinite(ferr[1]) && std::isfinite(berr[1]));
    ASC_DENSE_TEST_EQ(test, solution.At(0, 0), T{1});
    ExpertMath(test, provider, original, factors, pivots, rhs, solution, value,
               rcond, ferr, berr);
  }
}

}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar = argv[1];
  if (scalar == "s") {
    Check<float>(test, provider);
  } else if (scalar == "d") {
    Check<double>(test, provider);
  } else if (scalar == "c") {
    Check<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Check<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
