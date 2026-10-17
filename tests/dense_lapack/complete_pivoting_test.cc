#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>
#include <utility>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_complete_pivoting.h"
#include "complete_pivoting_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace support = asc_complete_pivoting_test;
using support::Take;
using support::TestContext;
using support::ToWide;
using support::Value;
using support::Vector;
using support::Wide;

template <typename Operation>
auto Observe(TestContext& test, bool audit, Operation operation) {
  return audit ? support::WithoutAllocation(test, operation) : operation();
}

asc::LapackReport PriorReport() {
  asc::LapackReport report;
  report.routine.fill('Q');
  report.called_provider = true;
  report.native_info = 79;
  report.native_argument = 31;
  report.diagnostic_index = 17;
  report.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
  report.outcome = asc::LapackOutcome::kSingular;
  report.output_validity = asc::LapackOutputValidity::kUnusable;
  return report;
}

void Report(TestContext& test, const asc::ReferenceLapackProvider& provider,
            const asc::LapackReport& report, asc::extent_t n, bool factor,
            bool warning) {
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), factor && n != 0);
  if (factor && n != 0) {
    ASC_DENSE_TEST_EQ(test, report.native_info, warning ? n : 0);
  }
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.has_value(),
                    factor && warning);
  if (factor && warning) {
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, n - 1);
  }
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    warning ? asc::LapackOutcome::kAccuracyWarning
                            : asc::LapackOutcome::kSuccess);
}

template <typename T>
void Reconstruction(TestContext& test, const support::Problem<T>& original,
                    const support::Problem<T>& factors) {
  const auto n = original.n;
  std::array<asc::extent_t, 8> rows{};
  std::array<asc::extent_t, 8> columns{};
  for (asc::extent_t i = 0; i < n; ++i) {
    rows[static_cast<std::size_t>(i)] = i;
    columns[static_cast<std::size_t>(i)] = i;
  }
  for (asc::extent_t i = 0; i < n; ++i) {
    const auto k = static_cast<std::size_t>(i + 1);
    const auto row = factors.rows[k];
    const auto column = factors.columns[k];
    const bool valid =
        row >= i + 1 && row <= n && column >= i + 1 && column <= n;
    ASC_DENSE_TEST_CHECK(test, valid);
    if (!valid) {
      return;
    }
    std::swap(rows[static_cast<std::size_t>(i)],
              rows[static_cast<std::size_t>(row - 1)]);
    std::swap(columns[static_cast<std::size_t>(i)],
              columns[static_cast<std::size_t>(column - 1)]);
  }
  const long double tolerance =
      64 * static_cast<long double>(n) *
      std::numeric_limits<typename support::Problem<T>::Real>::epsilon();
  for (asc::extent_t j = 0; j < n; ++j) {
    for (asc::extent_t i = 0; i < n; ++i) {
      Wide value{};
      long double bound = 0;
      for (asc::extent_t k = 0; k < n; ++k) {
        Wide l{};
        if (i == k) {
          l = Wide{1, 0};
        } else if (i > k) {
          l = ToWide(factors.At(i, k));
        }
        const auto u = k <= j ? ToWide(factors.At(k, j)) : Wide{};
        value += l * u;
        bound += std::abs(l) * std::abs(u);
      }
      const auto row = rows[static_cast<std::size_t>(i)];
      const auto column = columns[static_cast<std::size_t>(j)];
      auto expected = ToWide(original.At(row, column));
      if (original.zero) {
        expected =
            row == column ? Wide{support::Problem<T>::Small(), 0} : Wide{};
      }
      ASC_DENSE_TEST_CHECK(
          test, std::isfinite(value.real()) && std::isfinite(value.imag()));
      ASC_DENSE_TEST_CHECK(test, std::abs(value - expected) <=
                                     tolerance * (bound + std::abs(expected)));
    }
  }
}

template <typename T>
void Residual(TestContext& test, const support::Problem<T>& original,
              const support::Problem<T>& computed) {
  const long double tolerance =
      64 * static_cast<long double>(original.n) *
      std::numeric_limits<typename support::Problem<T>::Real>::epsilon();
  const long double scale = computed.scale[1];
  ASC_DENSE_TEST_CHECK(test, std::isfinite(scale) && scale >= 0 && scale <= 1);
  for (asc::extent_t i = 0; i < original.n; ++i) {
    Wide value{};
    long double bound = 0;
    for (asc::extent_t j = 0; j < original.n; ++j) {
      auto a = ToWide(original.At(i, j));
      if (original.zero) {
        a = i == j ? Wide{support::Problem<T>::Small(), 0} : Wide{};
      }
      const auto x = ToWide(computed.rhs[static_cast<std::size_t>(j + 1)]);
      ASC_DENSE_TEST_CHECK(test,
                           std::isfinite(x.real()) && std::isfinite(x.imag()));
      value += a * x;
      bound += std::abs(a) * std::abs(x);
    }
    const auto rhs =
        scale * ToWide(original.rhs[static_cast<std::size_t>(i + 1)]);
    ASC_DENSE_TEST_CHECK(
        test, std::abs(value - rhs) <= tolerance * (bound + std::abs(rhs)));
  }
}

