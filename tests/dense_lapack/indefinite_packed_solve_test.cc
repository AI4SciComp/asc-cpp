#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_solve_native.h"
#include "indefinite_packed_solve_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"

namespace {
namespace solve = asc_packed_solve_test;
namespace base = asc_indefinite_test;

template <typename T>
void Fidelity(solve::TestContext& test, const solve::Fixture<T>& sample) {
  const auto& factor = sample.a;
  std::array<T, 2200> a;
  std::array<T, 700> b;
  std::array<lapack_int, 68> pivots;
  a.fill(solve::Value<T>(-719, 31));
  b.fill(solve::Value<T>(-727, 37));
  pivots.fill(std::numeric_limits<lapack_int>::max() - 73);
  const lapack_int n = factor.n;
  const lapack_int nrhs = sample.nrhs;
  const lapack_int ldb =
      sample.rhs_layout == solve::kRow || nrhs == 1 ? n : sample.Leading();
  std::size_t slot = 1;
  // Independent physical column enumeration, with no production packing helper.
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      if (factor.Selected(i, j)) {
        a[slot++] = factor.a[factor.Offset(i, j)];
      }
    }
  }
  for (int i = 0; i < n; ++i) {
    pivots[i + 1] = static_cast<lapack_int>(factor.pivots[i + 1]);
  }
  for (int j = 0; j < nrhs; ++j) {
    for (int i = 0; i < n; ++i) {
      b[1 + j * ldb + i] = sample.original_rhs[sample.Offset(i, j)];
    }
  }
  const auto before_a = a;
  const auto before_b = b;
  const auto before_pivots = pivots;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 83;
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  const char triangle = factor.triangle == solve::kUpper ? 'U' : 'L';
  solve::Native(factor.hermitian, &triangle, &n, &nrhs, a.data() + 1,
                pivots.data() + 1, b.data() + 1, &ldb, &info[1]);
  ASC_DENSE_TEST_EQ(test, info[0], kGuard);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  ASC_DENSE_TEST_EQ(test, info[2], kGuard);
  ASC_DENSE_TEST_CHECK(test,
                       solve::EqualBytes(a.data(), before_a.data(), sizeof(a)));
  ASC_DENSE_TEST_EQ(test, pivots, before_pivots);
  auto expected_padding = before_b;
  for (int j = 0; j < nrhs; ++j) {
    for (int i = 0; i < n; ++i) {
      ASC_DENSE_TEST_CHECK(test,
                           solve::EqualBytes(&sample.rhs[sample.Offset(i, j)],
                                             &b[1 + j * ldb + i], sizeof(T)));
      expected_padding[1 + j * ldb + i] = b[1 + j * ldb + i];
    }
  }
  ASC_DENSE_TEST_CHECK(
      test, solve::EqualBytes(b.data(), expected_padding.data(), sizeof(b)));
}

template <typename T>
void Case(solve::TestContext& test,
          const asc::ReferenceLapackProvider& provider,
          solve::Fixture<T> sample, bool fidelity) {
  sample.Prepare(test, provider);
  const auto pivots = sample.Pivots();
  const auto rhs = sample.Rhs();
  const auto plan = solve::Take(base::WithoutAllocation(test, [&] {
    return solve::Query(provider, sample.a.triangle, sample.a.hermitian,
                        sample.a.ConstView(), pivots, rhs);
  }));
  sample.UnchangedInputs(test);
  ASC_DENSE_TEST_CHECK(
      test, solve::EqualBytes(sample.rhs.data(), sample.original_rhs.data(),
                              sizeof(sample.rhs)));
  const bool active = sample.a.n != 0 && sample.nrhs != 0;
  const int packed_a =
      sample.a.layout == solve::kRow ? sample.a.n * (sample.a.n + 1) / 2 : 0;
  const int packed_b =
      sample.rhs_layout == solve::kRow ? sample.a.n * sample.nrhs : 0;
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kPivot].minimum_entries,
                    active ? sample.a.n : 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kLayout].minimum_entries,
                    active ? packed_a + packed_b : 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kScalar].minimum_entries, 0);
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return solve::Solve(provider, sample.a.triangle, sample.a.hermitian,
                        sample.a.ConstView(), pivots, rhs, plan, workspace,
                        report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, active);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  if (active) {
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    if (fidelity) {
      Fidelity(test, sample);
    } else {
      sample.Mathematics(test);
    }
  } else {
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(
        test, solve::EqualBytes(sample.rhs.data(), sample.original_rhs.data(),
                                sizeof(sample.rhs)));
  }
  sample.UnchangedInputs(test);
  sample.Padding(test);
  scratch.Guards(test, workspace);
}

