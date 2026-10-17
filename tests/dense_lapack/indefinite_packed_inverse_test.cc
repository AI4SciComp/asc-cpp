#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_inverse_native.h"
#include "indefinite_packed_inverse_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"

namespace {
namespace inverse = asc_packed_inverse_test;
namespace base = asc_indefinite_test;

template <typename T>
void Fidelity(inverse::TestContext& test, const inverse::Sample<T>& sample,
              const std::array<T, 5000>& factors, lapack_int expected_info) {
  std::array<T, 2200> a;
  std::array<T, 68> work;
  std::array<lapack_int, 68> pivots;
  a.fill(inverse::Value<T>(-809, 29));
  work.fill(inverse::Value<T>(-811, 31));
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 97;
  pivots.fill(kGuard);
  std::size_t slot = 1;
  for (int j = 0; j < sample.n; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        a[slot++] = factors[sample.Offset(i, j)];
      }
    }
  }
  for (int i = 0; i < sample.n; ++i) {
    pivots[i + 1] = static_cast<lapack_int>(sample.pivots[i + 1]);
  }
  const auto old_pivots = pivots;
  const lapack_int n = sample.n;
  const char triangle = sample.triangle == inverse::kUpper ? 'U' : 'L';
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  inverse::Native(sample.hermitian, &triangle, &n, a.data() + 1,
                  pivots.data() + 1, work.data() + 1, &info[1]);
  ASC_DENSE_TEST_EQ(test, info[0], kGuard);
  ASC_DENSE_TEST_EQ(test, info[1], expected_info);
  ASC_DENSE_TEST_EQ(test, info[2], kGuard);
  ASC_DENSE_TEST_EQ(test, pivots, old_pivots);
  slot = 1;
  for (int j = 0; j < sample.n; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(test,
                             inverse::EqualBytes(&sample.a[sample.Offset(i, j)],
                                                 &a[slot++], sizeof(T)));
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, a.front(), inverse::Value<T>(-809, 29));
  for (std::size_t i = slot; i < a.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, a[i], inverse::Value<T>(-809, 29));
  }
  ASC_DENSE_TEST_EQ(test, work.front(), inverse::Value<T>(-811, 31));
  for (std::size_t i = sample.n + 1; i < work.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, work[i], inverse::Value<T>(-811, 31));
  }
}

template <typename T>
void Case(inverse::TestContext& test,
          const asc::ReferenceLapackProvider& provider,
          inverse::Sample<T> sample, bool fidelity) {
  const auto expected_info = inverse::Prepare(test, provider, sample);
  const auto original_factors = sample.a;
  const auto original_pivots = sample.pivots;
  const auto raw = base::Raw(sample.pivots, sample.n);
  const auto plan = inverse::Take(base::WithoutAllocation(test, [&] {
    return inverse::Query(provider, sample.triangle, sample.hermitian,
                          sample.View(), raw);
  }));
  ASC_DENSE_TEST_CHECK(
      test, inverse::EqualBytes(sample.a.data(), original_factors.data(),
                                sizeof(sample.a)));
  ASC_DENSE_TEST_EQ(test, sample.pivots, original_pivots);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kScalar].minimum_entries,
                    sample.n);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kPivot].minimum_entries, sample.n);
  ASC_DENSE_TEST_EQ(
      test, plan.regions[base::kLayout].minimum_entries,
      sample.layout == inverse::kRow ? sample.n * (sample.n + 1) / 2 : 0);
  base::Scratch<T> scratch;
  const auto work = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return inverse::Inverse(provider, sample.triangle, sample.hermitian,
                            sample.View(), raw, plan, work, report);
  });
  ASC_DENSE_TEST_EQ(
      test, status.code(),
      expected_info == 0 ? asc::ErrorCode::kOk : asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.called_provider, sample.n != 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), sample.n != 0);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  if (sample.n != 0) {
    ASC_DENSE_TEST_EQ(test, report.native_info, expected_info);
  }
  if (expected_info > 0) {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, expected_info - 1);
    ASC_DENSE_TEST_CHECK(
        test, inverse::EqualBytes(sample.a.data(), original_factors.data(),
                                  sizeof(sample.a)));
  } else {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    if (!fidelity) {
      inverse::Mathematics(test, sample);
    }
  }
  if (fidelity && sample.n != 0) {
    Fidelity(test, sample, original_factors,
             static_cast<lapack_int>(expected_info));
  }
  ASC_DENSE_TEST_EQ(test, sample.pivots, original_pivots);
  sample.Guards(test);
  scratch.Guards(test, work);
}

template <typename T>
int Run(bool hermitian, bool fidelity) {
  inverse::TestContext test;
  const auto provider = inverse::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (int n : {0, 1, 2, 5, 17, 65}) {
    for (auto triangle : {inverse::kUpper, inverse::kLower}) {
      for (auto layout : {inverse::kColumn, inverse::kRow}) {
        for (int exponent : {-20, 0, 20}) {
          for (int kind : {0, 1, 2, 3}) {
            inverse::Sample<T> sample(n, hermitian, triangle, layout, exponent);
            sample.Reset(kind);
            Case(test, provider, sample, fidelity);
            ++cases;
          }
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, 288);
  std::printf("Packed inverse ordinary cases=%d fidelity=%d\n", cases,
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
