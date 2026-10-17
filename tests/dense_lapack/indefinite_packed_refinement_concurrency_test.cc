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
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_refinement_operands.h"
#include "indefinite_packed_refinement_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"
namespace {
namespace base = asc_indefinite_test;
namespace refinement = asc_packed_refinement_test;
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
            const refinement::Fixture<T>& initial,
            const refinement::Fixture<T>& expected,
            refinement::Operands<T> shared,
            const asc::LapackWorkspacePlan& plan,
            const asc::LapackWorkspacePlan& stale_plan, std::barrier<>& start) {
  using Real = asc::DenseBlasRealType<T>;
  auto sample = initial;
  shared.x = sample.Solution();
  shared.ferr = sample.Forward();
  shared.berr = sample.Backward();
  refinement::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  const bool active = sample.system.a.n != 0 && sample.system.nrhs != 0;
  for (int repeat = 0; repeat < 4; ++repeat) {
    sample.x = sample.initial_x;
    sample.ferr.fill(Real{-293});
    sample.berr.fill(Real{-307});
    asc::LapackReport report;
    start.arrive_and_wait();
    const auto status = shared.Execute(provider, plan, workspace, report);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.called_provider, active);
    ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active);
    if (active) {
      ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    }
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    sample.Mathematics(test);
    sample.Padding(test);
    ASC_DENSE_TEST_EQ(test, sample.x, expected.x);
    ASC_DENSE_TEST_EQ(test, sample.ferr, expected.ferr);
    ASC_DENSE_TEST_EQ(test, sample.berr, expected.berr);
    const auto scratch_before = scratch;
    start.arrive_and_wait();
    const auto rejected =
        shared.Execute(provider, stale_plan, workspace, report);
    ASC_DENSE_TEST_EQ(test, rejected.code(), asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, sample.x, expected.x);
    ASC_DENSE_TEST_EQ(test, sample.ferr, expected.ferr);
    ASC_DENSE_TEST_EQ(test, sample.berr, expected.berr);
    ASC_DENSE_TEST_EQ(test, scratch.scalar, scratch_before.scalar);
    ASC_DENSE_TEST_EQ(test, scratch.real, scratch_before.real);
    ASC_DENSE_TEST_EQ(test, scratch.packed, scratch_before.packed);
    ASC_DENSE_TEST_EQ(test, scratch.pivot, scratch_before.pivot);
    scratch.Guards(test, workspace);
  }
}

template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           refinement::Fixture<T> sample) {
  sample.Prepare(test, provider);
  ReadOnlyArray protected_a(test, sample.original);
  ReadOnlyArray protected_af(test, sample.system.a.a);
  ReadOnlyArray protected_b(test, sample.system.rhs);
  ReadOnlyArray protected_p(test, sample.system.a.pivots);
  if (!protected_a.valid() || !protected_af.valid() || !protected_b.valid() ||
      !protected_p.valid()) {
    return;
  }
  auto serial = sample;
  refinement::Operands<T> shared(serial);
  shared.a = base::Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      protected_a.data() + 1, sample.system.a.n, sample.original_layout,
      {protected_a.data(), sizeof(sample.original), base::kHost}));
  shared.af = base::Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      protected_af.data() + 1, sample.system.a.n, sample.system.a.layout,
      {protected_af.data(), sizeof(sample.system.a.a), base::kHost}));
  shared.b = base::Take(asc::DenseBlasMatrixView<const T>::Create(
      protected_b.data() + 1, sample.system.a.n, sample.system.nrhs,
      sample.system.rhs_layout, sample.system.Leading(),
      {protected_b.data(), sizeof(sample.system.rhs), base::kHost}));
  shared.pivots = base::Take(asc::RawLapackPivotView::Create(
      protected_p.data() + 1, sample.system.a.n,
      asc::LapackFactorFamily::kBunchKaufman,
      {protected_p.data(), sizeof(sample.system.a.pivots), base::kHost}));
  const auto plan = base::Take(shared.Query(provider));
  auto stale_plan = plan;
  ++stale_plan.regions[refinement::kReal].minimum_entries;
  refinement::Scratch<T> scratch;
  const auto work = scratch.Workspace(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, shared.Execute(provider, plan, work, report).ok());
  serial.Mathematics(test);
  serial.Padding(test);
  scratch.Guards(test, work);
  std::array<std::thread, 4> threads;
  std::array<TestContext, 4> results;
  std::barrier start(4);
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::thread([&, i] {
      Worker(results[i], provider, sample, serial, shared, plan, stale_plan,
             start);
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (const auto& result : results) {
    ASC_DENSE_TEST_EQ(test, result.Finish(), 0);
  }
  protected_a.Check(test, sample.original);
  protected_af.Check(test, sample.system.a.a);
  protected_b.Check(test, sample.system.rhs);
  protected_p.Check(test, sample.system.a.pivots);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int groups = 0;
  for (int n : {0, 1, 2, 5, 17, 65}) {
    for (auto triangle : {base::kUpper, base::kLower}) {
      for (int layouts = 0; layouts < 16; ++layouts) {
        const int nrhs = std::array{0, 1, 3}[(layouts + n) % 3];
        Group(test, provider,
              refinement::Fixture<T>(n, nrhs, hermitian, triangle,
                                     (layouts & 1) ? base::kRow : base::kColumn,
                                     (layouts & 2) ? base::kRow : base::kColumn,
                                     (layouts & 4) ? base::kRow : base::kColumn,
                                     (layouts & 8) ? base::kRow : base::kColumn,
                                     0, layouts % 2 == 0 ? 3 : 0));
        ++groups;
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, groups, 192);
  std::printf("Packed refinement concurrent groups=%d workers=4 repeats=4\n",
              groups);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
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
