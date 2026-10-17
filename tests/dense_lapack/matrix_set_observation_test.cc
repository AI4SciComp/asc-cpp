#include <linux/prctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <complex>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>  // IWYU pragma: keep; starts the containing array lifetime.

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_copy.h"
#include "asc/dense/providers/lapack_matrix_set.h"
#include "installed_lu/normal_return_guard.h"
#include "matrix_copy_test_support.h"
#include "matrix_set_entry.h"
namespace {
namespace support = asc_copy_test;
namespace fault = asc_set_entry;
using support::kColumn;
using support::kHost;
using support::kRow;
using support::Take;
using support::TestContext;
// Linux-local observation uses initialized, aligned containing arrays. There
// is no fault handler resumption and no uninitialized numeric read.
template <typename T>
class Page {
 public:
  explicit Page(std::size_t pages = 1) {
    const auto size = sysconf(_SC_PAGESIZE);
    if (size <= 0 || static_cast<std::size_t>(size) % sizeof(T) != 0) {
      std::abort();
    }
    bytes_ = pages * static_cast<std::size_t>(size);
    void* memory = mmap(nullptr, bytes_, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
      std::abort();
    }
    data_ = ::new (memory) T[bytes_ / sizeof(T)];
    std::fill_n(data_, bytes_ / sizeof(T), T{2});
  }
  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;
  Page(Page&&) = delete;
  Page& operator=(Page&&) = delete;
  ~Page() {
    Protect(PROT_READ | PROT_WRITE);
    std::destroy_n(data_, bytes_ / sizeof(T));
    if (munmap(data_, bytes_) != 0) {
      std::abort();
    }
  }
  void ProtectLastPage(int mode) {
    const auto bytes = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    if (mprotect(reinterpret_cast<unsigned char*>(data_) + bytes_ - bytes,
                 bytes, mode) != 0) {
      std::abort();
    }
  }
  void Protect(int mode) {
    if (mprotect(data_, bytes_, mode) != 0) {
      std::abort();
    }
  }
  auto Vector(asc::extent_t n) {
    return Take(asc::DenseBlasVectorView<T>::Create(data_, n, 1, Storage()));
  }
  auto Matrix(asc::extent_t n, asc::extent_t nrhs,
              asc::DenseBlasLayout layout) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        data_, n, nrhs, layout,
        layout == kRow ? std::max<asc::extent_t>(1, nrhs)
                       : std::max<asc::extent_t>(1, n),
        Storage()));
  }
  [[nodiscard]] asc::ConstMemoryView Storage() const {
    return {data_, bytes_, kHost};
  }
  T* data() { return data_; }

 private:
  T* data_ = nullptr;
  std::size_t bytes_ = 0;
};

// Piped core collectors can ignore RLIMIT_CORE and delay each deliberate
// fault. Suppress dumps in the calibration child only; keep the expected
// SIGSEGV and never resume a faulting instruction.
void PrepareFaultChild() {
  if (prctl(PR_SET_DUMPABLE, 0) != 0 ||
      std::signal(SIGSEGV, SIG_DFL) == SIG_ERR) {
    _exit(95);
  }
}

thread_local std::array<asc::ConstMemoryView, 1> g_outputs{
    asc::ConstMemoryView{nullptr, 0, kHost}};
