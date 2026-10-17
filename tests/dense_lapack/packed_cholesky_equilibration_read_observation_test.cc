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
#include "asc/dense/providers/lapack_cholesky_packed_equilibration.h"
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
           asc::DenseBlasVectorView<asc::DenseBlasRealType<T>> scales,
           Mapping<asc::DenseBlasRealType<T>>& statistics,
           const asc::LapackWorkspacePlan& plan) {
  if (!statistics.Restore()) {
    return;
  }
  asc::LapackReport report;
  asc_dense_test::AllocationProbe allocations;
  const auto status =
      asc::Ppequ(provider, triangle, a, scales, statistics.data()[1],
                 statistics.data()[2], plan, {}, report);
  const auto count = allocations.count();
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_CHECK(
      test, report.outcome == asc::LapackOutcome::kSuccess &&
                report.output_validity == asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, count, 0U);
  ASC_DENSE_TEST_EQ(test, statistics.data()[1], 1);
  ASC_DENSE_TEST_EQ(test, statistics.data()[2], 0);
  if (!statistics.Protect(0, statistics.page_count())) {
    return;
  }
  auto stale = plan;
  ++stale.regions[kLayout].minimum_entries;
  ++stale.regions[kLayout].preferred_entries;
  const auto rejected =
      asc::Ppequ(provider, triangle, a, scales, statistics.data()[1],
                 statistics.data()[2], stale, {}, report);
  const auto rejected_count = allocations.count();
  ASC_DENSE_TEST_EQ(test, rejected_count, 0U);
  ASC_DENSE_TEST_CHECK(test, rejected.code() == asc::ErrorCode::kInvalidState &&
                                 !report.called_provider &&
                                 !report.native_info);
}

template <typename T>
void Query(TestContext& test, const asc::ReferenceLapackProvider& provider,
           asc::extent_t n, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  Mapping<T> a(test);
  Mapping<Real> scales(test);
  Mapping<Real> statistics(test);
  if (!a.Initialize() || !scales.Initialize() || !statistics.Initialize()) {
    return;
  }
  const auto ap = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      a.data() + 1, n, layout, {a.data(), a.bytes(), kHost}));
  const auto sv = Take(asc::DenseBlasVectorView<Real>::Create(
      scales.data() + 1, n, 1, {scales.data(), scales.bytes(), kHost}));
  if (!a.Protect(0, a.page_count()) ||
      !scales.Protect(0, scales.page_count()) ||
      !statistics.Protect(0, statistics.page_count())) {
    return;
  }
  asc_dense_test::AllocationProbe allocations;
  const auto query = asc::QueryPpequWorkspace(
      provider, triangle, ap, sv, statistics.data()[1], statistics.data()[2]);
  const auto count = allocations.count();
  ASC_DENSE_TEST_CHECK(test, query.ok());
  ASC_DENSE_TEST_EQ(test, count, 0U);
  const auto plan = Take(query);
  for (const auto& region : plan.regions) {
    ASC_DENSE_TEST_CHECK(
        test, region.minimum_entries == 0 && region.preferred_entries == 0);
  }
  if (n == 0) {
    Local(test, provider, triangle, ap, sv, statistics, plan);
  }
  if (!a.Restore() || !scales.Restore() || !statistics.Restore()) {
    return;
  }
  ASC_DENSE_TEST_CHECK(test, std::all_of(a.data(), a.data() + a.count(),
                                         [](T v) { return v == T{19}; }));
  ASC_DENSE_TEST_CHECK(
      test, std::all_of(scales.data(), scales.data() + scales.count(),
                        [](Real v) { return v == Real{19}; }));
  for (std::size_t i = 0; i < statistics.count(); ++i) {
    Real expected{19};
    if (n == 0 && (i == 1 || i == 2)) {
      expected = i == 1 ? Real{1} : Real{};
    }
    ASC_DENSE_TEST_EQ(test, statistics.data()[i], expected);
  }
}

// Sum the stored major blocks independently. Upper-column/lower-row blocks
// end at their diagonal; lower-column/upper-row blocks start at it.
template <typename Visit>
void Diagonals(asc::extent_t n, bool end, const Visit& visit) {
  std::size_t cursor = 1;
  for (asc::extent_t major = 0; major < n; ++major) {
    const auto length = static_cast<std::size_t>(end ? major + 1 : n - major);
    visit(end ? cursor + length - 1 : cursor);
    cursor += length;
  }
}

