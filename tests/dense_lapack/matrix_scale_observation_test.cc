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
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_scale.h"
#include "installed_lu/normal_return_guard.h"
#include "matrix_scale_entry.h"
#include "matrix_scale_test_support.h"
#include "tridiagonal_test_support.h"
namespace {
namespace support = asc_tridiagonal_test;
namespace fault = asc_scale_entry;
using support::kColumn;
using support::kHost;
using support::kRow;
using support::Take;
using support::TestContext;
using Extent = asc::extent_t;
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
  void ProtectPage(std::size_t page, int mode) {
    const auto bytes = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    if (page >= bytes_ / bytes ||
        mprotect(reinterpret_cast<unsigned char*>(data_) + page * bytes, bytes,
                 mode) != 0) {
      std::abort();
    }
  }
  [[nodiscard]] std::size_t PageEntries() const {
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE)) / sizeof(T);
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

struct Totals {
  std::size_t negative = 0;
  std::size_t input_controls = 0;
  std::size_t active = 0;
  std::size_t local = 0;
  std::size_t unused = 0;
};
thread_local asc::ConstMemoryView g_restore{nullptr, 0, kHost};
void RestoreAtEntry() {
  if (mprotect(const_cast<void*>(g_restore.data()), g_restore.size(),
               PROT_READ | PROT_WRITE) != 0) {
    std::abort();
  }
}
void ExpectFault(TestContext& test, pid_t child) {
  ASC_DENSE_TEST_CHECK(test, child >= 0);
  if (child > 0) {
    int status = 0;
    ASC_DENSE_TEST_EQ(test, waitpid(child, &status, 0), child);
    ASC_DENSE_TEST_CHECK(test,
                         WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
  }
}
template <typename T>
void Negative(TestContext& test, const T* protected_value, Totals& totals) {
  const auto child = fork();
  if (child == 0) {
    PrepareFaultChild();
    const auto* address =
        reinterpret_cast<const volatile unsigned char*>(protected_value);
    const auto value = *address;
    static_cast<void>(value);
    _exit(99);
  }
  ExpectFault(test, child);
  ++totals.negative;
}

template <typename T>
struct Matrix {
  using Real = asc::DenseBlasRealType<T>;
  T* data;
  asc::ConstMemoryView backing;
  char type;
  Extent m;
  Extent n;
  Extent lower;
  Extent upper;
  Extent leading;
  asc::DenseBlasLayout layout;

  [[nodiscard]] auto Full() const {
    return Take(asc::DenseBlasMatrixView<T>::Create(data, m, n, layout, leading,
                                                    backing));
  }
  [[nodiscard]] auto PositiveBand() const {
    return Take(asc::LapackPositiveDefiniteBandView<T>::Create(
        data, m, lower,
        type == 'B' ? asc::DenseBlasTriangle::kLower
                    : asc::DenseBlasTriangle::kUpper,
        layout, leading, backing));
  }
  [[nodiscard]] auto GeneralBand() const {
    return Take(asc::LapackLuBandView<T>::Create(data, m, n, lower, upper,
                                                 leading, backing));
  }
  auto Query(const asc::ReferenceLapackProvider& provider) const {
    if (type == 'Z') {
      return asc::QueryLasclWorkspace(provider, GeneralBand());
    }
    if (type == 'B' || type == 'Q') {
      return asc::QueryLasclWorkspace(provider, PositiveBand());
    }
    return asc::QueryLasclWorkspace(provider, asc_scale_test::PartFor(type),
                                    Full());
  }
  asc::Status Run(const asc::ReferenceLapackProvider& provider, Real from,
                  Real to, const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) const {
    if (type == 'Z') {
      return asc::Lascl(provider, from, to, GeneralBand(), plan, workspace,
                        report);
    }
    if (type == 'B' || type == 'Q') {
      return asc::Lascl(provider, from, to, PositiveBand(), plan, workspace,
                        report);
    }
    return asc::Lascl(provider, asc_scale_test::PartFor(type), from, to, Full(),
                      plan, workspace, report);
  }
  [[nodiscard]] Extent FirstDiagonal() const {
    if (type == 'Z') {
      return lower + upper;
    }
    if ((type == 'Q' && layout == kColumn) || (type == 'B' && layout == kRow)) {
      return lower;
    }
    return 0;
  }
};

template <typename T>
asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan,
                               Page<T>& page) {
  asc::LapackWorkspace workspace;
  const auto count = plan.regions[support::kLayout].minimum_entries;
  if (count > 0) {
    if (static_cast<std::size_t>(count) > page.PageEntries()) {
      std::abort();
    }
    workspace.regions[support::kLayout] = {
        page.data(), static_cast<std::size_t>(count) * sizeof(T), kHost};
  }
  return workspace;
}

