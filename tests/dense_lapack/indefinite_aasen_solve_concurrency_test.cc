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
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_solve_fixture.h"
#include "indefinite_aasen_solve_test_support.h"
#include "indefinite_aasen_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_solve_test;
using aa::Sample;
using base::Take;
using base::TestContext;
template <typename T, std::size_t Size>
class ReadOnlyArray {
 public:
  ReadOnlyArray(TestContext& test, const std::array<T, Size>& values)
      : data_(values.data()) {
#if defined(__linux__)
    const auto page = sysconf(_SC_PAGESIZE);
    ASC_DENSE_TEST_CHECK(test, page > 0);
    if (page <= 0) {
      valid_ = false;
      return;
    }
    bytes_ = ((sizeof(values) + static_cast<std::size_t>(page) - 1) /
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
    std::memcpy(memory_, values.data(), sizeof(values));
    const auto result = mprotect(memory_, bytes_, PROT_READ);
    ASC_DENSE_TEST_EQ(test, result, 0);
    valid_ = result == 0;
    data_ = static_cast<const T*>(memory_);
#else
    (void)test;
#endif
  }
  ReadOnlyArray(const ReadOnlyArray&) = delete;
  ReadOnlyArray& operator=(const ReadOnlyArray&) = delete;
  ReadOnlyArray(ReadOnlyArray&&) = delete;
  ReadOnlyArray& operator=(ReadOnlyArray&&) = delete;
  ~ReadOnlyArray() {
#if defined(__linux__)
    if (memory_ != nullptr) {
      munmap(memory_, bytes_);
    }
#endif
  }
  [[nodiscard]] bool valid() const { return valid_; }
  [[nodiscard]] const T* data() const { return data_; }
  void Check(TestContext& test, const std::array<T, Size>& original) const {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(data_, original.data(), sizeof(original)));
  }

