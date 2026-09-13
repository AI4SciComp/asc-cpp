#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_refinement.h"
#include "indefinite_expert_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_indefinite_expert_test::Abs1;
using asc_indefinite_expert_test::Offset;
using asc_indefinite_expert_test::Problem;
using asc_indefinite_expert_test::Scratch;
using asc_indefinite_expert_test::Vector;
using asc_indefinite_test::EqualBytes;
using asc_indefinite_test::Factor;
using asc_indefinite_test::kColumn;
using asc_indefinite_test::kHost;
using asc_indefinite_test::kLower;
using asc_indefinite_test::kPivot;
using asc_indefinite_test::kRow;
using asc_indefinite_test::kScalar;
using asc_indefinite_test::kUpper;
using asc_indefinite_test::Matrix;
using asc_indefinite_test::Pivots;
using asc_indefinite_test::QueryFactor;
using asc_indefinite_test::Raw;
using asc_indefinite_test::Take;
using asc_indefinite_test::TestContext;
using asc_indefinite_test::ToWide;
using asc_indefinite_test::Value;
using asc_indefinite_test::WithoutAllocation;

template <typename T>
struct Operands {
  asc::DenseBlasMatrixView<const T> a;
  asc::DenseBlasMatrixView<const T> af;
  asc::RawLapackPivotView pivots;
  asc::DenseBlasMatrixView<const T> b;
  asc::DenseBlasMatrixView<T> x;
  asc::DenseBlasVectorView<asc::DenseBlasRealType<T>> ferr;
  asc::DenseBlasVectorView<asc::DenseBlasRealType<T>> berr;
};

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           const Operands<T>& values) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHerfsWorkspace(provider, triangle, values.a, values.af,
                                      values.pivots, values.b, values.x,
                                      values.ferr, values.berr);
    }
  }
  return asc::QuerySyrfsWorkspace(provider, triangle, values.a, values.af,
                                  values.pivots, values.b, values.x,
                                  values.ferr, values.berr);
}

