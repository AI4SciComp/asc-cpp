// POSIX SIGBUS is outside the ISO C++ <csignal> contract.
#include <signal.h>  // NOLINT(modernize-deprecated-headers)
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_driver.h"
#include "installed_lu/normal_return_guard.h"
#include "positive_tridiagonal_driver_entry.h"
#include "tridiagonal_test_support.h"

namespace {
namespace probe = asc::internal_ptsv_test;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
template <typename T>
using Real = asc::DenseBlasRealType<T>;

template <typename T>
class Page {
 public:
  Page() {
    const auto page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 ||
        static_cast<std::size_t>(page_size) % sizeof(T) != 0) {
      std::abort();
    }
    bytes_ = static_cast<std::size_t>(page_size);
    void* memory = mmap(nullptr, bytes_, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
      std::abort();
    }
    data_ = new (memory) T[bytes_ / sizeof(T)]{};
  }
  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;
  Page(Page&&) = delete;
  Page& operator=(Page&&) = delete;
  ~Page() {
    Open();
    std::destroy_n(data_, bytes_ / sizeof(T));
    if (munmap(data_, bytes_) != 0) {
      std::abort();
    }
  }
  T* data() { return data_; }
  asc::MutableMemoryView storage() { return {data_, bytes_, kHost}; }
  void Protect() {
    if (mprotect(data_, bytes_, PROT_NONE) != 0) {
      std::abort();
    }
  }
  void Open() {
    if (mprotect(data_, bytes_, PROT_READ | PROT_WRITE) != 0) {
      std::abort();
    }
  }

 private:
  T* data_ = nullptr;
  std::size_t bytes_ = 0;
};

