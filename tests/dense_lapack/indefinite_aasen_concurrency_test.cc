#include <array>
#include <barrier>
#include <complex>
#include <cstdio>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_fixture.h"
#include "indefinite_aasen_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_test;
using base::TestContext;
constexpr int kWorkers = 4;
constexpr int kRepeats = 8;
struct Mode {
  int n;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  bool preferred;
};
template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           bool he, Mode mode) {
  aa::Sample<T> prototype(mode.n, he, mode.triangle, mode.layout, 0, false);
  const auto plan =
      base::Take(aa::Query(provider, mode.triangle, he, prototype.View(),
                           base::Pivots(prototype.pivots, mode.n)));
  const auto before_prototype = prototype;
  std::array<int, kWorkers> results{};
  std::barrier barrier(kWorkers);
  std::array<std::thread, kWorkers> threads;
  for (int worker = 0; worker < kWorkers; ++worker) {
    threads[worker] = std::thread([&, worker] {
      TestContext local;
      for (int repeat = 0; repeat < kRepeats; ++repeat) {
        aa::Sample<T> sample(mode.n, he, mode.triangle, mode.layout,
                             repeat % 2 == 0 ? -20 : 20, worker == 1);
        const bool stale = worker == 3 && repeat % 2 == 1;
        auto selected = mode.triangle;
        if (stale) {
          selected =
              mode.triangle == base::kUpper ? base::kLower : base::kUpper;
        }
        base::Scratch<T> scratch;
        const auto workspace = scratch.Workspace(
            plan,
            mode.preferred ? -1 : plan.regions[base::kScalar].minimum_entries);
        asc::LapackReport report;
        barrier.arrive_and_wait();
        const auto status = aa::Factor(provider, selected, he, sample.View(),
                                       base::Pivots(sample.pivots, mode.n),
                                       plan, workspace, report);
        barrier.arrive_and_wait();
        if (stale) {
          ASC_DENSE_TEST_EQ(local, status.code(),
                            asc::ErrorCode::kInvalidState);
          ASC_DENSE_TEST_CHECK(local, !report.called_provider &&
                                          !report.native_info.has_value());
          ASC_DENSE_TEST_CHECK(
              local, base::EqualBytes(sample.a.data(), sample.original.data(),
                                      sizeof(sample.a)));
          for (auto pivot : sample.pivots) {
            ASC_DENSE_TEST_EQ(local, pivot, -509);
          }
          const base::Scratch<T> untouched;
          ASC_DENSE_TEST_EQ(local, scratch.scalar, untouched.scalar);
          ASC_DENSE_TEST_EQ(local, scratch.packed, untouched.packed);
          ASC_DENSE_TEST_EQ(local, scratch.pivot, untouched.pivot);
        } else {
          ASC_DENSE_TEST_CHECK(local, status.ok());
          ASC_DENSE_TEST_CHECK(local, report.called_provider);
          ASC_DENSE_TEST_EQ(local, report.native_info.value_or(-1), 0);
          ASC_DENSE_TEST_EQ(local, report.factor_family,
                            asc::LapackFactorFamily::kAasen);
          ASC_DENSE_TEST_EQ(local, report.output_validity,
                            asc::LapackOutputValidity::kComplete);
          sample.Reconstruction(local);
        }
        sample.Guards(local);
        scratch.Guards(local, workspace);
      }
      results[worker] = local.Finish();
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (int result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(prototype.a.data(), before_prototype.a.data(),
                             sizeof(prototype.a)));
  ASC_DENSE_TEST_EQ(test, prototype.pivots, before_prototype.pivots);
}
template <typename T>
int Run(bool he) {
  TestContext test;
  int groups = 0;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (int n : {7, 67}) {
    for (const auto tri : {base::kUpper, base::kLower}) {
      for (const auto layout : {base::kColumn, base::kRow}) {
        for (bool preferred : {false, true}) {
          Group<T>(test, provider, he, {n, tri, layout, preferred});
          ++groups;
        }
      }
    }
  }
  std::printf(
      "Aasen concurrency groups=%d workers=%d repeats=%d native_calls=%d "
      "structural_rejections=%d\n",
      groups, kWorkers, kRepeats, groups * 28, groups * 4);
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
