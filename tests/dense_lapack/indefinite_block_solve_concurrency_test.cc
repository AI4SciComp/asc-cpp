#include <array>
#include <barrier>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <thread>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_block_solve_fixture.h"
#include "indefinite_block_solve_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_test;
using asc_block_solve_test::RightHandSides;
using asc_block_solve_test::Sample;
using base::Take;
using base::TestContext;
template <typename T>
class ReadOnlyFactors {
 public:
  ReadOnlyFactors(TestContext& test, const Sample<T>& sample)
      : view_(sample.ConstView()) {
#if defined(__linux__)
    const auto page = sysconf(_SC_PAGESIZE);
    ASC_DENSE_TEST_CHECK(test, page > 0);
    if (page <= 0) {
      valid_ = false;
      return;
    }
    bytes_ = ((sizeof(sample.a) + static_cast<std::size_t>(page) - 1) /
              static_cast<std::size_t>(page)) *
             static_cast<std::size_t>(page);
    memory_ = mmap(nullptr, bytes_, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASC_DENSE_TEST_CHECK(test, memory_ != MAP_FAILED);
    if (memory_ == MAP_FAILED) {
      memory_ = nullptr;
      valid_ = false;
      return;
    }
    std::memcpy(memory_, sample.a.data(), sizeof(sample.a));
    const auto protected_result = mprotect(memory_, bytes_, PROT_READ);
    ASC_DENSE_TEST_EQ(test, protected_result, 0);
    if (protected_result != 0) {
      valid_ = false;
      return;
    }
    view_ = Take(asc::DenseBlasMatrixView<const T>::Create(
        static_cast<const T*>(memory_) + 1, sample.n, sample.n, sample.layout,
        sample.Ld(), {memory_, sizeof(sample.a), base::kHost}));
#else
    (void)test;
#endif
  }
  ReadOnlyFactors(const ReadOnlyFactors&) = delete;
  ReadOnlyFactors& operator=(const ReadOnlyFactors&) = delete;
  ReadOnlyFactors(ReadOnlyFactors&&) = delete;
  ReadOnlyFactors& operator=(ReadOnlyFactors&&) = delete;
  ~ReadOnlyFactors() {
#if defined(__linux__)
    if (memory_ != nullptr) {
      munmap(memory_, bytes_);
    }
#endif
  }
  [[nodiscard]] bool valid() const { return valid_; }
  [[nodiscard]] auto view() const { return view_; }
  void Check(TestContext& test, const Sample<T>& sample) const {
#if defined(__linux__)
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(memory_, sample.a.data(), sizeof(sample.a)));
#else
    (void)test;
    (void)sample;
#endif
  }

