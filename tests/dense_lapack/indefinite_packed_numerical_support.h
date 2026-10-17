#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_NUMERICAL_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_NUMERICAL_SUPPORT_H_
#include <array>
#include <cstddef>
#include <limits>

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_native.h"
#include "indefinite_packed_test_support.h"
namespace asc_packed_indefinite_test {
// Test-owned walk of the signed one-based BK encoding; no checked adapter
// helper participates in deciding whether the raw provider result is usable.
inline bool NativePivotsValid(const lapack_int* pivots, int n, bool upper) {
  for (int i = 0; i < n;) {
    const auto p = pivots[i];
    if (p == 0 || p < -n || p > n) {
      return false;
    }
    if (p > 0) {
      if (upper ? p > i + 1 : p < i + 1) {
        return false;
      }
      ++i;
    } else {
      if (i + 1 == n || pivots[i + 1] != p ||
          (upper ? -p > i + 1 : -p < i + 2)) {
        return false;
      }
      i += 2;
    }
  }
  return true;
}
template <typename T>
void Fidelity(TestContext& test, const Sample<T>& sample,
              const asc::Status& status, const asc::LapackReport& report) {
  auto native = sample;
  native.layout = kColumn;
  for (int j = 0; j < sample.n; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        native.a[native.Offset(i, j)] = sample.original[sample.Offset(i, j)];
      }
    }
  }
  // Match the live native endpoint objects used by the checked call. Raw
  // native span integrity remains a separate mandatory failing test gate.
  const int count = sample.n * (sample.n + 1) / 2;
  const T guard = Value<T>(-173, 19);
  native.a[0] = guard;
  native.a[count + 1] = guard;
  std::array<lapack_int, 69> pivots;
  pivots.fill(std::numeric_limits<lapack_int>::min());
  lapack_int info = std::numeric_limits<lapack_int>::min();
  const lapack_int n = sample.n;
  const char tri = sample.triangle == kUpper ? 'U' : 'L';
  Native(sample.hermitian, &tri, &n, native.a.data() + 1, pivots.data() + 1,
         &info);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), info);
  const bool defect = info < 0 || info > n || native.a[0] != guard ||
                      native.a[count + 1] != guard ||
                      pivots[0] != std::numeric_limits<lapack_int>::min() ||
                      pivots[n + 1] != std::numeric_limits<lapack_int>::min() ||
                      !NativePivotsValid(pivots.data() + 1, n, tri == 'U');
  if (defect) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      info < 0 ? asc::LapackOutcome::kProviderArgument
                               : asc::LapackOutcome::kPartialResult);
    return;
  }
  ASC_DENSE_TEST_EQ(
      test, status.code(),
      info == 0 ? asc::ErrorCode::kOk : asc::ErrorCode::kNumerical);
  for (int j = 0; j < sample.n; ++j) {
    ASC_DENSE_TEST_EQ(test, sample.pivots[static_cast<std::size_t>(j) + 1],
                      pivots[j + 1]);
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(
            test, EqualBytes(&sample.a[sample.Offset(i, j)],
                             &native.a[native.Offset(i, j)], sizeof(T)));
      }
    }
  }
}
template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, bool fidelity) {
  const auto before_pivots = sample.pivots;
  const auto a = sample.View();
  const auto p = base::Pivots(sample.pivots, sample.n);
  const auto plan = Take(base::WithoutAllocation(test, [&] {
    return Query(provider, sample.triangle, sample.hermitian, a, p);
  }));
  ASC_DENSE_TEST_CHECK(test, EqualBytes(sample.a.data(), sample.original.data(),
                                        sizeof(sample.a)));
  ASC_DENSE_TEST_EQ(test, sample.pivots, before_pivots);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kScalar].minimum_entries, 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kScalar].preferred_entries, 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kPivot].minimum_entries,
                    sample.n == 0 ? 0 : sample.n + 2);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kLayout].minimum_entries,
                    sample.n == 0 ? 0 : sample.n * (sample.n + 1) / 2 + 2);
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return Factor(provider, sample.triangle, sample.hermitian, a, p, plan,
                  workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kBunchKaufman);
  if (sample.n == 0) {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_CHECK(
        test,
        EqualBytes(sample.a.data(), sample.original.data(), sizeof(sample.a)));
    ASC_DENSE_TEST_EQ(test, sample.pivots, before_pivots);
  } else {
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
    const auto info = report.native_info.value_or(-1);
    ASC_DENSE_TEST_CHECK(test, info >= 0 && info <= sample.n);
    if (fidelity) {
      Fidelity(test, sample, status, report);
    }
    // Required mathematics continues to demand valid factors even when the
    // provider defect is correctly contained. Fidelity checks that defect
    // separately; all accepted-result report assertions remain active.
    if ((!fidelity || status.code() != asc::ErrorCode::kProvider) &&
        info == 0) {
      ASC_DENSE_TEST_CHECK(test, status.ok());
      ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
      ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    } else if ((!fidelity || status.code() != asc::ErrorCode::kProvider) &&
               info > 0 && info <= sample.n) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), info - 1);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kDocumentedPartial);
      ASC_DENSE_TEST_EQ(
          test, report.outcome,
          sample.a[sample.Offset(static_cast<int>(info - 1),
                                 static_cast<int>(info - 1))] == T{}
              ? asc::LapackOutcome::kSingular
              : asc::LapackOutcome::kPartialResult);
    }
    if (!fidelity &&
        (status.ok() || status.code() == asc::ErrorCode::kNumerical)) {
      sample.Reconstruction(test);
    }
    if (status.code() == asc::ErrorCode::kProvider) {
      ASC_DENSE_TEST_CHECK(test,
                           EqualBytes(sample.a.data(), sample.original.data(),
                                      sizeof(sample.a)));
      ASC_DENSE_TEST_EQ(test, sample.pivots, before_pivots);
    }
  }
  sample.Guards(test);
  scratch.Guards(test, workspace);
}
}  // namespace asc_packed_indefinite_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_NUMERICAL_SUPPORT_H_
