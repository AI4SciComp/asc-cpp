#include <array>
#include <barrier>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <thread>
#include <vector>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_solve_fixture.h"
#include "indefinite_aasen_two_stage_solve_test_support.h"
#include "indefinite_aasen_two_stage_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_two_stage_solve_test;
using base::Take;
using base::TestContext;
template <typename T>
class ReadOnlyVector {
 public:
  ReadOnlyVector(TestContext& test, const std::vector<T>& values)
      : data_(values.data()) {
#if defined(__linux__)
    const auto page = sysconf(_SC_PAGESIZE);
    ASC_DENSE_TEST_CHECK(test, page > 0);
    if (page <= 0) {
      valid_ = false;
      return;
    }
    bytes_ = ((values.size() * sizeof(T) + static_cast<std::size_t>(page) - 1) /
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
    std::memcpy(memory_, values.data(), values.size() * sizeof(T));
    const auto result = mprotect(memory_, bytes_, PROT_READ);
    ASC_DENSE_TEST_EQ(test, result, 0);
    valid_ = result == 0;
    data_ = static_cast<const T*>(memory_);
#else
    ASC_DENSE_TEST_CHECK(test, false);
    valid_ = false;
#endif
  }
  ReadOnlyVector(const ReadOnlyVector&) = delete;
  ReadOnlyVector& operator=(const ReadOnlyVector&) = delete;
  ReadOnlyVector(ReadOnlyVector&&) = delete;
  ReadOnlyVector& operator=(ReadOnlyVector&&) = delete;
  ~ReadOnlyVector() {
#if defined(__linux__)
    if (memory_ != nullptr) {
      munmap(memory_, bytes_);
    }
#endif
  }
  [[nodiscard]] bool valid() const { return valid_; }
  [[nodiscard]] const T* data() const { return data_; }
  void Check(TestContext& test, const std::vector<T>& original) const {
    ASC_DENSE_TEST_CHECK(test, base::EqualBytes(data_, original.data(),
                                                original.size() * sizeof(T)));
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
struct Inputs {
  asc::DenseBlasMatrixView<const T> a;
  asc::DenseBlasVectorView<const T> tb;
  asc::RawLapackPivotView p;
  asc::DenseBlasVectorView<const asc::index_t> q;
};
struct Plans {
  asc::LapackWorkspacePlan active;
  asc::LapackWorkspacePlan empty;
  asc::LapackWorkspacePlan stale;
};
template <typename T>
void Worker(TestContext& test, const asc::ReferenceLapackProvider& provider,
            const aa::Sample<T>& baseline, const Inputs<T>& inputs,
            const Plans& plans, int worker, std::barrier<>& start) {
  asc_aasen_two_stage_test::Scratch<T> scratch(plans.active, 0);
  for (int repeat = 0; repeat < 4; ++repeat) {
    auto sample = baseline;
    const auto scale = static_cast<asc::DenseBlasRealType<T>>(1 << worker);
    for (auto& x : sample.solution) {
      x *= scale;
    }
    for (int j = 0; j < sample.nrhs; ++j) {
      for (int i = 0; i < sample.n; ++i) {
        sample.b[sample.BOffset(i, j)] *= scale;
      }
    }
    sample.before_b = sample.b;
    const auto rhs = aa::Rhs(sample);
    const auto tri = sample.original.upper ? base::kUpper : base::kLower;
    asc::LapackReport report;
    start.arrive_and_wait();
    const auto status = aa::Solve(provider, tri, sample.original.hermitian,
                                  inputs.a, inputs.tb, inputs.p, inputs.q, rhs,
                                  plans.active, scratch.workspace, report);
    ASC_DENSE_TEST_CHECK(test, status.ok() && report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_EQ(test, report.factor_family,
                      asc::LapackFactorFamily::kAasen);
    sample.Solution(test);
    const auto after_b = sample.b;
    const auto after_packed = scratch.packed;
    const auto after_integers = scratch.integers;
    const auto empty = Take(asc::DenseBlasMatrixView<T>::Create(
        sample.b.data() + 1, sample.n, 0,
        sample.rhs_row ? base::kRow : base::kColumn, sample.ldb,
        {sample.b.data(), sample.b.size() * sizeof(T), base::kHost}));
    start.arrive_and_wait();
    const auto empty_status =
        aa::Solve(provider, tri, sample.original.hermitian, inputs.a, inputs.tb,
                  inputs.p, inputs.q, empty, plans.empty, {}, report);
    ASC_DENSE_TEST_CHECK(test, empty_status.ok() && !report.called_provider &&
                                   !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    start.arrive_and_wait();
    const auto rejected = aa::Solve(
        provider, tri, sample.original.hermitian, inputs.a, inputs.tb, inputs.p,
        inputs.q, rhs, plans.stale, scratch.workspace, report);
    ASC_DENSE_TEST_EQ(test, rejected.code(), asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_CHECK(test, base::EqualBytes(sample.b.data(), after_b.data(),
                                                sample.b.size() * sizeof(T)));
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(scratch.packed.data(), after_packed.data(),
                               scratch.packed.size() * sizeof(T)));
    ASC_DENSE_TEST_EQ(test, scratch.integers, after_integers);
    sample.RhsGuards(test);
    scratch.Guards(test);
  }
}
template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           aa::Sample<T> sample) {
  sample.Produce(test, true);
  ReadOnlyVector protected_a(test, sample.a);
  ReadOnlyVector protected_tb(test, sample.tb);
  ReadOnlyVector protected_p(test, sample.p);
  ReadOnlyVector protected_q(test, sample.q);
  if (!protected_a.valid() || !protected_tb.valid() || !protected_p.valid() ||
      !protected_q.valid()) {
    return;
  }
  const Inputs<T> inputs{
      Take(asc::DenseBlasMatrixView<const T>::Create(
          protected_a.data() + 1, sample.n, sample.n,
          sample.row ? base::kRow : base::kColumn, sample.original.lda,
          {protected_a.data(), sample.a.size() * sizeof(T), base::kHost})),
      Take(asc::DenseBlasVectorView<const T>::Create(
          protected_tb.data() + 1, sample.ltb, 1,
          {protected_tb.data(), sample.tb.size() * sizeof(T), base::kHost})),
      Take(asc::RawLapackPivotView::Create(
          protected_p.data() + 1, sample.n, asc::LapackFactorFamily::kAasen,
          {protected_p.data(), sample.p.size() * sizeof(asc::index_t),
           base::kHost})),
      Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
          protected_q.data() + 1, sample.n, 1,
          {protected_q.data(), sample.q.size() * sizeof(asc::index_t),
           base::kHost}))};
  const auto tri = sample.original.upper ? base::kUpper : base::kLower;
  const auto plan =
      Take(aa::Query(provider, tri, sample.original.hermitian, inputs.a,
                     inputs.tb, inputs.p, inputs.q, aa::Rhs(sample)));
  const auto empty = Take(asc::DenseBlasMatrixView<T>::Create(
      sample.b.data() + 1, sample.n, 0,
      sample.rhs_row ? base::kRow : base::kColumn, sample.ldb,
      {sample.b.data(), sample.b.size() * sizeof(T), base::kHost}));
  const auto empty_plan =
      Take(aa::Query(provider, tri, sample.original.hermitian, inputs.a,
                     inputs.tb, inputs.p, inputs.q, empty));
  auto stale = plan;
  ++stale.regions[base::kPivot].minimum_entries;
  const Plans plans{plan, empty_plan, stale};
  std::array<std::thread, 4> workers;
  std::array<int, 4> results{};
  std::barrier start(4);
  for (std::size_t i = 0; i < workers.size(); ++i) {
    workers[i] = std::thread([&, i] {
      TestContext local;
      Worker(local, provider, sample, inputs, plans, static_cast<int>(i),
             start);
      results[i] = local.Finish();
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  for (auto result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
  protected_a.Check(test, sample.a);
  protected_tb.Check(test, sample.tb);
  protected_p.Check(test, sample.p);
  protected_q.Check(test, sample.q);
}
template <typename T>
int Run(bool he) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int groups = 0;
  for (int n : {3, 67}) {
    for (bool upper : {false, true}) {
      for (bool row : {false, true}) {
        for (bool rhs_row : {false, true}) {
          for (int capacity : {0, 2}) {
            Group(test, provider,
                  aa::Sample<T>(n, 3, he, upper, row, rhs_row, capacity,
                                capacity));
            ++groups;
          }
        }
      }
    }
  }
  std::printf(
      "Two-stage Aasen solve concurrency groups=%d native_calls=%d "
      "empty_noncalls=%d structural_rejections=%d\n",
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
