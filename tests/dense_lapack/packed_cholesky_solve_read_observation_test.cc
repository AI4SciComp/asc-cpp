#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.

#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_solve.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
#include "test_support.h"

namespace {
using asc_dense_test::TestContext;
using installed_internal::Take;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

// Linux-only observation: a real array, full backing and actual page access
// protection. Standard non-allocating placement array new establishes the
// containing array lifetime in C++20; no scalar-pointer capacity is invented.
template <typename T>
class Mapping {
 public:
  explicit Mapping(TestContext& test) : test_(test) {}
  Mapping(const Mapping&) = delete;
  Mapping& operator=(const Mapping&) = delete;
  Mapping(Mapping&&) = delete;
  Mapping& operator=(Mapping&&) = delete;
  ~Mapping() {
    if (data_ != nullptr) {
      if (Restore()) {
        std::destroy_n(data_, count());
      }
      ASC_DENSE_TEST_EQ(test_, munmap(data_, bytes()), 0);
    }
  }
  bool Initialize() {
    const auto page = sysconf(_SC_PAGESIZE);
    ASC_DENSE_TEST_CHECK(test_, page > 0);
    if (page <= 0) {
      return false;
    }
    const auto size = static_cast<std::size_t>(page);
    const bool valid = size % sizeof(T) == 0 && size / sizeof(T) > 4 &&
                       size <= std::numeric_limits<std::size_t>::max() / 5 &&
                       size <= static_cast<std::size_t>(
                                   std::numeric_limits<asc::extent_t>::max()) /
                                   10;
    ASC_DENSE_TEST_CHECK(test_, valid);
    if (!valid) {
      return false;
    }
    page_bytes_ = size;
    void* memory = mmap(nullptr, bytes(), PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASC_DENSE_TEST_CHECK(test_, memory != MAP_FAILED);
    if (memory == MAP_FAILED) {
      return false;
    }
    data_ = ::new (memory) T[count()];
    std::fill_n(data_, count(), T{19});
    return true;
  }
  bool Protect(std::size_t first_page, std::size_t pages) {
    const int code = mprotect(data_ + first_page * page_entries(),
                              pages * page_bytes_, PROT_NONE);
    ASC_DENSE_TEST_EQ(test_, code, 0);
    return code == 0;
  }
  bool Restore() {
    const int code = mprotect(data_, bytes(), PROT_READ | PROT_WRITE);
    ASC_DENSE_TEST_EQ(test_, code, 0);
    return code == 0;
  }
  T* data() { return data_; }
  [[nodiscard]] std::size_t bytes() const { return page_bytes_ * 5; }
  [[nodiscard]] std::size_t count() const { return bytes() / sizeof(T); }
  [[nodiscard]] std::size_t page_entries() const {
    return page_bytes_ / sizeof(T);
  }

 private:
  TestContext& test_;
  T* data_ = nullptr;
  std::size_t page_bytes_ = 0;
};

struct Profile {
  asc::extent_t n;
  asc::extent_t nrhs;
};

template <typename T>
void Observe(TestContext& test, const asc::ReferenceLapackProvider& provider,
             const Profile& p, asc::DenseBlasTriangle triangle,
             asc::DenseBlasLayout a_layout, asc::DenseBlasLayout b_layout) {
  Mapping<T> a(test);
  Mapping<T> b(test);
  if (!a.Initialize() || !b.Initialize()) {
    return;
  }
  const auto ap = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      a.data() + 1, p.n, a_layout, {a.data(), a.bytes(), kHost}));
  const auto rhs = Take(asc::DenseBlasMatrixView<T>::Create(
      b.data() + 1, p.n, p.nrhs, b_layout,
      (b_layout == kRow ? p.nrhs : p.n) + 2, {b.data(), b.bytes(), kHost}));
  if (!a.Protect(0, 5) || !b.Protect(0, 5)) {
    return;
  }
  // Both complete containing arrays are protected, including every logical
  // element and every guard. Any numeric query read or write faults here.
  asc_dense_test::AllocationProbe allocations;
  const auto query = asc::QueryPptrsWorkspace(provider, triangle, ap, rhs);
  const auto query_allocations = allocations.count();
  ASC_DENSE_TEST_CHECK(test, query.ok());
  ASC_DENSE_TEST_EQ(test, query_allocations, 0U);
  const auto plan = Take(query);
  const bool local = p.n == 0 || p.nrhs == 0;
  const asc::extent_t expected =
      local ? 0
            : (a_layout == kRow ? p.n * (p.n + 1) / 2 : 0) +
                  (b_layout == kRow ? p.n * p.nrhs : 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries, expected);
  if (local) {
    const asc::LapackWorkspace workspace;
    asc::LapackReport report;
    const auto status =
        asc::Pptrs(provider, triangle, ap, rhs, plan, workspace, report);
    const auto call_allocations = allocations.count();
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, call_allocations, 0U);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_CHECK(test, report.outcome == asc::LapackOutcome::kSuccess &&
                                   report.output_validity ==
                                       asc::LapackOutputValidity::kComplete);
  }
  if (!a.Restore() || !b.Restore()) {
    return;
  }
  ASC_DENSE_TEST_CHECK(test,
                       std::all_of(a.data(), a.data() + a.count(),
                                   [](T value) { return value == T{19}; }));
  ASC_DENSE_TEST_CHECK(test,
                       std::all_of(b.data(), b.data() + b.count(),
                                   [](T value) { return value == T{19}; }));
}

template <typename T>
void Scalar(TestContext& test, const asc::ReferenceLapackProvider& provider,
            std::size_t& profiles) {
  for (const auto p : {Profile{0, 0}, Profile{0, 3}, Profile{1, 0},
                       Profile{1, 2}, Profile{5, 0}, Profile{5, 3}}) {
    for (auto triangle : {kUpper, kLower}) {
      for (auto a_layout : {kColumn, kRow}) {
        for (auto b_layout : {kColumn, kRow}) {
          Observe<T>(test, provider, p, triangle, a_layout, b_layout);
          ++profiles;
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
  std::size_t profiles = 0;
  Scalar<float>(test, provider, profiles);
  Scalar<double>(test, provider, profiles);
  Scalar<std::complex<float>>(test, provider, profiles);
  Scalar<std::complex<double>>(test, provider, profiles);
  ASC_DENSE_TEST_EQ(test, profiles, 192U);
  std::printf(
      "Packed Cholesky solve protected-memory profiles: %zu; 192 queries "
      "and128 local executions\n",
      profiles);
  return test.Finish();
}