template <typename T>
asc::Status Refine(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   const Operands<T>& values,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Herfs(provider, triangle, values.a, values.af, values.pivots,
                        values.b, values.x, values.ferr, values.berr, plan,
                        workspace, report);
    }
  }
  return asc::Syrfs(provider, triangle, values.a, values.af, values.pivots,
                    values.b, values.x, values.ferr, values.berr, plan,
                    workspace, report);
}

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  Problem<T> problem;
  asc::DenseBlasTriangle triangle;
  std::array<asc::DenseBlasLayout, 4> layouts;
  std::array<int, 4> ld;
  std::array<T, 90> a{};
  std::array<T, 90> af{};
  std::array<T, 90> b{};
  std::array<T, 90> x{};
  std::array<asc::index_t, 10> pivots{};
  std::array<Real, 6> ferr{};
  std::array<Real, 6> berr{};

  Sample(int n, int nrhs, bool hermitian, asc::DenseBlasTriangle tri,
         std::array<asc::DenseBlasLayout, 4> storage, int exponent)
      : problem(n, nrhs, hermitian, exponent),
        triangle(tri),
        layouts(storage),
        ld{n + 2, n + 2, layouts[2] == kColumn ? n + 2 : nrhs + 2,
           layouts[3] == kColumn ? n + 2 : nrhs + 2} {
    problem.Original(a, triangle, layouts[0], ld[0]);
    problem.Original(af, triangle, layouts[1], ld[1]);
    problem.Rhs(b, layouts[2], ld[2]);
    x.fill(Value<T>(-229, 23));
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        x[Offset(i, j, layouts[3], ld[3])] =
            problem.expected[j * n + i] * Real{1.0625} + Value<T>(0.125, 0.125);
      }
    }
    pivots.fill(-233);
    ferr.fill(-239);
    berr.fill(-241);
  }

  [[nodiscard]] Operands<T> Views() {
    const int n = problem.n;
    const int nrhs = problem.nrhs;
    return {Matrix(std::as_const(a), n, n, layouts[0], ld[0]),
            Matrix(std::as_const(af), n, n, layouts[1], ld[1]),
            Raw(pivots, n),
            Matrix(std::as_const(b), n, nrhs, layouts[2], ld[2]),
            Matrix(x, n, nrhs, layouts[3], ld[3]),
            Vector(ferr, nrhs),
            Vector(berr, nrhs)};
  }

  void Factorize(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
    auto matrix = Matrix(af, problem.n, problem.n, layouts[1], ld[1]);
    auto pivot = Pivots(pivots, problem.n);
    const auto plan = Take(QueryFactor(provider, triangle, problem.hermitian,
                                       true, matrix, pivot));
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return Factor(provider, triangle,
                                               problem.hermitian, true, matrix,
                                               pivot, plan, workspace, report);
                               }).ok());
  }

  void Errors(TestContext& test, const std::array<T, 90>& old_x) const {
    const Real epsilon = std::numeric_limits<Real>::epsilon();
    for (int j = 0; j < problem.nrhs; ++j) {
      if (problem.n == 0) {
        ASC_DENSE_TEST_EQ(test, ferr[j + 1], 0);
        ASC_DENSE_TEST_EQ(test, berr[j + 1], 0);
        continue;
      }
      const auto before = problem.Backward(old_x, layouts[3], ld[3], j, false);
      const auto after = problem.Backward(x, layouts[3], ld[3], j, false);
      const auto guarded = problem.Backward(x, layouts[3], ld[3], j);
      bool zero_rhs = true;
      for (int i = 0; i < problem.n; ++i) {
        zero_rhs = zero_rhs && problem.rhs[j * problem.n + i] == T{};
      }
      if (zero_rhs) {
        // SAFE1 is added to both zero residual and zero denominator, so the
        // source BERR is exactly one even for the exact zero solution.
        ASC_DENSE_TEST_EQ(test, guarded, 1);
        ASC_DENSE_TEST_EQ(test, after, 0);
        ASC_DENSE_TEST_EQ(test, berr[j + 1], 1);
        for (int i = 0; i < problem.n; ++i) {
          ASC_DENSE_TEST_EQ(test, (x[Offset(i, j, layouts[3], ld[3])]), T{});
        }
        ASC_DENSE_TEST_CHECK(test, std::isfinite(ferr[j + 1]));
        ASC_DENSE_TEST_CHECK(test, ferr[j + 1] >= 0);
        continue;
      }
      ASC_DENSE_TEST_CHECK(test, before > 0.001L);
      if (!(after <= 32 * problem.n * epsilon)) {
        std::fprintf(stderr,
                     "RFS residual n=%d nrhs=%d rhs=%d layouts=%d/%d/%d/%d "
                     "before=%Lg after=%Lg berr=%g\n",
                     problem.n, problem.nrhs, j, static_cast<int>(layouts[0]),
                     static_cast<int>(layouts[1]), static_cast<int>(layouts[2]),
                     static_cast<int>(layouts[3]), before, after,
                     static_cast<double>(berr[j + 1]));
      }
      ASC_DENSE_TEST_CHECK(test, after < before * 0.01L);
      ASC_DENSE_TEST_CHECK(test, after <= 32 * problem.n * epsilon);
      ASC_DENSE_TEST_CHECK(test, std::isfinite(ferr[j + 1]));
      ASC_DENSE_TEST_CHECK(test, ferr[j + 1] >= 0);
      ASC_DENSE_TEST_CHECK(test, std::isfinite(berr[j + 1]));
      ASC_DENSE_TEST_CHECK(test, berr[j + 1] >= 0);
      // Independent wide residual differs from source working-precision
      // cancellation by a bounded n-term rounding allowance.
      ASC_DENSE_TEST_CHECK(
          test, std::abs(guarded - berr[j + 1]) <= 16 * problem.n * epsilon);
      long double error = 0;
      long double norm = 0;
      for (int i = 0; i < problem.n; ++i) {
        const auto actual = ToWide(x[Offset(i, j, layouts[3], ld[3])]);
        error = std::max(
            error, Abs1(actual - ToWide(problem.expected[j * problem.n + i])));
        norm = std::max(norm, Abs1(actual));
      }
      ASC_DENSE_TEST_CHECK(test, norm > 0);
      ASC_DENSE_TEST_CHECK(test,
                           error / norm <= 32 * ferr[j + 1] + 32 * epsilon);
    }
  }
};