template <typename T>
struct NativeResult {
  std::array<T, 160> matrix{};
  std::array<T, 10> rhs{};
  std::array<lapack_int, 10> rows{};
  std::array<lapack_int, 10> columns{};
  lapack_int info = 0;
  typename support::Problem<T>::Real scale = 1;
};

template <typename T>
auto Native(TestContext& test, const support::Problem<T>& original) {
  NativeResult<T> native;
  const auto n = original.n;
  for (asc::extent_t j = 0; j < n; ++j) {
    for (asc::extent_t i = 0; i < n; ++i) {
      native.matrix[static_cast<std::size_t>(j * n + i)] = original.At(i, j);
    }
  }
  std::copy_n(original.rhs.data() + 1, n, native.rhs.data());
  native.info =
      support::NativeFactor(static_cast<lapack_int>(n), native.matrix.data(),
                            native.rows.data(), native.columns.data());
  bool valid = true;
  for (asc::extent_t i = 0; i < n; ++i) {
    const auto k = static_cast<std::size_t>(i);
    valid = valid && native.rows[k] >= i + 1 && native.rows[k] <= n &&
            native.columns[k] >= i + 1 && native.columns[k] <= n;
  }
  ASC_DENSE_TEST_CHECK(test, valid);
  if (n != 0 && valid) {
    native.scale = support::NativeSolve(
        static_cast<lapack_int>(n), native.matrix.data(), native.rhs.data(),
        native.rows.data(), native.columns.data());
  }
  return native;
}

template <typename T>
void Fidelity(TestContext& test, const support::Problem<T>& actual,
              const NativeResult<T>& native, bool solve) {
  const auto n = actual.n;
  for (asc::extent_t j = 0; j < n; ++j) {
    const auto k = static_cast<std::size_t>(j);
    ASC_DENSE_TEST_EQ(test, actual.rows[k + 1], native.rows[k]);
    ASC_DENSE_TEST_EQ(test, actual.columns[k + 1], native.columns[k]);
    for (asc::extent_t i = 0; i < n; ++i) {
      ASC_DENSE_TEST_CHECK(
          test,
          support::Same(actual.At(i, j),
                        native.matrix[static_cast<std::size_t>(j * n + i)]));
    }
    if (solve) {
      ASC_DENSE_TEST_CHECK(test,
                           support::Same(actual.rhs[k + 1], native.rhs[k]));
    }
  }
  if (solve) {
    ASC_DENSE_TEST_CHECK(test, support::Same(actual.scale[1], native.scale));
  }
}

template <typename T>
void Solves(TestContext& test, const asc::ReferenceLapackProvider& provider,
            const support::Problem<T>& original, support::Problem<T>& problem,
            const asc::LapackReport& provenance, bool fidelity,
            const NativeResult<T>& native, bool audit) {
  const auto factor =
      Take(Observe(test, audit, [&] { return problem.View(provenance); }));
  ASC_DENSE_TEST_EQ(test, factor.perturbed(), original.zero && original.n != 0);
  const auto plan = Take(Observe(test, audit, [&] {
    return asc::QueryGesc2Workspace(provider, factor,
                                    Vector(problem.rhs, problem.n),
                                    Vector(problem.scale, 1));
  }));
  const auto matrix_before = problem.dense;
  const auto rows_before = problem.rows;
  const auto columns_before = problem.columns;
  for (int reuse = 0; reuse < 2; ++reuse) {
    problem.rhs = original.rhs;
    problem.scale = original.scale;
    support::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    auto report = PriorReport();
    const auto status = Observe(test, audit, [&] {
      return asc::Gesc2(provider, factor, Vector(problem.rhs, problem.n),
                        Vector(problem.scale, 1), plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    if (!status.ok()) {
      return;
    }
    Report(test, provider, report, problem.n, false,
           factor.perturbed() || problem.scale[1] < 1);
    if (problem.n == 0) {
      ASC_DENSE_TEST_EQ(test, problem.scale[1], 1);
    }
    Residual(test, original, problem);
    if (fidelity) {
      Fidelity(test, problem, native, true);
    }
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, matrix_before));
    ASC_DENSE_TEST_EQ(test, problem.rows, rows_before);
    ASC_DENSE_TEST_EQ(test, problem.columns, columns_before);
    problem.Guards(test);
    scratch.Guards(test);
  }
}

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          const support::Problem<T>& original, bool fidelity,
          bool audit = true) {
  auto problem = original;
  const auto plan =
      Take(Observe(test, audit, [&] { return problem.Query(provider); }));
  const auto native = fidelity ? Native(test, original) : NativeResult<T>{};
  for (int reuse = 0; reuse < 2; ++reuse) {
    problem = original;
    support::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    auto report = PriorReport();
    const auto status = Observe(test, audit, [&] {
      return problem.Factor(provider, plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    if (!status.ok()) {
      return;
    }
    Report(test, provider, report, problem.n, true,
           problem.zero && problem.n != 0);
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.rhs, original.rhs));
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.scale, original.scale));
    Reconstruction(test, original, problem);
    if (fidelity) {
      if (problem.n != 0) {
        ASC_DENSE_TEST_EQ(test, report.native_info, native.info);
      }
      Fidelity(test, problem, native, false);
    }
    problem.Guards(test);
    scratch.Guards(test);
    Solves(test, provider, original, problem, report, fidelity, native, audit);
  }
}