template <typename T>
void ObserveActive(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   const Matrix<T>& matrix,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace, Page<T>& page,
                   Page<T>& packing, Totals& totals) {
  using Real = asc::DenseBlasRealType<T>;
  asc::LapackReport report;
  packing.Protect(PROT_READ | PROT_WRITE);
  if (matrix.layout == kRow) {
    const auto child = fork();
    if (child == 0) {
      PrepareFaultChild();
      fault::OnEntry([] { _exit(96); });
      static_cast<void>(
          matrix.Run(provider, Real{1}, Real{2}, plan, workspace, report));
      _exit(97);
    }
    ExpectFault(test, child);
    ++totals.input_controls;
    page.Protect(PROT_READ | PROT_WRITE);
  } else {
    g_restore = page.Storage();
    fault::OnEntry(RestoreAtEntry);
  }
  ASC_DENSE_TEST_CHECK(
      test,
      matrix.Run(provider, Real{1}, Real{2}, plan, workspace, report).ok());
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_EQ(test, matrix.data[matrix.FirstDiagonal()], T{4});
  ++totals.active;
}

template <typename T>
void ObserveCase(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, char type,
                 asc::DenseBlasLayout layout, Extent m, Extent n,
                 Totals& totals) {
  using Real = asc::DenseBlasRealType<T>;
  Page<T> page;
  Page<T> packing;
  const bool band = type == 'B' || type == 'Q' || type == 'Z';
  const Extent lower = band && m > 1 ? 1 : 0;
  const Extent upper = band && n > 1 ? 1 : 0;
  Extent leading = std::max<Extent>(1, layout == kColumn ? m : n);
  if (type == 'Z') {
    leading = 2 * lower + upper + 1;
  } else if (band) {
    leading = lower + 1;
  }
  Matrix<T> matrix{page.data(), page.Storage(), type,    m,     n,
                   lower,       upper,          leading, layout};
  const auto plan = Take(matrix.Query(provider));
  const auto workspace = Workspace(plan, packing);
  asc::LapackReport report;
  page.Protect(PROT_NONE);
  packing.Protect(PROT_NONE);
  fault::Reset();
  ASC_DENSE_TEST_CHECK(test, matrix.Query(provider).ok());
  auto stale = plan;
  ++stale.regions[support::kLayout].minimum_entries;
  ASC_DENSE_TEST_CHECK(
      test,
      !matrix.Run(provider, Real{1}, Real{2}, stale, workspace, report).ok());
  ASC_DENSE_TEST_CHECK(
      test,
      !matrix.Run(provider, Real{0}, Real{2}, plan, workspace, report).ok());
  if (workspace.regions[support::kLayout].size() > 0) {
    auto short_workspace = workspace;
    auto& region = short_workspace.regions[support::kLayout];
    region = {region.data(), region.size() - 1, kHost};
    ASC_DENSE_TEST_CHECK(
        test,
        !matrix.Run(provider, Real{1}, Real{2}, plan, short_workspace, report)
             .ok());
  }
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  Negative(test, page.data(), totals);
  // Equal finite factors are a proven local identity even at nonzero order.
  ASC_DENSE_TEST_CHECK(
      test,
      matrix.Run(provider, Real{-3}, Real{-3}, plan, workspace, report).ok());
  ASC_DENSE_TEST_CHECK(test, !report.native_info);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  const bool active = m > 0 && n > 0;
  if (!active) {
    ASC_DENSE_TEST_CHECK(
        test,
        matrix.Run(provider, Real{1}, Real{2}, plan, workspace, report).ok());
    ASC_DENSE_TEST_CHECK(test, !report.native_info);
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
    ++totals.local;
    return;
  }
  ObserveActive(test, provider, matrix, plan, workspace, page, packing, totals);
}

