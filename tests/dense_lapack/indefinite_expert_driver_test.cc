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
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_driver.h"
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
using asc_indefinite_test::kLower;
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
using asc_indefinite_test::Wide;
using asc_indefinite_test::WithoutAllocation;

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
  Real rcond = -257;

  Sample(int n, int nrhs, bool hermitian, asc::DenseBlasTriangle tri,
         std::array<asc::DenseBlasLayout, 4> storage, int exponent)
      : problem(n, nrhs, hermitian, exponent),
        triangle(tri),
        layouts(storage),
        ld{n + 2, n + 2, layouts[2] == kColumn ? n + 2 : nrhs + 2,
           layouts[3] == kColumn ? n + 2 : nrhs + 2} {
    problem.Original(a, triangle, layouts[0], ld[0]);
    problem.Rhs(b, layouts[2], ld[2]);
    af.fill(Value<T>(-251, 27));
    pivots.fill(-253);
    x.fill(Value<T>(-259, 29));
    ferr.fill(-263);
    berr.fill(-269);
  }

  void Factorize(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
    problem.Original(af, triangle, layouts[1], ld[1]);
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
};

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider, Sample<T>& sample,
           bool factored) {
  const auto& p = sample.problem;
  const auto a = Matrix(std::as_const(sample.a), p.n, p.n, sample.layouts[0],
                        sample.ld[0]);
  auto af = Matrix(sample.af, p.n, p.n, sample.layouts[1], sample.ld[1]);
  const auto b = Matrix(std::as_const(sample.b), p.n, p.nrhs, sample.layouts[2],
                        sample.ld[2]);
  auto x = Matrix(sample.x, p.n, p.nrhs, sample.layouts[3], sample.ld[3]);
  auto ferr = Vector(sample.ferr, p.nrhs);
  auto berr = Vector(sample.berr, p.nrhs);
  if (factored) {
    const asc::DenseBlasMatrixView<const T> factors = af;
    const auto pivots = Raw(sample.pivots, p.n);
    if constexpr (asc::DenseBlasComplex<T>) {
      if (p.hermitian) {
        return asc::QueryHesvxFactoredWorkspace(provider, sample.triangle, a,
                                                factors, pivots, b, x,
                                                sample.rcond, ferr, berr);
      }
    }
    return asc::QuerySysvxFactoredWorkspace(provider, sample.triangle, a,
                                            factors, pivots, b, x, sample.rcond,
                                            ferr, berr);
  }
  auto pivots = Pivots(sample.pivots, p.n);
  if constexpr (asc::DenseBlasComplex<T>) {
    if (p.hermitian) {
      return asc::QueryHesvxWorkspace(provider, sample.triangle, a, af, pivots,
                                      b, x, sample.rcond, ferr, berr);
    }
  }
  return asc::QuerySysvxWorkspace(provider, sample.triangle, a, af, pivots, b,
                                  x, sample.rcond, ferr, berr);
}

template <typename T>
asc::Status Driver(const asc::ReferenceLapackProvider& provider,
                   Sample<T>& sample, bool factored,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  const auto& p = sample.problem;
  const auto a = Matrix(std::as_const(sample.a), p.n, p.n, sample.layouts[0],
                        sample.ld[0]);
  auto af = Matrix(sample.af, p.n, p.n, sample.layouts[1], sample.ld[1]);
  const auto b = Matrix(std::as_const(sample.b), p.n, p.nrhs, sample.layouts[2],
                        sample.ld[2]);
  auto x = Matrix(sample.x, p.n, p.nrhs, sample.layouts[3], sample.ld[3]);
  auto ferr = Vector(sample.ferr, p.nrhs);
  auto berr = Vector(sample.berr, p.nrhs);
  if (factored) {
    const asc::DenseBlasMatrixView<const T> factors = af;
    const auto pivots = Raw(sample.pivots, p.n);
    if constexpr (asc::DenseBlasComplex<T>) {
      if (p.hermitian) {
        return asc::HesvxFactored(provider, sample.triangle, a, factors, pivots,
                                  b, x, sample.rcond, ferr, berr, plan,
                                  workspace, report);
      }
    }
    return asc::SysvxFactored(provider, sample.triangle, a, factors, pivots, b,
                              x, sample.rcond, ferr, berr, plan, workspace,
                              report);
  }
  auto pivots = Pivots(sample.pivots, p.n);
  if constexpr (asc::DenseBlasComplex<T>) {
    if (p.hermitian) {
      return asc::Hesvx(provider, sample.triangle, a, af, pivots, b, x,
                        sample.rcond, ferr, berr, plan, workspace, report);
    }
  }
  return asc::Sysvx(provider, sample.triangle, a, af, pivots, b, x,
                    sample.rcond, ferr, berr, plan, workspace, report);
}

