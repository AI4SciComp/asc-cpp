#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
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
void Initialize(condition::Fixture<T>& sample,
                asc::DenseBlasRealType<T> scale) {
  const int n = sample.a.n;
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      T value{};
      if (n == 1) {
        value = T{scale};
      } else if (i != j) {
        value = base::Value<T>(scale, scale);
        if constexpr (asc::DenseBlasComplex<T>) {
          if (sample.a.hermitian && i < j) {
            value = std::conj(value);
          }
        }
      }
      sample.a.full[i * n + j] = base::ToWide(value);
      if (sample.a.Selected(i, j)) {
        sample.a.a[sample.a.Offset(i, j)] = value;
      }
    }
  }
  const long double magnitude =
      n == 2 && asc::DenseBlasComplex<T>
          ? std::hypot(static_cast<long double>(scale),
                       static_cast<long double>(scale))
          : static_cast<long double>(scale);
  using Real = asc::DenseBlasRealType<T>;
  sample.norm = magnitude <= std::numeric_limits<Real>::max()
                    ? static_cast<Real>(magnitude)
                    : std::numeric_limits<Real>::infinity();
  sample.expected = 1;
  sample.a.original = sample.a.a;
}

template <typename T>
void Active(base::TestContext& test, const condition::Fixture<T>& sample,
            asc::DenseBlasRealType<T> actual, bool fidelity,
            const asc::Status& status, const asc::LapackReport& report) {
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  ASC_DENSE_TEST_EQ(test, status.ok(), std::isfinite(actual));
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    std::isfinite(actual)
                        ? asc::LapackOutcome::kSuccess
                        : asc::LapackOutcome::kAccuracyWarning);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    std::isfinite(actual)
                        ? asc::LapackOutputValidity::kComplete
                        : asc::LapackOutputValidity::kDocumentedPartial);
  if (fidelity) {
    Fidelity(test, sample, actual);
  } else {
    sample.Mathematics(test, actual);
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
  const bool admitted = std::isfinite(sample.norm);
  // Keep a positive-branch plan for explicit rejection of an unrepresentable
  // true norm. The substitute is never used as a mathematical oracle.
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return condition::Query(provider, sample.a.triangle, sample.a.hermitian,
                            sample.a.ConstView(), raw,
                            admitted ? sample.norm : Real{1}, rcond[1]);
  }));
  condition::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto before_scratch = scratch;
  if (!admitted) {
    const auto rejected =
        condition::Query(provider, sample.a.triangle, sample.a.hermitian,
                         sample.a.ConstView(), raw, sample.norm, rcond[1]);
    ASC_DENSE_TEST_EQ(test, rejected.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return condition::Condition(provider, sample.a.triangle, sample.a.hermitian,
                                sample.a.ConstView(), raw, sample.norm,
                                rcond[1], plan, workspace, report);
  });
  if (admitted) {
    Active(test, sample, rcond[1], fidelity, status, report);
  } else {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, rcond[1], Real{-157});
    ASC_DENSE_TEST_EQ(test, scratch.scalar, before_scratch.scalar);
    ASC_DENSE_TEST_EQ(test, scratch.packed, before_scratch.packed);
    ASC_DENSE_TEST_EQ(test, scratch.pivot, before_scratch.pivot);
  }
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value() &&
                                 !report.diagnostic_index.has_value());
  ASC_DENSE_TEST_EQ(test, rcond[0], Real{-151});
  ASC_DENSE_TEST_EQ(test, rcond[2], Real{-163});
  ASC_DENSE_TEST_CHECK(
      test,
      base::EqualBytes(sample.a.a.data(), before_a.data(), sizeof(before_a)));
  ASC_DENSE_TEST_EQ(test, sample.a.pivots, before_pivots);
  sample.a.Guards(test);
  scratch.Guards(test, workspace);
  std::printf("ANORM=%La RCOND=%La admitted=%d\n",
              static_cast<long double>(sample.norm),
              static_cast<long double>(rcond[1]), static_cast<int>(admitted));
}

template <typename T>
int Run(bool hermitian, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  base::TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const auto low = std::numeric_limits<Real>::min();
  const auto high = std::numeric_limits<Real>::max();
  int cases = 0;
  int admitted = 0;
  for (int n : {1, 2}) {
    for (auto triangle : {base::kUpper, base::kLower}) {
      for (auto layout : {base::kColumn, base::kRow}) {
        for (Real scale :
             {low / Real{8}, low / Real{2}, low, Real{1}, high / Real{8},
              high / Real{4}, high / Real{2}, high * Real{0.75}}) {
          condition::Fixture<T> sample(n, hermitian, triangle, layout, 0, 0);
          Initialize(sample, scale);
          std::printf(
              "Packed condition range n=%d he=%d tri=%d layout=%d scale=%La\n",
              n, static_cast<int>(hermitian), static_cast<int>(triangle),
              static_cast<int>(layout), static_cast<long double>(scale));
          std::fflush(stdout);
          Case(test, provider, sample, fidelity);
          admitted += static_cast<int>(std::isfinite(sample.norm));
          ++cases;
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, 64);
  ASC_DENSE_TEST_EQ(test, admitted, asc::DenseBlasComplex<T> ? 60 : 64);
  std::printf(
      "Packed condition range cases=%d representable_norm=%d "
      "nonrepresentable_norm=%d\n",
      cases, admitted, cases - admitted);
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
