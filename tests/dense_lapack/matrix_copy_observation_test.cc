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
#include "installed_lu/normal_return_guard.h"
#include "matrix_copy_entry.h"
#include "matrix_copy_test_support.h"
namespace {
namespace support = asc_copy_test;
namespace fault = asc_copy_entry;
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
template <typename Input>
void ObserveCase(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 Page<Input>& input, Page<support::Output<Input>>& output,
                 asc::DenseBlasLayout il, asc::DenseBlasLayout ol,
                 asc::extent_t m, asc::extent_t n, asc::LapackMatrixPart part) {
  std::fill_n(output.data(), static_cast<std::size_t>(m * n), Input{-5});
  const asc::DenseBlasMatrixView<const Input> iv = input.Matrix(m, n, il);
  const auto ov = output.Matrix(m, n, ol);
  const auto query = [&] {
    return support::Query<Input>(provider, iv, ov, part);
  };
  const auto plan = Take(query());
  support::Scratch<Input> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto call = [&](const asc::LapackWorkspacePlan& selected,
                        const asc::LapackWorkspace& work) {
    return support::Execute<Input>(provider, iv, ov, selected, work, report,
                                   part);
  };
  input.Protect(PROT_NONE);
  output.Protect(PROT_NONE);
  ASC_DENSE_TEST_CHECK(test, query().ok());
  auto stale = plan;
  ++stale.regions[support::kScratch].minimum_entries;
  fault::Reset();
  ASC_DENSE_TEST_CHECK(test, !call(stale, workspace).ok());
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  const bool active = m > 0 && n > 0;
  if (active) {
    ASC_DENSE_TEST_CHECK(test, !call(plan, {}).ok());
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  }
  Calibrate(test, output, [&] { return call(plan, workspace); });
  if (!active) {
    ASC_DENSE_TEST_CHECK(test, call(plan, workspace).ok());
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
    ASC_DENSE_TEST_CHECK(test, !report.native_info);
  }
  // The row packing path must legitimately read the immutable numeric input.
  // A child exits distinctly if it reaches foreign entry without that read.
  if (active && il == kRow) {
    const pid_t child = fork();
    ASC_DENSE_TEST_CHECK(test, child >= 0);
    if (child == 0) {
      PrepareFaultChild();
      fault::OnEntry([] { _exit(96); });
      static_cast<void>(call(plan, workspace));
      _exit(97);
    }
    if (child > 0) {
      int status = 0;
      ASC_DENSE_TEST_EQ(test, waitpid(child, &status, 0), child);
      ASC_DENSE_TEST_CHECK(test,
                           WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
    }
  }
  input.Protect(PROT_READ | PROT_WRITE);
  if (active) {
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
        const auto offset = ol == kRow ? i * n + j : j * m + i;
        ASC_DENSE_TEST_EQ(test, output.data()[offset],
                          selected ? Input{2} : Input{-5});
      }
    }
  }
  output.Protect(PROT_READ | PROT_WRITE);
  scratch.Guards(test, workspace);
}
template <typename Input>
void Observe(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  Page<Input> input;
  Page<support::Output<Input>> output;
  for (auto il : {kColumn, kRow}) {
    for (auto ol : {kColumn, kRow}) {
      for (asc::extent_t m : {0, 3}) {
        for (asc::extent_t n : {0, 2}) {
          for (auto part :
               {asc::LapackMatrixPart::kAll, asc::LapackMatrixPart::kUpper,
                asc::LapackMatrixPart::kLower}) {
            ObserveCase(test, provider, input, output, il, ol, m, n, part);
          }
        }
      }
    }
  }
}
template <typename T>
void UnselectedInput(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  const auto count = static_cast<asc::extent_t>(sysconf(_SC_PAGESIZE)) /
                     static_cast<asc::extent_t>(sizeof(T));
  for (auto il : {kRow, kColumn}) {
    for (auto part :
         {asc::LapackMatrixPart::kUpper, asc::LapackMatrixPart::kLower}) {
      Page<T> input(2);
      Page<T> output;
      const bool upper = part == asc::LapackMatrixPart::kUpper;
      const bool strided = (upper && il == kRow) || (!upper && il == kColumn);
      T* data = input.data() + (strided ? 0 : count - 1);
      const asc::extent_t leading = strided ? count : 2;
      const auto iv = Take(asc::DenseBlasMatrixView<const T>::Create(
          data, upper ? 2 : 1, upper ? 1 : 2, il, leading, input.Storage()));
      output.data()[0] = T{-5};
      const auto ov = output.Matrix(upper ? 2 : 1, upper ? 1 : 2, kColumn);
      const auto plan = Take(support::Query<T>(provider, iv, ov, part));
      support::Scratch<T> scratch;
      const auto workspace = scratch.Workspace(plan);
      asc::LapackReport report;
      input.ProtectLastPage(PROT_NONE);
      // The first numeric cell on page2 is an unselected matrix cell in each
      // valid descriptor. Calibrate it independently before the real call.
      const pid_t child = fork();
      ASC_DENSE_TEST_CHECK(test, child >= 0);
      if (child == 0) {
        PrepareFaultChild();
        const volatile auto* byte =
            reinterpret_cast<const volatile unsigned char*>(input.data() +
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
                           support::Query<T>(provider, iv, ov, part).ok());
      ASC_DENSE_TEST_CHECK(test, support::Execute<T>(provider, iv, ov, plan,
                                                     workspace, report, part)
                                     .ok());
      ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
      ASC_DENSE_TEST_CHECK(test, !report.native_info);
      ASC_DENSE_TEST_EQ(test, output.data()[0], T{2});
      input.ProtectLastPage(PROT_READ | PROT_WRITE);
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
  UnselectedInput<float>(test, provider);
  UnselectedInput<double>(test, provider);
  UnselectedInput<std::complex<float>>(test, provider);
  UnselectedInput<std::complex<double>>(test, provider);
  std::puts(
      "LACPY observation: 192 calibrated output controls, 24 legitimate "
      "row-input controls, 16 calibrated unselected-input controls; actual "
      "foreign entry, no provider instrumentation");
  return test.Finish();
}