 private:
  const T* data_;
  bool valid_ = true;
#if defined(__linux__)
  void* memory_ = nullptr;
  std::size_t bytes_ = 0;
#endif
};
template <typename T>
void Worker(TestContext& test, const asc::ReferenceLapackProvider& provider,
            const Sample<T>& baseline,
            asc::DenseBlasMatrixView<const T> factors,
            asc::RawLapackPivotView pivots,
            const asc::LapackWorkspacePlan& plan,
            const asc::LapackWorkspacePlan& empty_plan,
            const asc::LapackWorkspacePlan& bad_plan, bool preferred,
            std::barrier<>& start) {
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(
      plan, preferred ? -1 : plan.regions[base::kScalar].minimum_entries);
  for (int repeat = 0; repeat < 4; ++repeat) {
    auto sample = baseline;
    asc::LapackReport report;
    start.arrive_and_wait();
    const auto status =
        aa::Solve(provider, sample.hermitian, sample.triangle, factors, pivots,
                  sample.Rhs(), plan, workspace, report);
    ASC_DENSE_TEST_CHECK(test, status.ok() && report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_EQ(test, report.factor_family,
                      asc::LapackFactorFamily::kAasen);
    sample.Solution(test);
    const auto after = sample.b;
    const auto after_scratch = scratch;
    const auto empty =
        base::Matrix(sample.b, sample.n, 0, sample.rhs_layout, sample.Ldb());
    start.arrive_and_wait();
    const auto empty_status =
        aa::Solve(provider, sample.hermitian, sample.triangle, factors, pivots,
                  empty, empty_plan, {}, report);
    ASC_DENSE_TEST_CHECK(test, empty_status.ok() && !report.called_provider &&
                                   !report.native_info.has_value());
    start.arrive_and_wait();
    const auto rejected =
        aa::Solve(provider, sample.hermitian, sample.triangle, factors, pivots,
                  sample.Rhs(), bad_plan, workspace, report);
    ASC_DENSE_TEST_EQ(test, rejected.code(), asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(sample.b.data(), after.data(), sizeof(after)));
    ASC_DENSE_TEST_EQ(test, scratch.scalar, after_scratch.scalar);
    ASC_DENSE_TEST_EQ(test, scratch.packed, after_scratch.packed);
    ASC_DENSE_TEST_EQ(test, scratch.pivot, after_scratch.pivot);
    sample.RhsGuards(test);
    scratch.Guards(test, workspace);
  }
}
template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           Sample<T> sample, bool preferred) {
  base::Scratch<T> factor_scratch;
  const auto factor_plan = Take(asc_aasen_test::Query(
      provider, sample.triangle, sample.hermitian, sample.View(),
      base::Pivots(sample.pivots, sample.n)));
  const auto factor_work = factor_scratch.Workspace(factor_plan);
  asc::LapackReport factor_report;
  ASC_DENSE_TEST_CHECK(
      test, asc_aasen_test::Factor(provider, sample.triangle, sample.hermitian,
                                   sample.View(),
                                   base::Pivots(sample.pivots, sample.n),
                                   factor_plan, factor_work, factor_report)
                .ok());
  sample.Reconstruction(test);
  const auto before_a = sample.a;
  const auto before_pivots = sample.pivots;
  ReadOnlyArray protected_a(test, sample.a);
  ReadOnlyArray protected_p(test, sample.pivots);
  if (!protected_a.valid() || !protected_p.valid()) {
    return;
  }
  const auto factors = Take(asc::DenseBlasMatrixView<const T>::Create(
      protected_a.data() + 1, sample.n, sample.n, sample.layout, sample.Ld(),
      {protected_a.data(), sizeof(sample.a), base::kHost}));
  const auto pivots = Take(asc::RawLapackPivotView::Create(
      protected_p.data() + 1, sample.n, asc::LapackFactorFamily::kAasen,
      {protected_p.data(), sizeof(sample.pivots), base::kHost}));
  const auto plan = Take(aa::Query(provider, sample.hermitian, sample.triangle,
                                   factors, pivots, sample.Rhs()));
  const auto empty =
      base::Matrix(sample.b, sample.n, 0, sample.rhs_layout, sample.Ldb());
  const auto empty_plan = Take(aa::Query(
      provider, sample.hermitian, sample.triangle, factors, pivots, empty));
  auto bad_plan = plan;
  ++bad_plan.regions[base::kScalar].minimum_entries;
  std::array<std::thread, 4> workers;
  std::array<int, 4> results{};
  std::barrier start(4);
  for (std::size_t i = 0; i < workers.size(); ++i) {
    workers[i] = std::thread([&, i] {
      TestContext local;
      Worker(local, provider, sample, factors, pivots, plan, empty_plan,
             bad_plan, preferred, start);
      results[i] = local.Finish();
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  for (const auto result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
  protected_a.Check(test, before_a);
  protected_p.Check(test, before_pivots);
  ASC_DENSE_TEST_CHECK(test, base::EqualBytes(sample.a.data(), before_a.data(),
                                              sizeof(before_a)));
  ASC_DENSE_TEST_EQ(test, sample.pivots, before_pivots);
  sample.Guards(test);
  factor_scratch.Guards(test, factor_work);
}
template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int groups = 0;
  for (const int n : {3, 67}) {
    for (const auto tri : {base::kUpper, base::kLower}) {
      for (const auto al : {base::kColumn, base::kRow}) {
        for (const auto bl : {base::kColumn, base::kRow}) {
          for (const bool preferred : {false, true}) {
            Group(test, provider,
                  Sample<T>(n, 3, hermitian, tri, al, bl, 0, false), preferred);
            ++groups;
          }
        }
      }
    }
  }
  std::printf(
      "Aasen solve concurrency groups=%d native_calls=%d empty_noncalls=%d "
      "structural_rejections=%d\n",
      groups, groups * 16, groups * 16, groups * 16);
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
