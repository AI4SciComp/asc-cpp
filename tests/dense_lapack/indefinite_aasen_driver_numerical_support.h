#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_DRIVER_NUMERICAL_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_DRIVER_NUMERICAL_SUPPORT_H_
#include <algorithm>
#include <array>
#include <complex>
#include <limits>

#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_driver_native.h"
#include "indefinite_aasen_driver_test_support.h"
#include "indefinite_aasen_solve_fixture.h"
#include "indefinite_aasen_solve_test_support.h"
#include "indefinite_rook_test_support.h"
namespace asc_aasen_driver_test {
namespace base = asc_indefinite_rook_test;
using asc_aasen_solve_test::Sample;
template <typename T>
void Fidelity(base::TestContext& test, const Sample<T>& sample, bool preferred,
              const asc::LapackReport& report) {
  std::array<T, 4489> a{};
  std::array<T, 201> b{};
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
  for (int j = 0; j < sample.nrhs; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      b[j * sample.n + i] = sample.original_b[sample.BOffset(i, j)];
    }
  }
  const int minimum = std::max(2 * sample.n, 3 * sample.n - 2);
  const int entries = preferred ? 65 * sample.n : minimum;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  Native(sample.hermitian, sample.triangle == base::kUpper ? 'U' : 'L',
         sample.n, sample.nrhs, a.data(), sample.n, pivots.data(), b.data(),
         sample.n, work.data(), entries, info);
  ASC_DENSE_TEST_EQ(test, info, report.native_info.value_or(-1));
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
  if (info == 0 || sample.rhs_layout == base::kColumn) {
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
void Reuse(base::TestContext& test,
           const asc::ReferenceLapackProvider& provider, Sample<T> sample) {
  sample.b = sample.original_b;
  const auto raw = asc_aasen_solve_test::Raw(sample.pivots, sample.n);
  const auto plan = base::Take(
      asc_aasen_solve_test::Query(provider, sample.hermitian, sample.triangle,
                                  sample.ConstView(), raw, sample.Rhs()));
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, asc_aasen_solve_test::Solve(
                provider, sample.hermitian, sample.triangle, sample.ConstView(),
                raw, sample.Rhs(), plan, workspace, report)
                .ok());
  sample.Solution(test);
  sample.Guards(test);
  sample.RhsGuards(test);
  scratch.Guards(test, workspace);
}
template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, bool singular, bool preferred, bool fidelity) {
  const bool active = sample.n != 0;
  const bool failure = active && singular && sample.nrhs != 0;
  const auto pivots = base::Pivots(sample.pivots, sample.n);
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return Query(provider, sample.triangle, sample.hermitian, sample.View(),
                 pivots, sample.Rhs());
  }));
  const auto region = plan.regions[base::kScalar];
  const auto entries =
      preferred ? region.preferred_entries : region.minimum_entries;
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, entries);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return Driver(provider, sample.triangle, sample.hermitian, sample.View(),
                  pivots, sample.Rhs(), plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, report.called_provider, active);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kAasen);
  ASC_DENSE_TEST_EQ(test, status.ok(), !failure);
  if (failure) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), sample.n);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), sample.n - 1);
  } else {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
  }
  if (!active || sample.nrhs == 0 ||
      (failure && sample.rhs_layout == base::kRow)) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), sample.original_b.data(),
                               sizeof(sample.b)));
  }
  if (active) {
    if (fidelity) {
      Fidelity(test, sample, preferred, report);
    } else {
      sample.Reconstruction(test);
      if (!failure) {
        sample.Solution(test);
        Reuse(test, provider, sample);
      }
    }
  }
  sample.Guards(test);
  sample.RhsGuards(test);
  scratch.Guards(test, workspace);
}
}  // namespace asc_aasen_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_DRIVER_NUMERICAL_SUPPORT_H_