template <typename Function>
bool Child(Function function, bool expect_fault) {
  const pid_t pid = fork();
  if (pid < 0) {
    std::abort();
  }
  if (pid == 0) {
    ::signal(SIGSEGV, SIG_DFL);
    ::signal(SIGBUS, SIG_DFL);
    std::_Exit(function() ? 0 : 1);
  }
  int status = 0;
  pid_t result = 0;
  do {
    result = waitpid(pid, &status, 0);
  } while (result < 0 && errno == EINTR);
  if (result != pid) {
    std::abort();
  }
  return expect_fault ? WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV
                      : WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

template <typename T>
struct Pages {
  Page<Real<T>> d;
  Page<T> e;
  Page<T> b;
  Page<T> scratch;
  void Protect() {
    d.Protect();
    e.Protect();
    b.Protect();
    scratch.Protect();
  }
  void Open() {
    d.Open();
    e.Open();
    b.Open();
    scratch.Open();
  }
};
template <typename T>
thread_local Pages<T>* active_pages = nullptr;
template <typename T>
void RestoreAtEntry() {
  active_pages<T>->Open();
}

template <typename T>
struct Observation {
  TestContext& test;
  const asc::ReferenceLapackProvider& provider;
  Pages<T>& pages;
  const asc::LapackPositiveDefiniteTridiagonalView<T>& matrix;
  const asc::DenseBlasMatrixView<T>& rhs;
  const asc::LapackWorkspacePlan& plan;
  const asc::LapackWorkspace& workspace;

  void Calibrate() {
    // Valid containing objects are initialized before their pages are
    // protected. The negative control deliberately reads a real object,
    // including when the logical descriptor has zero length; it never
    // dereferences an empty view.
    ASC_DENSE_TEST_CHECK(test, Child(
                                   [&] {
                                     pages.d.Protect();
                                     volatile Real<T> value = pages.d.data()[0];
                                     static_cast<void>(value);
                                     return true;
                                   },
                                   true));
    // Ordinary output-writing calibration after restoring access. Protection is
    // not claimed for numeric inputs during an active provider computation.
    ASC_DENSE_TEST_CHECK(test, Child(
                                   [&] {
                                     pages.Protect();
                                     pages.Open();
                                     pages.scratch.data()[0] = T{9};
                                     return pages.scratch.data()[0] == T{9};
                                   },
                                   false));
  }
  void Metadata() {
    const auto bytes =
        static_cast<std::size_t>(plan.regions[kLayout].minimum_entries) *
        sizeof(T);
    ASC_DENSE_TEST_CHECK(
        test,
        Child(
            [&] {
              pages.Protect();
              probe::SetFault(probe::Fault::kNone);
              return asc::QueryPtsvWorkspace(provider, matrix, rhs).ok() &&
                     probe::EntryCount() == 0;
            },
            false));
    ASC_DENSE_TEST_CHECK(
        test, Child(
                  [&] {
                    auto stale = plan;
                    ++stale.regions[kLayout].minimum_entries;
                    pages.Protect();
                    probe::SetFault(probe::Fault::kNone);
                    asc::LapackReport report;
                    const auto status = asc::Ptsv(provider, matrix, rhs, stale,
                                                  workspace, report);
                    return status.code() == asc::ErrorCode::kInvalidState &&
                           !report.called_provider && probe::EntryCount() == 0;
                  },
                  false));
    if (bytes > 0) {
      ASC_DENSE_TEST_CHECK(
          test,
          Child(
              [&] {
                auto short_workspace = workspace;
                short_workspace.regions[kLayout] = {pages.scratch.data(),
                                                    bytes - 1, kHost};
                pages.Protect();
                probe::SetFault(probe::Fault::kNone);
                asc::LapackReport report;
                const auto status = asc::Ptsv(provider, matrix, rhs, plan,
                                              short_workspace, report);
                return status.code() == asc::ErrorCode::kInvalidArgument &&
                       !report.called_provider && probe::EntryCount() == 0;
              },
              false));
    }
  }
  void Execution() {
    const auto n = matrix.order();
    const auto nrhs = rhs.columns();
    if (n == 0) {
      ASC_DENSE_TEST_CHECK(
          test, Child(
                    [&] {
                      pages.Protect();
                      probe::SetFault(probe::Fault::kNone);
                      asc::LapackReport report;
                      const auto status = asc::Ptsv(provider, matrix, rhs, plan,
                                                    workspace, report);
                      return status.ok() && !report.called_provider &&
                             !report.native_info && probe::EntryCount() == 0;
                    },
                    false));
    } else {
      ASC_DENSE_TEST_CHECK(
          test, Child(
                    [&] {
                      active_pages<T> = &pages;
                      probe::SetFault(probe::Fault::kNone);
                      probe::SetEntryHook(RestoreAtEntry<T>);
                      if (nrhs == 0) {
                        pages.b.Protect();
                        pages.scratch.Protect();
                      }
                      asc::LapackReport report;
                      const auto status = asc::Ptsv(provider, matrix, rhs, plan,
                                                    workspace, report);
                      return status.ok() && report.called_provider &&
                             report.native_info == 0 &&
                             probe::EntryCount() == 1;
                    },
                    false));
      if (nrhs > 0) {
        // B is a legitimate input. Restoring access only at actual foreign
        // entry makes the required ASC input-packing read observable before
        // entry.
        ASC_DENSE_TEST_CHECK(test, Child(
                                       [&] {
                                         active_pages<T> = &pages;
                                         probe::SetFault(probe::Fault::kNone);
                                         probe::SetEntryHook(RestoreAtEntry<T>);
                                         pages.b.Protect();
                                         asc::LapackReport report;
                                         return asc::Ptsv(provider, matrix, rhs,
                                                          plan, workspace,
                                                          report)
                                             .ok();
                                       },
                                       true));
      }
    }
  }
};

template <typename T>
void Observe(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::extent_t n, asc::extent_t nrhs, asc::DenseBlasLayout layout) {
  Pages<T> pages;
  for (std::size_t i = 0; i < 3; ++i) {
    pages.d.data()[i] = Real<T>{4};
  }
  pages.e.data()[0] = T{0.25};
  pages.e.data()[1] = T{0.25};
  for (std::size_t i = 0; i < 12; ++i) {
    pages.b.data()[i] = T{1};
  }
  const auto d = Take(asc::DenseBlasVectorView<Real<T>>::Create(
      pages.d.data(), n, 1, pages.d.storage()));
  const auto e = Take(asc::DenseBlasVectorView<T>::Create(
      pages.e.data(), n == 0 ? 0 : n - 1, 1, pages.e.storage()));
  const auto matrix =
      Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(d, e));
  const auto leading = layout == asc::DenseBlasLayout::kColumnMajor
                           ? std::max(asc::extent_t{1}, n) + 1
                           : std::max(asc::extent_t{1}, nrhs) + 1;
  const auto rhs = Take(asc::DenseBlasMatrixView<T>::Create(
      pages.b.data(), n, nrhs, layout, leading, pages.b.storage()));
  const auto plan = Take(asc::QueryPtsvWorkspace(provider, matrix, rhs));
  asc::LapackWorkspace workspace;
  const auto bytes =
      static_cast<std::size_t>(plan.regions[kLayout].minimum_entries) *
      sizeof(T);
  workspace.regions[kLayout] = {pages.scratch.data(), bytes, kHost};
  Observation<T> observation{test, provider, pages,    matrix,
                             rhs,  plan,     workspace};
  observation.Calibrate();
  observation.Metadata();
  observation.Execution();
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto layout :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    for (const asc::extent_t n : {0, 3}) {
      for (const asc::extent_t nrhs : {0, 2}) {
        Observe<T>(test, provider, n, nrhs, layout);
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Run<float>(test, provider);
  Run<double>(test, provider);
  Run<std::complex<float>>(test, provider);
  Run<std::complex<double>>(test, provider);
  std::printf(
      "PTSV observation:32 negative read controls,32 metadata queries,32 stale "
      "plans,8 exact short workspaces,16 local N0 completions,8 native NRHS0 "
      "paths,8 active native controls,8 legitimate B input-read controls,32 "
      "restored write controls. ASC scope; provider/runtime uninstrumented.\n");
  return test.Finish();
}