void RestoreAtEntry() {
  for (const auto memory : g_outputs) {
    if (mprotect(const_cast<void*>(memory.data()), memory.size(),
                 PROT_READ | PROT_WRITE) != 0) {
      std::abort();
    }
  }
}
template <typename T, typename Operation>
void Calibrate(TestContext& test, Page<T>& output, Operation call) {
  // Negative control lies before the actual adapter/native entry,
  // on exactly the output storage protected for the successful call.
  const pid_t child = fork();
  ASC_DENSE_TEST_CHECK(test, child >= 0);
  if (child == 0) {
    PrepareFaultChild();
    const volatile auto* byte =
        reinterpret_cast<const volatile unsigned char*>(output.data());
    const unsigned char value = *byte;
    static_cast<void>(value);
    static_cast<void>(call());
    _exit(99);
  }
  if (child > 0) {
    int status = 0;
    ASC_DENSE_TEST_EQ(test, waitpid(child, &status, 0), child);
    ASC_DENSE_TEST_CHECK(test,
                         WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
  }
}
template <typename T>
void ObserveCase(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Page<T>& output,
                 asc::DenseBlasLayout layout, asc::extent_t m, asc::extent_t n,
                 asc::LapackMatrixPart part) {
  std::fill_n(output.data(), static_cast<std::size_t>(m * n), T{-5});
  const auto view = output.Matrix(m, n, layout);
  const auto query = [&] {
    return asc::QueryLasetWorkspace(provider, part, view);
  };
  const auto plan = Take(query());
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto call = [&](const asc::LapackWorkspacePlan& selected,
                        const asc::LapackWorkspace& work) {
    return asc::Laset(provider, part, T{2}, T{3}, view, selected, work, report);
  };
  output.Protect(PROT_NONE);
  ASC_DENSE_TEST_CHECK(test, query().ok());
  auto stale = plan;
  ++stale.regions[support::kLayout].minimum_entries;
  fault::Reset();
  ASC_DENSE_TEST_CHECK(test, !call(stale, workspace).ok());
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  const bool active = m > 0 && n > 0;
  if (active && layout == kRow) {
    auto undersized = workspace;
    undersized.regions[support::kLayout] = {
        workspace.regions[support::kLayout].data(),
        workspace.regions[support::kLayout].size() - 1, kHost};
    ASC_DENSE_TEST_CHECK(test, !call(plan, undersized).ok());
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  }
  Calibrate(test, output, [&] { return call(plan, workspace); });
  if (!active) {
    ASC_DENSE_TEST_CHECK(test, call(plan, workspace).ok());
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
    ASC_DENSE_TEST_CHECK(test, !report.native_info);
  } else {
    g_outputs = {output.Storage()};
    fault::OnEntry(RestoreAtEntry);
    ASC_DENSE_TEST_CHECK(test, call(plan, workspace).ok());
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
    ASC_DENSE_TEST_CHECK(test, !report.native_info);
    for (asc::extent_t i = 0; i < m; ++i) {
      for (asc::extent_t j = 0; j < n; ++j) {
        const bool selected =
            part == asc::LapackMatrixPart::kAll ||
            (part == asc::LapackMatrixPart::kUpper ? i <= j : i >= j);
        const auto offset = layout == kRow ? i * n + j : j * m + i;
        ASC_DENSE_TEST_EQ(test, output.data()[offset],
                          selected ? (i == j ? T{3} : T{2}) : T{-5});
      }
    }
  }
  output.Protect(PROT_READ | PROT_WRITE);
  scratch.Guards(test, workspace);
}
template <typename T>
void Observe(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  Page<T> output;
  for (auto layout : {kColumn, kRow}) {
    for (asc::extent_t m : {0, 3}) {
      for (asc::extent_t n : {0, 2}) {
        for (auto part :
             {asc::LapackMatrixPart::kAll, asc::LapackMatrixPart::kUpper,
              asc::LapackMatrixPart::kLower}) {
          ObserveCase(test, provider, output, layout, m, n, part);
        }
      }
    }
  }
}
template <typename T>
void UnselectedOutput(TestContext& test,
                      const asc::ReferenceLapackProvider& provider) {
  const auto count = static_cast<asc::extent_t>(sysconf(_SC_PAGESIZE)) /
                     static_cast<asc::extent_t>(sizeof(T));
  for (auto layout : {kRow, kColumn}) {
    for (auto part :
         {asc::LapackMatrixPart::kUpper, asc::LapackMatrixPart::kLower}) {
      Page<T> output(2);
      const bool upper = part == asc::LapackMatrixPart::kUpper;
      const bool strided =
          (upper && layout == kRow) || (!upper && layout == kColumn);
      T* data = output.data() + (strided ? 0 : count - 1);
      const asc::extent_t leading = strided ? count : 2;
      const auto view = Take(asc::DenseBlasMatrixView<T>::Create(
          data, upper ? 2 : 1, upper ? 1 : 2, layout, leading,
          output.Storage()));
      const auto plan = Take(asc::QueryLasetWorkspace(provider, part, view));
      support::Scratch<T> scratch;
      const auto workspace = scratch.Workspace(plan);
      asc::LapackReport report;
      output.ProtectLastPage(PROT_NONE);
      const pid_t child = fork();
      ASC_DENSE_TEST_CHECK(test, child >= 0);
      if (child == 0) {
        PrepareFaultChild();
        const volatile auto* byte =
            reinterpret_cast<const volatile unsigned char*>(output.data() +
                                                            count);
        const unsigned char value = *byte;
        static_cast<void>(value);
        _exit(98);
      }
      if (child > 0) {
        int status = 0;
        ASC_DENSE_TEST_EQ(test, waitpid(child, &status, 0), child);
        ASC_DENSE_TEST_CHECK(
            test, WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
      }
      fault::Reset();
      ASC_DENSE_TEST_CHECK(test,
                           asc::QueryLasetWorkspace(provider, part, view).ok());
      ASC_DENSE_TEST_CHECK(test, asc::Laset(provider, part, T{7}, T{-3}, view,
                                            plan, workspace, report)
                                     .ok());
      ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
      ASC_DENSE_TEST_CHECK(test, !report.native_info);
      ASC_DENSE_TEST_EQ(test, data[0], T{-3});
      output.ProtectLastPage(PROT_READ | PROT_WRITE);
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Observe<float>(test, provider);
  Observe<double>(test, provider);
  Observe<std::complex<float>>(test, provider);
  Observe<std::complex<double>>(test, provider);
  UnselectedOutput<float>(test, provider);
  UnselectedOutput<double>(test, provider);
  UnselectedOutput<std::complex<float>>(test, provider);
  UnselectedOutput<std::complex<double>>(test, provider);
  std::puts(
      "LASET observation: 96 calibrated pre-entry output-read controls, 24 "
      "actual native output-writing calls, 72 local empty completions and 16 "
      "calibrated unselected-output controls. Scalar alpha/beta are by-value "
      "inputs; no input array exists. Actual foreign boundary; no provider "
      "instrumentation.");
  return test.Finish();
}
