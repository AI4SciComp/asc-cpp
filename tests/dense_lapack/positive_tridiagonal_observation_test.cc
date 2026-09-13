#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <complex>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>  // IWYU pragma: keep; starts the containing array lifetime.

#include "../../src/dense/lapack/internal_positive_tridiagonal_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "installed_lu/normal_return_guard.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
// Localized Linux observation, matching the existing packed-family mechanism.
// Real initialized arrays remain alive and within bounds while inaccessible.
// No signal handler resumes a fault, and no numeric values are uninitialized.
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

template <typename T>
void Observe(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  Page<asc::DenseBlasRealType<T>> d;
  Page<T> e;
  Page<T> b;
  const auto matrix =
      Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(d.Vector(3),
                                                                 e.Vector(2)));
  const auto factor_plan = Take(asc::QueryPttrfWorkspace(provider, matrix));
  const auto factor =
      Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::FromRaw(
          provider, kLower, d.Vector(3), e.Vector(2)));
  for (const auto layout : {kColumn, kRow}) {
    const auto rhs = b.Matrix(3, 2, layout);
    const auto plan = Take(asc::QueryPttrsWorkspace(provider, factor, rhs));
    d.Protect(PROT_NONE);
    e.Protect(PROT_NONE);
    b.Protect(PROT_NONE);
    ASC_DENSE_TEST_CHECK(test, asc::QueryPttrfWorkspace(provider, matrix).ok());
    ASC_DENSE_TEST_CHECK(test,
                         asc::QueryPttrsWorkspace(provider, factor, rhs).ok());
    ASC_DENSE_TEST_CHECK(
        test, asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::FromRaw(
                  provider, kLower, d.Vector(3), e.Vector(2))
                  .ok());
    asc::LapackReport report;
    auto stale = factor_plan;
    ++stale.regions[asc_tridiagonal_test::kLayout].minimum_entries;
    ASC_DENSE_TEST_CHECK(test,
                         !asc::Pttrf(provider, matrix, stale, {}, report).ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    auto stale_solve = plan;
    ++stale_solve.regions[asc_tridiagonal_test::kLayout].minimum_entries;
    ASC_DENSE_TEST_CHECK(
        test, !asc::Pttrs(provider, factor, rhs, stale_solve, {}, report).ok());
    if (layout == kRow) {
      ASC_DENSE_TEST_CHECK(
          test, !asc::Pttrs(provider, factor, rhs, plan, {}, report).ok());
    }
    const auto empty = b.Matrix(3, 0, layout);
    const auto empty_plan =
        Take(asc::QueryPttrsWorkspace(provider, factor, empty));
    ASC_DENSE_TEST_CHECK(
        test, asc::Pttrs(provider, factor, empty, empty_plan, {}, report).ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    const auto zero =
        Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
            d.Vector(0), e.Vector(0)));
    const auto zero_plan = Take(asc::QueryPttrfWorkspace(provider, zero));
    ASC_DENSE_TEST_CHECK(
        test, asc::Pttrf(provider, zero, zero_plan, {}, report).ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    // Calibrate on the same protected live storage. Child intentionally reads
    // one initialized representation byte; successful detection is SIGSEGV.
    const pid_t child = fork();
    ASC_DENSE_TEST_CHECK(test, child >= 0);
    if (child == 0) {
      std::signal(SIGSEGV, SIG_DFL);
      const volatile auto* byte =
          reinterpret_cast<const volatile unsigned char*>(d.data());
      const unsigned char value = *byte;
      static_cast<void>(value);
      _exit(99);
    }
    if (child > 0) {
      int status = 0;
      ASC_DENSE_TEST_EQ(test, waitpid(child, &status, 0), child);
      ASC_DENSE_TEST_CHECK(test,
                           WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
    }
    d.Protect(PROT_READ | PROT_WRITE);
    e.Protect(PROT_READ | PROT_WRITE);
    b.Protect(PROT_READ | PROT_WRITE);
    // Positive control: legitimate input read and output write after restore.
    b.data()[0] = T{d.data()[0]};
    ASC_DENSE_TEST_EQ(test, b.data()[0], T{2});
  }
}
void Counts(TestContext& test) {
  namespace counts = asc::internal_positive_tridiagonal_counts;
  for (const asc::extent_t limit :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    ASC_DENSE_TEST_CHECK(test, counts::Factor(limit, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, counts::Solve(limit - 1, limit - 1, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Solve(limit, 1, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Solve(2, limit, 2, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Solve(1, 1, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(limit, 0, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(0, limit, 1, limit).ok());
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Counts(test);
  Observe<float>(test, provider);
  Observe<double>(test, provider);
  Observe<std::complex<float>>(test, provider);
  Observe<std::complex<double>>(test, provider);
  std::puts(
      "PT protected numeric arrays: metadata queries, stale/short-workspace "
      "rejection, N=0/NRHS=0; calibrated forbidden-read child detected. ASC "
      "region only; no foreign-code instrumentation.");
  return test.Finish();
}