template <typename T>
int Range(bool hermitian, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  solve::TestContext test;
  const auto provider = solve::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::array scales{std::numeric_limits<Real>::min() / Real{8},
                          std::numeric_limits<Real>::min() / Real{2},
                          std::numeric_limits<Real>::min(),
                          std::numeric_limits<Real>::max() * Real{0.75}};
  int cases = 0;
  for (auto triangle : {solve::kUpper, solve::kLower}) {
    for (auto al : {solve::kColumn, solve::kRow}) {
      for (auto bl : {solve::kColumn, solve::kRow}) {
        for (int nrhs : {1, 3}) {
          for (int n : {1, 2}) {
            for (const auto scale : scales) {
              solve::Fixture<T> sample(n, nrhs, hermitian, triangle, al, bl, 0,
                                       0);
              for (int j = 0; j < n; ++j) {
                for (int i = 0; i < n; ++i) {
                  T value{};
                  if (n == 1) {
                    value = T{scale};
                  } else if (i != j) {
                    value = solve::Value<T>(scale, scale);
                    if constexpr (asc::DenseBlasComplex<T>) {
                      if (hermitian && i < j) {
                        value = std::conj(value);
                      }
                    }
                  }
                  sample.a.full[i * n + j] = solve::ToWide(value);
                  if (sample.a.Selected(i, j)) {
                    sample.a.a[sample.a.Offset(i, j)] = value;
                  }
                }
              }
              sample.a.original = sample.a.a;
              std::printf(
                  "Packed solve range he=%d tri=%d al=%d bl=%d n=%d nrhs=%d "
                  "scale=%La\n",
                  static_cast<int>(hermitian), static_cast<int>(triangle),
                  static_cast<int>(al), static_cast<int>(bl), n, nrhs,
                  static_cast<long double>(scale));
              std::fflush(stdout);
              Case(test, provider, std::move(sample), fidelity);
              ++cases;
            }
          }
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, 128);
  std::printf("Packed solve required range cases=%d fidelity=%d\n", cases,
              static_cast<int>(fidelity));
  return test.Finish();
}

template <typename T>
int Run(bool hermitian, bool fidelity, bool range) {
  if (range) {
    return Range<T>(hermitian, fidelity);
  }
  solve::TestContext test;
  const auto provider = solve::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (int n : {0, 1, 2, 5, 17, 65}) {
    for (int nrhs : {0, 1, 3}) {
      for (auto triangle : {solve::kUpper, solve::kLower}) {
        for (auto a_layout : {solve::kColumn, solve::kRow}) {
          for (auto b_layout : {solve::kColumn, solve::kRow}) {
            for (int exponent : {-20, 0, 20}) {
              for (int kind : {0, 3}) {
                Case(test, provider,
                     solve::Fixture<T>(n, nrhs, hermitian, triangle, a_layout,
                                       b_layout, exponent, kind),
                     fidelity);
                ++cases;
              }
            }
          }
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, 864);
  std::printf("Packed solve ordinary cases=%d fidelity=%d\n", cases,
              static_cast<int>(fidelity));
  return test.Finish();
}
}  // namespace

int main(int argc, char** argv) {
  asc_lapack_test::NormalReturnGuard normal_return;
  if (argc != 3) {
    return 2;
  }
  const std::string_view scalar = argv[1];
  const std::string_view mode = argv[2];
  if (mode != "mathematical" && mode != "fidelity" &&
      mode != "range_mathematical" && mode != "range_fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity" || mode == "range_fidelity";
  const bool range = mode.starts_with("range_");
  if (scalar == "s") {
    return Run<float>(false, fidelity, range);
  }
  if (scalar == "d") {
    return Run<double>(false, fidelity, range);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false, fidelity, range);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false, fidelity, range);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true, fidelity, range);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true, fidelity, range);
  }
  return 2;
}
