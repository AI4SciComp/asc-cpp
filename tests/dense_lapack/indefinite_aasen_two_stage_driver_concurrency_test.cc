#include <array>
#include <barrier>
#include <complex>
#include <cstdio>
#include <string_view>
#include <thread>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_driver_numerical_support.h"
#include "indefinite_aasen_two_stage_driver_test_support.h"
#include "indefinite_aasen_two_stage_numerical_support.h"
#include "indefinite_aasen_two_stage_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_two_stage_driver_test;
using base::TestContext;
constexpr int kWorkers = 4;
constexpr int kRepeats = 8;
struct Mode {
  int n;
  int nrhs;
  bool upper;
  bool row;
  bool rhs_row;
  int band;
  int work;
};
template <typename T>
asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                    aa::Sample<T>& sample, bool stale,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  auto tri = sample.original.upper ? base::kUpper : base::kLower;
  if (stale) {
    tri = tri == base::kUpper ? base::kLower : base::kUpper;
  }
  return aa::Driver(
      provider, tri, sample.original.hermitian, aa::Matrix(sample),
      aa::Vector(sample.tb, sample.ltb), aa::Vector(sample.p, sample.n),
      aa::Vector(sample.q, sample.n), aa::Rhs(sample), plan, workspace, report);
}
template <typename T>
void Equal(TestContext& test, const aa::Sample<T>& actual,
           const aa::Sample<T>& expected) {
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(actual.a.data(), expected.a.data(),
                                        actual.a.size() * sizeof(T)));
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(actual.tb.data(), expected.tb.data(),
                                        actual.tb.size() * sizeof(T)));
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(actual.b.data(), expected.b.data(),
                                        actual.b.size() * sizeof(T)));
  ASC_DENSE_TEST_EQ(test, actual.p, expected.p);
  ASC_DENSE_TEST_EQ(test, actual.q, expected.q);
}
template <typename T>
void Guards(TestContext& test, const aa::Sample<T>& sample,
            const aa::Sample<T>& before) {
  aa::factor::OutputGuards(test, sample.original, sample.row, sample.a,
                           before.a, sample.tb, sample.p, sample.q);
  sample.RhsGuards(test);
  if (sample.nrhs == 0 || sample.singular) {
    ASC_DENSE_TEST_CHECK(test,
                         base::EqualBytes(sample.b.data(), before.b.data(),
                                          sample.b.size() * sizeof(T)));
  }
}
template <typename T>
int Worker(const asc::ReferenceLapackProvider& provider, int worker,
           const asc::LapackWorkspacePlan& plan,
           const std::vector<aa::Sample<T>>& original,
           const std::vector<aa::Sample<T>>& expected,
           std::barrier<>& barrier) {
  TestContext local;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    auto sample = original[worker];
    const bool stale = worker == 3 && repeat % 2 == 1;
    aa::factor::Scratch<T> scratch(plan, sample.work_entries);
    const auto scalar_before = scratch.scalar;
    const auto packed_before = scratch.packed;
    const auto integers_before = scratch.integers;
    asc::LapackReport report;
    barrier.arrive_and_wait();
    const auto status =
        Execute(provider, sample, stale, plan, scratch.workspace, report);
    barrier.arrive_and_wait();
    Equal(local, sample, stale ? original[worker] : expected[worker]);
    if (stale) {
      ASC_DENSE_TEST_EQ(local, status.code(), asc::ErrorCode::kInvalidState);
      ASC_DENSE_TEST_CHECK(
          local, !report.called_provider && !report.native_info.has_value());
      ASC_DENSE_TEST_EQ(local, scratch.scalar, scalar_before);
      ASC_DENSE_TEST_EQ(local, scratch.packed, packed_before);
      ASC_DENSE_TEST_EQ(local, scratch.integers, integers_before);
    } else {
      aa::factor::Outcome(local, sample.n, sample.singular, status, report);
    }
    Guards(local, sample, original[worker]);
    scratch.Guards(local);
  }
  return local.Finish();
}
template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           bool he, Mode mode) {
  aa::Sample<T> prototype(mode.n, mode.nrhs, he, mode.upper, mode.row,
                          mode.rhs_row, mode.band, mode.work);
  const auto plan = base::Take(
      aa::Query(provider, mode.upper ? base::kUpper : base::kLower, he,
                aa::Matrix(prototype), aa::Vector(prototype.tb, prototype.ltb),
                aa::Vector(prototype.p, mode.n),
                aa::Vector(prototype.q, mode.n), aa::Rhs(prototype)));
  std::vector<aa::Sample<T>> original;
  std::vector<aa::Sample<T>> expected;
  for (int worker = 0; worker < kWorkers; ++worker) {
    original.emplace_back(mode.n, mode.nrhs, he, mode.upper, mode.row,
                          mode.rhs_row, mode.band, mode.work, worker == 1);
    original.back().Scale((worker - 2) * 10);
    aa::Initialize(original.back());
    expected.push_back(original.back());
    auto& baseline = expected.back();
    aa::factor::Scratch<T> scratch(plan, baseline.work_entries);
    asc::LapackReport report;
    const auto status =
        Execute(provider, baseline, false, plan, scratch.workspace, report);
    aa::factor::Outcome(test, mode.n, baseline.singular, status, report);
    if (status.ok() || status.code() == asc::ErrorCode::kNumerical) {
      aa::factor::VerifyActive(test, baseline.original, baseline.row,
                               baseline.ltb, baseline.work_entries, baseline.a,
                               baseline.tb, baseline.p, baseline.q,
                               baseline.singular, false, report);
      if (!baseline.singular && mode.nrhs != 0) {
        baseline.Solution(test);
      }
    }
    Guards(test, baseline, original.back());
    scratch.Guards(test);
  }
  std::array<int, kWorkers> results{};
  std::barrier barrier(kWorkers);
  std::array<std::thread, kWorkers> threads;
  for (int worker = 0; worker < kWorkers; ++worker) {
    threads[worker] = std::thread([&, worker] {
      results[worker] =
          Worker(provider, worker, plan, original, expected, barrier);
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
int Run(bool he) {
  TestContext test;
  int groups = 0;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (int n : {7, 67}) {
    for (int nrhs : {0, 3}) {
      for (bool upper : {false, true}) {
        for (bool row : {false, true}) {
          for (bool rhs_row : {false, true}) {
            for (int band : {0, 2}) {
              for (int work : {0, 2}) {
                Group<T>(test, provider, he,
                         {n, nrhs, upper, row, rhs_row, band, work});
                ++groups;
              }
            }
          }
        }
      }
    }
  }
  std::printf(
      "Two-stage Aasen driver concurrent groups=%d workers=%d repeats=%d "
      "serial_baselines=%d native_parallel_calls=%d structural_rejections=%d\n",
      groups, kWorkers, kRepeats, groups * 4, groups * 28, groups * 4);
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
