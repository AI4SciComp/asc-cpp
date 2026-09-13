#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_NUMERICAL_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_NUMERICAL_SUPPORT_H_
#include <algorithm>
#include <array>
#include <complex>
#include <limits>
#include <vector>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "indefinite_aasen_two_stage_fixture.h"
#include "indefinite_aasen_two_stage_native.h"
#include "indefinite_aasen_two_stage_test_support.h"
namespace asc_aasen_two_stage_test {
template <typename T>
void VerifyActive(base::TestContext& test, Sample<T>& sample, bool row, int ltb,
                  int entries, const std::vector<T>& a,
                  const std::vector<T>& tb, const std::vector<asc::index_t>& p,
                  const std::vector<asc::index_t>& q, bool singular,
                  bool fidelity, const asc::LapackReport& report) {
  const auto n = sample.n;
  const auto offset = [&](int i, int j) {
    return 1 + (row ? i * sample.lda + j : j * sample.lda + i);
  };
  std::vector<T> direct_tb(static_cast<std::size_t>(ltb + 2), T{-73});
  const int nb = std::min({192, (ltb / n - 1) / 3, entries / n});
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), singular ? n : 0);
  ASC_DENSE_TEST_EQ(test, tb[1], base::Value<T>(nb));
  if (fidelity) {
    auto direct = sample.before;
    // Match the documented original-HE packing policy, including signed
    // imaginary zero. Native A diagonals can retain their input bytes.
    if constexpr (asc::DenseBlasComplex<T>) {
      if (sample.hermitian) {
        for (int i = 0; i < n; ++i) {
          direct[1 + i * sample.lda + i].imag(0);
        }
      }
    }
    std::vector<lapack_int> dp(static_cast<std::size_t>(n));
    std::vector<lapack_int> dq(dp);
    std::vector<T> work(static_cast<std::size_t>(entries));
    lapack_int info = std::numeric_limits<lapack_int>::min();
    Native(sample.hermitian, sample.upper ? 'U' : 'L', n, direct.data() + 1,
           sample.lda, direct_tb.data() + 1, ltb, dp.data(), dq.data(),
           work.data(), entries, info);
    ASC_DENSE_TEST_EQ(test, info, report.native_info.value_or(-1));
    ASC_DENSE_TEST_CHECK(test, base::EqualBytes(tb.data(), direct_tb.data(),
                                                tb.size() * sizeof(T)));
    for (int j = 0; j < n; ++j) {
      ASC_DENSE_TEST_EQ(test, p[j + 1], dp[j]);
      ASC_DENSE_TEST_EQ(test, q[j + 1], dq[j]);
      for (int i = 0; i < n; ++i) {
        if (sample.Selected(i, j)) {
          ASC_DENSE_TEST_CHECK(
              test,
              base::EqualBytes(&a[offset(i, j)],
                               &direct[1 + j * sample.lda + i], sizeof(T)));
        }
      }
    }
  } else {
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if (sample.Selected(i, j)) {
          sample.a[1 + j * sample.lda + i] = a[offset(i, j)];
        }
      }
    }
    Reconstruction(test, n, sample.hermitian, sample.upper, sample.a.data() + 1,
                   sample.lda, tb.data() + 1, ltb / n, nb, p.data() + 1,
                   q.data() + 1, sample.full);
  }
}
template <typename T>
void OutputGuards(base::TestContext& test, const Sample<T>& sample, bool row,
                  const std::vector<T>& a, const std::vector<T>& before,
                  const std::vector<T>& tb, const std::vector<asc::index_t>& p,
                  const std::vector<asc::index_t>& q) {
  const auto n = sample.n;
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < sample.lda; ++i) {
      const bool selected =
          i < n && (row ? sample.Selected(j, i) : sample.Selected(i, j));
      const auto at = 1 + j * sample.lda + i;
      if (!selected) {
        ASC_DENSE_TEST_CHECK(test,
                             base::EqualBytes(&a[at], &before[at], sizeof(T)));
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, a.front(), before.front());
  ASC_DENSE_TEST_EQ(test, a.back(), before.back());
  ASC_DENSE_TEST_EQ(test, tb.front(), T{-73});
  ASC_DENSE_TEST_EQ(test, tb.back(), T{-73});
  ASC_DENSE_TEST_EQ(test, p.front(), -71);
  ASC_DENSE_TEST_EQ(test, p.back(), -71);
  ASC_DENSE_TEST_EQ(test, q.front(), -71);
  ASC_DENSE_TEST_EQ(test, q.back(), -71);
}
inline void Outcome(base::TestContext& test, int n, bool singular,
                    const asc::Status& status,
                    const asc::LapackReport& report) {
  ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
  ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), n != 0);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kAasen);
  ASC_DENSE_TEST_EQ(
      test, report.outcome,
      singular ? asc::LapackOutcome::kSingular : asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    singular ? asc::LapackOutputValidity::kDocumentedPartial
                             : asc::LapackOutputValidity::kComplete);
  if (singular) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), n - 1);
  }
}
template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, bool row, int band_mode, int work_mode,
          bool singular, bool fidelity) {
  const auto n = sample.n;
  const auto tri = sample.upper ? base::kUpper : base::kLower;
  const auto layout = row ? base::kRow : base::kColumn;
  const auto ltb = n * std::array{4, 10, 577}[band_mode];
  const auto entries = n * std::array{1, 3, 192}[work_mode];
  const auto offset = [&](int i, int j) {
    return 1 + (row ? i * sample.lda + j : j * sample.lda + i);
  };
  auto a = sample.before;
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      a[offset(i, j)] = sample.before[1 + j * sample.lda + i];
    }
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    if (sample.hermitian) {
      for (int i = 0; i < n; ++i) {
        a[offset(i, i)].imag(
            std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
      }
    }
  }
  const auto before = a;
  std::vector<T> tb(static_cast<std::size_t>(ltb + 2), T{-73});
  auto direct_tb = tb;
  std::vector<asc::index_t> p(static_cast<std::size_t>(n + 2), -71);
  std::vector<asc::index_t> q(p);
  auto matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
      a.data() + 1, n, n, layout, sample.lda,
      {a.data(), a.size() * sizeof(T), base::kHost}));
  auto band = Vector(tb, ltb);
  auto pivots = Vector(p, n);
  auto band_pivots = Vector(q, n);
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return Query(provider, tri, sample.hermitian, matrix, band, pivots,
                 band_pivots);
  }));
  Scratch<T> scratch(plan, entries);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return Factor(provider, tri, sample.hermitian, matrix, band, pivots,
                  band_pivots, plan, scratch.workspace, report);
  });
  Outcome(test, n, singular, status, report);
  if (n != 0) {
    VerifyActive(test, sample, row, ltb, entries, a, tb, p, q, singular,
                 fidelity, report);
  } else {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(a.data(), before.data(), a.size() * sizeof(T)));
    ASC_DENSE_TEST_CHECK(test, tb == direct_tb);
  }
  OutputGuards(test, sample, row, a, before, tb, p, q);
  scratch.Guards(test);
}
}  // namespace asc_aasen_two_stage_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_NUMERICAL_SUPPORT_H_
