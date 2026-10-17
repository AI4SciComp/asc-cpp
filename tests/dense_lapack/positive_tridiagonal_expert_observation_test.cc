// POSIX SIGBUS is outside the ISO C++ <csignal> contract.
#include <signal.h>  // NOLINT(modernize-deprecated-headers)
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.
#include <type_traits>

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
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_expert.h"
#include "installed_lu/normal_return_guard.h"
#include "positive_tridiagonal_expert_entry.h"
#include "positive_tridiagonal_refinement_test_support.h"
#include "tridiagonal_test_support.h"

namespace {
namespace probe = asc::internal_ptsvx_test;
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
  Page<Real<T>> d, df, condition, ferr, berr;
  Page<T> e, ef, b, x;
  void Protect() {
    d.Protect();
    df.Protect();
    condition.Protect();
    ferr.Protect();
    berr.Protect();
    e.Protect();
    ef.Protect();
    b.Protect();
    x.Protect();
  }
  void Open() {
    d.Open();
    df.Open();
    condition.Open();
    ferr.Open();
    berr.Open();
    e.Open();
    ef.Open();
    b.Open();
    x.Open();
  }
};
template <typename T>
thread_local Pages<T>* active_pages = nullptr;
template <typename T>
void RestoreAtEntry() {
  active_pages<T>->Open();
}
template <typename T>
auto Vector(Page<T>& page, asc::extent_t n) {
  return Take(
      asc::DenseBlasVectorView<T>::Create(page.data(), n, 1, page.storage()));
}
template <typename T, typename F>
struct Observation {
  TestContext& test;
  const asc::ReferenceLapackProvider& provider;
  Pages<T>& pages;
  asc::LapackPositiveDefiniteTridiagonalView<const T> original;
  F factor;
  asc::extent_t n;
  asc::extent_t nrhs;
  asc::DenseBlasMatrixView<const T> b;
  asc::DenseBlasMatrixView<T> x;
  asc::DenseBlasVectorView<Real<T>> ferr;
  asc::DenseBlasVectorView<Real<T>> berr;
  Real<T>& condition;
  const asc::LapackWorkspacePlan& plan;
  const asc::LapackWorkspace& workspace;
  static constexpr bool kSupplied =
      std::is_same_v<F, asc::ReferencePositiveDefiniteTridiagonalFactorView<T>>;
  asc::Status Execute(const asc::LapackWorkspacePlan& selected,
                      const asc::LapackWorkspace& space,
                      asc::LapackReport& report) {
    return asc::Ptsvx(provider, original, factor, b, x, condition, ferr, berr,
                      selected, space, report);
  }
  void Calibrate() {
    // Calibrate each old scalar output, plus a scalar object on complex X's
    // containing array. Negative controls run before entry and must fault.
    for (auto* page : {&pages.condition, &pages.ferr, &pages.berr}) {
      ASC_DENSE_TEST_CHECK(test, Child(
                                     [&] {
                                       page->Protect();
                                       volatile Real<T> value = page->data()[0];
                                       static_cast<void>(value);
                                       return true;
                                     },
                                     true));
    }
    ASC_DENSE_TEST_CHECK(test, Child(
                                   [&] {
                                     pages.x.Protect();
                                     volatile T value = pages.x.data()[0];
                                     static_cast<void>(value);
                                     return true;
                                   },
                                   true));
    ASC_DENSE_TEST_CHECK(test, Child(
                                   [&] {
                                     pages.Protect();
                                     pages.Open();
                                     pages.x.data()[0] = T{9};
                                     condition = 7;
                                     return pages.x.data()[0] == T{9} &&
                                            condition == 7;
                                   },
                                   false));
  }
  void Metadata() {
    ASC_DENSE_TEST_CHECK(test, Child(
                                   [&] {
                                     pages.Protect();
                                     probe::SetFault(probe::Fault::kNone);
                                     return asc::QueryPtsvxWorkspace(
                                                provider, original, factor, b,
                                                x, condition, ferr, berr)
                                                .ok() &&
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
                    return Execute(stale, workspace, report).code() ==
                               asc::ErrorCode::kInvalidState &&
                           !report.called_provider && probe::EntryCount() == 0;
                  },
                  false));
    for (std::size_t i = 0; i < workspace.regions.size(); ++i) {
      if (workspace.regions[i].size() == 0) {
        continue;
      }
      ASC_DENSE_TEST_CHECK(
          test, Child(
                    [&] {
                      auto short_workspace = workspace;
                      short_workspace.regions[i] = {
                          workspace.regions[i].data(),
                          workspace.regions[i].size() - 1, kHost};
                      pages.Protect();
                      probe::SetFault(probe::Fault::kNone);
                      asc::LapackReport report;
                      return Execute(plan, short_workspace, report).code() ==
                                 asc::ErrorCode::kInvalidArgument &&
                             !report.called_provider &&
                             probe::EntryCount() == 0;
                    },
                    false));
    }
  }
  void Execution() {
    if (n == 0) {
      // Local numeric inputs/inactive arrays are inaccessible. Writable scalar
      // outputs are checked separately; PROT_WRITE is not claimed unreadable.
      ASC_DENSE_TEST_CHECK(
          test, Child(
                    [&] {
                      pages.d.Protect();
                      pages.e.Protect();
                      pages.df.Protect();
                      pages.ef.Protect();
                      pages.b.Protect();
                      pages.x.Protect();
                      probe::SetFault(probe::Fault::kNone);
                      asc::LapackReport report;
                      if (!Execute(plan, workspace, report).ok() ||
                          report.called_provider || report.native_info ||
                          condition != 1 || probe::EntryCount() != 0) {
                        return false;
                      }
                      for (asc::extent_t j = 0; j < nrhs; ++j) {
                        if (ferr.data()[j] != 0 || berr.data()[j] != 0) {
                          return false;
                        }
                      }
                      return true;
                    },
                    false));
    } else {
      ASC_DENSE_TEST_CHECK(test,
                           Child(
                               [&] {
                                 active_pages<T> = &pages;
                                 probe::SetFault(probe::Fault::kNone);
                                 probe::SetEntryHook(RestoreAtEntry<T>);
                                 pages.x.Protect();
                                 pages.condition.Protect();
                                 pages.ferr.Protect();
                                 pages.berr.Protect();
                                 if constexpr (!kSupplied) {
                                   pages.df.Protect();
                                   pages.ef.Protect();
                                 }
                                 if (nrhs == 0) {
                                   pages.b.Protect();
                                 }
                                 asc::LapackReport report;
                                 return Execute(plan, workspace, report).ok() &&
                                        report.called_provider &&
                                        report.native_info == 0 &&
                                        probe::EntryCount() == 1;
                               },
                               false));
      if constexpr (kSupplied) {
        ASC_DENSE_TEST_CHECK(test,
                             Child(
                                 [&] {
                                   active_pages<T> = &pages;
                                   probe::SetFault(probe::Fault::kNone);
                                   probe::SetEntryHook(RestoreAtEntry<T>);
                                   pages.df.Protect();
                                   asc::LapackReport report;
                                   return Execute(plan, workspace, report).ok();
                                 },
                                 true));
      }
    }
  }
};
template <typename T, typename F>
void ObserveCase(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Pages<T>& pages,
                 asc::LapackPositiveDefiniteTridiagonalView<const T> original,
                 F factor, asc::extent_t n, asc::extent_t nrhs,
                 asc::DenseBlasLayout b_layout, asc::DenseBlasLayout x_layout) {
  const auto b = Take(asc::DenseBlasMatrixView<const T>::Create(
      pages.b.data(), n, nrhs, b_layout, 4, pages.b.storage()));
  const auto x = Take(asc::DenseBlasMatrixView<T>::Create(
      pages.x.data(), n, nrhs, x_layout, 4, pages.x.storage()));
  const auto ferr = Vector(pages.ferr, nrhs);
  const auto berr = Vector(pages.berr, nrhs);
  Real<T>& condition = *pages.condition.data();
  const auto plan = Take(asc::QueryPtsvxWorkspace(provider, original, factor, b,
                                                  x, condition, ferr, berr));
  asc_ptrfs_test::Storage<T> storage;
  const auto workspace = storage.Workspace(plan);
  Observation<T, F> observation{test, provider,  pages, original, factor,
                                n,    nrhs,      b,     x,        ferr,
                                berr, condition, plan,  workspace};
  observation.Calibrate();
  observation.Metadata();
  observation.Execution();
}
template <typename T>
void Observe(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::extent_t n, asc::extent_t nrhs, asc::DenseBlasLayout b,
             asc::DenseBlasLayout x) {
  Pages<T> pages;
  for (std::size_t i = 0; i < 3; ++i) {
    pages.d.data()[i] = 4;
    pages.df.data()[i] = 4;
  }
  for (std::size_t i = 0; i < 12; ++i) {
    pages.b.data()[i] = T{1};
  }
  const auto original =
      Take(asc::LapackPositiveDefiniteTridiagonalView<const T>::Create(
          Vector(pages.d, n), Vector(pages.e, n == 0 ? 0 : n - 1)));
  const auto fresh = Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
      Vector(pages.df, n), Vector(pages.ef, n == 0 ? 0 : n - 1)));
  ObserveCase(test, provider, pages, original, fresh, n, nrhs, b, x);
  for (const auto triangle :
       {asc::DenseBlasTriangle::kLower, asc::DenseBlasTriangle::kUpper}) {
    const auto supplied =
        Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::FromRaw(
            provider, triangle, Vector(pages.df, n),
            Vector(pages.ef, n == 0 ? 0 : n - 1)));
    ObserveCase(test, provider, pages, original, supplied, n, nrhs, b, x);
  }
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto b :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    for (const auto x : {asc::DenseBlasLayout::kColumnMajor,
                         asc::DenseBlasLayout::kRowMajor}) {
      for (const asc::extent_t n : {0, 3}) {
        for (const asc::extent_t nrhs : {0, 2}) {
          Observe<T>(test, provider, n, nrhs, b, x);
        }
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
      "PTSVX observation: 192 mode/layout/shape/scalar cases, 768 negative "
      "output-read controls, 192 protected queries and stale plans each, every "
      "active workspace shortened by one byte, 96 local completions, 96 "
      "actual-entry output controls, 64 legitimate supplied-factor reads, 192 "
      "restored writes. ASC pre-native scope; provider/runtime uninstrumented. "
      "Local outputs writable; local no-read semantics are source-reviewed.\n");
  return test.Finish();
}
