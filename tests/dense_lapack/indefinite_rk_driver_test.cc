#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_driver_native.h"
#include "indefinite_rk_driver_test_support.h"
#include "indefinite_rk_solve_fixture.h"
#include "indefinite_rk_solve_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_indefinite_rook_test;
namespace driver = asc_rk_driver_test;
using asc_rk_solve_test::RightHandSides;
using asc_rk_test::Sample;
using base::TestContext;

// A separate direct driver invocation checks scalar/layout/argument fidelity;
// mathematical acceptance uses the maintained independent RK/RHS oracles.
template <typename T>
void Fidelity(TestContext& test, const Sample<T>& sample,
              const RightHandSides<T>& rhs, bool preferred,
              const asc::LapackReport& report) {
  if (sample.n == 0) {
    return;
  }
  std::array<T, 4489> a{};
  std::array<T, 201> b{};
  std::array<T, 67> e{};
  std::array<lapack_int, 67> pivots{};
  std::array<T, 4288> work{};
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
  for (int j = 0; j < rhs.nrhs; ++j) {
    for (int i = 0; i < rhs.n; ++i) {
      b[j * rhs.n + i] = rhs.before[rhs.Offset(i, j)];
    }
  }
  lapack_int info = std::numeric_limits<lapack_int>::min();
  driver::Native(sample.hermitian, sample.triangle == base::kUpper ? 'U' : 'L',
                 sample.n, rhs.nrhs, a.data(), sample.n, e.data(),
                 pivots.data(), b.data(), sample.n, work.data(),
                 preferred ? 64 * sample.n : 1, info);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), info);
  for (int j = 0; j < sample.n; ++j) {
    ASC_DENSE_TEST_EQ(test, sample.pivots[j + 1], pivots[j]);
    ASC_DENSE_TEST_CHECK(test,
                         base::EqualBytes(&sample.e[j + 1], &e[j], sizeof(T)));
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(test,
                             base::EqualBytes(&sample.a[sample.Offset(i, j)],
                                              &a[j * sample.n + i], sizeof(T)));
      }
    }
  }
  for (int j = 0; j < rhs.nrhs; ++j) {
    for (int i = 0; i < rhs.n; ++i) {
      ASC_DENSE_TEST_CHECK(test,
                           base::EqualBytes(&rhs.values[rhs.Offset(i, j)],
                                            &b[j * rhs.n + i], sizeof(T)));
    }
  }
}

template <typename T>
void Reuse(TestContext& test, const asc::ReferenceLapackProvider& provider,
           const Sample<T>& sample) {
  RightHandSides<T> rhs(sample, 1, base::kRow);
  const auto extra = asc_rk_solve_test::OffDiagonal(sample.e, sample.n);
  const auto pivots = base::Raw(sample.pivots, sample.n);
  const auto plan = base::Take(
      asc_rk_solve_test::Query(provider, sample.triangle, sample.hermitian,
                               sample.ConstView(), extra, pivots, rhs.View()));
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, asc_rk_solve_test::Solve(
                provider, sample.triangle, sample.hermitian, sample.ConstView(),
                extra, pivots, rhs.View(), plan, workspace, report)
                .ok());
  rhs.Verify(test, sample);
  rhs.Guards(test);
  scratch.Guards(test, workspace);
}

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, int nrhs, asc::DenseBlasLayout rhs_layout,
          bool preferred, bool singular, bool fidelity) {
  RightHandSides<T> rhs(sample, nrhs, rhs_layout);
  const auto extra = asc_rk_test::OffDiagonal(sample.e, sample.n);
  const auto pivots = base::Pivots(sample.pivots, sample.n);
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return driver::Query(provider, sample.triangle, sample.hermitian,
                         sample.View(), extra, pivots, rhs.View());
  }));
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(
      plan, preferred ? plan.regions[base::kScalar].preferred_entries
                      : plan.regions[base::kScalar].minimum_entries);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return driver::Driver(provider, sample.triangle, sample.hermitian,
                          sample.View(), extra, pivots, rhs.View(), plan,
                          workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
  ASC_DENSE_TEST_EQ(test, report.called_provider, sample.n != 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), sample.n != 0);
  ASC_DENSE_TEST_EQ(test, report.factor_family, asc::LapackFactorFamily::kRook);
  ASC_DENSE_TEST_EQ(
      test, report.outcome,
      singular ? asc::LapackOutcome::kSingular : asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    singular ? asc::LapackOutputValidity::kDocumentedPartial
                             : asc::LapackOutputValidity::kComplete);
  if (singular) {
    ASC_DENSE_TEST_CHECK(test, report.native_info.value_or(0) > 0);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-999),
                      report.native_info.value_or(0) - 1);
    ASC_DENSE_TEST_CHECK(test,
                         base::EqualBytes(rhs.values.data(), rhs.before.data(),
                                          sizeof(rhs.values)));
  } else if (!fidelity) {
    rhs.Verify(test, sample);
    Reuse(test, provider, sample);
  }
  if (fidelity) {
    Fidelity(test, sample, rhs, preferred, report);
  } else {
    sample.Reconstruction(test);
  }
  sample.Guards(test);
  rhs.Guards(test);
  scratch.Guards(test, workspace);
}

template <typename T>
int Run(bool hermitian, bool fidelity) {
  TestContext test;
  const auto context = asc::ExecutionContext::Serial();
  const auto provider =
      base::Take(asc::ReferenceLapackProvider::Create(context));
  int cases = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto a_layout : {base::kColumn, base::kRow}) {
      for (const auto b_layout : {base::kColumn, base::kRow}) {
        for (const bool preferred : {false, true}) {
          for (const int nrhs : {0, 1, 3}) {
            for (const int n : {0, 1, 2, 3, 7, 67}) {
              Case(test, provider,
                   Sample<T>(n, hermitian, triangle, a_layout, 0, false), nrhs,
                   b_layout, preferred, false, fidelity);
              ++cases;
            }
            for (const int n : {1, 7, 67}) {
              Case(test, provider,
                   Sample<T>(n, hermitian, triangle, a_layout, 0, true), nrhs,
                   b_layout, preferred, true, fidelity);
              ++cases;
            }
          }
          const int exponent =
              sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
          for (const int scale : {-exponent, exponent}) {
            for (const int nrhs : {1, 3}) {
              Case(test, provider,
                   Sample<T>(7, hermitian, triangle, a_layout, scale, false),
                   nrhs, b_layout, preferred, false, fidelity);
              ++cases;
            }
          }
        }
      }
    }
  }
  std::printf("RK driver cases=%d mode=%s\n", cases,
              fidelity ? "fidelity" : "mathematical");
  return test.Finish();
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
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
