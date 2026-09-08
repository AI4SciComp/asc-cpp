#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.
#include <span>

#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_refinement.h"
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

struct Profile {
  asc::extent_t n;
  asc::extent_t nrhs;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout a_layout;
  asc::DenseBlasLayout af_layout;
  asc::DenseBlasLayout b_layout;
  asc::DenseBlasLayout x_layout;
};

template <typename T>
struct Operands {
  using Real = asc::DenseBlasRealType<T>;
  asc::DenseBlasPackedMatrixView<const T> a;
  asc::DenseBlasPackedMatrixView<const T> af;
  asc::DenseBlasMatrixView<const T> b;
  asc::DenseBlasMatrixView<T> x;
  asc::DenseBlasVectorView<Real> f;
  asc::DenseBlasVectorView<Real> e;
};

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider, const Profile& p,
           const Operands<T>& v) {
  return asc::QueryPprfsWorkspace(provider, p.triangle, v.a, v.af, v.b, v.x,
                                  v.f, v.e);
}

template <typename T>
auto Call(const asc::ReferenceLapackProvider& provider, const Profile& p,
          const Operands<T>& v, const asc::LapackWorkspacePlan& plan,
          const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  return asc::Pprfs(provider, p.triangle, v.a, v.af, v.b, v.x, v.f, v.e, plan,
                    workspace, report);
}

template <typename T>
void Unchanged(TestContext& test, Mapping<T>& map) {
  ASC_DENSE_TEST_CHECK(test,
                       std::all_of(map.data(), map.data() + map.count(),
                                   [](T value) { return value == T{19}; }));
}

template <typename T>
struct QueryStorage {
  using Real = asc::DenseBlasRealType<T>;
  Mapping<T> a;
  Mapping<T> af;
  Mapping<T> b;
  Mapping<T> x;
  Mapping<Real> f;
  Mapping<Real> e;
  Mapping<T> scratch;
  explicit QueryStorage(TestContext& test)
      : a(test), af(test), b(test), x(test), f(test), e(test), scratch(test) {}
  bool Initialize() {
    return a.Initialize() && af.Initialize() && b.Initialize() &&
           x.Initialize() && f.Initialize() && e.Initialize() &&
           scratch.Initialize();
  }
  bool Protect() {
    return a.Protect(0, a.page_count()) && af.Protect(0, af.page_count()) &&
           b.Protect(0, b.page_count()) && x.Protect(0, x.page_count()) &&
           f.Protect(0, f.page_count()) && e.Protect(0, e.page_count()) &&
           scratch.Protect(0, scratch.page_count());
  }
  bool Restore() {
    return a.Restore() && af.Restore() && b.Restore() && x.Restore() &&
           f.Restore() && e.Restore() && scratch.Restore();
  }
  Operands<T> Views(const Profile& p) {
    const bool local = p.n == 0 || p.nrhs == 0;
    // Local unused full strides exceed both native INTEGER interfaces where
    // representable. Descriptor creation still proves the actual empty span.
    const asc::extent_t leading =
        local ? std::numeric_limits<asc::extent_t>::max() : 8;
    return {
        Take(asc::DenseBlasPackedMatrixView<const T>::Create(
            a.data() + 1, p.n, p.a_layout, {a.data(), a.bytes(), kHost})),
        Take(asc::DenseBlasPackedMatrixView<const T>::Create(
            af.data() + 1, p.n, p.af_layout, {af.data(), af.bytes(), kHost})),
        Take(asc::DenseBlasMatrixView<const T>::Create(
            b.data() + 1, p.n, p.nrhs, p.b_layout, leading,
            {b.data(), b.bytes(), kHost})),
        Take(asc::DenseBlasMatrixView<T>::Create(x.data() + 1, p.n, p.nrhs,
                                                 p.x_layout, leading,
                                                 {x.data(), x.bytes(), kHost})),
        Take(asc::DenseBlasVectorView<Real>::Create(
            f.data() + 1, p.nrhs, 1, {f.data(), f.bytes(), kHost})),
        Take(asc::DenseBlasVectorView<Real>::Create(
            e.data() + 1, p.nrhs, 1, {e.data(), e.bytes(), kHost}))};
  }
  void CheckRestored(TestContext& test, const Profile& p) {
    Unchanged(test, a);
    Unchanged(test, af);
    Unchanged(test, b);
    Unchanged(test, x);
    Unchanged(test, scratch);
    const bool local = p.n == 0 || p.nrhs == 0;
    for (std::size_t i = 0; i < f.count(); ++i) {
      const bool output =
          local && i >= 1 && i <= static_cast<std::size_t>(p.nrhs);
      const Real expected = output ? Real{} : Real{19};
      ASC_DENSE_TEST_CHECK(test,
                           f.data()[i] == expected && e.data()[i] == expected);
    }
  }
};