// Independently invert the original small matrix by Gauss-Jordan row
// elimination in long-double complex arithmetic. No LAPACK factor/solve
// constructs this condition-number oracle.
template <typename T>
long double ReciprocalCondition(const Problem<T>& problem) {
  const int n = problem.n;
  if (n == 0) {
    return 1;
  }
  auto a = problem.full;
  std::array<Wide, 49> inverse{};
  long double a_norm = 0;
  for (int i = 0; i < n; ++i) {
    inverse[i * n + i] = 1;
    long double sum = 0;
    for (int j = 0; j < n; ++j) {
      sum += std::abs(a[i * n + j]);
    }
    a_norm = std::max(a_norm, sum);
  }
  for (int k = 0; k < n; ++k) {
    int selected = k;
    for (int i = k + 1; i < n; ++i) {
      if (std::abs(a[i * n + k]) > std::abs(a[selected * n + k])) {
        selected = i;
      }
    }
    for (int j = 0; j < n; ++j) {
      std::swap(a[k * n + j], a[selected * n + j]);
      std::swap(inverse[k * n + j], inverse[selected * n + j]);
    }
    const auto diagonal = a[k * n + k];
    for (int j = 0; j < n; ++j) {
      a[k * n + j] /= diagonal;
      inverse[k * n + j] /= diagonal;
    }
    for (int i = 0; i < n; ++i) {
      if (i == k) {
        continue;
      }
      const auto multiplier = a[i * n + k];
      for (int j = 0; j < n; ++j) {
        a[i * n + j] -= multiplier * a[k * n + j];
        inverse[i * n + j] -= multiplier * inverse[k * n + j];
      }
    }
  }
  long double inverse_norm = 0;
  for (int i = 0; i < n; ++i) {
    long double sum = 0;
    for (int j = 0; j < n; ++j) {
      sum += std::abs(inverse[i * n + j]);
    }
    inverse_norm = std::max(inverse_norm, sum);
  }
  return 1 / (a_norm * inverse_norm);
}

template <typename T>
void Errors(TestContext& test, const Sample<T>& sample) {
  using Real = asc::DenseBlasRealType<T>;
  const auto& p = sample.problem;
  const Real epsilon = std::numeric_limits<Real>::epsilon();
  const auto condition = ReciprocalCondition(p);
  ASC_DENSE_TEST_CHECK(test, std::isfinite(sample.rcond));
  // A condition estimator is not an exact inverse norm. These independently
  // selected small matrices require a factor-two estimate, not bit equality.
  ASC_DENSE_TEST_CHECK(test, sample.rcond >= condition * (1 - 128 * epsilon));
  ASC_DENSE_TEST_CHECK(test, sample.rcond <= 2 * condition);
  for (int j = 0; j < p.nrhs; ++j) {
    if (p.n == 0) {
      ASC_DENSE_TEST_EQ(test, sample.ferr[j + 1], 0);
      ASC_DENSE_TEST_EQ(test, sample.berr[j + 1], 0);
      continue;
    }
    const auto residual =
        p.Backward(sample.x, sample.layouts[3], sample.ld[3], j, false);
    const auto guarded =
        p.Backward(sample.x, sample.layouts[3], sample.ld[3], j);
    ASC_DENSE_TEST_CHECK(test, residual <= 32 * p.n * epsilon);
    ASC_DENSE_TEST_CHECK(
        test, std::abs(guarded - sample.berr[j + 1]) <= 16 * p.n * epsilon);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(sample.ferr[j + 1]));
    ASC_DENSE_TEST_CHECK(test, sample.ferr[j + 1] >= 0);
    ASC_DENSE_TEST_CHECK(test, sample.berr[j + 1] >= 0);
    long double error = 0;
    long double norm = 0;
    for (int i = 0; i < p.n; ++i) {
      const auto actual =
          ToWide(sample.x[Offset(i, j, sample.layouts[3], sample.ld[3])]);
      error = std::max(error, Abs1(actual - ToWide(p.expected[j * p.n + i])));
      norm = std::max(norm, Abs1(actual));
    }
    if (norm == 0) {
      ASC_DENSE_TEST_EQ(test, error, 0);
    } else {
      ASC_DENSE_TEST_CHECK(
          test, error / norm <= 32 * sample.ferr[j + 1] + 32 * epsilon);
    }
  }
}