template <typename T>
bool ProtectGap(TestContext& test, Mapping<T>& a, asc::extent_t n, bool end) {
  std::size_t preceding = 0;
  std::size_t following = 0;
  std::size_t previous = 0;
  Diagonals(n, end, [&](std::size_t offset) {
    a.data()[offset] = T{4};
    if (previous != 0 && offset - previous >= static_cast<std::size_t>(n)) {
      preceding = previous;
      following = offset;
    }
    previous = offset;
  });
  const auto page = (preceding + 1 + a.page_entries() - 1) / a.page_entries();
  const bool inside = preceding != 0 && page * a.page_entries() > preceding &&
                      (page + 1) * a.page_entries() <= following;
  ASC_DENSE_TEST_CHECK(test, inside);
  return inside && a.Protect(page, 1);
}

template <typename T>
void OffDiagonal(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr asc::extent_t kOrder = 4097;
  constexpr std::size_t kEntries =
      static_cast<std::size_t>(kOrder * (kOrder + 1) / 2);
  Mapping<T> a(test);
  Mapping<Real> scales(test);
  if (!a.Initialize(kEntries + 2) ||
      !scales.Initialize(static_cast<std::size_t>(kOrder) + 2)) {
    return;
  }
  const bool end = (triangle == kUpper) == (layout == kColumn);
  if (!ProtectGap(test, a, kOrder, end)) {
    return;
  }
  const auto ap = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      a.data() + 1, kOrder, layout, {a.data(), a.bytes(), kHost}));
  const auto sv = Take(asc::DenseBlasVectorView<Real>::Create(
      scales.data() + 1, kOrder, 1, {scales.data(), scales.bytes(), kHost}));
  std::array<Real, 3> condition{Real{19}, Real{19}, Real{19}};
  std::array<Real, 3> maximum = condition;
  const auto plan = Take(asc::QueryPpequWorkspace(provider, triangle, ap, sv,
                                                  condition[1], maximum[1]));
  asc::LapackReport report;
  asc_dense_test::AllocationProbe allocations;
  const auto status = asc::Ppequ(provider, triangle, ap, sv, condition[1],
                                 maximum[1], plan, {}, report);
  const auto count = allocations.count();
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test, count, 0U);
  ASC_DENSE_TEST_CHECK(test, condition[1] == 1 && maximum[1] == 4 &&
                                 condition.front() == 19 &&
                                 condition.back() == 19 &&
                                 maximum.front() == 19 && maximum.back() == 19);
  ASC_DENSE_TEST_CHECK(test,
                       std::all_of(sv.data(), sv.data() + kOrder,
                                   [](Real v) { return v == Real{0.5}; }));
  ASC_DENSE_TEST_CHECK(test,
                       scales.data()[0] == 19 &&
                           std::all_of(scales.data() + kOrder + 1,
                                       scales.data() + scales.count(),
                                       [](Real v) { return v == Real{19}; }));
  if (!a.Restore()) {
    return;
  }
  // Replace just the independently enumerated diagonal values with the original
  // guard value, then inspect every object, including the formerly protected
  // page.
  Diagonals(kOrder, end, [&](std::size_t offset) {
    ASC_DENSE_TEST_CHECK(test, a.data()[offset] == T{4});
    a.data()[offset] = T{19};
  });
  ASC_DENSE_TEST_CHECK(test, std::all_of(a.data(), a.data() + a.count(),
                                         [](T v) { return v == T{19}; }));
}

