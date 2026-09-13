#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_NUMERICAL_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_NUMERICAL_SUPPORT_H_
#include <algorithm>
#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_fixture.h"
#include "indefinite_aasen_native.h"
#include "indefinite_aasen_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace asc_aasen_test {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_test;
using base::TestContext;
template <typename T>
void Fidelity(TestContext& test, const aa::Sample<T>& sample,
              asc::extent_t entries, const asc::LapackReport& report) {
  if (sample.n == 0) {
    return;
  }
  std::array<T, 4489> a{};
  std::array<T, 4355> work{};
  std::array<lapack_int, 67> pivots{};
  for (int j = 0; j < sample.n; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        T value = sample.original[sample.Offset(i, j)];
        if constexpr (asc::DenseBlasComplex<T>) {
          if (sample.hermitian && i == j) {
            value.imag(0);
          }
        }
        a[j * sample.n + i] = value;
      }
    }
  }
  lapack_int info = std::numeric_limits<lapack_int>::min();
  aa::Native(sample.hermitian, sample.triangle == base::kUpper ? 'U' : 'L',
             sample.n, a.data(), sample.n, pivots.data(), work.data(),
             static_cast<lapack_int>(entries), info);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), info);
  for (int j = 0; j < sample.n; ++j) {
    ASC_DENSE_TEST_EQ(test, sample.pivots[j + 1], pivots[j]);
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(test,
                             base::EqualBytes(&sample.a[sample.Offset(i, j)],
                                              &a[j * sample.n + i], sizeof(T)));
      }
    }
  }
}
template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          aa::Sample<T> sample, int work_mode, bool fidelity) {
  const auto pivots = base::Pivots(sample.pivots, sample.n);
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return aa::Query(provider, sample.triangle, sample.hermitian, sample.View(),
                     pivots);
  }));
  const auto& scalar = plan.regions[base::kScalar];
  asc::extent_t entries = scalar.preferred_entries;
  if (work_mode == 0) {
    entries = scalar.minimum_entries;
  } else if (work_mode == 1) {
    entries =
        std::min(scalar.preferred_entries,
                 std::max(scalar.minimum_entries, asc::extent_t{3} * sample.n));
  }
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, entries);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return aa::Factor(provider, sample.triangle, sample.hermitian,
                      sample.View(), pivots, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, sample.n != 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), sample.n != 0);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kAasen);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  if (fidelity) {
    Fidelity(test, sample, entries, report);
  } else {
    sample.Reconstruction(test);
  }
  sample.Guards(test);
  scratch.Guards(test, workspace);
}
}  // namespace asc_aasen_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_NUMERICAL_SUPPORT_H_