template <typename T>
void Numerical(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Sample<T>& sample) {
  sample.Factorize(test, provider);
  const auto old_a = sample.a;
  const auto old_af = sample.af;
  const auto old_pivots = sample.pivots;
  const auto old_b = sample.b;
  const auto old_x = sample.x;
  const auto values = sample.Views();
  const auto plan = Take(WithoutAllocation(test, [&] {
    return Query(provider, sample.triangle, sample.problem.hermitian, values);
  }));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return Refine(provider, sample.triangle,
                                             sample.problem.hermitian, values,
                                             plan, workspace, report);
                             }).ok());
  const bool active = sample.problem.n != 0 && sample.problem.nrhs != 0;
  ASC_DENSE_TEST_EQ(test, report.called_provider, active);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), 0);
  sample.Errors(test, old_x);
  for (const auto& pair : {std::pair{sample.a.data(), old_a.data()},
                           std::pair{sample.af.data(), old_af.data()},
                           std::pair{sample.b.data(), old_b.data()}}) {
    ASC_DENSE_TEST_CHECK(test,
                         EqualBytes(pair.first, pair.second, sizeof(old_a)));
  }
  ASC_DENSE_TEST_CHECK(test, EqualBytes(sample.pivots.data(), old_pivots.data(),
                                        sizeof(old_pivots)));
  for (std::size_t k = 0; k < sample.x.size(); ++k) {
    const int relative = static_cast<int>(k) - 1;
    const int i = sample.layouts[3] == kColumn ? relative % sample.ld[3]
                                               : relative / sample.ld[3];
    const int j = sample.layouts[3] == kColumn ? relative / sample.ld[3]
                                               : relative % sample.ld[3];
    if (k == 0 || i >= sample.problem.n || j >= sample.problem.nrhs) {
      ASC_DENSE_TEST_CHECK(test,
                           EqualBytes(&sample.x[k], &old_x[k], sizeof(T)));
    }
  }
  for (std::size_t i = 0; i < sample.ferr.size(); ++i) {
    if (i == 0 || i > static_cast<std::size_t>(sample.problem.nrhs)) {
      ASC_DENSE_TEST_EQ(test, sample.ferr[i], -239);
      ASC_DENSE_TEST_EQ(test, sample.berr[i], -241);
    }
  }
  scratch.Guards(test, workspace);
}

