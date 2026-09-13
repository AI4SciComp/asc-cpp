#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_NUMERICAL_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_NUMERICAL_SUPPORT_H_
#include <limits>
#include <vector>

#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_solve_fixture.h"
#include "indefinite_aasen_two_stage_solve_native.h"
#include "indefinite_aasen_two_stage_solve_test_support.h"
namespace asc_aasen_two_stage_solve_test {
inline void Outcome(base::TestContext& test, bool active, bool singular, int n,
                    const asc::Status& status,
                    const asc::LapackReport& report) {
  ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
  ASC_DENSE_TEST_EQ(test, report.called_provider, active && !singular);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active && !singular);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kAasen);
  ASC_DENSE_TEST_EQ(
      test, report.outcome,
      singular ? asc::LapackOutcome::kSingular : asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    singular ? asc::LapackOutputValidity::kUnchanged
                             : asc::LapackOutputValidity::kComplete);
  if (singular) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), n - 1);
  } else {
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    if (active) {
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
    }
  }
}
template <typename T>
void Fidelity(base::TestContext& test, const Sample<T>& sample) {
  const auto n = sample.n;
  const int ldb = n + 3;
  std::vector<T> b(static_cast<std::size_t>(ldb * sample.nrhs + 2), T{-79});
  for (int j = 0; j < sample.nrhs; ++j) {
    for (int i = 0; i < n; ++i) {
      b[1 + j * ldb + i] = sample.before_b[sample.BOffset(i, j)];
    }
  }
  std::vector<lapack_int> p(sample.p.begin(), sample.p.end());
  std::vector<lapack_int> q(sample.q.begin(), sample.q.end());
  lapack_int info = std::numeric_limits<lapack_int>::min();
  Native(sample.original.hermitian, sample.original.upper ? 'U' : 'L', n,
         sample.nrhs, sample.original.a.data() + 1, sample.original.lda,
         sample.tb.data() + 1, sample.ltb, p.data() + 1, q.data() + 1,
         b.data() + 1, ldb, info);
  ASC_DENSE_TEST_EQ(test, info, 0);
  for (int j = 0; j < sample.nrhs; ++j) {
    for (int i = 0; i < n; ++i) {
      ASC_DENSE_TEST_CHECK(
          test, base::EqualBytes(&b[1 + j * ldb + i],
                                 &sample.b[sample.BOffset(i, j)], sizeof(T)));
    }
  }
}
template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, bool fidelity, bool reconstruct_factors = true) {
  const auto tri = sample.original.upper ? base::kUpper : base::kLower;
  const auto a = Factors(sample);
  const auto tb = Vector(sample.tb, sample.ltb);
  const auto p = Pivots(sample);
  const auto q = Vector(sample.q, sample.n);
  const auto b = Rhs(sample);
  // The query must succeed before producer outputs replace the invalid
  // sentinels.
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return Query(provider, tri, sample.original.hermitian, a, tb, p, q, b);
  }));
  const bool active = sample.n != 0 && sample.nrhs != 0;
  if (active) {
    sample.Produce(test, !fidelity && reconstruct_factors);
  }
  factor::Scratch<T> scratch(plan, 0);
  const auto before_a = sample.a;
  const auto before_tb = sample.tb;
  const auto before_p = sample.p;
  const auto before_q = sample.q;
  const auto before_integers = scratch.integers;
  const auto before_packed = scratch.packed;
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return Solve(provider, tri, sample.original.hermitian, a, tb, p, q, b, plan,
                 scratch.workspace, report);
  });
  const bool singular = active && sample.singular;
  Outcome(test, active, singular, sample.n, status, report);
  ASC_DENSE_TEST_CHECK(test, base::EqualBytes(sample.a.data(), before_a.data(),
                                              sample.a.size() * sizeof(T)));
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(sample.tb.data(), before_tb.data(),
                                        sample.tb.size() * sizeof(T)));
  ASC_DENSE_TEST_EQ(test, sample.p, before_p);
  ASC_DENSE_TEST_EQ(test, sample.q, before_q);
  sample.RhsGuards(test);
  scratch.Guards(test);
  if (!active || singular) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), sample.before_b.data(),
                               sample.b.size() * sizeof(T)));
    ASC_DENSE_TEST_EQ(test, scratch.integers, before_integers);
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(scratch.packed.data(), before_packed.data(),
                               scratch.packed.size() * sizeof(T)));
  } else if (fidelity) {
    Fidelity(test, sample);
  } else {
    sample.Solution(test);
  }
}
}  // namespace asc_aasen_two_stage_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_NUMERICAL_SUPPORT_H_
