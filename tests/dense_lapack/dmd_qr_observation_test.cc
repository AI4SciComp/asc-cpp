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
#include <cstdlib>
#include <memory>
#include <new>  // IWYU pragma: keep; starts valid containing-array lifetimes.

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_dmd.h"
#include "asc/dense/providers/lapack_dmd_qr.h"
#include "dmd_qr_entry.h"
#include "dmd_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace support = asc_dmd_test;
namespace fault = asc_dmd_qr_entry;
using support::kColumn;
using support::kHost;
using support::kRow;
using support::Take;
using support::TestContext;
// Reuse the maintained LACPY observation mechanism with distinct GEDMDQ pages.
// Only ASC before the actual wrapped GEDMDQ entry is observed. The pinned
// provider is not instrumented; rank/report scalar objects are not watched.
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

thread_local std::array<asc::ConstMemoryView, 10> g_outputs{
    asc::ConstMemoryView{nullptr, 0, kHost},
    asc::ConstMemoryView{nullptr, 0, kHost},
    asc::ConstMemoryView{nullptr, 0, kHost},
    asc::ConstMemoryView{nullptr, 0, kHost},
    asc::ConstMemoryView{nullptr, 0, kHost},
    asc::ConstMemoryView{nullptr, 0, kHost},
    asc::ConstMemoryView{nullptr, 0, kHost},
    asc::ConstMemoryView{nullptr, 0, kHost},
    asc::ConstMemoryView{nullptr, 0, kHost},
    asc::ConstMemoryView{nullptr, 0, kHost}};
