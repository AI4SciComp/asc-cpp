#include <sys/mman.h>
#include <sys/types.h>
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
#include <new>  // IWYU pragma: keep; initialized containing array lifetime.
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_refinement.h"
#include "installed_lu/normal_return_guard.h"
#include "positive_tridiagonal_refinement_entry.h"
#include "tridiagonal_test_support.h"
namespace {
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::Value;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
template <typename T>
using Real = asc::DenseBlasRealType<T>;
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
  [[nodiscard]] auto MutableVector(asc::extent_t n) {
    return Take(asc::DenseBlasVectorView<T>::Create(data_, n, 1,
                                                    {data_, bytes_, kHost}));
  }
  [[nodiscard]] auto Matrix(asc::extent_t n, asc::extent_t nrhs,
                            asc::DenseBlasLayout layout) {
    const auto ld = (layout == asc::DenseBlasLayout::kRowMajor ? nrhs : n) + 3;
    return Take(asc::DenseBlasMatrixView<T>::Create(data_, n, nrhs, layout, ld,
                                                    {data_, bytes_, kHost}));
  }
  [[nodiscard]] auto Matrix(asc::extent_t n, asc::extent_t nrhs,
                            asc::DenseBlasLayout layout) const {
    const auto ld = (layout == asc::DenseBlasLayout::kRowMajor ? nrhs : n) + 3;
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        data_, n, nrhs, layout, ld, {data_, bytes_, kHost}));
  }
  [[nodiscard]] std::size_t bytes() const { return bytes_; }
  T* data() { return data_; }

 private:
  T* data_ = nullptr;
  std::size_t bytes_ = 0;
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
  int local = 0;
};
template <typename T>
struct Buffers {
  Page<Real<T>> d, df, ferr, berr, real, estimates;
  Page<T> e, ef, rhs, solution, scalar, layout;
  void ProtectAll(int mode) {
    d.Protect(mode);
    df.Protect(mode);
    ferr.Protect(mode);
    berr.Protect(mode);
    real.Protect(mode);
    estimates.Protect(mode);
    e.Protect(mode);
    ef.Protect(mode);
    rhs.Protect(mode);
    solution.Protect(mode);
    scalar.Protect(mode);
    layout.Protect(mode);
  }
  void ProtectInputs(int mode) {
    d.Protect(mode);
    df.Protect(mode);
    e.Protect(mode);
    ef.Protect(mode);
    rhs.Protect(mode);
    solution.Protect(mode);
    scalar.Protect(mode);
    real.Protect(mode);
    estimates.Protect(mode);
    layout.Protect(mode);
  }
  static void Enter(void* argument) {
    auto& self = *static_cast<Buffers*>(argument);
    self.ferr.Protect(PROT_READ | PROT_WRITE);
    self.berr.Protect(PROT_READ | PROT_WRITE);
    self.scalar.Protect(PROT_READ | PROT_WRITE);
    self.real.Protect(PROT_READ | PROT_WRITE);
    // Input-read controls would be restored here if ASC failed to consume them.
    self.df.Protect(PROT_READ | PROT_WRITE);
    self.solution.Protect(PROT_READ | PROT_WRITE);
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    for (const auto role :
         {asc_tridiagonal_test::kScalar, asc_tridiagonal_test::kReal,
          asc_tridiagonal_test::kLayout, asc_tridiagonal_test::kScratch}) {
      const auto& region = plan.regions[role];
      const auto bytes = static_cast<std::size_t>(region.preferred_entries) *
                         region.entry_bytes;
      if (bytes == 0) {
        continue;
      }
      void* pointer = nullptr;
      std::size_t capacity = 0;
      if (role == asc_tridiagonal_test::kScalar) {
        pointer = scalar.data();
        capacity = scalar.bytes();
      }
      if (role == asc_tridiagonal_test::kReal) {
        pointer = real.data();
        capacity = real.bytes();
      }
      if (role == asc_tridiagonal_test::kLayout) {
        pointer = layout.data();
        capacity = layout.bytes();
      }
      if (role == asc_tridiagonal_test::kScratch) {
        pointer = estimates.data();
        capacity = estimates.bytes();
      }
      if (bytes > capacity) {
        std::abort();
      }
      workspace.regions[role] = {pointer, bytes, kHost};
    }
    return workspace;
  }
};
template <typename T, typename Operation>
void ObserveExecution(TestContext& test, Buffers<T>& storage, asc::extent_t n,
                      asc::extent_t nrhs, const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      const Operation& call, Counts& counts) {
  asc::LapackReport report;
  if (n == 0 || nrhs == 0) {
    storage.ProtectInputs(PROT_NONE);
    ASC_DENSE_TEST_CHECK(test, call(plan, workspace, report).ok());
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, asc_ptrfs_entry::Calls(), 0U);
    for (asc::extent_t j = 0; j < nrhs; ++j) {
      ASC_DENSE_TEST_EQ(test, storage.ferr.data()[j], Real<T>{0});
      ASC_DENSE_TEST_EQ(test, storage.berr.data()[j], Real<T>{0});
    }
    ++counts.local;
  } else {
    storage.ferr.Protect(PROT_NONE);
    storage.berr.Protect(PROT_NONE);
    storage.scalar.Protect(PROT_NONE);
    storage.real.Protect(PROT_NONE);
    for (int region = 0; region < 2; ++region) {
      MustFault(test, [&] {
        const volatile Real<T>* pointer =
            region == 0 ? storage.ferr.data() : storage.berr.data();
        const Real<T> forbidden = *pointer;
        (void)forbidden;
        (void)call(plan, workspace, report);
      });
      ++counts.negative;
    }
    MustFault(test, [&] {
      storage.df.Protect(PROT_NONE);
      (void)call(plan, workspace, report);
    });
    MustFault(test, [&] {
      storage.solution.Protect(PROT_NONE);
      (void)call(plan, workspace, report);
    });
    counts.input += 2;
    ASC_DENSE_TEST_CHECK(test, call(plan, workspace, report).ok());
    ASC_DENSE_TEST_EQ(test, asc_ptrfs_entry::Calls(), 1U);
    ASC_DENSE_TEST_CHECK(
        test, report.called_provider && report.native_info.value_or(-99) == 0);
    ++counts.native;
  }
}
template <typename T>
void One(TestContext& test, const asc::ReferenceLapackProvider& provider,
         asc::extent_t n, asc::extent_t nrhs, asc::DenseBlasTriangle triangle,
         asc::DenseBlasLayout b_layout, asc::DenseBlasLayout x_layout,
         Counts& counts) {
  Buffers<T> storage;
  const T off = Value<T>(0.25L, 0.25L);
  storage.d.data()[0] = 2;
  storage.d.data()[1] =
      asc::DenseBlasComplex<T> ? Real<T>{3.0625} : Real<T>{3.03125};
  storage.df.data()[0] = 2;
  storage.df.data()[1] = 3;
  storage.e.data()[0] = off;
  storage.ef.data()[0] = off / Real<T>{2};
  if constexpr (asc::DenseBlasComplex<T>) {
    if (triangle == kUpper) {
      storage.ef.data()[0] = std::conj(storage.ef.data()[0]);
    }
  }
  const auto original =
      Take(asc::LapackPositiveDefiniteTridiagonalView<const T>::Create(
          storage.d.Vector(n), storage.e.Vector(n == 0 ? 0 : n - 1)));
  const auto factor =
      Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::FromRaw(
          provider, triangle, storage.df.Vector(n),
          storage.ef.Vector(n == 0 ? 0 : n - 1)));
  const auto rhs = std::as_const(storage.rhs).Matrix(n, nrhs, b_layout);
  const auto solution = storage.solution.Matrix(n, nrhs, x_layout);
  const auto ferr = storage.ferr.MutableVector(nrhs);
  const auto berr = storage.berr.MutableVector(nrhs);
  const auto plan = Take(asc::QueryPtrfsWorkspace(provider, original, factor,
                                                  rhs, solution, ferr, berr));
  const auto workspace = storage.Workspace(plan);
  const auto query = [&] {
    return asc::QueryPtrfsWorkspace(provider, original, factor, rhs, solution,
                                    ferr, berr);
  };
  const auto call = [&](const asc::LapackWorkspacePlan& given,
                        const asc::LapackWorkspace& scratch,
                        asc::LapackReport& report) {
    return asc::Ptrfs(provider, original, factor, rhs, solution, ferr, berr,
                      given, scratch, report);
  };
  asc_ptrfs_entry::Reset();
  asc_ptrfs_entry::OnEntry(&Buffers<T>::Enter, &storage);
  storage.ProtectAll(PROT_NONE);
  ASC_DENSE_TEST_CHECK(test, query().ok());
  auto stale = plan;
  ++stale.regions[asc_tridiagonal_test::kScalar].preferred_entries;
  asc::LapackReport report;
  ASC_DENSE_TEST_EQ(test, call(stale, workspace, report).code(),
                    asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  if (n > 0 && nrhs > 0) {
    auto short_work = workspace;
    auto& region = short_work.regions[asc_tridiagonal_test::kScalar];
    region = {region.data(), region.size() - 1, kHost};
    ASC_DENSE_TEST_EQ(test, call(plan, short_work, report).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  }
  storage.ProtectAll(PROT_READ | PROT_WRITE);
  ObserveExecution(test, storage, n, nrhs, plan, workspace, call, counts);
  storage.ProtectAll(PROT_READ | PROT_WRITE);
  asc_ptrfs_entry::Reset();
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         Counts& counts) {
  for (const auto triangle : {kLower, kUpper}) {
    for (const auto b : {kColumn, kRow}) {
      for (const auto x : {kColumn, kRow}) {
        One<T>(test, provider, 2, 3, triangle, b, x, counts);
        One<T>(test, provider, 0, 3, triangle, b, x, counts);
        One<T>(test, provider, 2, 0, triangle, b, x, counts);
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
  Counts counts;
  Run<float>(test, provider, counts);
  Run<double>(test, provider, counts);
  Run<std::complex<float>>(test, provider, counts);
  Run<std::complex<double>>(test, provider, counts);
  ASC_DENSE_TEST_EQ(test, counts.negative, 64);
  ASC_DENSE_TEST_EQ(test, counts.input, 64);
  ASC_DENSE_TEST_EQ(test, counts.native, 32);
  ASC_DENSE_TEST_EQ(test, counts.local, 64);
  std::printf(
      "PTRFS observer:64 forbidden caller-estimate reads,64 legitimate "
      "factor/initial-X reads,32 actual native entries,64 local completions. "
      "Active caller FERR/BERR and native work protected until foreign entry; "
      "estimate staging sentinel accesses permitted. Local inputs protected, "
      "output writes checked separately. ASC/test scope; provider/runtime "
      "uninstrumented.\n");
  return test.Finish();
}