struct UnusedShape {
  Extent m = 1;
  Extent n = 1;
  Extent lower = 0;
  Extent upper = 0;
  Extent leading = 2;
  std::size_t base = 0;
  std::size_t protected_page = 1;
  std::size_t protected_entry = 0;
};
UnusedShape MakeUnusedShape(char type, asc::DenseBlasLayout layout,
                            bool trailing, std::size_t size) {
  UnusedShape shape{.base = size - 1, .protected_entry = size};
  if (type == 'L' || type == 'U') {
    const bool horizontal = type == 'L';
    shape.m = horizontal ? 1 : 2;
    shape.n = horizontal ? 2 : 1;
    shape.leading = layout == kColumn ? shape.m : shape.n;
  } else if (type == 'H') {
    shape.m = 3;
    if (layout == kColumn) {
      shape.leading = 3;
      shape.base = size - 2;
    } else {
      shape.leading = static_cast<Extent>(size);
      shape.base = 0;
      shape.protected_page = 2;
      shape.protected_entry = 2 * size;
    }
  } else if (type == 'B' || type == 'Q') {
    shape.m = 2;
    shape.n = 2;
    shape.lower = 1;
    shape.upper = 1;
    const bool first =
        (type == 'Q' && layout == kColumn) || (type == 'B' && layout == kRow);
    if (first) {
      shape.protected_page = 0;
      shape.protected_entry = shape.base;
    } else {
      shape.base = size - 3;
    }
  } else if (type == 'Z') {
    shape.m = 2;
    shape.n = 2;
    shape.lower = 1;
    shape.upper = 1;
    shape.leading = 4;
    if (trailing) {
      shape.base = size - 7;
    } else {
      shape.protected_page = 0;
      shape.protected_entry = shape.base;
    }
  }
  return shape;
}

template <typename T>
void UnusedCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
                char type, asc::DenseBlasLayout layout, bool trailing,
                Totals& totals) {
  using Real = asc::DenseBlasRealType<T>;
  const bool h_row = type == 'H' && layout == kRow;
  Page<T> page(h_row ? 3 : 2);
  Page<T> packing;
  const auto size = page.PageEntries();
  const auto shape = MakeUnusedShape(type, layout, trailing, size);
  Matrix<T> matrix{page.data() + shape.base,
                   page.Storage(),
                   type,
                   shape.m,
                   shape.n,
                   shape.lower,
                   shape.upper,
                   shape.leading,
                   layout};
  const auto plan = Take(matrix.Query(provider));
  const auto workspace = Workspace(plan, packing);
  page.ProtectPage(shape.protected_page, PROT_NONE);
  fault::Reset();
  Negative(test, page.data() + shape.protected_entry, totals);
  ASC_DENSE_TEST_CHECK(test, matrix.Query(provider).ok());
  asc::LapackReport report;
  // The unused page stays protected across both ASC and real native execution.
  ASC_DENSE_TEST_CHECK(
      test,
      matrix.Run(provider, Real{1}, Real{2}, plan, workspace, report).ok());
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_EQ(test, matrix.data[matrix.FirstDiagonal()], T{4});
  page.Protect(PROT_READ | PROT_WRITE);
  ASC_DENSE_TEST_EQ(test, page.data()[shape.protected_entry], T{2});
  ++totals.unused;
}

template <typename T>
void Observe(TestContext& test, const asc::ReferenceLapackProvider& provider,
             Totals& totals) {
  for (const auto layout : {kColumn, kRow}) {
    for (const auto type : {'G', 'L', 'U', 'H', 'B', 'Q', 'Z'}) {
      if (type == 'Z' && layout == kRow) {
        continue;
      }
      if (type == 'B' || type == 'Q') {
        ObserveCase<T>(test, provider, type, layout, 0, 0, totals);
        ObserveCase<T>(test, provider, type, layout, 3, 3, totals);
      } else {
        for (const auto m : {0, 3}) {
          for (const auto n : {0, 2}) {
            ObserveCase<T>(test, provider, type, layout, m, n, totals);
          }
        }
      }
      UnusedCase<T>(test, provider, type, layout, false, totals);
      if (type == 'Z') {
        UnusedCase<T>(test, provider, type, layout, true, totals);
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  TestContext test;
  Totals totals;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Observe<float>(test, provider, totals);
  Observe<double>(test, provider, totals);
  Observe<std::complex<float>>(test, provider, totals);
  Observe<std::complex<double>>(test, provider, totals);
  ASC_DENSE_TEST_EQ(test, totals.negative, 232U);
  ASC_DENSE_TEST_EQ(test, totals.input_controls, 24U);
  ASC_DENSE_TEST_EQ(test, totals.active, 52U);
  ASC_DENSE_TEST_EQ(test, totals.local, 124U);
  ASC_DENSE_TEST_EQ(test, totals.unused, 56U);
  std::printf(
      "LASCL observation: negative=%zu input=%zu active=%zu local=%zu "
      "unused=%zu; provider code uninstrumented\n",
      totals.negative, totals.input_controls, totals.active, totals.local,
      totals.unused);
  return test.Finish();
}