template <typename T>
void Local(TestContext& test, const asc::ReferenceLapackProvider& provider,
           const Profile& p, const Operands<T>& values,
           QueryStorage<T>& storage, const asc::LapackWorkspacePlan& plan) {
  for (const auto& region : plan.regions) {
    ASC_DENSE_TEST_CHECK(
        test, region.minimum_entries == 0 && region.preferred_entries == 0);
  }
  if (!storage.f.Restore() || !storage.e.Restore()) {
    return;
  }
  asc::LapackWorkspace workspace;
  workspace.regions[kLayout] = {storage.scratch.data(), storage.scratch.bytes(),
                                kHost};
  asc::LapackReport report;
  asc_dense_test::AllocationProbe allocations;
  const auto status = Call(provider, p, values, plan, workspace, report);
  const auto count = allocations.count();
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_CHECK(
      test, report.outcome == asc::LapackOutcome::kSuccess &&
                report.output_validity == asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, count, 0U);
  for (asc::extent_t i = 0; i < p.nrhs; ++i) {
    ASC_DENSE_TEST_CHECK(test,
                         values.f.data()[i] == 0 && values.e.data()[i] == 0);
  }
}

template <typename T>
void ObserveQuery(TestContext& test,
                  const asc::ReferenceLapackProvider& provider,
                  const Profile& p) {
  QueryStorage<T> storage(test);
  if (!storage.Initialize()) {
    return;
  }
  const auto values = storage.Views(p);
  if (!storage.Protect()) {
    return;
  }
  asc_dense_test::AllocationProbe allocations;
  const auto result = Query(provider, p, values);
  ASC_DENSE_TEST_CHECK(test, result.ok());
  const auto plan = Take(result);
  auto stale = plan;
  ++stale.regions[kLayout].minimum_entries;
  ++stale.regions[kLayout].preferred_entries;
  asc::LapackReport report;
  const auto status = Call(provider, p, values, stale, {}, report);
  ASC_DENSE_TEST_CHECK(test, status.code() == asc::ErrorCode::kInvalidState &&
                                 !report.called_provider &&
                                 !report.native_info);
  if (p.n == 0 || p.nrhs == 0) {
    Local(test, provider, p, values, storage, plan);
  }
  const auto count = allocations.count();
  ASC_DENSE_TEST_EQ(test, count, 0U);
  if (!storage.Restore()) {
    return;
  }
  storage.CheckRestored(test, p);
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
T* PackedStorage(Mapping<T>& map, const Profile& p, asc::DenseBlasLayout layout,
                 bool at_end, T diagonal) {
  const auto packed = static_cast<std::size_t>(p.n * (p.n + 1) / 2);
  T* data = map.data() +
            (at_end ? 3 * map.page_entries() - packed : 2 * map.page_entries());
  Slots(p.n, p.triangle, layout,
        [&](std::size_t i, bool diag) { data[i] = diag ? diagonal : T{}; });
  return data;
}

template <typename T>
struct FullStorage {
  Mapping<T> map;
  T* data = nullptr;
  asc::extent_t leading = 0;
  asc::DenseBlasLayout layout = kColumn;
  explicit FullStorage(TestContext& test) : map(test) {}
  [[nodiscard]] std::size_t Offset(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(layout == kColumn ? j * leading + i
                                                      : i * leading + j);
  }
  bool Initialize(const Profile& p, asc::DenseBlasLayout selected, bool at_end,
                  T scale) {
    layout = selected;
    const auto major =
        static_cast<std::size_t>(layout == kColumn ? p.nrhs : p.n);
    const auto minor =
        static_cast<std::size_t>(layout == kColumn ? p.n : p.nrhs);
    const auto page = sysconf(_SC_PAGESIZE);
    if (page <= 0 ||
        !map.Initialize((2 * major + 1) * static_cast<std::size_t>(page) /
                        sizeof(T))) {
      return false;
    }
    leading = static_cast<asc::extent_t>(2 * map.page_entries());
    data = map.data() +
           (at_end ? 2 * map.page_entries() - minor : map.page_entries());
    for (asc::extent_t j = 0; j < p.nrhs; ++j) {
      for (asc::extent_t i = 0; i < p.n; ++i) {
        data[Offset(i, j)] = scale;
      }
    }
    for (std::size_t i = 0; i < map.page_count(); i += 2) {
      if (!map.Protect(i, 1)) {
        return false;
      }
    }
    return true;
  }
  void Verify(TestContext& test, const Profile& p, T expected,
              bool approximate) {
    if (!map.Restore()) {
      return;
    }
    for (asc::extent_t j = 0; j < p.nrhs; ++j) {
      for (asc::extent_t i = 0; i < p.n; ++i) {
        const auto at = Offset(i, j);
        if (approximate) {
          ASC_DENSE_TEST_CHECK(
              test, std::abs(data[at] - expected) <=
                        64 * std::numeric_limits<
                                 asc::DenseBlasRealType<T>>::epsilon());
        } else {
          ASC_DENSE_TEST_EQ(test, data[at], expected);
        }
        data[at] = T{19};
      }
    }
    Unchanged(test, map);
  }
};

template <typename T>
struct BoundaryStorage {
  using Real = asc::DenseBlasRealType<T>;
  Mapping<T> a;
  Mapping<T> af;
  FullStorage<T> b;
  FullStorage<T> x;
  Mapping<Real> f;
  Mapping<Real> e;
  T* ap = nullptr;
  T* afp = nullptr;
  Real* ferr = nullptr;
  Real* berr = nullptr;
  explicit BoundaryStorage(TestContext& test)
      : a(test), af(test), b(test), x(test), f(test), e(test) {}
  bool Initialize(const Profile& p, bool at_end) {
    if (!a.Initialize() || !af.Initialize() || !f.Initialize() ||
        !e.Initialize() || !b.Initialize(p, p.b_layout, at_end, T{4}) ||
        !x.Initialize(p, p.x_layout, at_end, T{0.75})) {
      return false;
    }
    ap = PackedStorage(a, p, p.a_layout, at_end, T{4});
    afp = PackedStorage(af, p, p.af_layout, at_end, T{2});
    const auto offset =
        at_end ? 3 * f.page_entries() - static_cast<std::size_t>(p.nrhs)
               : 2 * f.page_entries();
    ferr = f.data() + offset;
    berr = e.data() + offset;
    return a.Protect(0, 2) && a.Protect(3, 2) && af.Protect(0, 2) &&
           af.Protect(3, 2) && f.Protect(0, 2) && f.Protect(3, 2) &&
           e.Protect(0, 2) && e.Protect(3, 2);
  }
  Operands<T> Views(const Profile& p) {
    return {Take(asc::DenseBlasPackedMatrixView<const T>::Create(
                ap, p.n, p.a_layout, {a.data(), a.bytes(), kHost})),
            Take(asc::DenseBlasPackedMatrixView<const T>::Create(
                afp, p.n, p.af_layout, {af.data(), af.bytes(), kHost})),
            Take(asc::DenseBlasMatrixView<const T>::Create(
                b.data, p.n, p.nrhs, p.b_layout, b.leading,
                {b.map.data(), b.map.bytes(), kHost})),
            Take(asc::DenseBlasMatrixView<T>::Create(
                x.data, p.n, p.nrhs, p.x_layout, x.leading,
                {x.map.data(), x.map.bytes(), kHost})),
            Take(asc::DenseBlasVectorView<Real>::Create(
                ferr, p.nrhs, 1, {f.data(), f.bytes(), kHost})),
            Take(asc::DenseBlasVectorView<Real>::Create(
                berr, p.nrhs, 1, {e.data(), e.bytes(), kHost}))};
  }
  void Verify(TestContext& test, const Profile& p) {
    if (!a.Restore() || !af.Restore() || !f.Restore() || !e.Restore()) {
      return;
    }
    Slots(p.n, p.triangle, p.a_layout, [&](std::size_t i, bool diag) {
      ASC_DENSE_TEST_EQ(test, ap[i], diag ? T{4} : T{});
      ap[i] = T{19};
    });
    Slots(p.n, p.triangle, p.af_layout, [&](std::size_t i, bool diag) {
      ASC_DENSE_TEST_EQ(test, afp[i], diag ? T{2} : T{});
      afp[i] = T{19};
    });
    for (asc::extent_t i = 0; i < p.nrhs; ++i) {
      ASC_DENSE_TEST_CHECK(test, std::isfinite(ferr[i]) && ferr[i] >= 0 &&
                                     std::isfinite(berr[i]) && berr[i] >= 0);
      ferr[i] = Real{19};
      berr[i] = Real{19};
    }
    Unchanged(test, a);
    Unchanged(test, af);
    Unchanged(test, f);
    Unchanged(test, e);
    b.Verify(test, p, T{4}, false);
    x.Verify(test, p, T{1}, true);
  }
};

template <typename T>
void Boundaries(TestContext& test, const asc::ReferenceLapackProvider& provider,
                const Profile& p, bool at_end) {
  BoundaryStorage<T> storage(test);
  if (!storage.Initialize(p, at_end)) {
    return;
  }
  const auto values = storage.Views(p);
  const auto plan = Take(Query(provider, p, values));
  installed_internal::Scratch<T> scratch;
  std::array<asc::DenseBlasRealType<T>, 8> real{};
  constexpr auto kReal =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
  scratch.workspace.regions[kReal] = {real.data(), sizeof(real), kHost};
  asc::LapackReport report;
  asc_dense_test::AllocationProbe allocations;
  const auto status =
      Call(provider, p, values, plan, scratch.workspace, report);
  const auto count = allocations.count();
  ASC_DENSE_TEST_EQ(test, count, 0U);
  ASC_DENSE_TEST_CHECK(test, installed_internal::Succeeded(status, report));
  storage.Verify(test, p);
}

template <typename T>
class SplitDiagonal {
 public:
  using Real = asc::DenseBlasRealType<T>;
  explicit SplitDiagonal(TestContext& test) : test_(test) {}
  SplitDiagonal(const SplitDiagonal&) = delete;
  SplitDiagonal& operator=(const SplitDiagonal&) = delete;
  SplitDiagonal(SplitDiagonal&&) = delete;
  SplitDiagonal& operator=(SplitDiagonal&&) = delete;
  ~SplitDiagonal() {
    if (memory_ != nullptr) {
      if (Restore() && data_ != nullptr) {
        std::destroy_n(data_, 1);
      }
      ASC_DENSE_TEST_EQ(test_, munmap(memory_, 2 * page_bytes_), 0);
    }
  }
  bool Initialize() {
    static_assert(asc::DenseBlasComplex<T> && sizeof(T) == 2 * sizeof(Real));
    static_assert(alignof(T) == alignof(Real));
    const auto page = sysconf(_SC_PAGESIZE);
    ASC_DENSE_TEST_CHECK(test_, page > 0);
    if (page <= 0) {
      return false;
    }
    page_bytes_ = static_cast<std::size_t>(page);
    const bool valid =
        page_bytes_ > sizeof(T) && page_bytes_ % alignof(T) == 0 &&
        page_bytes_ <= std::numeric_limits<std::size_t>::max() / 2;
    ASC_DENSE_TEST_CHECK(test_, valid);
    if (!valid) {
      return false;
    }
    void* memory = mmap(nullptr, 2 * page_bytes_, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASC_DENSE_TEST_CHECK(test_, memory != MAP_FAILED);
    if (memory == MAP_FAILED) {
      return false;
    }
    memory_ = memory;
    // A containing one-element complex array straddles the page boundary.
    // Its real component is accessible; its imaginary component is protected.
    void* placement =
        static_cast<std::byte*>(memory_) + page_bytes_ - sizeof(Real);
    data_ = ::new (placement) T[1];
    data_[0] = T{4, 19};
    const auto result = mprotect(static_cast<std::byte*>(memory_) + page_bytes_,
                                 page_bytes_, PROT_NONE);
    ASC_DENSE_TEST_EQ(test_, result, 0);
    return result == 0;
  }
  bool Restore() {
    const auto result =
        mprotect(memory_, 2 * page_bytes_, PROT_READ | PROT_WRITE);
    ASC_DENSE_TEST_EQ(test_, result, 0);
    return result == 0;
  }
  T* data() { return data_; }

 private:
  TestContext& test_;
  void* memory_ = nullptr;
  T* data_ = nullptr;
  std::size_t page_bytes_ = 0;
};

template <typename T>
void IgnoredDiagonal(TestContext& test,
                     const asc::ReferenceLapackProvider& provider,
                     const Profile& p) {
  using Real = asc::DenseBlasRealType<T>;
  SplitDiagonal<T> original(test);
  if (!original.Initialize()) {
    return;
  }
  std::array<T, 3> af{T{19}, T{2}, T{19}};
  std::array<T, 3> b{T{19}, T{4}, T{19}};
  std::array<T, 3> x{T{19}, T{0.75}, T{19}};
  std::array<Real, 3> f{Real{19}, Real{19}, Real{19}};
  std::array<Real, 3> e{Real{19}, Real{19}, Real{19}};
  const Operands<T> values{
      Take(asc::DenseBlasPackedMatrixView<const T>::Create(
          original.data(), 1, p.a_layout, {original.data(), sizeof(T), kHost})),
      Take(asc::DenseBlasPackedMatrixView<const T>::Create(
          af.data() + 1, 1, p.af_layout, {af.data(), sizeof(af), kHost})),
      Take(asc::DenseBlasMatrixView<const T>::Create(
          b.data() + 1, 1, 1, p.b_layout, 1, {b.data(), sizeof(b), kHost})),
      Take(asc::DenseBlasMatrixView<T>::Create(
          x.data() + 1, 1, 1, p.x_layout, 1, {x.data(), sizeof(x), kHost})),
      Take(asc::DenseBlasVectorView<Real>::Create(
          f.data() + 1, 1, 1, {f.data(), sizeof(f), kHost})),
      Take(asc::DenseBlasVectorView<Real>::Create(
          e.data() + 1, 1, 1, {e.data(), sizeof(e), kHost}))};
  const auto plan = Take(Query(provider, p, values));
  installed_internal::Scratch<T> scratch;
  std::array<Real, 8> real{};
  constexpr auto kReal =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
  scratch.workspace.regions[kReal] = {real.data(), sizeof(real), kHost};
  asc::LapackReport report;
  asc_dense_test::AllocationProbe allocations;
  const auto status =
      Call(provider, p, values, plan, scratch.workspace, report);
  const auto count = allocations.count();
  ASC_DENSE_TEST_EQ(test, count, 0U);
  ASC_DENSE_TEST_CHECK(test, installed_internal::Succeeded(status, report));
  ASC_DENSE_TEST_CHECK(
      test, std::abs(x[1] - T{1}) <= 64 * std::numeric_limits<Real>::epsilon());
  ASC_DENSE_TEST_CHECK(test, std::isfinite(f[1]) && f[1] >= 0 &&
                                 std::isfinite(e[1]) && e[1] >= 0);
  ASC_DENSE_TEST_CHECK(test, af[1] == T{2} && b[1] == T{4});
  ASC_DENSE_TEST_CHECK(test, af.front() == T{19} && af.back() == T{19} &&
                                 b.front() == T{19} && b.back() == T{19} &&
                                 x.front() == T{19} && x.back() == T{19} &&
                                 f.front() == 19 && f.back() == 19 &&
                                 e.front() == 19 && e.back() == 19);
  if (!original.Restore()) {
    return;
  }
  ASC_DENSE_TEST_EQ(test, original.data()[0], (T{4, 19}));
}

enum class RawCase : std::uint8_t { kNegative, kPhase, kZero, kNan, kInfinite };

template <typename T>
struct RawStorage {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 3> a{T{19}, T{4}, T{19}};
  std::array<T, 3> af{T{19}, T{-2}, T{19}};
  std::array<T, 3> b{T{19}, T{4}, T{19}};
  std::array<T, 3> x{T{19}, T{1}, T{19}};
  std::array<Real, 3> f{Real{19}, Real{19}, Real{19}};
  std::array<Real, 3> e{Real{19}, Real{19}, Real{19}};
  explicit RawStorage(RawCase kind) {
    switch (kind) {
      case RawCase::kNegative:
        x[1] = T{0.75};
        break;
      case RawCase::kPhase:
        if constexpr (asc::DenseBlasComplex<T>) {
          af[1] = T{2, 1};
        }
        a[1] = T{5};
        b[1] = T{5};
        x[1] = T{0.75};
        break;
      case RawCase::kZero:
        af[1] = T{};
        break;
      case RawCase::kNan:
        af[1] = T{std::numeric_limits<Real>::quiet_NaN()};
        break;
      case RawCase::kInfinite:
        af[1] = T{std::numeric_limits<Real>::infinity()};
        break;
    }
  }
  Operands<T> Views(const Profile& p) {
    return {
        Take(asc::DenseBlasPackedMatrixView<const T>::Create(
            a.data() + 1, 1, p.a_layout, {a.data(), sizeof(a), kHost})),
        Take(asc::DenseBlasPackedMatrixView<const T>::Create(
            af.data() + 1, 1, p.af_layout, {af.data(), sizeof(af), kHost})),
        Take(asc::DenseBlasMatrixView<const T>::Create(
            b.data() + 1, 1, 1, p.b_layout, 1, {b.data(), sizeof(b), kHost})),
        Take(asc::DenseBlasMatrixView<T>::Create(
            x.data() + 1, 1, 1, p.x_layout, 1, {x.data(), sizeof(x), kHost})),
        Take(asc::DenseBlasVectorView<Real>::Create(
            f.data() + 1, 1, 1, {f.data(), sizeof(f), kHost})),
        Take(asc::DenseBlasVectorView<Real>::Create(
            e.data() + 1, 1, 1, {e.data(), sizeof(e), kHost}))};
  }
  void Preserved(TestContext& test, const RawStorage& before) const {
    const auto old = std::as_bytes(std::span(before.af));
    const auto current = std::as_bytes(std::span(af));
    ASC_DENSE_TEST_CHECK(test, a == before.a && b == before.b &&
                                   std::equal(current.begin(), current.end(),
                                              old.begin(), old.end()));
    ASC_DENSE_TEST_CHECK(test, x.front() == T{19} && x.back() == T{19} &&
                                   f.front() == 19 && f.back() == 19 &&
                                   e.front() == 19 && e.back() == 19);
  }
};

template <typename T>
void RawFactor(TestContext& test, const asc::ReferenceLapackProvider& provider,
               const Profile& p, RawCase kind) {
  using Real = asc::DenseBlasRealType<T>;
  RawStorage<T> storage(kind);
  const auto before = storage;
  const auto values = storage.Views(p);
  const auto plan = Take(Query(provider, p, values));
  installed_internal::Scratch<T> scratch;
  std::array<Real, 8> real{};
  constexpr auto kReal =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
  scratch.workspace.regions[kReal] = {real.data(), sizeof(real), kHost};
  asc::LapackReport report;
  asc_dense_test::AllocationProbe allocations;
  const auto status =
      Call(provider, p, values, plan, scratch.workspace, report);
  const auto count = allocations.count();
  ASC_DENSE_TEST_EQ(test, count, 0U);
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_CHECK(test, std::abs(storage.x[1] - T{1}) <=
                                 64 * std::numeric_limits<Real>::epsilon());
  if (kind == RawCase::kZero || kind == RawCase::kNan) {
    // Invalid raw-factor arithmetic is diagnostic evidence only: no accuracy
    // or factor certificate is claimed for an unmatched zero/nonfinite AFP.
    ASC_DENSE_TEST_CHECK(
        test, status.code() == asc::ErrorCode::kNumerical &&
                  report.outcome == asc::LapackOutcome::kAccuracyWarning &&
                  report.output_validity ==
                      asc::LapackOutputValidity::kDocumentedPartial &&
                  !std::isfinite(storage.f[1]));
  } else {
    ASC_DENSE_TEST_CHECK(test, installed_internal::Succeeded(status, report));
    ASC_DENSE_TEST_CHECK(
        test, std::isfinite(storage.f[1]) && storage.f[1] >= 0 &&
                  storage.f[1] <= 64 * std::numeric_limits<Real>::epsilon());
    if (kind == RawCase::kInfinite) {
      // INFO0/finite diagnostics do not certify the caller's factor provenance.
      ASC_DENSE_TEST_EQ(test, storage.f[1], Real{});
    }
  }
  ASC_DENSE_TEST_CHECK(
      test, std::isfinite(storage.e[1]) && storage.e[1] >= 0 &&
                storage.e[1] <= 64 * std::numeric_limits<Real>::epsilon());
  storage.Preserved(test, before);
}

template <typename T>
void Scalar(TestContext& test, const asc::ReferenceLapackProvider& provider,
            std::size_t& profiles) {
  for (auto triangle : {kUpper, kLower}) {
    for (int bits = 0; bits < 16; ++bits) {
      Profile p{0,
                0,
                triangle,
                bits & 1 ? kRow : kColumn,
                bits & 2 ? kRow : kColumn,
                bits & 4 ? kRow : kColumn,
                bits & 8 ? kRow : kColumn};
      if constexpr (asc::DenseBlasComplex<T>) {
        p.n = 1;
        p.nrhs = 1;
        IgnoredDiagonal<T>(test, provider, p);
        ++profiles;
      }
      p.n = 1;
      p.nrhs = 1;
      for (auto kind : {RawCase::kNegative, RawCase::kZero, RawCase::kNan,
                        RawCase::kInfinite}) {
        RawFactor<T>(test, provider, p, kind);
        ++profiles;
      }
      if constexpr (asc::DenseBlasComplex<T>) {
        RawFactor<T>(test, provider, p, RawCase::kPhase);
        ++profiles;
      }
      for (const asc::extent_t n : {0, 1, 5}) {
        p.n = n;
        for (const asc::extent_t nrhs : {0, 2}) {
          p.nrhs = nrhs;
          ObserveQuery<T>(test, provider, p);
          ++profiles;
        }
      }
      p.nrhs = 2;
      for (const asc::extent_t n : {1, 2, 5}) {
        p.n = n;
        for (bool at_end : {false, true}) {
          Boundaries<T>(test, provider, p, at_end);
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
  ASC_DENSE_TEST_EQ(test, profiles, 2176U);
  std::printf(
      "Packed Cholesky refinement protected-memory profiles: %zu;768 "
      "queries,512 local calls,768 stale-plan rejections,768 native boundary "
      "calls,64 complex ignored-diagonal calls,576 raw-factor calls\n",
      profiles);
  return test.Finish();
}
