#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_equilibration_modes_support.h"

namespace {
using asc_equilibration_modes_test::ExpectedScales;
using asc_equilibration_modes_test::kColumn;
using asc_equilibration_modes_test::kRow;
using asc_equilibration_modes_test::Magnitude;
using asc_equilibration_modes_test::Sample;
using asc_equilibration_modes_test::Take;
using asc_equilibration_modes_test::TestContext;
template <typename T>
using Real = asc::DenseBlasRealType<T>;

template <typename T>
void Guards(TestContext& test, const Sample<T>& sample) {
  ASC_DENSE_TEST_EQ(test, sample.rows.front(), Real<T>{-53});
  ASC_DENSE_TEST_EQ(test, sample.rows.back(), Real<T>{-53});
  ASC_DENSE_TEST_EQ(test, sample.columns.front(), Real<T>{-59});
  ASC_DENSE_TEST_EQ(test, sample.columns.back(), Real<T>{-59});
  for (std::size_t i = 0; i < sample.scratch.size(); ++i) {
    if (sample.layout == kColumn || i == 0 || i > 6) {
      ASC_DENSE_TEST_EQ(test, sample.scratch[i],
                        asc_equilibration_modes_test::Value<T>(-719, 31));
    }
  }
}

template <typename T>
void Worker(TestContext& test, int worker, asc::DenseBlasLayout layout,
            bool radix, const asc::LapackWorkspacePlan& plan) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Sample<T> sample(layout, 4 * worker - 6);
  const auto before = sample.matrix;
  asc::LapackReport report;
  for (int repeat = 0; repeat < 32; ++repeat) {
    ASC_DENSE_TEST_CHECK(
        test,
        sample.Execute(provider, radix, plan, sample.Workspace(), report).ok());
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
    ExpectedScales(test, sample, radix);
  }
  ASC_DENSE_TEST_EQ(test, sample.matrix, before);
  sample.statistics = {-31, -37, -41};
  auto final_plan = plan;
  if (worker == 0) {
    sample.At(1, 0) = T{};
    sample.At(1, 1) = T{};
  } else if (worker == 1) {
    for (int i = 0; i < 3; ++i) {
      sample.At(i, 1) = T{};
    }
  } else if (worker == 2) {
    final_plan.total_byte_limit = 0;
  }
  const auto rows_before = sample.rows;
  const auto columns_before = sample.columns;
  const auto status =
      sample.Execute(provider, radix, final_plan, sample.Workspace(), report);
  if (worker < 2) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info, worker == 0 ? 2 : 5);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      radix ? asc::LapackOutcome::kPartialResult
                            : asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_EQ(test, sample.statistics.column_condition, Real<T>{-37});
    if (worker == 0) {
      ASC_DENSE_TEST_EQ(test, sample.statistics.row_condition, Real<T>{-31});
      ASC_DENSE_TEST_EQ(test, sample.columns, columns_before);
    } else {
      ASC_DENSE_TEST_CHECK(test, sample.statistics.row_condition > 0);
      ASC_DENSE_TEST_EQ(test, sample.rows, rows_before);
    }
  } else if (worker == 2) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, sample.rows, rows_before);
    ASC_DENSE_TEST_EQ(test, sample.columns, columns_before);
    ASC_DENSE_TEST_EQ(test, sample.statistics.row_condition, Real<T>{-31});
    ASC_DENSE_TEST_EQ(test, sample.statistics.column_condition, Real<T>{-37});
    ASC_DENSE_TEST_EQ(test, sample.statistics.absolute_maximum, Real<T>{-41});
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    ExpectedScales(test, sample, radix);
  }
  Guards(test, sample);
}

template <typename T>
void Concurrent(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto layout : {kRow, kColumn}) {
    for (bool radix : {false, true}) {
      Sample<T> sample(layout, 0);
      const auto plan = Take(sample.Query(provider, radix));
      std::array<std::thread, 4> workers;
      std::array<int, 4> results{};
      for (int i = 0; i < 4; ++i) {
        workers[i] = std::thread([&, i] {
          TestContext local;
          Worker<T>(local, i, layout, radix, plan);
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

template <typename T>
void RequiredSubnormalMath(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  constexpr int kExponent =
      std::numeric_limits<Real<T>>::digits == 24 ? -140 : -1050;
  // Preserve the existing nonzero fixture and 16-epsilon scale oracle. The
  // separate fidelity tests retain the known LP64 computed-scale failure.
  for (auto layout : {kRow, kColumn}) {
    for (bool radix : {false, true}) {
      Sample<T> sample(layout, kExponent);
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
          ASC_DENSE_TEST_CHECK(test, Magnitude(sample.At(i, j)) > 0);
        }
      }
      const auto before = sample.matrix;
      const auto plan = Take(sample.Query(provider, radix));
      asc::LapackReport report;
      const auto status =
          sample.Execute(provider, radix, plan, sample.Workspace(), report);
      std::printf(
          "GEEQU%s bytes=%zu layout=%d exponent=%d INFO=%lld "
          "R=%La,%La,%La C=%La,%La ROWCND=%La COLCND=%La AMAX=%La\n",
          radix ? "B" : "", sizeof(T), static_cast<int>(layout), kExponent,
          static_cast<long long>(report.native_info.value_or(-999)),
          static_cast<long double>(sample.rows[1]),
          static_cast<long double>(sample.rows[2]),
          static_cast<long double>(sample.rows[3]),
          static_cast<long double>(sample.columns[1]),
          static_cast<long double>(sample.columns[2]),
          static_cast<long double>(sample.statistics.row_condition),
          static_cast<long double>(sample.statistics.column_condition),
          static_cast<long double>(sample.statistics.absolute_maximum));
      ASC_DENSE_TEST_CHECK(test, status.ok());
      ASC_DENSE_TEST_EQ(test, report.native_info, 0);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
      ExpectedScales(test, sample, radix);
      ASC_DENSE_TEST_EQ(test, sample.matrix, before);
      Guards(test, sample);
    }
  }
}

template <typename T>
void Run(TestContext& test, bool mathematical) {
  if (mathematical) {
    RequiredSubnormalMath<T>(test);
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
