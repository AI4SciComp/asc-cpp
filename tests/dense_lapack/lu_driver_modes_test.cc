#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_driver_test_support.h"

namespace {
using asc_driver_test::kConjugate;
using asc_driver_test::kNone;
using asc_driver_test::kTranspose;
using asc_driver_test::Sample;
using asc_driver_test::Take;
using asc_driver_test::TestContext;
using asc_driver_test::Value;
template <typename T>
using Real = asc::DenseBlasRealType<T>;

struct Mode {
  char fact;
  asc::LapackEquilibration equilibration;
};
constexpr std::array<Mode, 6> kModes{{
    {'N', asc::LapackEquilibration::kNone},
    {'E', asc::LapackEquilibration::kNone},
    {'F', asc::LapackEquilibration::kNone},
    {'F', asc::LapackEquilibration::kRows},
    {'F', asc::LapackEquilibration::kColumns},
    {'F', asc::LapackEquilibration::kBoth},
}};

template <typename T>
T Solution(asc::extent_t i, asc::extent_t j) {
  return Value<T>(1 + i + j, 0.25L * (i - j));
}

template <typename T>
void PrepareDiagonal(Sample<T>& sample, asc::DenseBlasTranspose trans,
                     Mode mode, int worker) {
  sample.equed = mode.equilibration;
  for (asc::extent_t i = 0; i < sample.n; ++i) {
    const T diagonal = std::ldexp(Real<T>{1}, 2 * worker - 4) *
                       Value<T>(2 + i, 0.5L * (i + 1));
    const bool rows = mode.equilibration == asc::LapackEquilibration::kRows ||
                      mode.equilibration == asc::LapackEquilibration::kBoth;
    const bool columns =
        mode.equilibration == asc::LapackEquilibration::kColumns ||
        mode.equilibration == asc::LapackEquilibration::kBoth;
    const Real<T> row_scale =
        rows ? std::ldexp(Real<T>{1}, static_cast<int>(i) - worker) : 1;
    const Real<T> column_scale =
        columns ? std::ldexp(Real<T>{1}, worker - 2 * static_cast<int>(i)) : 1;
    if (mode.fact == 'F') {
      sample.rows[1 + i] = row_scale;
      sample.columns[1 + i] = column_scale;
      sample.pivots[1 + i] = i + 1;
    }
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      const T entry = i == j ? row_scale * diagonal * column_scale : T{};
      sample.a[sample.Offset(0, i, j)] = entry;
      if (mode.fact == 'F') {
        sample.af[sample.Offset(1, i, j)] = entry;
      }
    }
    T operation = diagonal;
    if constexpr (asc::DenseBlasComplex<T>) {
      if (trans == kConjugate) {
        operation = std::conj(diagonal);
      }
    }
    for (asc::extent_t j = 0; j < sample.nrhs; ++j) {
      sample.b[sample.Offset(2, i, j)] = operation * Solution<T>(i, j);
    }
  }
}

template <typename T>
void CheckSolution(TestContext& test, const Sample<T>& sample) {
  for (asc::extent_t j = 0; j < sample.nrhs; ++j) {
    for (asc::extent_t i = 0; i < sample.n; ++i) {
      const T expected = Solution<T>(i, j);
      ASC_DENSE_TEST_CHECK(
          test, std::abs(sample.x[sample.Offset(3, i, j)] - expected) <=
                    16 * std::numeric_limits<Real<T>>::epsilon() *
                        std::abs(expected));
    }
    ASC_DENSE_TEST_CHECK(
        test, std::isfinite(sample.ferr[1 + j]) && sample.ferr[1 + j] >= 0);
    ASC_DENSE_TEST_CHECK(
        test, std::isfinite(sample.berr[1 + j]) && sample.berr[1 + j] >= 0);
  }
  ASC_DENSE_TEST_CHECK(test,
                       std::isfinite(sample.statistics.reciprocal_condition) &&
                           sample.statistics.reciprocal_condition > 0);
  ASC_DENSE_TEST_CHECK(
      test, std::isfinite(sample.statistics.reciprocal_pivot_growth) &&
                sample.statistics.reciprocal_pivot_growth > 0);
}

template <typename T>
void FinalOutcome(TestContext& test, int worker, Sample<T>& sample,
                  asc::DenseBlasTranspose trans, Mode mode,
                  const asc::ReferenceLapackProvider& provider,
                  const asc::LapackWorkspacePlan& plan) {
  const auto before = sample;
  auto final_plan = plan;
  auto final_work = sample.Workspace(plan);
  if (worker == 0) {
    if (mode.fact == 'F') {
      sample.af[sample.Offset(1, 1, 1)] = T{};
    } else {
      sample.a[sample.Offset(0, 1, 1)] = T{};
    }
  } else if (worker == 1) {
    if (mode.fact == 'F') {
      sample.pivots[1] = 0;
    } else {
      final_work.regions[asc_driver_test::kScalar] = {nullptr, 0,
                                                      asc_driver_test::kHost};
    }
  } else if (worker == 2) {
    final_plan.total_byte_limit = 0;
  }
  asc::LapackReport report;
  const auto status = sample.Execute(provider, trans, mode.fact, final_plan,
                                     final_work, report);
  if (worker < 3) {
    ASC_DENSE_TEST_EQ(test, sample.x, before.x);
    ASC_DENSE_TEST_EQ(test, sample.ferr, before.ferr);
    ASC_DENSE_TEST_EQ(test, sample.berr, before.berr);
    if (worker == 0) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
      if (mode.fact == 'F') {
        ASC_DENSE_TEST_CHECK(test,
                             !report.called_provider && !report.native_info);
      } else {
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        ASC_DENSE_TEST_EQ(test, report.native_info, 2);
        ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition, 0);
      }
    } else {
      ASC_DENSE_TEST_CHECK(test,
                           !report.called_provider && !report.native_info);
      if (worker == 1) {
        ASC_DENSE_TEST_EQ(test, status.code(),
                          mode.fact == 'F' ? asc::ErrorCode::kIndex
                                           : asc::ErrorCode::kInvalidArgument);
      } else {
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
      }
    }
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    CheckSolution(test, sample);
  }
}