template <typename T>
void Preflight(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool hermitian) {
  Sample<T> sample(2, 1, hermitian, kUpper, {kRow, kColumn, kRow, kRow}, 0);
  sample.Factorize(test, provider);
  const auto values = sample.Views();
  const auto plan = Take(Query(provider, kUpper, hermitian, values));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  auto reject = [&](const Operands<T>& supplied,
                    const asc::LapackWorkspacePlan& supplied_plan,
                    const asc::LapackWorkspace& supplied_workspace,
                    asc::ErrorCode code) {
    const auto before = sample;
    const auto scratch_before = scratch;
    asc::LapackReport report;
    report.called_provider = true;
    report.native_info = -99;
    const auto result = WithoutAllocation(test, [&] {
      return Refine(provider, kUpper, hermitian, supplied, supplied_plan,
                    supplied_workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, result.code(), code);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    for (const auto& pair : {std::pair{sample.a.data(), before.a.data()},
                             std::pair{sample.af.data(), before.af.data()},
                             std::pair{sample.b.data(), before.b.data()},
                             std::pair{sample.x.data(), before.x.data()}}) {
      ASC_DENSE_TEST_CHECK(
          test, EqualBytes(pair.first, pair.second, sizeof(sample.a)));
    }
    ASC_DENSE_TEST_EQ(test, sample.pivots, before.pivots);
    ASC_DENSE_TEST_EQ(test, sample.ferr, before.ferr);
    ASC_DENSE_TEST_EQ(test, sample.berr, before.berr);
    ASC_DENSE_TEST_EQ(test, scratch.scalar, scratch_before.scalar);
    ASC_DENSE_TEST_EQ(test, scratch.packed, scratch_before.packed);
    ASC_DENSE_TEST_EQ(test, scratch.pivot, scratch_before.pivot);
    ASC_DENSE_TEST_EQ(test, scratch.real, scratch_before.real);
  };
  auto short_work = workspace;
  auto& integers = short_work.regions[kPivot];
  integers = {integers.data(), integers.size() - 1, kHost};
  reject(values, plan, short_work, asc::ErrorCode::kInvalidArgument);
  auto stale = plan;
  ++stale.regions[kScalar].preferred_entries;
  reject(values, stale, workspace, asc::ErrorCode::kInvalidState);
  auto inaccessible = workspace;
  const auto scalar = workspace.regions[kScalar];
  inaccessible.regions[kScalar] = {scalar.data(), scalar.size(),
                                   asc::MemorySpace::kPinnedHost};
  reject(values, plan, inaccessible, asc::ErrorCode::kMemoryAccess);
  auto aliases = values;
  aliases.berr = aliases.ferr;
  reject(aliases, plan, workspace, asc::ErrorCode::kInvalidArgument);
  const auto saved_pivots = sample.pivots;
  sample.pivots[1] = std::numeric_limits<asc::index_t>::min();
  reject(values, plan, workspace, asc::ErrorCode::kInvalidArgument);
  sample.pivots = saved_pivots;
  // This nonsingular 2-by-2 factor has zero 1-by-1 diagonal entries. Setting
  // its meaningful off-diagonal divisor to zero must be rejected before calls.
  sample.af[Offset(0, 1, kColumn, sample.ld[1])] = T{};
  reject(values, plan, workspace, asc::ErrorCode::kNumerical);
}

template <typename T>
void WideAndQuick(TestContext& test,
                  const asc::ReferenceLapackProvider& provider,
                  bool hermitian) {
  for (const auto layout : {kColumn, kRow}) {
    Sample<T> sample(1, 1, hermitian, kUpper, {layout, layout, layout, layout},
                     0);
    sample.Factorize(test, provider);
    auto values = sample.Views();
    const auto wide = std::numeric_limits<asc::extent_t>::max();
    values.a = Matrix(std::as_const(sample.a), 1, 1, layout, wide);
    values.af = Matrix(std::as_const(sample.af), 1, 1, layout, wide);
    values.b = Matrix(std::as_const(sample.b), 1, 1, layout, wide);
    values.x = Matrix(sample.x, 1, 1, layout, wide);
    const auto plan = Take(Query(provider, kUpper, hermitian, values));
    Scratch<T> scratch;
    asc::LapackReport report;
    const auto workspace = scratch.Workspace(plan);
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return Refine(provider, kUpper, hermitian,
                                               values, plan, workspace, report);
                               }).ok());
    sample.pivots[1] = 0;
    values.b = Matrix(std::as_const(sample.b), 1, 0, layout, wide);
    values.x = Matrix(sample.x, 1, 0, layout, wide);
    values.ferr = Vector(sample.ferr, 0);
    values.berr = Vector(sample.berr, 0);
    const auto quick = Take(Query(provider, kUpper, hermitian, values));
    asc::LapackWorkspace empty;
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return Refine(provider, kUpper, hermitian,
                                               values, quick, empty, report);
                               }).ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  }
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto a : {kColumn, kRow}) {
      for (const auto af : {kColumn, kRow}) {
        for (const auto b : {kColumn, kRow}) {
          for (const auto x : {kColumn, kRow}) {
            for (const int n : {0, 1, 2, 7}) {
              for (const int nrhs : {0, 1, 3}) {
                Sample<T> sample(n, nrhs, hermitian, triangle, {a, af, b, x},
                                 0);
                Numerical(test, provider, sample);
                ++cases;
              }
            }
            const int exponent =
                sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
            for (const int scale : {-exponent, exponent}) {
              Sample<T> sample(7, 3, hermitian, triangle, {a, af, b, x}, scale);
              Numerical(test, provider, sample);
              ++cases;
            }
          }
        }
      }
    }
  }
  Preflight<T>(test, provider, hermitian);
  WideAndQuick<T>(test, provider, hermitian);
  std::printf("refinement cases=%d\n", cases);
  return test.Finish();
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false);
  }
  if (scalar == "d") {
    return Run<double>(false);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true);
  }
  return 2;
}
