#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_solve_fixture.h"
#include "indefinite_rk_solve_native.h"
#include "indefinite_rk_solve_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
using asc_indefinite_rook_test::EqualBytes;
using asc_indefinite_rook_test::kColumn;
using asc_indefinite_rook_test::kLayout;
using asc_indefinite_rook_test::kLower;
using asc_indefinite_rook_test::kPivot;
using asc_indefinite_rook_test::kRow;
using asc_indefinite_rook_test::kScalar;
using asc_indefinite_rook_test::kUpper;
using asc_indefinite_rook_test::Pivots;
using asc_indefinite_rook_test::Raw;
using asc_indefinite_rook_test::Scratch;
using asc_indefinite_rook_test::Take;
using asc_indefinite_rook_test::TestContext;
using asc_indefinite_rook_test::WithoutAllocation;
using asc_rk_solve_test::RightHandSides;
using asc_rk_solve_test::Sample;
template <typename T>
void Fidelity(TestContext& test, const Sample<T>& sample,
              const RightHandSides<T>& rhs) {
  std::array<T, 4489> a{};
  std::array<T, 201> b{};
  const auto extra = sample.e;
  std::array<lapack_int, 67> pivots{};
  for (int j = 0; j < sample.n; ++j) {
    pivots[j] = static_cast<lapack_int>(sample.pivots[j + 1]);
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        a[j * sample.n + i] = sample.a[sample.Offset(i, j)];
      }
    }
  }
  for (int j = 0; j < rhs.nrhs; ++j) {
    for (int i = 0; i < rhs.n; ++i) {
      b[j * rhs.n + i] = rhs.before[rhs.Offset(i, j)];
    }
  }
  const auto original_a = a;
  const auto original_pivots = pivots;
  const auto original_e = extra;
  const char triangle = sample.triangle == kUpper ? 'U' : 'L';
  const lapack_int n = sample.n;
  const lapack_int nrhs = rhs.nrhs;
  const lapack_int ld = std::max(1, sample.n);
  lapack_int info = std::numeric_limits<lapack_int>::min();
  asc_rk_solve_test::Native(sample.hermitian, &triangle, &n, &nrhs, a.data(),
                            &ld, extra.data() + 1, pivots.data(), b.data(), &ld,
                            &info);
  ASC_DENSE_TEST_EQ(test, info, 0);
  ASC_DENSE_TEST_CHECK(test,
                       EqualBytes(a.data(), original_a.data(), sizeof(a)));
  ASC_DENSE_TEST_EQ(test, pivots, original_pivots);
  ASC_DENSE_TEST_CHECK(
      test, EqualBytes(extra.data(), original_e.data(), sizeof(extra)));
  for (int j = 0; j < rhs.nrhs; ++j) {
    for (int i = 0; i < rhs.n; ++i) {
      ASC_DENSE_TEST_CHECK(test, EqualBytes(&rhs.values[rhs.Offset(i, j)],
                                            &b[j * rhs.n + i], sizeof(T)));
    }
  }
}
template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, bool blocked, bool singular, int nrhs,
          asc::DenseBlasLayout rhs_layout, bool fidelity) {
  RightHandSides<T> rhs(sample, nrhs, rhs_layout);
  const auto factor_plan = Take(asc_rk_test::QueryFactor(
      provider, sample.triangle, sample.hermitian, blocked, sample.View(),
      asc_rk_test::OffDiagonal(sample.e, sample.n),
      Pivots(sample.pivots, sample.n)));
  Scratch<T> factor_scratch;
  const auto factor_work = factor_scratch.Workspace(factor_plan);
  asc::LapackReport factor_report;
  const auto factored = asc_rk_test::Factor(
      provider, sample.triangle, sample.hermitian, blocked, sample.View(),
      asc_rk_test::OffDiagonal(sample.e, sample.n),
      Pivots(sample.pivots, sample.n), factor_plan, factor_work, factor_report);
  ASC_DENSE_TEST_EQ(test, factored.ok(), !singular);
  // Native TRS_3 ignores boundary, 1-block and unused-partner E entries.
  // Poison those slots after factorization, preserving every active block.
  for (int i = 0; i < sample.n;) {
    const bool paired = sample.pivots[static_cast<std::size_t>(i) + 1] < 0;
    const int ignored = paired && sample.triangle == kLower ? i + 1 : i;
    sample.e[static_cast<std::size_t>(ignored) + 1] = asc_rk_test::Value<T>(
        std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN(), 19);
    i += paired ? 2 : 1;
  }
  const auto original_a = sample.a;
  const auto original_pivots = sample.pivots;
  const auto original_e = sample.e;
  const auto extra = asc_rk_solve_test::OffDiagonal(sample.e, sample.n);
  const auto raw = Raw(sample.pivots, sample.n);
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc_rk_solve_test::Query(provider, sample.triangle, sample.hermitian,
                                    sample.ConstView(), extra, raw, rhs.View());
  }));
  const bool active = sample.n != 0 && nrhs != 0;
  const asc::extent_t expected_n = active ? sample.n : 0;
  const asc::extent_t expected_layout =
      active ? (sample.layout == kRow ? sample.n * sample.n : 0) +
                   (rhs_layout == kRow ? sample.n * nrhs : 0)
             : 0;
  ASC_DENSE_TEST_EQ(test, plan.regions[kScalar].minimum_entries, 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[kScalar].preferred_entries, 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[kPivot].minimum_entries, expected_n);
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries,
                    expected_layout);
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc_rk_solve_test::Solve(provider, sample.triangle, sample.hermitian,
                                    sample.ConstView(), extra, raw, rhs.View(),
                                    plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, active);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value() &&
                                 !report.native_argument.has_value());
  if (active) {
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
  }
  ASC_DENSE_TEST_CHECK(
      test, EqualBytes(sample.a.data(), original_a.data(), sizeof(original_a)));
  ASC_DENSE_TEST_EQ(test, sample.pivots, original_pivots);
  ASC_DENSE_TEST_CHECK(
      test, EqualBytes(sample.e.data(), original_e.data(), sizeof(original_e)));
  ASC_DENSE_TEST_EQ(test, report.factor_family, asc::LapackFactorFamily::kRook);
  if (fidelity || singular) {
    Fidelity(test, sample, rhs);
  }
  if (!singular && !fidelity) {
    rhs.Verify(test, sample);
  }
  sample.Guards(test);
  rhs.Guards(test);
  scratch.Guards(test, workspace);
  factor_scratch.Guards(test, factor_work);
}
template <typename T>
int Run(bool hermitian, bool fidelity) {
  TestContext test;
  const auto context = asc::ExecutionContext::Serial();
  const auto provider = Take(asc::ReferenceLapackProvider::Create(context));
  int cases = 0;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto a_layout : {kColumn, kRow}) {
      for (const auto b_layout : {kColumn, kRow}) {
        for (const bool blocked : {false, true}) {
          for (const int nrhs : {0, 1, 3}) {
            for (const int n : {0, 1, 2, 3, 7, 67}) {
              Case(test, provider,
                   Sample<T>(n, hermitian, triangle, a_layout, 0, false),
                   blocked, false, nrhs, b_layout, fidelity);
              ++cases;
            }
            for (const int n : {1, 7, 67}) {
              Case(test, provider,
                   Sample<T>(n, hermitian, triangle, a_layout, 0, true),
                   blocked, true, nrhs, b_layout, fidelity);
              ++cases;
            }
          }
          const int exponent =
              sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
          for (const int scale : {-exponent, exponent}) {
            for (const int nrhs : {1, 3}) {
              Case(test, provider,
                   Sample<T>(7, hermitian, triangle, a_layout, scale, false),
                   blocked, false, nrhs, b_layout, fidelity);
              ++cases;
            }
          }
        }
      }
    }
  }
  std::printf("RK solve cases=%d mode=%s\n", cases,
              fidelity ? "fidelity" : "mathematical");
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity";
  const std::string_view scalar(argv[1]);
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
