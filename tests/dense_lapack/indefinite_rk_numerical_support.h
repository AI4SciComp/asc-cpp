#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_NUMERICAL_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_NUMERICAL_SUPPORT_H_
#include <array>
#include <cstddef>
#include <limits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_fixture.h"
#include "indefinite_rk_native.h"
#include "indefinite_rk_test_support.h"
#include "indefinite_rook_test_support.h"
namespace asc_rk_test {
template <typename T>
void NativeGuards(TestContext& test, const Sample<T>& sample, lapack_int n,
                  lapack_int lda, asc::extent_t scalar_entries,
                  const std::array<T, 5000>& native_a,
                  const std::array<T, 5000>& saved_native,
                  const std::array<T, 72>& native_e,
                  const std::array<lapack_int, 72>& native_pivots,
                  const std::array<T, 4500>& work) {
  for (std::size_t k = 0; k < native_a.size(); ++k) {
    const auto offset = static_cast<int>(k) - 1;
    const int i = offset % static_cast<int>(lda);
    const int j = offset / static_cast<int>(lda);
    if (k == 0 || i >= n || j >= n || !sample.Selected(i, j)) {
      ASC_DENSE_TEST_CHECK(
          test, EqualBytes(&native_a[k], &saved_native[k], sizeof(T)));
    }
  }
  for (std::size_t k = 0; k < native_e.size(); ++k) {
    if (k == 0 || k > static_cast<std::size_t>(n)) {
      ASC_DENSE_TEST_EQ(test, native_e[k], Value<T>(-511, 17));
      ASC_DENSE_TEST_EQ(test, native_pivots[k], -617);
    }
  }
  ASC_DENSE_TEST_EQ(test, work.front(), Value<T>(-107, 11));
  for (std::size_t k = 1 + static_cast<std::size_t>(scalar_entries);
       k < work.size(); ++k) {
    ASC_DENSE_TEST_EQ(test, work[k], Value<T>(-107, 11));
  }
}

template <typename T>
void Fidelity(TestContext& test, const Sample<T>& sample, bool blocked,
              asc::extent_t scalar_entries, const asc::LapackReport& report) {
  if (sample.n == 0) {
    ASC_DENSE_TEST_CHECK(
        test,
        EqualBytes(sample.a.data(), sample.original.data(), sizeof(sample.a)));
    for (const auto value : sample.e) {
      ASC_DENSE_TEST_EQ(test, value, Value<T>(-511, 17));
    }
    return;
  }
  const bool packed = sample.hermitian || sample.layout == kRow;
  lapack_int n = sample.n;
  lapack_int lda = packed || n == 1 ? n : sample.Ld();
  char uplo = sample.triangle == kUpper ? 'U' : 'L';
  std::array<T, 5000> native_a{};
  std::array<T, 72> native_e{};
  std::array<lapack_int, 72> native_pivots{};
  std::array<T, 4500> work{};
  native_a.fill(Value<T>(-613, 23));
  native_e.fill(Value<T>(-511, 17));
  native_pivots.fill(-617);
  work.fill(Value<T>(-107, 11));
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      if (sample.Selected(i, j)) {
        T value = sample.original[sample.Offset(i, j)];
        if (sample.hermitian && i == j) {
          value = Value<T>(ToWide(value).real());
        }
        native_a[1 + j * lda + i] = value;
      }
    }
  }
  const auto saved_native = native_a;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  auto lwork = static_cast<lapack_int>(scalar_entries);
  if (blocked) {
    NativeTrf(sample.hermitian, &uplo, &n, native_a.data() + 1, &lda,
              native_e.data() + 1, native_pivots.data() + 1, work.data() + 1,
              &lwork, &info);
    ASC_DENSE_TEST_EQ(test, work[1], Value<T>(64 * n));
  } else {
    NativeTf2(sample.hermitian, &uplo, &n, native_a.data() + 1, &lda,
              native_e.data() + 1, native_pivots.data() + 1, &info);
  }
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), info);
  ASC_DENSE_TEST_EQ(test, n, sample.n);
  ASC_DENSE_TEST_EQ(test, lda, packed || n == 1 ? n : sample.Ld());
  ASC_DENSE_TEST_EQ(test, uplo, sample.triangle == kUpper ? 'U' : 'L');
  ASC_DENSE_TEST_EQ(test, lwork, scalar_entries);
  for (int j = 0; j < n; ++j) {
    ASC_DENSE_TEST_EQ(test, sample.pivots[j + 1], native_pivots[j + 1]);
    ASC_DENSE_TEST_CHECK(
        test, EqualBytes(&sample.e[j + 1], &native_e[j + 1], sizeof(T)));
    for (int i = 0; i < n; ++i) {
      if (sample.Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(test,
                             EqualBytes(&sample.a[sample.Offset(i, j)],
                                        &native_a[1 + j * lda + i], sizeof(T)));
      }
    }
  }
  NativeGuards(test, sample, n, lda, scalar_entries, native_a, saved_native,
               native_e, native_pivots, work);
}

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, bool singular, bool blocked, int mode,
          bool fidelity) {
  const auto a = sample.View();
  const auto e = OffDiagonal(sample.e, sample.n);
  const auto pivots = Pivots(sample.pivots, sample.n);
  const auto saved_e = sample.e;
  const auto saved_pivots = sample.pivots;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return QueryFactor(provider, sample.triangle, sample.hermitian, blocked, a,
                       e, pivots);
  }));
  ASC_DENSE_TEST_CHECK(test, EqualBytes(sample.a.data(), sample.original.data(),
                                        sizeof(sample.a)));
  ASC_DENSE_TEST_EQ(test, sample.e, saved_e);
  ASC_DENSE_TEST_EQ(test, sample.pivots, saved_pivots);
  asc::extent_t entries = 0;
  if (blocked && sample.n != 0) {
    const std::array<asc::extent_t, 4> capacities{
        1, (sample.hermitian ? 2 : 8) * sample.n,
        (sample.hermitian ? 1 : 7) * sample.n, 64 * sample.n};
    entries = capacities[static_cast<std::size_t>(mode)];
  }
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, entries);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return Factor(provider, sample.triangle, sample.hermitian, blocked, a, e,
                  pivots, plan, workspace, report);
  });
  const bool positive = singular && sample.n != 0;
  ASC_DENSE_TEST_EQ(
      test, status.code(),
      positive ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk);
  ASC_DENSE_TEST_EQ(test, report.called_provider, sample.n != 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), sample.n != 0);
  ASC_DENSE_TEST_EQ(test, report.factor_family, asc::LapackFactorFamily::kRook);
  ASC_DENSE_TEST_EQ(
      test, report.outcome,
      positive ? asc::LapackOutcome::kSingular : asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    positive ? asc::LapackOutputValidity::kDocumentedPartial
                             : asc::LapackOutputValidity::kComplete);
  if (positive) {
    ASC_DENSE_TEST_CHECK(test, report.native_info.value_or(0) > 0);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                      report.native_info.value_or(0) - 1);
  } else if (sample.n != 0) {
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  }
  if (fidelity) {
    Fidelity(test, sample, blocked, entries, report);
  } else if (report.output_validity != asc::LapackOutputValidity::kUnusable) {
    sample.Reconstruction(test);
  }
  sample.Guards(test);
  scratch.Guards(test, workspace);
}

}  // namespace asc_rk_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_NUMERICAL_SUPPORT_H_
