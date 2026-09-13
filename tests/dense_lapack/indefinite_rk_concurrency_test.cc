#include <array>
#include <barrier>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_fixture.h"
#include "indefinite_rk_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_rk_test;
constexpr int kWorkers = 4;
constexpr int kRepeats = 4;

template <typename T>
void Noncalls(base::TestContext& local,
              const asc::ReferenceLapackProvider& provider,
              base::Sample<T>& sample, bool blocked, base::Scratch<T>& scratch,
              const asc::LapackWorkspacePlan& plan,
              const asc::LapackWorkspace& workspace,
              const asc::LapackWorkspacePlan& empty_plan,
              asc::LapackReport& report) {
  const auto a = sample.View();
  const auto e = base::OffDiagonal(sample.e, sample.n);
  const auto p = base::Pivots(sample.pivots, sample.n);
  const auto saved_a = sample.a;
  const auto saved_e = sample.e;
  const auto saved_p = sample.pivots;
  const auto saved_scalar = scratch.scalar;
  const auto saved_packed = scratch.packed;
  const auto saved_bytes = scratch.pivot;
  auto bad = plan;
  ++bad.regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger)]
        .preferred_entries;
  const auto rejected =
      base::Factor(provider, sample.triangle, sample.hermitian, blocked, a, e,
                   p, bad, workspace, report);
  ASC_DENSE_TEST_CHECK(
      local, !rejected.ok() && !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_CHECK(local, base::EqualBytes(sample.a.data(), saved_a.data(),
                                               sizeof(sample.a)));
  ASC_DENSE_TEST_EQ(local, sample.e, saved_e);
  ASC_DENSE_TEST_EQ(local, sample.pivots, saved_p);
  ASC_DENSE_TEST_EQ(local, scratch.scalar, saved_scalar);
  ASC_DENSE_TEST_EQ(local, scratch.packed, saved_packed);
  ASC_DENSE_TEST_EQ(local, scratch.pivot, saved_bytes);
  const auto zero_a = base::Matrix(sample.a, 0, 0, sample.layout, sample.Ld());
  const auto zero_e = base::OffDiagonal(sample.e, 0);
  const auto zero_p = base::Pivots(sample.pivots, 0);
  const auto empty =
      base::Factor(provider, sample.triangle, sample.hermitian, blocked, zero_a,
                   zero_e, zero_p, empty_plan, {}, report);
  ASC_DENSE_TEST_CHECK(
      local, empty.ok() && !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_EQ(local, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(local, base::EqualBytes(sample.a.data(), saved_a.data(),
                                               sizeof(sample.a)));
  ASC_DENSE_TEST_EQ(local, sample.e, saved_e);
  ASC_DENSE_TEST_EQ(local, sample.pivots, saved_p);
}

template <typename T>
void Iteration(base::TestContext& local,
               const asc::ReferenceLapackProvider& provider,
               const base::Sample<T>& input, bool blocked, bool minimum,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspacePlan& empty_plan) {
  auto sample = input;
  const auto a = sample.View();
  const auto e = base::OffDiagonal(sample.e, sample.n);
  const auto p = base::Pivots(sample.pivots, sample.n);
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, blocked && minimum ? 1 : -1);
  asc::LapackReport report;
  const auto status = base::Factor(provider, sample.triangle, sample.hermitian,
                                   blocked, a, e, p, plan, workspace, report);
  ASC_DENSE_TEST_CHECK(local, status.ok());
  ASC_DENSE_TEST_CHECK(local,
                       report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(local, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  if (status.ok()) {
    sample.Reconstruction(local);
  }
  sample.Guards(local);
  scratch.Guards(local, workspace);
  Noncalls(local, provider, sample, blocked, scratch, plan, workspace,
           empty_plan, report);
}

template <typename T>
void Group(base::TestContext& test,
           const asc::ReferenceLapackProvider& provider, base::Sample<T> input,
           bool blocked, bool minimum) {
  const auto plan = base::Take(
      base::QueryFactor(provider, input.triangle, input.hermitian, blocked,
                        input.View(), base::OffDiagonal(input.e, input.n),
                        base::Pivots(input.pivots, input.n)));
  const auto empty_a = base::Matrix(input.a, 0, 0, input.layout, input.Ld());
  const auto empty_e = base::OffDiagonal(input.e, 0);
  const auto empty_p = base::Pivots(input.pivots, 0);
  const auto empty_plan =
      base::Take(base::QueryFactor(provider, input.triangle, input.hermitian,
                                   blocked, empty_a, empty_e, empty_p));
  std::barrier ready(kWorkers);
  std::array<base::TestContext, kWorkers> contexts;
  std::array<std::thread, kWorkers> workers;
  for (int worker = 0; worker < kWorkers; ++worker) {
    workers[static_cast<std::size_t>(worker)] = std::thread([&, worker] {
      auto& local = contexts[static_cast<std::size_t>(worker)];
      ready.arrive_and_wait();
      for (int repeat = 0; repeat < kRepeats; ++repeat) {
        Iteration(local, provider, input, blocked, minimum, plan, empty_plan);
      }
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  for (const auto& context : contexts) {
    ASC_DENSE_TEST_EQ(test, context.Finish(), 0);
  }
  ASC_DENSE_TEST_CHECK(
      test,
      base::EqualBytes(input.a.data(), input.original.data(), sizeof(input.a)));
  for (const auto value : input.e) {
    ASC_DENSE_TEST_EQ(test, value, base::Value<T>(-511, 17));
  }
  for (const auto value : input.pivots) {
    ASC_DENSE_TEST_EQ(test, value, -509);
  }
}

template <typename T>
int Run(bool hermitian) {
  base::TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int groups = 0;
  for (const int n : {3, 67}) {
    for (const auto triangle : {base::kUpper, base::kLower}) {
      for (const auto layout : {base::kColumn, base::kRow}) {
        for (bool blocked : {false, true}) {
          for (bool minimum : {false, true}) {
            if (!blocked && minimum) {
              continue;
            }
            Group(test, provider,
                  base::Sample<T>(n, hermitian, triangle, layout, 0, false),
                  blocked, minimum);
            ++groups;
          }
        }
      }
    }
  }
  std::printf(
      "RK concurrency groups=%d workers=%d repeats=%d native_calls=%d "
      "empty_noncalls=%d rejections=%d\n",
      groups, kWorkers, kRepeats, groups * kWorkers * kRepeats,
      groups * kWorkers * kRepeats, groups * kWorkers * kRepeats);
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
