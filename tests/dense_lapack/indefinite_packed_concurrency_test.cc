#include <array>
#include <barrier>
#include <complex>
#include <cstdio>
#include <string_view>
#include <thread>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_fixture.h"
#include "indefinite_packed_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"

namespace {
namespace packed = asc_packed_indefinite_test;
constexpr int kWorkers = 4;
constexpr int kRepeats = 8;

void CheckReport(packed::TestContext& test, const asc::LapackReport& actual,
                 const asc::LapackReport& expected) {
  ASC_DENSE_TEST_EQ(test, actual.called_provider, expected.called_provider);
  ASC_DENSE_TEST_EQ(test, actual.native_info, expected.native_info);
  ASC_DENSE_TEST_EQ(test, actual.diagnostic_index, expected.diagnostic_index);
  ASC_DENSE_TEST_EQ(test, actual.outcome, expected.outcome);
  ASC_DENSE_TEST_EQ(test, actual.output_validity, expected.output_validity);
  ASC_DENSE_TEST_EQ(test, actual.factor_family, expected.factor_family);
}

template <typename T>
asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                    packed::Sample<T>& sample, bool stale,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  auto triangle = sample.triangle;
  if (stale) {
    triangle =
        sample.triangle == packed::kUpper ? packed::kLower : packed::kUpper;
  }
  return packed::Factor(provider, triangle, sample.hermitian, sample.View(),
                        packed::base::Pivots(sample.pivots, sample.n), plan,
                        workspace, report);
}

template <typename T>
int Worker(const asc::ReferenceLapackProvider& provider, int worker,
           const asc::LapackWorkspacePlan& plan,
           const std::vector<packed::Sample<T>>& original,
           const std::vector<packed::Sample<T>>& expected,
           const std::array<asc::LapackReport, kWorkers>& reports,
           const std::array<asc::ErrorCode, kWorkers>& codes,
           std::barrier<>& barrier) {
  packed::TestContext test;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    auto sample = original[worker];
    packed::base::Scratch<T> scratch;
    const auto before_scratch = scratch;
    const auto workspace = scratch.Workspace(plan);
    const bool stale = worker == kWorkers - 1 && repeat % 2 != 0;
    asc::LapackReport report;
    barrier.arrive_and_wait();
    const auto status =
        Execute(provider, sample, stale, plan, workspace, report);
    barrier.arrive_and_wait();
    const auto& reference = stale ? original[worker] : expected[worker];
    ASC_DENSE_TEST_CHECK(test,
                         packed::EqualBytes(sample.a.data(), reference.a.data(),
                                            sizeof(sample.a)));
    ASC_DENSE_TEST_EQ(test, sample.pivots, reference.pivots);
    if (stale) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
      ASC_DENSE_TEST_CHECK(test, !report.called_provider);
      ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
      ASC_DENSE_TEST_EQ(test, scratch.scalar, before_scratch.scalar);
      ASC_DENSE_TEST_EQ(test, scratch.packed, before_scratch.packed);
      ASC_DENSE_TEST_EQ(test, scratch.pivot, before_scratch.pivot);
    } else {
      ASC_DENSE_TEST_EQ(test, status.code(), codes[worker]);
      CheckReport(test, report, reports[worker]);
    }
    sample.Guards(test);
    scratch.Guards(test, workspace);
  }
  return test.Finish();
}

template <typename T>
void Group(packed::TestContext& test,
           const asc::ReferenceLapackProvider& provider, bool hermitian,
           int order, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout) {
  std::vector<packed::Sample<T>> original;
  std::vector<packed::Sample<T>> expected;
  original.reserve(kWorkers);
  expected.reserve(kWorkers);
  for (int worker = 0; worker < kWorkers; ++worker) {
    original.emplace_back(order, hermitian, triangle, layout,
                          (worker - 2) * 10);
    int sample_kind = 0;
    if (worker == 1) {
      sample_kind = 1;
    } else if (worker == 2) {
      sample_kind = 3;
    }
    original.back().Reset(sample_kind);
    expected.push_back(original.back());
  }
  const auto plan = packed::Take(
      packed::Query(provider, triangle, hermitian, original.front().View(),
                    packed::base::Pivots(original.front().pivots, order)));
  std::array<asc::LapackReport, kWorkers> reports;
  std::array<asc::ErrorCode, kWorkers> codes;
  for (int worker = 0; worker < kWorkers; ++worker) {
    auto& sample = expected[worker];
    packed::base::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    const auto status =
        Execute(provider, sample, false, plan, workspace, reports[worker]);
    codes[worker] = status.code();
    ASC_DENSE_TEST_EQ(test, status.ok(), order == 0 || worker != 1);
    ASC_DENSE_TEST_EQ(test, reports[worker].called_provider, order != 0);
    ASC_DENSE_TEST_EQ(test, reports[worker].output_validity,
                      order != 0 && worker == 1
                          ? asc::LapackOutputValidity::kDocumentedPartial
                          : asc::LapackOutputValidity::kComplete);
    // Check the mathematical equation before using serial bytes as a
    // concurrency oracle. The reconstruction uses no provider solve.
    if (status.ok() || status.code() == asc::ErrorCode::kNumerical) {
      sample.Reconstruction(test);
    }
    sample.Guards(test);
    scratch.Guards(test, workspace);
  }
  std::barrier barrier(kWorkers);
  std::array<int, kWorkers> results{};
  std::array<std::thread, kWorkers> threads;
  for (int worker = 0; worker < kWorkers; ++worker) {
    threads[worker] = std::thread([&, worker] {
      results[worker] = Worker(provider, worker, plan, original, expected,
                               reports, codes, barrier);
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (const int result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
}

template <typename T>
int Run(bool hermitian) {
  packed::TestContext test;
  const auto provider = packed::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int groups = 0;
  for (int order : {0, 1, 2, 5, 17, 65}) {
    for (auto triangle : {packed::kUpper, packed::kLower}) {
      for (auto layout : {packed::kColumn, packed::kRow}) {
        Group<T>(test, provider, hermitian, order, triangle, layout);
        ++groups;
      }
    }
  }
  std::printf("Packed concurrent groups=%d workers=%d repeats=%d\n", groups,
              kWorkers, kRepeats);
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