template <typename T>
void Numerical(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Sample<T>& sample, bool factored, bool minimum) {
  if (factored) {
    sample.Factorize(test, provider);
  }
  const auto a_before = sample.a;
  const auto b_before = sample.b;
  const auto af_before = sample.af;
  const auto pivots_before = sample.pivots;
  const auto x_before = sample.x;
  const auto plan = Take(WithoutAllocation(
      test, [&] { return Query(provider, sample, factored); }));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(
      plan, minimum ? plan.regions[kScalar].minimum_entries : -1);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return Driver(provider, sample, factored, plan,
                                             workspace, report);
                             }).ok());
  const bool active = sample.problem.n != 0;
  ASC_DENSE_TEST_EQ(test, report.called_provider, active);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), 0);
  Errors(test, sample);
  ASC_DENSE_TEST_CHECK(
      test, EqualBytes(sample.a.data(), a_before.data(), sizeof(a_before)));
  ASC_DENSE_TEST_CHECK(
      test, EqualBytes(sample.b.data(), b_before.data(), sizeof(b_before)));
  if (factored) {
    ASC_DENSE_TEST_CHECK(test, EqualBytes(sample.af.data(), af_before.data(),
                                          sizeof(af_before)));
    ASC_DENSE_TEST_CHECK(test,
                         EqualBytes(sample.pivots.data(), pivots_before.data(),
                                    sizeof(pivots_before)));
  } else {
    for (std::size_t k = 0; k < sample.af.size(); ++k) {
      const int relative = static_cast<int>(k) - 1;
      const int i = sample.layouts[1] == kColumn ? relative % sample.ld[1]
                                                 : relative / sample.ld[1];
      const int j = sample.layouts[1] == kColumn ? relative / sample.ld[1]
                                                 : relative % sample.ld[1];
      const bool selected = sample.triangle == kUpper ? i <= j : i >= j;
      if (k == 0 || i >= sample.problem.n || j >= sample.problem.n ||
          !selected) {
        ASC_DENSE_TEST_CHECK(
            test, EqualBytes(&sample.af[k], &af_before[k], sizeof(T)));
      }
    }
  }
  for (std::size_t k = 0; k < sample.x.size(); ++k) {
    const int relative = static_cast<int>(k) - 1;
    const int i = sample.layouts[3] == kColumn ? relative % sample.ld[3]
                                               : relative / sample.ld[3];
    const int j = sample.layouts[3] == kColumn ? relative / sample.ld[3]
                                               : relative % sample.ld[3];
    if (k == 0 || i >= sample.problem.n || j >= sample.problem.nrhs) {
      ASC_DENSE_TEST_CHECK(test,
                           EqualBytes(&sample.x[k], &x_before[k], sizeof(T)));
    }
  }
  for (std::size_t i = 0; i < sample.ferr.size(); ++i) {
    if (i == 0 || i > static_cast<std::size_t>(sample.problem.nrhs)) {
      ASC_DENSE_TEST_EQ(test, sample.ferr[i], -263);
      ASC_DENSE_TEST_EQ(test, sample.berr[i], -269);
    }
  }
  ASC_DENSE_TEST_EQ(test, sample.pivots.front(), -253);
  for (std::size_t i = static_cast<std::size_t>(sample.problem.n) + 1;
       i < sample.pivots.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, sample.pivots[i], -253);
  }
  scratch.Guards(test, workspace);
}