template <typename T>
void Worker(TestContext& test, int worker, unsigned int layouts,
            asc::DenseBlasTranspose trans, Mode mode,
            const asc::LapackWorkspacePlan& plan) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Sample<T> sample(3, 2, layouts);
  PrepareDiagonal(sample, trans, mode, worker);
  const auto before = sample;
  for (int repeat = 0; repeat < 32; ++repeat) {
    sample = before;
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(test, sample
                                   .Execute(provider, trans, mode.fact, plan,
                                            sample.Workspace(plan), report)
                                   .ok());
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
    CheckSolution(test, sample);
    if (mode.fact != 'E') {
      ASC_DENSE_TEST_EQ(test, sample.a, before.a);
    }
    if (mode.fact == 'N') {
      ASC_DENSE_TEST_EQ(test, sample.b, before.b);
    }
    if (mode.fact == 'F') {
      ASC_DENSE_TEST_EQ(test, sample.af, before.af);
      ASC_DENSE_TEST_EQ(test, sample.pivots, before.pivots);
      ASC_DENSE_TEST_EQ(test, sample.rows, before.rows);
      ASC_DENSE_TEST_EQ(test, sample.columns, before.columns);
    }
    sample.Guards(test, plan);
  }
  sample = before;
  FinalOutcome(test, worker, sample, trans, mode, provider, plan);
  sample.Guards(test, plan);
}

template <typename T>
void Concurrent(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (const auto mode : kModes) {
    for (unsigned int layouts = 0; layouts < 16; ++layouts) {
      for (auto trans : {kNone, kTranspose, kConjugate}) {
        Sample<T> sample(3, 2, layouts);
        PrepareDiagonal(sample, trans, mode, 0);
        const auto plan = Take(sample.Query(provider, trans, mode.fact));
        std::array<std::thread, 4> workers;
        std::array<int, 4> results{};
        for (int i = 0; i < 4; ++i) {
          workers[i] = std::thread([&, i] {
            TestContext local;
            Worker<T>(local, i, layouts, trans, mode, plan);
            results[i] = local.Finish();
          });
        }
        for (auto& thread : workers) {
          thread.join();
        }
        for (int result : results) {
          ASC_DENSE_TEST_EQ(test, result, 0);
        }
      }
    }
  }
}

template <typename T>
void RequiredScalarMath(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (char fact : {'N', 'E', 'F'}) {
    for (auto trans : {kNone, kTranspose, kConjugate}) {
      for (unsigned int layouts : {0U, 15U}) {
        for (Real<T> value :
             {std::numeric_limits<Real<T>>::min() / Real<T>{1024},
              2 * std::numeric_limits<Real<T>>::min()}) {
          Sample<T> sample(1, 1, layouts);
          sample.a[1] = value;
          sample.af[1] = value;
          sample.pivots[1] = 1;
          sample.b[1] = value;
          const auto plan = Take(sample.Query(provider, trans, fact));
          asc::LapackReport report;
          const auto status = sample.Execute(provider, trans, fact, plan,
                                             sample.Workspace(plan), report);
          std::printf(
              "GESVX bytes=%zu FACT=%c trans=%d layouts=%u a=%La RCOND=%La "
              "FERR=%La BERR=%La growth=%La INFO=%lld status=%d\n",
              sizeof(T), fact, static_cast<int>(trans), layouts,
              static_cast<long double>(value),
              static_cast<long double>(sample.statistics.reciprocal_condition),
              static_cast<long double>(sample.ferr[1]),
              static_cast<long double>(sample.berr[1]),
              static_cast<long double>(
                  sample.statistics.reciprocal_pivot_growth),
              static_cast<long long>(report.native_info.value_or(-999)),
              static_cast<int>(status.code()));
          // Preserve the original direct probe's scalar oracle exactly:
          // X=1, RCOND=1, INFO=0 and finite estimates in every explicit mode.
          ASC_DENSE_TEST_CHECK(test, status.ok());
          ASC_DENSE_TEST_EQ(test, report.native_info, 0);
          ASC_DENSE_TEST_EQ(test, report.output_validity,
                            asc::LapackOutputValidity::kComplete);
          ASC_DENSE_TEST_EQ(test, sample.x[1], T{1});
          ASC_DENSE_TEST_EQ(test, sample.statistics.reciprocal_condition, 1);
          ASC_DENSE_TEST_CHECK(test, std::isfinite(sample.ferr[1]));
          ASC_DENSE_TEST_CHECK(test, std::isfinite(sample.berr[1]));
          sample.Guards(test, plan);
        }
      }
    }
  }
}

template <typename T>
void Run(TestContext& test, bool mathematical) {
  if (mathematical) {
    RequiredScalarMath<T>(test);
  } else {
    Concurrent<T>(test);
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view mode(argv[2]);
  if (mode != "concurrency" && mode != "extreme") {
    return 2;
  }
  TestContext test;
  const std::string_view scalar(argv[1]);
  const bool mathematical = mode == "extreme";
  if (scalar == "s") {
    Run<float>(test, mathematical);
  } else if (scalar == "d") {
    Run<double>(test, mathematical);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, mathematical);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, mathematical);
  } else {
    return 2;
  }
  return test.Finish();
}
