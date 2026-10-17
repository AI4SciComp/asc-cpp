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
#include "asc/core/types.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_inverse_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"
namespace {
namespace base = asc_indefinite_test;
namespace inverse = asc_packed_inverse_test;
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

void Report(TestContext& test, int n, asc::index_t expected_info,
            const asc::Status& status, const asc::LapackReport& report) {
  ASC_DENSE_TEST_EQ(
      test, status.code(),
      expected_info == 0 ? asc::ErrorCode::kOk : asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), n != 0);
  if (n != 0) {
    ASC_DENSE_TEST_EQ(test, report.native_info, expected_info);
  }
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    expected_info == 0 ? asc::LapackOutcome::kSuccess
                                       : asc::LapackOutcome::kSingular);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    expected_info == 0
                        ? asc::LapackOutputValidity::kComplete
                        : asc::LapackOutputValidity::kDocumentedPartial);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.has_value(),
                    expected_info > 0);
  if (expected_info > 0) {
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, expected_info - 1);
  }
}

template <typename T>
void Worker(TestContext& test, const asc::ReferenceLapackProvider& provider,
            const inverse::Sample<T>& factors,
            const std::array<T, 5000>& expected, asc::RawLapackPivotView pivots,
            const asc::LapackWorkspacePlan& plan, asc::index_t expected_info,
            std::barrier<>& start) {
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  auto stale = plan;
  ++stale.regions[base::kScalar].minimum_entries;
  for (int repeat = 0; repeat < 4; ++repeat) {
    auto sample = factors;
    asc::LapackReport report;
    start.arrive_and_wait();
    const auto status =
        inverse::Inverse(provider, sample.triangle, sample.hermitian,
                         sample.View(), pivots, plan, workspace, report);
    Report(test, sample.n, expected_info, status, report);
    ASC_DENSE_TEST_CHECK(
        test,
        base::EqualBytes(sample.a.data(), expected.data(), sizeof(expected)));
    ASC_DENSE_TEST_EQ(test, sample.pivots, factors.pivots);
    sample.Guards(test);
    const auto before_scratch = scratch;
    start.arrive_and_wait();
    const auto rejected =
        inverse::Inverse(provider, sample.triangle, sample.hermitian,
                         sample.View(), pivots, stale, workspace, report);
    ASC_DENSE_TEST_EQ(test, rejected.code(), asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(
        test,
        base::EqualBytes(sample.a.data(), expected.data(), sizeof(expected)));
    ASC_DENSE_TEST_EQ(test, scratch.scalar, before_scratch.scalar);
    ASC_DENSE_TEST_EQ(test, scratch.packed, before_scratch.packed);
    ASC_DENSE_TEST_EQ(test, scratch.pivot, before_scratch.pivot);
    scratch.Guards(test, workspace);
  }
}

template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           inverse::Sample<T>& sample) {
  const auto expected_info = inverse::Prepare(test, provider, sample);
  const auto before = sample;
  ReadOnlyArray protected_p(test, sample.pivots);
  if (!protected_p.valid()) {
    return;
  }
  const auto pivots = inverse::Take(asc::RawLapackPivotView::Create(
      protected_p.data() + 1, sample.n, asc::LapackFactorFamily::kBunchKaufman,
      {protected_p.data(), sizeof(sample.pivots), base::kHost}));
  const auto plan = inverse::Take(inverse::Query(
      provider, sample.triangle, sample.hermitian, sample.View(), pivots));
  auto serial = sample;
  base::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status =
      inverse::Inverse(provider, serial.triangle, serial.hermitian,
                       serial.View(), pivots, plan, workspace, report);
  Report(test, sample.n, expected_info, status, report);
  if (expected_info == 0) {
    inverse::Mathematics(test, serial);
  } else {
    ASC_DENSE_TEST_CHECK(
        test,
        base::EqualBytes(serial.a.data(), sample.a.data(), sizeof(sample.a)));
  }
  serial.Guards(test);
  scratch.Guards(test, workspace);
  std::array<int, 4> results{};
  std::array<std::thread, 4> workers;
  std::barrier start(4);
  for (int i = 0; i < 4; ++i) {
    workers[i] = std::thread([&, i] {
      TestContext local;
      Worker(local, provider, sample, serial.a, pivots, plan, expected_info,
             start);
      results[i] = local.Finish();
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  for (const auto result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
  ASC_DENSE_TEST_CHECK(test, base::EqualBytes(sample.a.data(), before.a.data(),
                                              sizeof(sample.a)));
  ASC_DENSE_TEST_EQ(test, sample.pivots, before.pivots);
  protected_p.Check(test, sample.pivots);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = inverse::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int groups = 0;
  for (int n : {0, 1, 2, 5, 17, 65}) {
    for (auto triangle : {base::kUpper, base::kLower}) {
      for (auto layout : {base::kColumn, base::kRow}) {
        for (int kind : {0, 1, 3}) {
          inverse::Sample<T> sample(n, hermitian, triangle, layout, -20);
          sample.Reset(kind);
          Group(test, provider, sample);
          ++groups;
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, groups, 72);
  std::printf("Packed inverse concurrent groups=%d workers=4 repeats=4\n",
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
