#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <array>
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
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_driver.h"
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
  const auto ap = Take(asc::DenseBlasPackedMatrixView<T>::Create(
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
  const auto query = asc::QueryPpsvWorkspace(provider, triangle, ap, rhs);
  const auto query_allocations = allocations.count();
  ASC_DENSE_TEST_CHECK(test, query.ok());
  ASC_DENSE_TEST_EQ(test, query_allocations, 0U);
  const auto plan = Take(query);
  const bool local = p.n == 0;
  const asc::extent_t expected =
      local ? 0
            : (a_layout == kRow ? p.n * (p.n + 1) / 2 : 0) +
                  (b_layout == kRow ? p.n * p.nrhs : 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries, expected);
  if (local) {
    const asc::LapackWorkspace workspace;
    asc::LapackReport report;
    const auto status =
        asc::Ppsv(provider, triangle, ap, rhs, plan, workspace, report);
    const auto call_allocations = allocations.count();
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, call_allocations, 0U);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_CHECK(test, report.outcome == asc::LapackOutcome::kSuccess &&
                                   report.output_validity ==
                                       asc::LapackOutputValidity::kComplete);
  }
  if (local) {
    auto stale = plan;
    ++stale.regions[kLayout].minimum_entries;
    ++stale.regions[kLayout].preferred_entries;
    const asc::LapackWorkspace workspace;
    asc::LapackReport report;
    const auto status =
        asc::Ppsv(provider, triangle, ap, rhs, stale, workspace, report);
    const auto stale_allocations = allocations.count();
    ASC_DENSE_TEST_EQ(test, stale_allocations, 0U);
    ASC_DENSE_TEST_CHECK(test, status.code() == asc::ErrorCode::kInvalidState &&
                                   !report.called_provider &&
                                   !report.native_info);
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

// Enumerate physical slots independently; no production offset helper.
template <typename Visit>
void Slots(asc::extent_t n, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout, const Visit& visit) {
  std::size_t slot = 1;
  for (asc::extent_t major = 0; major < n; ++major) {
    for (asc::extent_t minor = 0; minor < n; ++minor) {
      const auto i = layout == kColumn ? minor : major;
      const auto j = layout == kColumn ? major : minor;
      if (triangle == kUpper ? i <= j : i >= j) {
        visit(slot++, i, j);
      }
    }
  }
}

template <typename T>
T ExpectedFactor(asc::extent_t i, asc::extent_t j, asc::extent_t pivot) {
  if (i != j) {
    return T{};
  }
  if (pivot < 0 || i < pivot) {
    return T{2};
  }
  return i == pivot ? T{-1} : T{4};
}

template <typename T>
void NoRhsAccess(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, asc::extent_t n,
                 asc::extent_t nrhs, asc::DenseBlasTriangle triangle,
                 asc::DenseBlasLayout a_layout, asc::DenseBlasLayout b_layout,
                 asc::extent_t negative_pivot) {
  Mapping<T> a(test);
  Mapping<T> b(test);
  if (!a.Initialize() || !b.Initialize()) {
    return;
  }
  Slots(n, triangle, a_layout, [&](std::size_t slot, auto i, auto j) {
    a.data()[slot] = T{};
    if (i == j) {
      a.data()[slot] = i == negative_pivot ? T{-1} : T{4};
    }
  });
  const auto ap = Take(asc::DenseBlasPackedMatrixView<T>::Create(
      a.data() + 1, n, a_layout, {a.data(), a.bytes(), kHost}));
  const auto rhs = Take(asc::DenseBlasMatrixView<T>::Create(
      b.data() + 1, n, nrhs, b_layout, (b_layout == kRow ? nrhs : n) + 2,
      {b.data(), b.bytes(), kHost}));
  if (!b.Protect(0, 5)) {
    return;
  }
  const auto plan = Take(asc::QueryPpsvWorkspace(provider, triangle, ap, rhs));
  std::array<T, 64> packing;
  packing.fill(T{-97});
  const auto count = plan.regions[kLayout].minimum_entries;
  ASC_DENSE_TEST_CHECK(test, count >= 0 && count < 63);
  asc::LapackWorkspace workspace;
  if (count > 0) {
    workspace.regions[kLayout] = {
        packing.data() + 1, static_cast<std::size_t>(count) * sizeof(T), kHost};
  }
  asc::LapackReport report;
  asc_dense_test::AllocationProbe allocations;
  const auto status =
      asc::Ppsv(provider, triangle, ap, rhs, plan, workspace, report);
  const auto observed = allocations.count();
  ASC_DENSE_TEST_EQ(test, observed, 0U);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  if (negative_pivot < 0) {
    ASC_DENSE_TEST_CHECK(test,
                         status.ok() && report.native_info == 0 &&
                             report.outcome == asc::LapackOutcome::kSuccess);
  } else {
    ASC_DENSE_TEST_CHECK(
        test, status.code() == asc::ErrorCode::kNumerical &&
                  report.native_info == negative_pivot + 1 &&
                  report.diagnostic_index == negative_pivot &&
                  report.outcome == asc::LapackOutcome::kNotPositiveDefinite);
  }
  Slots(n, triangle, a_layout, [&](std::size_t slot, auto i, auto j) {
    const auto expected = ExpectedFactor<T>(i, j, negative_pivot);
    ASC_DENSE_TEST_CHECK(test, a.data()[slot] == expected);
  });
  const auto entries = static_cast<std::size_t>(n * (n + 1) / 2);
  ASC_DENSE_TEST_CHECK(
      test, a.data()[0] == T{19} &&
                std::all_of(a.data() + entries + 1, a.data() + a.count(),
                            [](T value) { return value == T{19}; }));
  ASC_DENSE_TEST_CHECK(
      test, packing.front() == T{-97} &&
                std::all_of(packing.begin() + 1 + count, packing.end(),
                            [](T value) { return value == T{-97}; }));
  if (!b.Restore()) {
    return;
  }
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
  for (const asc::extent_t n : {1, 5}) {
    for (auto triangle : {kUpper, kLower}) {
      for (auto a_layout : {kColumn, kRow}) {
        for (auto b_layout : {kColumn, kRow}) {
          NoRhsAccess<T>(test, provider, n, 0, triangle, a_layout, b_layout,
                         -1);
          ++profiles;
        }
        // Native PPSV never reaches B after a failed factorization. Nonempty
        // row B requires an explicit pre-entry copy and is excluded here.
        for (const asc::extent_t nrhs : {1, 3}) {
          NoRhsAccess<T>(test, provider, n, nrhs, triangle, a_layout, kColumn,
                         0);
          ++profiles;
          if (n > 1) {
            NoRhsAccess<T>(test, provider, n, nrhs, triangle, a_layout, kColumn,
                           n - 1);
            ++profiles;
          }
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
  ASC_DENSE_TEST_EQ(test, profiles, 352U);
  std::printf(
      "Packed Cholesky driver protected-memory profiles: %zu; 192 queries "
      "with64 local executions/64 stale-plan rejections;64 zero-RHS "
      "factorizations "
      "and96 nonpositive native calls with protected B\n",
      profiles);
  return test.Finish();
}