void Protect(asc::ConstMemoryView memory, int flags) {
  if (mprotect(const_cast<void*>(memory.data()), memory.size(), flags) != 0) {
    std::abort();
  }
}
void RestoreAtEntry() {
  for (const auto memory : g_outputs) {
    Protect(memory, PROT_READ | PROT_WRITE);
  }
}
template <typename Operation>
void ExpectFault(TestContext& test, Operation operation) {
  const pid_t child = fork();
  ASC_DENSE_TEST_CHECK(test, child >= 0);
  if (child == 0) {
    PrepareFaultChild();
    operation();
    _exit(99);
  }
  if (child > 0) {
    int status = 0;
    ASC_DENSE_TEST_EQ(test, waitpid(child, &status, 0), child);
    ASC_DENSE_TEST_CHECK(test,
                         WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
  }
}
template <typename T, typename Query, typename Operation>
void ProtectedPreflight(TestContext& test, Page<T>& f, bool empty,
                        const asc::LapackWorkspacePlan& plan,
                        const asc::LapackWorkspace& workspace,
                        asc::LapackReport& report, asc::index_t& rank,
                        Query query, Operation run) {
  // A legitimate input read must fault before entry restoration. The child
  // uses the actual adapter; outputs are readable in this calibration.
  if (!empty) {
    for (const auto input : {f.Storage()}) {
      ExpectFault(test, [&] {
        Protect(input, PROT_NONE);
        static_cast<void>(run(plan, workspace));
      });
    }
  }
  f.Protect(PROT_NONE);
  for (const auto memory : g_outputs) {
    Protect(memory, PROT_NONE);
  }
  ASC_DENSE_TEST_CHECK(test, query().ok());
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  if (!empty) {
    auto short_work = workspace;
    short_work.regions[support::kLayout] = {
        short_work.regions[support::kLayout].data(),
        short_work.regions[support::kLayout].size() - 1, kHost};
    ASC_DENSE_TEST_CHECK(test, !run(plan, short_work).ok());
    auto stale = plan;
    ++stale.regions[support::kScalar].minimum_entries;
    ASC_DENSE_TEST_CHECK(test, !run(stale, workspace).ok());
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, rank, -73);
    f.Protect(PROT_READ | PROT_WRITE);
    for (const auto memory : g_outputs) {
      // Intentional forbidden pre-entry read, from initialized output bytes.
      ExpectFault(test, [&] {
        const volatile auto* byte =
            static_cast<const volatile unsigned char*>(memory.data());
        const auto value = *byte;
        static_cast<void>(value);
        static_cast<void>(run(plan, workspace));
      });
    }
  }
}
template <typename T>
void Observe(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::LapackDmdQrOptions options, bool reverse, asc::extent_t n) {
  using Real = support::Real<T>;
  Page<T> f;
  Page<T> x;
  Page<T> y;
  Page<T> z;
  Page<T> b;
  Page<T> w;
  Page<T> s;
  Page<T> tau;
  Page<std::complex<Real>> eigen;
  Page<Real> singular;
  Page<Real> residual;
  const auto first = reverse ? kRow : kColumn;
  const auto second = reverse ? kColumn : kRow;
  const auto p = std::max<asc::extent_t>(0, n - 1);
  const bool empty = n <= 1;
  const auto fv = f.Matrix(3, n, first);
  if (!empty) {
    std::fill_n(f.data(), 6, T{});
    f.data()[0] = T{1};
    f.data()[first == kRow ? 1 : 3] = T{2};
  }
  const asc::LapackDmdQrBuffers<T> buffers{fv,
                                           x.Matrix(n, p, second),
                                           y.Matrix(n, n, first),
                                           z.Matrix(3, p, second),
                                           b.Matrix(n, p, first),
                                           w.Matrix(p, p, second),
                                           s.Matrix(p, p, first),
                                           eigen.Vector(p),
                                           singular.Vector(p),
                                           residual.Vector(p),
                                           tau.Vector(n)};
  eigen.data()[0] = {-5, 3};
  singular.data()[0] = -7;
  residual.data()[0] = -9;
  const auto plan =
      Take(asc::QueryGedmdqWorkspace(provider, options, buffers, Real{0}));
  support::Scratch<T> scratch(plan, false);
  asc::index_t rank = -73;
  asc::LapackReport report;
  g_outputs = {x.Storage(),     y.Storage(),        z.Storage(),
               b.Storage(),     w.Storage(),        s.Storage(),
               eigen.Storage(), singular.Storage(), residual.Storage(),
               tau.Storage()};
  fault::Reset();
  fault::OnEntry(RestoreAtEntry);
  const auto run = [&](const asc::LapackWorkspacePlan& selected,
                       const asc::LapackWorkspace& workspace) {
    return asc::Gedmdq(provider, options, buffers, Real{0}, rank, selected,
                       workspace, report);
  };
  ProtectedPreflight(
      test, f, empty, plan, scratch.workspace, report, rank,
      [&] {
        return asc::QueryGedmdqWorkspace(provider, options, buffers, Real{0});
      },
      run);
  ASC_DENSE_TEST_CHECK(test, run(plan, scratch.workspace).ok());
  ASC_DENSE_TEST_EQ(test, rank, empty ? 0 : 1);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), empty ? 0U : 1U);
  if (empty) {
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    f.Protect(PROT_READ | PROT_WRITE);
    RestoreAtEntry();
  } else {
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    ASC_DENSE_TEST_EQ(test, eigen.data()[0], std::complex<Real>(2, 0));
    ASC_DENSE_TEST_EQ(test, singular.data()[0], Real{1});
  }
  scratch.Guards(test);
  fault::Reset();
}
template <typename T>
void Run(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  using Vectors = asc::LapackDmdQrVectors;
  using Extra = asc::LapackDmdExtra;
  for (const auto svd :
       {asc::LapackDmdSvd::kBidiagonalQr, asc::LapackDmdSvd::kDivideAndConquer,
        asc::LapackDmdSvd::kQrPreconditioned, asc::LapackDmdSvd::kJacobi}) {
    const std::array modes{
        asc::LapackDmdQrOptions{asc::LapackDmdScaling::kNone, Vectors::kNone,
                                Extra::kNone, svd, false, false, false, -1},
        asc::LapackDmdQrOptions{asc::LapackDmdScaling::kNone,
                                Vectors::kPodFactored, Extra::kNone, svd, false,
                                true, false, -1},
        asc::LapackDmdQrOptions{asc::LapackDmdScaling::kNone,
                                Vectors::kExplicit, Extra::kRefinement, svd,
                                true, false, true, -1},
        asc::LapackDmdQrOptions{asc::LapackDmdScaling::kNone,
                                Vectors::kQrFactored, Extra::kExact, svd, false,
                                true, true, -1}};
    for (const auto options : modes) {
      for (const bool reverse : {false, true}) {
        Observe<T>(test, provider, options, reverse, 2);
      }
    }
    for (asc::extent_t n : {0, 1}) {
      Observe<T>(test, provider, modes[0], false, n);
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  TestContext test;
  Run<float>(test);
  Run<double>(test);
  Run<std::complex<float>>(test);
  Run<std::complex<double>>(test);
  return test.Finish();
}