template <typename T>
void DiagonalCase(TestContext& test,
                  const asc::ReferenceLapackProvider& provider,
                  Sample<T>& sample, bool factored, bool warning, int bad_index,
                  bool minimum) {
  using Real = asc::DenseBlasRealType<T>;
  auto& p = sample.problem;
  p.full.fill({});
  const Real small = std::numeric_limits<Real>::epsilon() / 8;
  for (int i = 0; i < p.n; ++i) {
    Real diagonal = 1;
    if (i == bad_index) {
      diagonal = warning ? small : Real{};
    }
    p.full[i * p.n + i] = diagonal;
  }
  for (int j = 0; j < p.nrhs; ++j) {
    for (int i = 0; i < p.n; ++i) {
      const auto value = p.full[i * p.n + i] * ToWide(p.expected[j * p.n + i]);
      p.rhs[j * p.n + i] = Value<T>(value.real(), value.imag());
    }
  }
  p.Original(sample.a, sample.triangle, sample.layouts[0], sample.ld[0]);
  p.Rhs(sample.b, sample.layouts[2], sample.ld[2]);
  if (factored) {
    // This independent diagonal matrix already is its exact L*D*L^T/H
    // factorization with identity paired-pivot metadata.
    p.Original(sample.af, sample.triangle, sample.layouts[1], sample.ld[1]);
    for (int i = 0; i < p.n; ++i) {
      sample.af[Offset(i, i, sample.layouts[1], sample.ld[1])] =
          Value<T>(p.full[i * p.n + i].real());
      sample.pivots[i + 1] = i + 1;
    }
  }
  const auto old_x = sample.x;
  const auto old_ferr = sample.ferr;
  const auto old_berr = sample.berr;
  const auto plan = Take(Query(provider, sample, factored));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(
      plan, minimum ? plan.regions[kScalar].minimum_entries : -1);
  asc::LapackReport report;
  const auto result = WithoutAllocation(test, [&] {
    return Driver(provider, sample, factored, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, result.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.called_provider, warning || !factored);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), warning || !factored);
  if (warning) {
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), p.n + 1);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_EQ(test, sample.rcond, small);
    Errors(test, sample);
  } else {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), bad_index);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(bad_index + 1),
                      bad_index + 1);
    ASC_DENSE_TEST_EQ(test, sample.rcond, factored ? Real{-257} : Real{});
    ASC_DENSE_TEST_EQ(test, sample.x, old_x);
    ASC_DENSE_TEST_EQ(test, sample.ferr, old_ferr);
    ASC_DENSE_TEST_EQ(test, sample.berr, old_berr);
  }
  scratch.Guards(test, workspace);
}

template <typename T>
void Failures(TestContext& test, const asc::ReferenceLapackProvider& provider,
              bool hermitian) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto a : {kColumn, kRow}) {
      for (const auto af : {kColumn, kRow}) {
        for (const auto b : {kColumn, kRow}) {
          for (const auto x : {kColumn, kRow}) {
            for (const bool factored : {false, true}) {
              for (const bool minimum : {false, true}) {
                for (const bool warning : {false, true}) {
                  for (const int failed : {0, 6}) {
                    Sample<T> sample(7, 3, hermitian, triangle, {a, af, b, x},
                                     0);
                    DiagonalCase(test, provider, sample, factored, warning,
                                 failed, minimum);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

template <typename T>
void NanInput(TestContext& test, const asc::ReferenceLapackProvider& provider,
              bool hermitian, asc::DenseBlasTriangle triangle,
              asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  Sample<T> sample(1, 1, hermitian, triangle, {layout, layout, layout, layout},
                   0);
  sample.a[1] = Value<T>(std::numeric_limits<Real>::quiet_NaN());
  const auto x_before = sample.x;
  const auto ferr_before = sample.ferr;
  const auto berr_before = sample.berr;
  const auto plan = Take(Query(provider, sample, false));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto result = WithoutAllocation(test, [&] {
    return Driver(provider, sample, false, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, result.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 1);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kPartialResult);
  ASC_DENSE_TEST_EQ(test, sample.rcond, Real{});
  ASC_DENSE_TEST_EQ(test, sample.x, x_before);
  ASC_DENSE_TEST_EQ(test, sample.ferr, ferr_before);
  ASC_DENSE_TEST_EQ(test, sample.berr, berr_before);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      NanInput<T>(test, provider, hermitian, triangle, layout);
    }
    for (const auto a : {kColumn, kRow}) {
      for (const auto af : {kColumn, kRow}) {
        for (const auto b : {kColumn, kRow}) {
          for (const auto x : {kColumn, kRow}) {
            for (const bool factored : {false, true}) {
              for (const bool minimum : {false, true}) {
                for (const int n : {0, 1, 2, 7}) {
                  for (const int nrhs : {0, 1, 3}) {
                    Sample<T> sample(n, nrhs, hermitian, triangle,
                                     {a, af, b, x}, 0);
                    Numerical(test, provider, sample, factored, minimum);
                    ++cases;
                  }
                }
                const int exponent =
                    sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
                for (const int scale : {-exponent, exponent}) {
                  Sample<T> sample(7, 3, hermitian, triangle, {a, af, b, x},
                                   scale);
                  Numerical(test, provider, sample, factored, minimum);
                  ++cases;
                }
              }
            }
          }
        }
      }
    }
  }
  Failures<T>(test, provider, hermitian);
  std::printf("expert driver cases=%d failure/warning cases=512\n", cases);
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
