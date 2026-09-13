#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <complex>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>  // IWYU pragma: keep; starts the containing array lifetime.

#include "../../src/dense/lapack/internal_mixed_general_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_mixed_general.h"
#include "installed_lu/normal_return_guard.h"
#include "mixed_general_faults.h"
#include "mixed_general_test_support.h"

namespace {
namespace support = asc_mixed_test;
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

thread_local std::array<asc::ConstMemoryView, 2> g_outputs{
    asc::ConstMemoryView{nullptr, 0, kHost},
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
    std::signal(SIGSEGV, SIG_DFL);
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
void Observe(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  Page<T> a;
  Page<T> b;
  Page<T> x;
  Page<asc::index_t> pivots;
  for (const auto al : {kColumn, kRow}) {
    for (const auto bl : {kColumn, kRow}) {
      for (const auto xl : {kColumn, kRow}) {
        for (const asc::extent_t n : {0, 3}) {
          for (const asc::extent_t nrhs : {0, 2}) {
            const auto av = a.Matrix(n, n, al);
            const asc::DenseBlasMatrixView<const T> bv = b.Matrix(n, nrhs, bl);
            const auto xv = x.Matrix(n, nrhs, xl);
            const auto pv = pivots.Vector(n);
            const auto query = [&] {
              if constexpr (asc::DenseBlasComplex<T>) {
                return asc::QueryZcgesvWorkspace(provider, av, pv, bv, xv);
              } else {
                return asc::QueryDsgesvWorkspace(provider, av, pv, bv, xv);
              }
            };
            const auto plan = Take(query());
            support::Scratch<T> scratch;
            const auto workspace = scratch.Workspace(plan);
            asc::LapackMixedSolveStatistics statistics;
            asc::LapackReport report;
            const auto call = [&](const asc::LapackWorkspacePlan& selected,
                                  const asc::LapackWorkspace& work) {
              if constexpr (asc::DenseBlasComplex<T>) {
                return asc::Zcgesv(provider, av, pv, bv, xv, selected, work,
                                   statistics, report);
              } else {
                return asc::Dsgesv(provider, av, pv, bv, xv, selected, work,
                                   statistics, report);
              }
            };
            a.Protect(PROT_NONE);
            b.Protect(PROT_NONE);
            x.Protect(PROT_NONE);
            pivots.Protect(PROT_NONE);
            ASC_DENSE_TEST_CHECK(test, query().ok());
            auto stale = plan;
            ++stale.regions[support::kLayout].minimum_entries;
            asc_mixed_fault::Reset(asc_mixed_fault::Mode::kPass);
            ASC_DENSE_TEST_CHECK(test, !call(stale, workspace).ok());
            ASC_DENSE_TEST_EQ(test, asc_mixed_fault::Calls(), std::size_t{0});
            if (n > 0) {
              ASC_DENSE_TEST_CHECK(test, !call(plan, {}).ok());
              ASC_DENSE_TEST_EQ(test, asc_mixed_fault::Calls(), std::size_t{0});
            }
            Calibrate(test, x, [&] { return call(plan, workspace); });
            if (n == 0) {
              ASC_DENSE_TEST_CHECK(test, call(plan, workspace).ok());
              ASC_DENSE_TEST_EQ(test, asc_mixed_fault::Calls(), std::size_t{0});
              ASC_DENSE_TEST_CHECK(
                  test, !report.native_info && !statistics.native_iteration);
            }
            a.Protect(PROT_READ | PROT_WRITE);
            b.Protect(PROT_READ | PROT_WRITE);
            std::fill_n(a.data(), 9, T{});
            a.data()[0] = a.data()[4] = a.data()[8] = T{2};
            std::fill_n(b.data(), 6, T{2});
            if (n > 0) {
              g_outputs = {x.Storage(), pivots.Storage()};
              asc_mixed_fault::OnEntry(RestoreAtEntry);
              ASC_DENSE_TEST_CHECK(test, call(plan, workspace).ok());
              ASC_DENSE_TEST_EQ(test, asc_mixed_fault::Calls(), std::size_t{1});
              ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
              for (asc::extent_t i = 0; i < n * nrhs; ++i) {
                ASC_DENSE_TEST_EQ(test, x.data()[i], T{1});
              }
            }
            x.Protect(PROT_READ | PROT_WRITE);
            pivots.Protect(PROT_READ | PROT_WRITE);
            scratch.Guards(test, workspace);
          }
        }
      }
    }
  }
}
void Counts(TestContext& test) {
  namespace counts = asc::internal_mixed_general_counts;
  for (const asc::extent_t limit :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    for (const bool complex : {false, true}) {
      ASC_DENSE_TEST_CHECK(test,
                           counts::Query(0, limit, 1, limit, complex).ok());
      ASC_DENSE_TEST_CHECK(test,
                           !counts::Query(1, limit, 1, limit, complex).ok());
      ASC_DENSE_TEST_CHECK(
          test, !counts::Query(limit, 0, limit, limit, complex).ok());
      const auto empty_rhs = Take(counts::Query(3, 0, 3, limit, complex));
      ASC_DENSE_TEST_EQ(test, empty_rhs.lower, 10);
      ASC_DENSE_TEST_EQ(test, empty_rhs.residual, complex ? 0 : 3);
      const auto normal = Take(counts::Query(3, 2, 3, limit, complex));
      ASC_DENSE_TEST_EQ(test, normal.lower, 15);
      ASC_DENSE_TEST_EQ(test, normal.solution, 6);
      const auto boundary = limit == std::numeric_limits<std::int32_t>::max()
                                ? asc::extent_t{46340}
                                : asc::extent_t{3037000499};
      ASC_DENSE_TEST_CHECK(
          test, counts::Query(boundary, 0, boundary, limit, complex).ok());
      ASC_DENSE_TEST_CHECK(
          test,
          !counts::Query(boundary + 1, 0, boundary + 1, limit, complex).ok());
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Counts(test);
  Observe<double>(test, provider);
  Observe<std::complex<double>>(test, provider);
  std::puts(
      "Protected initialized X/pivots until wrapped native entry; both mixed "
      "drivers, "
      "independent layouts, N=0 and NRHS=0; negative read controls detected. "
      "ASC region only, native provider is not instrumented.");
  return test.Finish();
}
