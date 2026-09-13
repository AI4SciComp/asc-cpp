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
#include "asc/dense/providers/lapack_cholesky_packed_condition.h"
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
  bool Initialize(std::size_t entries = 0) {
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
    const bool fits =
        entries <= (std::numeric_limits<std::size_t>::max() - size) / sizeof(T);
    ASC_DENSE_TEST_CHECK(test_, fits);
    if (!fits) {
      return false;
    }
    pages_ = std::max<std::size_t>(5, (entries * sizeof(T) + size - 1) / size);
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
  [[nodiscard]] std::size_t bytes() const { return page_bytes_ * pages_; }
  [[nodiscard]] std::size_t count() const { return bytes() / sizeof(T); }
  [[nodiscard]] std::size_t page_count() const { return pages_; }
  [[nodiscard]] std::size_t page_bytes() const { return page_bytes_; }
  [[nodiscard]] std::size_t page_entries() const {
    return page_bytes_ / sizeof(T);
  }

 private:
  TestContext& test_;
  T* data_ = nullptr;
  std::size_t page_bytes_ = 0;
  std::size_t pages_ = 0;
};

template <typename T>
void Local(TestContext& test, const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle,
           asc::DenseBlasPackedMatrixView<const T> a,
           asc::DenseBlasRealType<T> norm,
           Mapping<asc::DenseBlasRealType<T>>& condition, Mapping<T>& scratch,
           const asc::LapackWorkspacePlan& plan) {
  if (!condition.Restore()) {
    return;
  }
  asc::LapackWorkspace workspace;
  // A real, disjoint optional workspace remains inaccessible in a local call.
  workspace.regions[kLayout] = {scratch.data(), scratch.bytes(), kHost};
  asc::LapackReport report;
  asc_dense_test::AllocationProbe allocations;
  const auto status = asc::Ppcon(provider, triangle, a, norm,
                                 condition.data()[1], plan, workspace, report);
  const auto count = allocations.count();
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_CHECK(
      test, report.outcome == asc::LapackOutcome::kSuccess &&
                report.output_validity == asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, count, 0U);
  ASC_DENSE_TEST_EQ(test, condition.data()[1], a.order() == 0 ? 1 : 0);
}

template <typename T>
void Query(TestContext& test, const asc::ReferenceLapackProvider& provider,
           asc::extent_t n, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout, bool zero_norm) {
  using Real = asc::DenseBlasRealType<T>;
  Mapping<T> a(test);
  Mapping<Real> condition(test);
  Mapping<T> scratch(test);
  if (!a.Initialize() || !condition.Initialize() || !scratch.Initialize()) {
    return;
  }
  const auto ap = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      a.data() + 1, n, layout, {a.data(), a.bytes(), kHost}));
  if (!a.Protect(0, a.page_count()) ||
      !condition.Protect(0, condition.page_count()) ||
      !scratch.Protect(0, scratch.page_count())) {
    return;
  }
  const Real norm = zero_norm ? 0 : 32;
  asc_dense_test::AllocationProbe allocations;
  const auto query = asc::QueryPpconWorkspace(provider, triangle, ap, norm,
                                              condition.data()[1]);
  const auto count = allocations.count();
  ASC_DENSE_TEST_CHECK(test, query.ok());
  ASC_DENSE_TEST_EQ(test, count, 0U);
  const auto plan = Take(query);
  auto stale = plan;
  ++stale.regions[kLayout].minimum_entries;
  ++stale.regions[kLayout].preferred_entries;
  asc::LapackReport report;
  const auto rejected = asc::Ppcon(provider, triangle, ap, norm,
                                   condition.data()[1], stale, {}, report);
  ASC_DENSE_TEST_CHECK(test, rejected.code() == asc::ErrorCode::kInvalidState &&
                                 !report.called_provider &&
                                 !report.native_info);
  for (const Real invalid : {-Real{1}, std::numeric_limits<Real>::quiet_NaN(),
                             std::numeric_limits<Real>::infinity()}) {
    const auto bad = asc::Ppcon(provider, triangle, ap, invalid,
                                condition.data()[1], plan, {}, report);
    ASC_DENSE_TEST_CHECK(test, bad.code() == asc::ErrorCode::kInvalidArgument &&
                                   !report.called_provider &&
                                   !report.native_info);
  }
  if (n == 0 || zero_norm) {
    for (const auto& region : plan.regions) {
      ASC_DENSE_TEST_CHECK(
          test, region.minimum_entries == 0 && region.preferred_entries == 0);
    }
    Local(test, provider, triangle, ap, norm, condition, scratch, plan);
  }
  const auto final_count = allocations.count();
  ASC_DENSE_TEST_EQ(test, final_count, 0U);
  if (!a.Restore() || !condition.Restore() || !scratch.Restore()) {
    return;
  }
  ASC_DENSE_TEST_CHECK(test, std::all_of(a.data(), a.data() + a.count(),
                                         [](T v) { return v == T{19}; }));
  ASC_DENSE_TEST_CHECK(
      test, std::all_of(scratch.data(), scratch.data() + scratch.count(),
                        [](T v) { return v == T{19}; }));
  for (std::size_t i = 0; i < condition.count(); ++i) {
    Real expected{19};
    if (i == 1 && (n == 0 || zero_norm)) {
      expected = n == 0 ? Real{1} : Real{};
    }
    ASC_DENSE_TEST_EQ(test, condition.data()[i], expected);
  }
}

