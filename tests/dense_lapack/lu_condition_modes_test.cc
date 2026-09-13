#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>
#include <type_traits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
#include "lu_aux_info_test_support.h"

#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapack.h>
#include <lapacke_config.h>

namespace {
using asc_lu_aux_info_test::Matrix;
using asc_lu_aux_info_test::Scratch;
using asc_lu_aux_info_test::Take;
using asc_lu_aux_info_test::TestContext;
constexpr auto kOne = asc::LapackConditionNorm::kOne;
constexpr auto kInfinity = asc::LapackConditionNorm::kInfinity;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
template <typename T>
using Real = asc::DenseBlasRealType<T>;

// Diagonal LU needs no pivot oracle; its two condition norms are exactly four.
template <typename T>
Real<T> Diagonal(Matrix<T>& matrix, int exponent) {
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      matrix.At(i, j) = i == j ? T{std::ldexp(Real<T>{1}, exponent + i)} : T{};
    }
  }
  return std::ldexp(Real<T>{1}, exponent + 2);
}

template <typename T>
void Worker(TestContext& test, int worker, asc::DenseBlasLayout layout,
            asc::LapackConditionNorm norm,
            const asc::LapackWorkspacePlan& plan) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Matrix<T> factors(3, 3, layout);
  const auto anorm = Diagonal(factors, worker - 2);
  const auto before = factors.data;
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  Real<T> rcond = -11;
  asc::LapackReport report;
  for (int repeat = 0; repeat < 32; ++repeat) {
    ASC_DENSE_TEST_CHECK(test, asc::Gecon(provider, norm, factors.ConstView(),
                                          anorm, rcond, plan, workspace, report)
                                   .ok());
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
    ASC_DENSE_TEST_CHECK(test,
                         std::abs(rcond - Real<T>{0.25}) <=
                             32 * std::numeric_limits<Real<T>>::epsilon());
  }
  ASC_DENSE_TEST_EQ(test, factors.data, before);
  // Distinct final outcomes verify private reports and outputs without any
  // synthetic fault callback or shared allocation-observer state.
  const Real<T> final_norm = worker == 0 ? Real<T>{-1} : anorm;
  if (worker == 1) {
    factors.At(0, 0) = T{};
  }
  auto final_plan = plan;
  if (worker == 2) {
    final_plan.total_byte_limit = 0;
  }
  rcond = -23;
  const auto status =
      asc::Gecon(provider, norm, factors.ConstView(), final_norm, rcond,
                 final_plan, workspace, report);
  if (worker == 0) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, rcond, Real<T>{-23});
  } else if (worker == 2) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, rcond, Real<T>{-23});
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    ASC_DENSE_TEST_EQ(test, rcond, worker == 1 ? Real<T>{0} : Real<T>{0.25});
  }
  scratch.Guards(test);
}

template <typename T>
void Concurrent(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto layout : {kRow, kColumn}) {
    for (auto norm : {kOne, kInfinity}) {
      Matrix<T> factors(3, 3, layout);
      const auto anorm = Diagonal(factors, 0);
      Real<T> rcond = -11;
      const auto plan = Take(asc::QueryGeconWorkspace(
          provider, norm, factors.ConstView(), anorm, rcond));
      std::array<std::thread, 4> workers;
      std::array<int, 4> results{};
      for (int i = 0; i < 4; ++i) {
        workers[i] = std::thread([&, i] {
          TestContext local;
          Worker<T>(local, i, layout, norm, plan);
          results[i] = local.Finish();
        });
      }
      for (auto& thread : workers) {
        thread.join();
      }
      for (int result : results) {
        ASC_DENSE_TEST_EQ(test, result, 0);
      }
    }
  }
}

template <typename T>
Real<T> DirectScalar(TestContext& test, char norm, T factor, Real<T> anorm) {
  const lapack_int n = 1;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  std::array<T, 4> work{};
  std::array<Real<T>, 2> real_work{};
  lapack_int integer_work = 0;
  Real<T> rcond = -31;
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sgecon(&norm, &n, &factor, &n, &anorm, &rcond, work.data(),
                  &integer_work, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dgecon(&norm, &n, &factor, &n, &anorm, &rcond, work.data(),
                  &integer_work, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cgecon(&norm, &n, &factor, &n, &anorm, &rcond, work.data(),
                  real_work.data(), &info);
  } else {
    LAPACK_zgecon(&norm, &n, &factor, &n, &anorm, &rcond, work.data(),
                  real_work.data(), &info);
  }
  ASC_DENSE_TEST_EQ(test, info, 0);
  return rcond;
}

template <typename T>
void RequiredScalarMath(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  // Retain the already recorded GESVX/GECON tiny fixture and its safeguard.
  // For a nonzero scalar, |a|*|1/a|=1 independently of either matrix norm.
  for (Real<T> value : {std::numeric_limits<Real<T>>::min() / Real<T>{1024},
                        Real<T>{2} * std::numeric_limits<Real<T>>::min()}) {
    for (auto norm : {kOne, kInfinity}) {
      const auto direct =
          DirectScalar(test, norm == kOne ? '1' : 'I', T{value}, value);
      for (auto layout : {kRow, kColumn}) {
        Matrix<T> factors(1, 1, layout);
        factors.At(0, 0) = T{value};
        const auto before = factors.data;
        Real<T> rcond = -11;
        const auto plan = Take(asc::QueryGeconWorkspace(
            provider, norm, factors.ConstView(), value, rcond));
        Scratch<T> scratch;
        const auto workspace = scratch.Workspace(plan);
        asc::LapackReport report;
        ASC_DENSE_TEST_CHECK(
            test, asc::Gecon(provider, norm, factors.ConstView(), value, rcond,
                             plan, workspace, report)
                      .ok());
        ASC_DENSE_TEST_EQ(test, report.native_info, 0);
        ASC_DENSE_TEST_EQ(test, factors.data, before);
        ASC_DENSE_TEST_EQ(test, rcond, direct);
        std::printf(
            "GECON bytes=%zu norm=%d layout=%d a=%La rcond=%La "
            "direct=%La INFO=%lld\n",
            sizeof(T), static_cast<int>(norm), static_cast<int>(layout),
            static_cast<long double>(value), static_cast<long double>(rcond),
            static_cast<long double>(direct),
            static_cast<long long>(report.native_info.value_or(-999)));
        ASC_DENSE_TEST_CHECK(test,
                             std::abs(rcond - Real<T>{1}) <=
                                 32 * std::numeric_limits<Real<T>>::epsilon());
        scratch.Guards(test);
      }
    }
  }
}

template <typename T>
void Run(TestContext& test, bool mathematical) {
  if (mathematical) {
    RequiredScalarMath<T>(test);
  } else {
    Concurrent<T>(test);
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view mode(argv[2]);
  if (mode != "concurrency" && mode != "extreme") {
    return 2;
  }
  TestContext test;
  const std::string_view scalar(argv[1]);
  const bool mathematical = mode == "extreme";
  if (scalar == "s") {
    Run<float>(test, mathematical);
  } else if (scalar == "d") {
    Run<double>(test, mathematical);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, mathematical);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, mathematical);
  } else {
    return 2;
  }
  return test.Finish();
}