 private:
  asc::DenseBlasMatrixView<const T> view_;
  bool valid_ = true;
#if defined(__linux__)
  void* memory_ = nullptr;
  std::size_t bytes_ = 0;
#endif
};
template <typename T>
void Worker(TestContext& test, const asc::ReferenceLapackProvider& provider,
            const Sample<T>& sample, asc::DenseBlasMatrixView<const T> factors,
            asc::DenseBlasLayout layout, const asc::LapackWorkspacePlan& plan,
            const asc::LapackWorkspacePlan& empty_plan,
            const asc::LapackWorkspacePlan& bad_plan, std::barrier<>& start) {
  const auto pivots = base::Raw(sample.pivots, sample.n);
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  for (int repeat = 0; repeat < 8; ++repeat) {
    RightHandSides<T> rhs(sample, 3, layout);
    asc::LapackReport report;
    start.arrive_and_wait();
    const auto status = asc_block_solve_test::Solve(
        provider, sample.triangle, sample.hermitian, factors, pivots,
        rhs.View(), plan, workspace, report);
    ASC_DENSE_TEST_CHECK(test, status.ok() && report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    rhs.Verify(test, sample);
    const auto after = rhs.values;
    const auto after_scratch = scratch;
    const auto empty = base::Matrix(rhs.values, sample.n, 0, layout, rhs.Ld());
    start.arrive_and_wait();
    const auto empty_status = asc_block_solve_test::Solve(
        provider, sample.triangle, sample.hermitian, factors, pivots, empty,
        empty_plan, {}, report);
    ASC_DENSE_TEST_CHECK(test, empty_status.ok() && !report.called_provider &&
                                   !report.native_info.has_value());
    start.arrive_and_wait();
    const auto rejected = asc_block_solve_test::Solve(
        provider, sample.triangle, sample.hermitian, factors, pivots,
        rhs.View(), bad_plan, workspace, report);
    ASC_DENSE_TEST_CHECK(test, !rejected.ok() && !report.called_provider &&
                                   !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(rhs.values.data(), after.data(), sizeof(after)));
    ASC_DENSE_TEST_CHECK(test, base::EqualBytes(scratch.scalar.data(),
                                                after_scratch.scalar.data(),
                                                sizeof(scratch.scalar)));
    ASC_DENSE_TEST_CHECK(test, base::EqualBytes(scratch.packed.data(),
                                                after_scratch.packed.data(),
                                                sizeof(scratch.packed)));
    ASC_DENSE_TEST_EQ(test, scratch.pivot, after_scratch.pivot);
    rhs.Guards(test);
    scratch.Guards(test, workspace);
  }
}
template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           Sample<T> sample, asc::DenseBlasLayout layout) {
  base::Scratch<T> factor_scratch;
  const auto factor_plan = Take(base::QueryFactor(
      provider, sample.triangle, sample.hermitian, sample.n > 64, sample.View(),
      base::Pivots(sample.pivots, sample.n)));
  const auto factor_work = factor_scratch.Workspace(factor_plan);
  asc::LapackReport factor_report;
  ASC_DENSE_TEST_CHECK(
      test,
      base::Factor(provider, sample.triangle, sample.hermitian, sample.n > 64,
                   sample.View(), base::Pivots(sample.pivots, sample.n),
                   factor_plan, factor_work, factor_report)
          .ok());
  const auto before_a = sample.a;
  const auto before_pivots = sample.pivots;
  ReadOnlyFactors<T> protected_a(test, sample);
  if (!protected_a.valid()) {
    return;
  }
  RightHandSides<T> baseline(sample, 3, layout);
  const auto pivots = base::Raw(sample.pivots, sample.n);
  const auto plan = Take(
      asc_block_solve_test::Query(provider, sample.triangle, sample.hermitian,
                                  protected_a.view(), pivots, baseline.View()));
  const auto empty =
      base::Matrix(baseline.values, sample.n, 0, layout, baseline.Ld());
  const auto empty_plan = Take(
      asc_block_solve_test::Query(provider, sample.triangle, sample.hermitian,
                                  protected_a.view(), pivots, empty));
  auto bad_plan = plan;
  ++bad_plan.regions[base::kScalar].minimum_entries;
  std::array<std::thread, 4> workers;
  std::array<int, 4> results{};
  std::barrier start(4);
  for (std::size_t i = 0; i < workers.size(); ++i) {
    workers[i] = std::thread([&, i] {
      TestContext local;
      Worker(local, provider, sample, protected_a.view(), layout, plan,
             empty_plan, bad_plan, start);
      results[i] = local.Finish();
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  for (const auto result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
  protected_a.Check(test, sample);
  ASC_DENSE_TEST_CHECK(test, base::EqualBytes(sample.a.data(), before_a.data(),
                                              sizeof(before_a)));
  ASC_DENSE_TEST_EQ(test, sample.pivots, before_pivots);
  sample.Guards(test);
  factor_scratch.Guards(test, factor_work);
}
template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto context = asc::ExecutionContext::Serial();
  const auto provider = Take(asc::ReferenceLapackProvider::Create(context));
  int groups = 0;
  for (const int n : {3, 67}) {
    for (const auto tri : {base::kUpper, base::kLower}) {
      for (const auto al : {base::kColumn, base::kRow}) {
        for (const auto bl : {base::kColumn, base::kRow}) {
          Group(test, provider, Sample<T>(n, hermitian, tri, al, 0, false), bl);
          ++groups;
        }
      }
    }
  }
  std::printf(
      "block solve concurrency groups=%d native_calls=%d empty_noncalls=%d "
      "structural_rejections=%d\n",
      groups, groups * 32, groups * 32, groups * 32);
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