template <typename T>
void Numerical(TestContext& test, bool fidelity) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  std::size_t cases = 0;
  for (auto layout : {support::kRow, support::kColumn}) {
    for (asc::extent_t n : {0, 1, 2, 3, 5, 8}) {
      for (asc::extent_t padding : {0, 1, 3}) {
        for (asc::extent_t seed = 0; seed < 12; ++seed) {
          for (int exponent : {-32, 0, 32}) {
            Case(test, provider,
                 support::Problem<T>(n, padding, layout, seed, exponent),
                 fidelity);
            ++cases;
          }
        }
      }
    }
    for (asc::extent_t n = 0; n <= 8; ++n) {
      for (asc::extent_t padding : {0, 1, 3}) {
        for (int rhs = 0; rhs < 3; ++rhs) {
          Case(test, provider,
               support::Problem<T>(n, padding, layout, 0, 0, true, rhs),
               fidelity);
          ++cases;
        }
      }
    }
    support::Problem<T> stress(1, 2, layout, 0, 0);
    using Real = typename support::Problem<T>::Real;
    stress.At(0, 0) = Value<T>(1);
    stress.rhs[1] = Value<T>(std::numeric_limits<Real>::max(),
                             std::numeric_limits<Real>::max());
    Case(test, provider, stress, fidelity);
    ++cases;
  }
  ASC_DENSE_TEST_EQ(test, cases, std::size_t{1460});
  std::printf("COMPLETE_PIVOTING numerical cases=%zu fidelity=%d\n", cases,
              static_cast<int>(fidelity));
}

template <typename T>
void Validation(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto layout : {support::kRow, support::kColumn}) {
    support::Problem<T> problem(3, 2, layout, 0, 0);
    auto plan = Take(problem.Query(provider));
    support::Scratch<T> scratch;
    auto workspace = scratch.Workspace(plan);
    const auto before = problem;
    const auto scratch_before = scratch;
    ++plan.regions[support::kInteger].minimum_entries;
    auto report = PriorReport();
    auto status = problem.Factor(provider, plan, workspace, report);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, before.dense) &&
                                   support::Same(problem.rhs, before.rhs) &&
                                   problem.rows == before.rows &&
                                   problem.columns == before.columns &&
                                   support::Same(problem.scale, before.scale));
    ASC_DENSE_TEST_CHECK(test, support::Same(scratch, scratch_before));
    plan = Take(problem.Query(provider));
    using Real = typename support::Problem<T>::Real;
    for (Real bad : {std::numeric_limits<Real>::quiet_NaN(),
                     std::numeric_limits<Real>::infinity(),
                     -std::numeric_limits<Real>::infinity()}) {
      problem = before;
      for (asc::extent_t j = 0; j < problem.n; ++j) {
        for (asc::extent_t i = 0; i < problem.n; ++i) {
          problem.At(i, j) = Value<T>(bad, bad);
        }
      }
      const auto invalid = problem;
      report = PriorReport();
      status = problem.Factor(provider, plan, workspace, report);
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
      ASC_DENSE_TEST_CHECK(test,
                           support::Same(problem.dense, invalid.dense) &&
                               support::Same(problem.rhs, invalid.rhs) &&
                               problem.rows == invalid.rows &&
                               problem.columns == invalid.columns &&
                               support::Same(problem.scale, invalid.scale));
      ASC_DENSE_TEST_CHECK(test, support::Same(scratch, scratch_before));
    }
  }
}

template <typename T>
void Concurrent(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Case(test, provider, support::Problem<T>(3, 1, support::kColumn, 0, 0),
       false);
  std::array<int, 4> results{};
  std::array<std::thread, 4> threads;
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::thread([&, i] {
      TestContext local;
      const auto context = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      for (int repetition = 0; repetition < 8; ++repetition) {
        for (auto layout : {support::kRow, support::kColumn}) {
          Case(local, context,
               support::Problem<T>(3 + static_cast<asc::extent_t>(i), 2, layout,
                                   repetition, 0),
               false, false);
        }
      }
      results[i] = local.Finish();
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (int result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
}

template <typename T>
void Run(TestContext& test, std::string_view mode) {
  if (mode == "mathematical" || mode == "fidelity") {
    Numerical<T>(test, mode == "fidelity");
  } else if (mode == "validation") {
    Validation<T>(test);
  } else if (mode == "concurrency") {
    Concurrent<T>(test);
  } else {
    ASC_DENSE_TEST_CHECK(test, false);
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 3) {
    return 2;
  }
  TestContext test;
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (scalar == "s") {
    Run<float>(test, mode);
  } else if (scalar == "d") {
    Run<double>(test, mode);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, mode);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, mode);
  } else {
    return 2;
  }
  return test.Finish();
}
