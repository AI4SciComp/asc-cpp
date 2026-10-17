#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_NUMERICAL_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_NUMERICAL_SUPPORT_H_
#include <algorithm>
#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_solve_fixture.h"
#include "indefinite_aasen_solve_native.h"
#include "indefinite_aasen_solve_test_support.h"
#include "indefinite_aasen_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace asc_aasen_solve_test {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_solve_test;
template <typename T>
void Fidelity(base::TestContext& test, const aa::Sample<T>& sample,
              bool singular, const asc::LapackReport& report) {
  std::array<T, 4489> a{};
  std::array<T, 201> b{};
  std::array<T, 200> work{};
  std::array<lapack_int, 67> pivots{};
  for (int j = 0; j < sample.n; ++j) {
    pivots[j] = sample.pivots[j + 1];
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        a[j * sample.n + i] = sample.a[sample.Offset(i, j)];
      }
    }
  }
  for (int j = 0; j < sample.nrhs; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      b[j * sample.n + i] = sample.original_b[sample.BOffset(i, j)];
    }
  }
  lapack_int info = std::numeric_limits<lapack_int>::min();
  aa::Native(sample.hermitian, sample.triangle == base::kUpper ? 'U' : 'L',
             sample.n, sample.nrhs, a.data(), sample.n, pivots.data(), b.data(),
             sample.n, work.data(), 3 * sample.n - 2, info);
  ASC_DENSE_TEST_EQ(test, info, report.native_info.value_or(-1));
  if (!singular || sample.rhs_layout == base::kColumn) {
    for (int j = 0; j < sample.nrhs; ++j) {
      for (int i = 0; i < sample.n; ++i) {
        ASC_DENSE_TEST_CHECK(test,
                             base::EqualBytes(&sample.b[sample.BOffset(i, j)],
                                              &b[j * sample.n + i], sizeof(T)));
      }
    }
  }
}
template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          aa::Sample<T> sample, bool singular, bool oversized, bool fidelity) {
  const bool active = sample.n != 0 && sample.nrhs != 0;
  if (active) {
    const auto pivots = base::Pivots(sample.pivots, sample.n);
    const auto plan = base::Take(asc_aasen_test::Query(
        provider, sample.triangle, sample.hermitian, sample.View(), pivots));
    base::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(
        test,
        asc_aasen_test::Factor(provider, sample.triangle, sample.hermitian,
                               sample.View(), pivots, plan, workspace, report)
            .ok());
    sample.Reconstruction(test);
    scratch.Guards(test, workspace);
  }
  const auto factor_before = sample.a;
  const auto pivot_before = sample.pivots;
  const auto raw = aa::Raw(sample.pivots, sample.n);
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return aa::Query(provider, sample.hermitian, sample.triangle,
                     sample.ConstView(), raw, sample.Rhs());
  }));
  auto entries = plan.regions[base::kScalar].minimum_entries;
  if (oversized && active) {
    entries = plan.regions[base::kScalar].preferred_entries + 7;
  }
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, entries);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return aa::Solve(provider, sample.hermitian, sample.triangle,
                     sample.ConstView(), raw, sample.Rhs(), plan, workspace,
                     report);
  });
  ASC_DENSE_TEST_EQ(test, report.called_provider, active);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kAasen);
  if (active && singular) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), sample.n);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), sample.n - 1);
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
  }
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(sample.a.data(), factor_before.data(),
                                        sizeof(factor_before)));
  ASC_DENSE_TEST_EQ(test, sample.pivots, pivot_before);
  if (!active || (singular && sample.rhs_layout == base::kRow)) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), sample.original_b.data(),
                               sizeof(sample.b)));
  }
  if (fidelity && active) {
    Fidelity(test, sample, singular, report);
  } else if (active && !singular) {
    sample.Solution(test);
  }
  sample.Guards(test);
  sample.RhsGuards(test);
  scratch.Guards(test, workspace);
}
}  // namespace asc_aasen_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_NUMERICAL_SUPPORT_H_