template <typename Visit>
void Slots(asc::extent_t n, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout, const Visit& visit) {
  std::size_t cursor = 0;
  for (asc::extent_t major = 0; major < n; ++major) {
    for (asc::extent_t minor = 0; minor < n; ++minor) {
      const auto row = layout == kColumn ? minor : major;
      const auto column = layout == kColumn ? major : minor;
      if (triangle == kUpper ? row <= column : row >= column) {
        visit(cursor++, row == column);
      }
    }
  }
}

template <typename T>
void Boundaries(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::extent_t n, asc::DenseBlasTriangle triangle,
                asc::DenseBlasLayout layout, bool at_end) {
  using Real = asc::DenseBlasRealType<T>;
  Mapping<T> a(test);
  if (!a.Initialize()) {
    return;
  }
  const auto packed = static_cast<std::size_t>(n * (n + 1) / 2);
  const auto offset =
      at_end ? 3 * a.page_entries() - packed : 2 * a.page_entries();
  T* factor = a.data() + offset;
  Slots(n, triangle, layout, [&](std::size_t index, bool diagonal) {
    factor[index] = diagonal ? T{2} : T{};
  });
  const auto ap = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      factor, n, layout, {a.data(), a.bytes(), kHost}));
  if (!a.Protect(0, 2) || !a.Protect(3, 2)) {
    return;
  }
  std::array<Real, 3> condition{Real{19}, Real{19}, Real{19}};
  const auto plan = Take(
      asc::QueryPpconWorkspace(provider, triangle, ap, Real{4}, condition[1]));
  installed_internal::Scratch<T> scratch;
  std::array<Real, 8> real{};
  constexpr auto kReal =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
  scratch.workspace.regions[kReal] = {real.data(), sizeof(real), kHost};
  asc::LapackReport report;
  asc_dense_test::AllocationProbe allocations;
  const auto status = asc::Ppcon(provider, triangle, ap, Real{4}, condition[1],
                                 plan, scratch.workspace, report);
  const auto count = allocations.count();
  ASC_DENSE_TEST_EQ(test, count, 0U);
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_CHECK(test, condition[1] == 1 && condition.front() == 19 &&
                                 condition.back() == 19);
  if (!a.Restore()) {
    return;
  }
  Slots(n, triangle, layout, [&](std::size_t index, bool diagonal) {
    ASC_DENSE_TEST_CHECK(test, factor[index] == (diagonal ? T{2} : T{}));
    factor[index] = T{19};
  });
  ASC_DENSE_TEST_CHECK(test, std::all_of(a.data(), a.data() + a.count(),
                                         [](T v) { return v == T{19}; }));
}

template <typename T>
void Scalar(TestContext& test, const asc::ReferenceLapackProvider& provider,
            std::size_t& profiles) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kRow, kColumn}) {
      for (const asc::extent_t n : {0, 1, 5}) {
        for (const bool zero_norm : {false, true}) {
          Query<T>(test, provider, n, triangle, layout, zero_norm);
          ++profiles;
        }
      }
      for (const asc::extent_t n : {1, 2, 5}) {
        for (const bool at_end : {false, true}) {
          Boundaries<T>(test, provider, n, triangle, layout, at_end);
          ++profiles;
        }
      }
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard guard;
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
      "Packed Cholesky condition protected-memory profiles: %zu;96 queries,64 "
      "local calls,96 stale-plan and288 invalid-norm rejections,96 native "
      "boundary calls\n",
      profiles);
  return test.Finish();
}
