#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_NUMERICAL_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_NUMERICAL_SUPPORT_H_
#include <algorithm>
#include <complex>
#include <limits>
#include <vector>

#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_driver_native.h"
#include "indefinite_aasen_two_stage_driver_test_support.h"
#include "indefinite_aasen_two_stage_numerical_support.h"
namespace asc_aasen_two_stage_driver_test {
template <typename T>
void Initialize(Sample<T>& sample) {
  for (int j = 0; j < sample.n; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      sample.a[sample.AOffset(i, j)] =
          sample.original.before[1 + j * sample.original.lda + i];
    }
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    if (sample.original.hermitian) {
      for (int i = 0; i < sample.n; ++i) {
        sample.a[sample.AOffset(i, i)].imag(
            std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
      }
    }
  }
  std::fill(sample.tb.begin(), sample.tb.end(), T{-73});
}
template <typename T>
void Fidelity(base::TestContext& test, const Sample<T>& sample,
              const asc::LapackReport& report) {
  const lapack_int n = sample.n;
  const lapack_int nrhs = sample.nrhs;
  const lapack_int lda = sample.original.lda;
  const lapack_int ldb = n + 3;
  const lapack_int ltb = sample.ltb;
  const lapack_int lwork = sample.work_entries;
  const char tri = sample.original.upper ? 'U' : 'L';
  auto a = sample.original.before;
  // Preserve the producer's source-proven original-HE normalization policy.
  if constexpr (asc::DenseBlasComplex<T>) {
    if (sample.original.hermitian) {
      for (int i = 0; i < n; ++i) {
        a[1 + i * lda + i].imag(0);
      }
    }
  }
  std::vector<T> b(static_cast<std::size_t>(ldb * nrhs + 2), T{-79});
  for (int j = 0; j < nrhs; ++j) {
    for (int i = 0; i < n; ++i) {
      b[1 + j * ldb + i] = sample.before_b[sample.BOffset(i, j)];
    }
  }
  std::vector<T> tb(static_cast<std::size_t>(ltb + 2), T{-73});
  std::vector<T> work(static_cast<std::size_t>(lwork),
                      base::Value<T>(-107, 11));
  std::vector<lapack_int> p(static_cast<std::size_t>(n), -71);
  auto q = p;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  NativePointers(sample.original.hermitian, &tri, &n, &nrhs, a.data() + 1, &lda,
                 tb.data() + 1, &ltb, p.data(), q.data(), b.data() + 1, &ldb,
                 work.data(), &lwork, &info);
  ASC_DENSE_TEST_EQ(test, info, report.native_info.value_or(-1));
  ASC_DENSE_TEST_CHECK(test, base::EqualBytes(tb.data(), sample.tb.data(),
                                              tb.size() * sizeof(T)));
  for (int j = 0; j < n; ++j) {
    ASC_DENSE_TEST_EQ(test, sample.p[j + 1], p[j]);
    ASC_DENSE_TEST_EQ(test, sample.q[j + 1], q[j]);
    for (int i = 0; i < n; ++i) {
      if (sample.original.Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(test,
                             base::EqualBytes(&sample.a[sample.AOffset(i, j)],
                                              &a[1 + j * lda + i], sizeof(T)));
      }
    }
  }
  for (int j = 0; j < nrhs; ++j) {
    for (int i = 0; i < n; ++i) {
      ASC_DENSE_TEST_CHECK(test,
                           base::EqualBytes(&sample.b[sample.BOffset(i, j)],
                                            &b[1 + j * ldb + i], sizeof(T)));
    }
  }
}
template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, bool fidelity) {
  Initialize(sample);
  const auto before_a = sample.a;
  const auto before_tb = sample.tb;
  const auto before_p = sample.p;
  const auto before_q = sample.q;
  const auto tri = sample.original.upper ? base::kUpper : base::kLower;
  const auto a = Matrix(sample);
  const auto tb = Vector(sample.tb, sample.ltb);
  const auto p = Vector(sample.p, sample.n);
  const auto q = Vector(sample.q, sample.n);
  const auto b = Rhs(sample);
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return Query(provider, tri, sample.original.hermitian, a, tb, p, q, b);
  }));
  factor::Scratch<T> scratch(plan, sample.work_entries);
  const auto before_scalar = scratch.scalar;
  const auto before_packed = scratch.packed;
  const auto before_integers = scratch.integers;
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return Driver(provider, tri, sample.original.hermitian, a, tb, p, q, b,
                  plan, scratch.workspace, report);
  });
  factor::Outcome(test, sample.n, sample.singular, status, report);
  if (sample.n != 0) {
    ASC_DENSE_TEST_EQ(test, scratch.scalar[1],
                      T{static_cast<asc::DenseBlasRealType<T>>(
                          plan.regions[base::kScalar].preferred_entries)});
    if (fidelity) {
      Fidelity(test, sample, report);
    } else {
      factor::VerifyActive(test, sample.original, sample.row, sample.ltb,
                           sample.work_entries, sample.a, sample.tb, sample.p,
                           sample.q, sample.singular, false, report);
      if (sample.nrhs != 0 && !sample.singular) {
        sample.Solution(test);
      }
    }
  } else {
    ASC_DENSE_TEST_CHECK(test,
                         base::EqualBytes(sample.a.data(), before_a.data(),
                                          sample.a.size() * sizeof(T)));
    ASC_DENSE_TEST_EQ(test, sample.tb, before_tb);
    ASC_DENSE_TEST_EQ(test, sample.p, before_p);
    ASC_DENSE_TEST_EQ(test, sample.q, before_q);
    ASC_DENSE_TEST_EQ(test, scratch.scalar, before_scalar);
    ASC_DENSE_TEST_EQ(test, scratch.packed, before_packed);
    ASC_DENSE_TEST_EQ(test, scratch.integers, before_integers);
  }
  if (sample.n == 0 || sample.nrhs == 0 || sample.singular) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), sample.before_b.data(),
                               sample.b.size() * sizeof(T)));
  }
  factor::OutputGuards(test, sample.original, sample.row, sample.a, before_a,
                       sample.tb, sample.p, sample.q);
  sample.RhsGuards(test);
  scratch.Guards(test);
}
}  // namespace asc_aasen_two_stage_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_NUMERICAL_SUPPORT_H_
