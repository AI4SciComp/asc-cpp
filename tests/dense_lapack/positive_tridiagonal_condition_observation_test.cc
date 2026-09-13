#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <complex>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>  // IWYU pragma: keep; starts the containing array lifetime.

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_condition.h"
#include "installed_lu/normal_return_guard.h"
#include "positive_tridiagonal_condition_entry.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kReal;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
// Same initialized-containing-array/default-fault mechanism as the admitted
// Linux matrix and packed observers; no fabricated pointers or signal resume.
template <typename T>
class Page {
 public:
  Page() {
    const auto size = sysconf(_SC_PAGESIZE);
    if (size <= 0 || static_cast<std::size_t>(size) % sizeof(T) != 0) {
      std::abort();
    }
    bytes_ = static_cast<std::size_t>(size);
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
  void Protect(int mode) {
    if (mprotect(data_, bytes_, mode) != 0) {
      std::abort();
    }
  }
  [[nodiscard]] auto Vector(asc::extent_t n) const {
    return Take(asc::DenseBlasVectorView<const T>::Create(
        data_, n, 1, {data_, bytes_, kHost}));
  }
  T* data() { return data_; }

 private:
  T* data_ = nullptr;
  std::size_t bytes_ = 0;
};
template <typename T>
struct EntryRestore {
  Page<asc::DenseBlasRealType<T>>* output;
  Page<asc::DenseBlasRealType<T>>* work;
  Page<asc::DenseBlasRealType<T>>* diagonal = nullptr;
  Page<T>* off = nullptr;
  static void Enter(void* argument) {
    auto& self = *static_cast<EntryRestore*>(argument);
    self.output->Protect(PROT_READ | PROT_WRITE);
    self.work->Protect(PROT_READ | PROT_WRITE);
    if (self.diagonal != nullptr) {
      self.diagonal->Protect(PROT_READ | PROT_WRITE);
    }
    if (self.off != nullptr) {
      self.off->Protect(PROT_READ | PROT_WRITE);
    }
  }
};
template <typename Operation>
void MustFault(TestContext& test, Operation operation) {
  const pid_t child = fork();
  ASC_DENSE_TEST_CHECK(test, child >= 0);
  if (child == 0) {
    std::signal(SIGSEGV, SIG_DFL);
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
struct Counts {
  int negative = 0;
  int input = 0;
  int native = 0;
  int quick = 0;
};
template <typename T>
void MetadataOnly(TestContext& test,
                  const asc::ReferenceLapackProvider& provider,
                  asc::ReferencePositiveDefiniteTridiagonalFactorView<T> factor,
                  asc::DenseBlasRealType<T> norm,
                  asc::DenseBlasRealType<T>& output,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace) {
  ASC_DENSE_TEST_CHECK(
      test, asc::QueryPtconWorkspace(provider, factor, norm, output).ok());
  auto stale = plan;
  ++stale.regions[kReal].minimum_entries;
  asc::LapackReport report;
  ASC_DENSE_TEST_EQ(
      test,
      asc::Ptcon(provider, factor, norm, output, stale, workspace, report)
          .code(),
      asc::ErrorCode::kInvalidState);
  if (workspace.regions[kReal].size() != 0) {
    auto short_workspace = workspace;
    short_workspace.regions[kReal] = {workspace.regions[kReal].data(),
                                      workspace.regions[kReal].size() - 1,
                                      kHost};
    ASC_DENSE_TEST_EQ(test,
                      asc::Ptcon(provider, factor, norm, output, plan,
                                 short_workspace, report)
                          .code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  ASC_DENSE_TEST_EQ(test, asc_ptcon_entry::Calls(), 0U);
}
template <typename T>
void ObserveCase(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Counts& counts,
                 asc::DenseBlasTriangle triangle, asc::extent_t n,
                 asc::DenseBlasRealType<T> norm) {
  using Real = asc::DenseBlasRealType<T>;
  Page<Real> diagonal;
  Page<Real> output;
  Page<Real> work;
  Page<T> off;
  const auto factor =
      Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::FromRaw(
          provider, triangle, diagonal.Vector(n),
          off.Vector(n == 0 ? 0 : n - 1)));
  const auto plan =
      Take(asc::QueryPtconWorkspace(provider, factor, norm, output.data()[0]));
  const bool active = n > 0 && norm > 0;
  asc::LapackWorkspace workspace;
  if (active) {
    workspace.regions[kReal] = {
        work.data(), static_cast<std::size_t>(n) * sizeof(Real), kHost};
  }
  diagonal.Protect(PROT_NONE);
  off.Protect(PROT_NONE);
  output.Protect(PROT_NONE);
  work.Protect(PROT_NONE);
  asc_ptcon_entry::Reset();
  MetadataOnly(test, provider, factor, norm, output.data()[0], plan, workspace);
  asc::LapackReport report;
  // Deliberate forbidden old-output read immediately before the actual
  // adapter call. This calibration must fault before foreign entry.
  MustFault(test, [&] {
    const volatile auto* byte =
        reinterpret_cast<const volatile unsigned char*>(output.data());
    const auto value = *byte;
    static_cast<void>(value);
    static_cast<void>(asc::Ptcon(provider, factor, norm, output.data()[0], plan,
                                 workspace, report));
  });
  ++counts.negative;
  EntryRestore<T> restore{&output, &work};
  if (active) {
    // Valid input factors legitimately read during ASC validation. If
    // validation were omitted, the entry callback would restore access
    // and the provider could return normally, failing this control.
    off.Protect(PROT_READ | PROT_WRITE);
    restore.diagonal = &diagonal;
    asc_ptcon_entry::OnEntry(EntryRestore<T>::Enter, &restore);
    MustFault(test, [&] {
      static_cast<void>(asc::Ptcon(provider, factor, norm, output.data()[0],
                                   plan, workspace, report));
    });
    ++counts.input;
    diagonal.Protect(PROT_READ | PROT_WRITE);
    restore.diagonal = nullptr;
    if (n > 1) {
      off.Protect(PROT_NONE);
      restore.off = &off;
      MustFault(test, [&] {
        static_cast<void>(asc::Ptcon(provider, factor, norm, output.data()[0],
                                     plan, workspace, report));
      });
      ++counts.input;
      off.Protect(PROT_READ | PROT_WRITE);
      restore.off = nullptr;
    }
  }
  asc_ptcon_entry::OnEntry(EntryRestore<T>::Enter, &restore);
  const auto status = asc::Ptcon(provider, factor, norm, output.data()[0], plan,
                                 workspace, report);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, asc_ptcon_entry::Calls(), 1U);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-99), 0);
  if (!active) {
    ASC_DENSE_TEST_EQ(test, output.data()[0], n == 0 ? Real{1} : Real{0});
    ++counts.quick;
  }
  ++counts.native;
  asc_ptcon_entry::Reset();
}
template <typename T>
void Observe(TestContext& test, const asc::ReferenceLapackProvider& provider,
             Counts& counts) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto triangle :
       {asc::DenseBlasTriangle::kLower, asc::DenseBlasTriangle::kUpper}) {
    for (const asc::extent_t n : {0, 1, 3}) {
      for (const Real norm : {Real{0}, Real{4}}) {
        ObserveCase<T>(test, provider, counts, triangle, n, norm);
      }
    }
  }
}

}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  TestContext test;
  Counts counts;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Observe<float>(test, provider, counts);
  Observe<double>(test, provider, counts);
  Observe<std::complex<float>>(test, provider, counts);
  Observe<std::complex<double>>(test, provider, counts);
  ASC_DENSE_TEST_EQ(test, counts.negative, 48);
  ASC_DENSE_TEST_EQ(test, counts.input, 24);
  ASC_DENSE_TEST_EQ(test, counts.native, 48);
  ASC_DENSE_TEST_EQ(test, counts.quick, 32);
  std::printf(
      "PTCON observer: %d forbidden pre-entry read controls, %d legitimate "
      "factor-read controls, %d native output writes (%d native quick "
      "returns). Caller output and scratch restored at actual foreign entry; "
      "inactive factors remain protected. ASC/test scope; no provider "
      "instrumentation.\n",
      counts.negative, counts.input, counts.native, counts.quick);
  return test.Finish();
}