template <typename T>
class ImaginaryMapping {
 public:
  explicit ImaginaryMapping(TestContext& test) : test_(test) {}
  ImaginaryMapping(const ImaginaryMapping&) = delete;
  ImaginaryMapping& operator=(const ImaginaryMapping&) = delete;
  ImaginaryMapping(ImaginaryMapping&&) = delete;
  ImaginaryMapping& operator=(ImaginaryMapping&&) = delete;
  ~ImaginaryMapping() {
    if (memory_ != nullptr) {
      if (Restore()) {
        std::destroy_n(value_, 1);
      }
      ASC_DENSE_TEST_EQ(test_, munmap(memory_, 2 * page_), 0);
    }
  }
  bool Initialize() {
    using Real = asc::DenseBlasRealType<T>;
    static_assert(asc::DenseBlasComplex<T>);
    static_assert(sizeof(T) == 2 * sizeof(Real) && alignof(T) == alignof(Real));
    const auto page = sysconf(_SC_PAGESIZE);
    ASC_DENSE_TEST_CHECK(test_, page > 0);
    if (page <= 0) {
      return false;
    }
    page_ = static_cast<std::size_t>(page);
    void* memory = mmap(nullptr, 2 * page_, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASC_DENSE_TEST_CHECK(test_, memory != MAP_FAILED);
    if (memory == MAP_FAILED) {
      return false;
    }
    // One actual containing T array crosses the page boundary: real is in the
    // first page, imaginary in the second. The checked backing is exactly T[1].
    memory_ = ::new (memory) std::byte[2 * page_];
    value_ = ::new (memory_ + page_ - sizeof(Real)) T[1];
    value_[0] = T{4, 29};
    const auto code = mprotect(memory_ + page_, page_, PROT_NONE);
    ASC_DENSE_TEST_EQ(test_, code, 0);
    return code == 0;
  }
  bool Restore() {
    const auto code = mprotect(memory_, 2 * page_, PROT_READ | PROT_WRITE);
    ASC_DENSE_TEST_EQ(test_, code, 0);
    return code == 0;
  }
  T* data() { return value_; }

 private:
  TestContext& test_;
  std::byte* memory_ = nullptr;
  T* value_ = nullptr;
  std::size_t page_ = 0;
};

template <typename T>
void Imaginary(TestContext& test, const asc::ReferenceLapackProvider& provider,
               asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  ImaginaryMapping<T> a(test);
  if (!a.Initialize()) {
    return;
  }
  const auto ap = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      a.data(), 1, layout, {a.data(), sizeof(T), kHost}));
  std::array<Real, 3> scales{Real{19}, Real{19}, Real{19}};
  std::array<Real, 3> condition = scales;
  std::array<Real, 3> maximum = scales;
  const auto sv = Take(asc::DenseBlasVectorView<Real>::Create(
      scales.data() + 1, 1, 1, {scales.data(), sizeof(scales), kHost}));
  const auto plan = Take(asc::QueryPpequWorkspace(provider, triangle, ap, sv,
                                                  condition[1], maximum[1]));
  asc::LapackReport report;
  asc_dense_test::AllocationProbe allocations;
  const auto status = asc::Ppequ(provider, triangle, ap, sv, condition[1],
                                 maximum[1], plan, {}, report);
  const auto count = allocations.count();
  ASC_DENSE_TEST_EQ(test, count, 0U);
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_CHECK(
      test, scales[1] == Real{0.5} && condition[1] == 1 && maximum[1] == 4);
  ASC_DENSE_TEST_CHECK(test, scales.front() == 19 && scales.back() == 19 &&
                                 condition.front() == 19 &&
                                 condition.back() == 19 &&
                                 maximum.front() == 19 && maximum.back() == 19);
  if (a.Restore()) {
    ASC_DENSE_TEST_CHECK(test, a.data()[0] == T(4, 29));
  }
}

template <typename T>
void Scalar(TestContext& test, const asc::ReferenceLapackProvider& provider,
            std::size_t& profiles) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kRow, kColumn}) {
      for (const asc::extent_t n : {0, 1, 5}) {
        Query<T>(test, provider, n, triangle, layout);
        ++profiles;
      }
      OffDiagonal<T>(test, provider, triangle, layout);
      ++profiles;
      if constexpr (asc::DenseBlasComplex<T>) {
        Imaginary<T>(test, provider, triangle, layout);
        ++profiles;
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
  ASC_DENSE_TEST_EQ(test, profiles, 72U);
  std::printf(
      "Packed Cholesky equilibration protected-memory profiles: %zu;48 "
      "queries,16 local executions/16 empty stale-plan rejections,16 native "
      "ignored-offdiagonal-page and8 ignored-imaginary-diagonal calls\n",
      profiles);
  return test.Finish();
}
