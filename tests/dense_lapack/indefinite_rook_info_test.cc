#include <array>
#include <complex>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rook.h"
#include "indefinite_rook_info_faults.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace support = asc_indefinite_rook_test;
namespace fault = asc_indefinite_rook_info_test;
using support::Take;
using support::TestContext;
constexpr std::array kFaults{fault::Fault::kPass, fault::Fault::kOmitInfo,
                             fault::Fault::kWrite32BitZero};

bool IsDefect(fault::Fault selected) {
  return selected == fault::Fault::kOmitInfo ||
         (selected == fault::Fault::kWrite32BitZero &&
          sizeof(lapack_int) > sizeof(std::uint32_t));
}

template <typename T>
auto MatrixValues(bool hermitian) {
  std::array<T, 8> data;
  data.fill(support::Value<T>(-31, 17));
  data[1] = support::Value<T>(4, 0.25L);
  data[2] = support::Value<T>(1, 0.5L);
  data[4] = support::Value<T>(1, hermitian ? -0.5L : 0.5L);
  data[5] = support::Value<T>(-2, -0.25L);
  return data;
}

void CheckReport(TestContext& test, fault::Fault selected, bool empty,
                 const asc::Status& status, const asc::LapackReport& report) {
  ASC_DENSE_TEST_EQ(test, fault::Calls(), empty ? 0U : 1U);
  ASC_DENSE_TEST_EQ(test, report.called_provider, !empty);
  if (empty) {
    ASC_DENSE_TEST_CHECK(test, status.ok() && !report.native_info);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
  } else {
    ASC_DENSE_TEST_EQ(test, fault::LastNativeInfo(), 0);
    if (IsDefect(selected)) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_EQ(test, report.native_info,
                        std::numeric_limits<lapack_int>::min());
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      ASC_DENSE_TEST_CHECK(test, !report.native_argument);
    } else {
      ASC_DENSE_TEST_CHECK(test, status.ok());
      ASC_DENSE_TEST_EQ(test, report.native_info, 0);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
    }
  }
}

template <typename T>
void FactorCases(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, bool hermitian) {
  for (const auto triangle : {support::kUpper, support::kLower}) {
    for (const auto layout : {support::kColumn, support::kRow}) {
      for (const bool blocked : {false, true}) {
        for (const asc::extent_t n : {0, 2}) {
          for (const auto selected : kFaults) {
            auto a = MatrixValues<T>(hermitian);
            std::array<asc::index_t, 4> pivots{-37, -37, -37, -37};
            const auto original = a;
            const auto original_pivots = pivots;
            const auto matrix = support::Matrix(a, n, n, layout, 3);
            const auto pivot = support::Pivots(pivots, n);
            fault::SetFault(
                blocked ? fault::Routine::kTrf : fault::Routine::kTf2,
                selected);
            const auto plan = Take(support::WithoutAllocation(test, [&] {
              return support::QueryFactor(provider, triangle, hermitian,
                                          blocked, matrix, pivot);
            }));
            ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
            support::Scratch<T> scratch;
            const auto workspace = scratch.Workspace(plan);
            asc::LapackReport report;
            const auto status = support::WithoutAllocation(test, [&] {
              return support::Factor(provider, triangle, hermitian, blocked,
                                     matrix, pivot, plan, workspace, report);
            });
            CheckReport(test, selected, n == 0, status, report);
            if (n == 0 || IsDefect(selected)) {
              ASC_DENSE_TEST_EQ(test, pivots, original_pivots);
              if (n == 0 || hermitian || layout == support::kRow) {
                ASC_DENSE_TEST_CHECK(
                    test,
                    support::EqualBytes(a.data(), original.data(), sizeof(a)));
              }
            }
            scratch.Guards(test, workspace);
            fault::SetFault(fault::Routine::kTrf, fault::Fault::kPass);
          }
        }
      }
    }
  }
}

template <typename T>
void SolveCases(TestContext& test, const asc::ReferenceLapackProvider& provider,
                const asc::ReferenceRookFactorView<T>& factor, bool hermitian) {
  for (const auto layout : {support::kColumn, support::kRow}) {
    for (const asc::extent_t count : {0, 2}) {
      for (const auto selected : kFaults) {
        std::array<T, 8> b;
        b.fill(T{3});
        const auto before = b;
        const auto rhs = support::Matrix(b, 2, count, layout, 3);
        fault::SetFault(fault::Routine::kTrs, selected);
        const auto plan = Take(support::WithoutAllocation(test, [&] {
          return support::QuerySolve(provider, hermitian, factor, rhs);
        }));
        ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
        support::Scratch<T> scratch;
        const auto workspace = scratch.Workspace(plan);
        asc::LapackReport report;
        const auto status = support::WithoutAllocation(test, [&] {
          return support::Solve(provider, hermitian, factor, rhs, plan,
                                workspace, report);
        });
        CheckReport(test, selected, count == 0, status, report);
        if (count == 0 || (IsDefect(selected) && layout == support::kRow)) {
          ASC_DENSE_TEST_CHECK(
              test, support::EqualBytes(b.data(), before.data(), sizeof(b)));
        }
        scratch.Guards(test, workspace);
        fault::SetFault(fault::Routine::kTrf, fault::Fault::kPass);
      }
    }
  }
}

template <typename T>
void SolveSetup(TestContext& test, const asc::ReferenceLapackProvider& provider,
                bool hermitian) {
  for (const auto triangle : {support::kUpper, support::kLower}) {
    for (const auto layout : {support::kColumn, support::kRow}) {
      for (const bool blocked : {false, true}) {
        auto a = MatrixValues<T>(hermitian);
        std::array<asc::index_t, 4> pivots{};
        const auto matrix = support::Matrix(a, 2, 2, layout, 3);
        const auto pivot = support::Pivots(pivots, 2);
        const auto plan = Take(support::QueryFactor(
            provider, triangle, hermitian, blocked, matrix, pivot));
        support::Scratch<T> scratch;
        asc::LapackReport report;
        ASC_DENSE_TEST_CHECK(
            test,
            support::Factor(provider, triangle, hermitian, blocked, matrix,
                            pivot, plan, scratch.Workspace(plan), report)
                .ok());
        const auto factor = Take(asc::ReferenceRookFactorView<T>::Create(
            provider, support::Matrix(std::as_const(a), 2, 2, layout, 3),
            triangle, hermitian ? support::kHermitian : support::kSymmetric,
            support::Raw(pivots, 2), report));
        const auto factors = a;
        const auto factor_pivots = pivots;
        SolveCases(test, provider, factor, hermitian);
        ASC_DENSE_TEST_CHECK(
            test, support::EqualBytes(a.data(), factors.data(), sizeof(a)));
        ASC_DENSE_TEST_EQ(test, pivots, factor_pivots);
      }
    }
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         bool hermitian = false) {
  FactorCases<T>(test, provider, hermitian);
  SolveSetup<T>(test, provider, hermitian);
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c" || scalar == "ch") {
    Run<std::complex<float>>(test, provider, scalar == "ch");
  } else if (scalar == "z" || scalar == "zh") {
    Run<std::complex<double>>(test, provider, scalar == "zh");
  } else {
    return 2;
  }
  return test.Finish();
}
