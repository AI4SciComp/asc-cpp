#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_condition_native.h"
#include "indefinite_packed_condition_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"
namespace {
namespace condition = asc_packed_condition_test;
namespace base = asc_indefinite_test;

template <typename T>
void Fidelity(base::TestContext& test, const condition::Fixture<T>& sample,
              asc::DenseBlasRealType<T> actual) {
  using Real = asc::DenseBlasRealType<T>;
  const auto& a = sample.a;
  const lapack_int n = a.n;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 97;
  std::array<T, 2200> packed;
  std::array<T, 132> work;
  std::array<lapack_int, 68> pivots;
  std::array<lapack_int, 68> iwork;
  packed.fill(base::Value<T>(-127, 19));
  work.fill(base::Value<T>(-131, 23));
  pivots.fill(kGuard);
  iwork.fill(kGuard);
  std::size_t slot = 1;
  for (int j = 0; j < n; ++j) {
    pivots[j + 1] = static_cast<lapack_int>(a.pivots[j + 1]);
    for (int i = 0; i < n; ++i) {
      if (a.Selected(i, j)) {
        packed[slot++] = a.a[a.Offset(i, j)];
      }
    }
  }
  const auto before_a = packed;
  const auto before_pivots = pivots;
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  std::array<Real, 3> result{Real{-137}, Real{-139}, Real{-149}};
  const char triangle = a.triangle == base::kUpper ? 'U' : 'L';
  condition::Native(a.hermitian, &triangle, &n, packed.data() + 1,
                    pivots.data() + 1, &sample.norm, &result[1],
                    work.data() + 1, iwork.data() + 1, &info[1]);
  ASC_DENSE_TEST_EQ(test, info[0], kGuard);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  ASC_DENSE_TEST_EQ(test, info[2], kGuard);
  ASC_DENSE_TEST_EQ(test, result[0], Real{-137});
  ASC_DENSE_TEST_EQ(test, result[2], Real{-149});
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(&actual, &result[1], sizeof(Real)));
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(packed.data(), before_a.data(), sizeof(packed)));
  ASC_DENSE_TEST_EQ(test, pivots, before_pivots);
  for (std::size_t i = 0; i < work.size(); ++i) {
    if (i == 0 || i >= 2 * static_cast<std::size_t>(n) + 1) {
      ASC_DENSE_TEST_EQ(test, work[i], base::Value<T>(-131, 23));
    }
  }
  for (std::size_t i = 0; i < iwork.size(); ++i) {
    if (asc::DenseBlasComplex<T> || i == 0 ||
        i >= static_cast<std::size_t>(n) + 1) {
      ASC_DENSE_TEST_EQ(test, iwork[i], kGuard);
    }
  }
}

template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          condition::Fixture<T>& sample, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  sample.Prepare(test, provider);
  const auto before_a = sample.a.a;
  const auto before_pivots = sample.a.pivots;
  const auto raw = base::Raw(sample.a.pivots, sample.a.n);
  std::array<Real, 3> rcond{Real{-151}, Real{-157}, Real{-163}};
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return condition::Query(provider, sample.a.triangle, sample.a.hermitian,
                            sample.a.ConstView(), raw, sample.norm, rcond[1]);
  }));
  ASC_DENSE_TEST_EQ(test, rcond[1], Real{-157});
  const bool active = sample.a.n != 0 && sample.norm != 0;
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kScalar].minimum_entries,
                    active ? 2 * sample.a.n : 0);
  ASC_DENSE_TEST_EQ(
      test, plan.regions[base::kPivot].minimum_entries,
      active ? (asc::DenseBlasComplex<T> ? sample.a.n : 2 * sample.a.n) : 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kLayout].minimum_entries,
                    active && sample.a.layout == base::kRow
                        ? sample.a.n * (sample.a.n + 1) / 2
                        : 0);
  condition::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return condition::Condition(provider, sample.a.triangle, sample.a.hermitian,
                                sample.a.ConstView(), raw, sample.norm,
                                rcond[1], plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, active);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active);
  if (active) {
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  }
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value() &&
                                 !report.diagnostic_index.has_value());
  if (fidelity) {
    Fidelity(test, sample, rcond[1]);
  } else {
    sample.Mathematics(test, rcond[1]);
  }
  ASC_DENSE_TEST_EQ(test, rcond[0], Real{-151});
  ASC_DENSE_TEST_EQ(test, rcond[2], Real{-163});
  ASC_DENSE_TEST_CHECK(
      test,
      base::EqualBytes(sample.a.a.data(), before_a.data(), sizeof(before_a)));
  ASC_DENSE_TEST_EQ(test, sample.a.pivots, before_pivots);
  sample.a.Guards(test);
  scratch.Guards(test, workspace);
}

template <typename T>
int Run(bool hermitian, bool fidelity) {
  base::TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (int n : {0, 1, 2, 5, 17, 65}) {
    for (auto triangle : {base::kUpper, base::kLower}) {
      for (auto layout : {base::kColumn, base::kRow}) {
        for (int exponent : {-20, 0, 20}) {
          for (int kind : {0, 1, 2, 3}) {
            condition::Fixture<T> sample(n, hermitian, triangle, layout,
                                         exponent, kind);
            Case(test, provider, sample, fidelity);
            ++cases;
          }
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, 288);
  std::printf("Packed condition ordinary cases=%d fidelity=%d\n", cases,
              static_cast<int>(fidelity));
  return test.Finish();
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
  if (argc != 3) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity";
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
