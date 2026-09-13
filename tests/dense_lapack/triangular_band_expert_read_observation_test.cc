#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
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
#include "asc/dense/lapack/triangular_band_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_triangular_band_condition.h"
#include "asc/dense/providers/lapack_triangular_band_error_bounds.h"
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
constexpr auto kUnit = asc::DenseBlasDiagonal::kUnit;
constexpr auto kNonUnit = asc::DenseBlasDiagonal::kNonUnit;
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
  asc::extent_t kd;
  asc::extent_t nrhs;
  asc::DenseBlasDiagonal diagonal;
};

template <typename T>
class Workspace {
 public:
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 64> scalar;
  std::array<T, 64> layout;
  std::array<Real, 16> real;
  alignas(std::max_align_t) std::array<std::byte, 64> integer;
  asc::LapackWorkspace value;
  explicit Workspace(const asc::LapackWorkspacePlan& plan) {
    scalar.fill(T{-17});
    layout.fill(T{-17});
    real.fill(Real{-17});
    integer.fill(std::byte{0x5a});
    for (std::size_t k = 0; k < plan.regions.size(); ++k) {
      const auto& region = plan.regions[k];
      if (region.minimum_entries == 0) {
        continue;
      }
      void* p = nullptr;
      if (k == static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)) {
        p = scalar.data() + 1;
      }
      if (k == kLayout) {
        p = layout.data() + 1;
      }
      if (k == static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal)) {
        p = real.data() + 1;
      }
      if (k == static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger)) {
        p = integer.data() + 16;
      }
      value.regions[k] = {
          p,
          static_cast<std::size_t>(region.minimum_entries) * region.entry_bytes,
          kHost};
    }
  }
  void Guards(TestContext& test, const asc::LapackWorkspacePlan& plan) const {
    for (std::size_t k = 0; k < plan.regions.size(); ++k) {
      const auto bytes =
          static_cast<std::size_t>(plan.regions[k].minimum_entries) *
          plan.regions[k].entry_bytes;
      if (k == static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar) ||
          k == kLayout) {
        const auto& a = k == kLayout ? layout : scalar;
        const auto used =
            static_cast<std::size_t>(plan.regions[k].minimum_entries);
        ASC_DENSE_TEST_EQ(test, a.front(), T{-17});
        ASC_DENSE_TEST_CHECK(test,
                             std::all_of(a.begin() + 1 + used, a.end(),
                                         [](T v) { return v == T{-17}; }));
      }
      if (k == static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal)) {
        const auto used =
            static_cast<std::size_t>(plan.regions[k].minimum_entries);
        ASC_DENSE_TEST_EQ(test, real.front(), Real{-17});
        ASC_DENSE_TEST_CHECK(
            test, std::all_of(real.begin() + 1 + used, real.end(),
                              [](Real v) { return v == Real{-17}; }));
      }
      if (k == static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger)) {
        ASC_DENSE_TEST_CHECK(
            test,
            std::all_of(integer.begin(), integer.begin() + 16,
                        [](std::byte v) { return v == std::byte{0x5a}; }));
        ASC_DENSE_TEST_CHECK(test, std::all_of(integer.begin() + 16 + bytes,
                                               integer.end(), [](std::byte v) {
                                                 return v == std::byte{0x5a};
                                               }));
      }
    }
  }
};

template <typename T>
void Condition(TestContext& test, const asc::ReferenceLapackProvider& provider,
               const Profile& profile, asc::LapackTriangularBandView<const T> a,
               asc::LapackConditionNorm norm) {
  using Real = asc::DenseBlasRealType<T>;
  Real rcond = Real{-31};
  const auto plan = Take(
      asc::QueryTbconWorkspace(provider, norm, profile.diagonal, a, rcond));
  ASC_DENSE_TEST_EQ(test, rcond, Real{-31});
  Workspace<T> work(plan);
  asc::LapackReport report;
  {
    const asc_dense_test::AllocationProbe probe;
    ASC_DENSE_TEST_CHECK(test, asc::Tbcon(provider, norm, profile.diagonal, a,
                                          rcond, plan, work.value, report)
                                   .ok());
    ASC_DENSE_TEST_EQ(test, probe.count(), std::size_t{0});
  }
  ASC_DENSE_TEST_EQ(test, rcond, Real{1});
  ASC_DENSE_TEST_EQ(test, report.called_provider, profile.n != 0);
  ASC_DENSE_TEST_CHECK(
      test, profile.n == 0 ? !report.native_info : report.native_info == 0);
  work.Guards(test, plan);
}

template <typename T>
void Errors(TestContext& test, const asc::ReferenceLapackProvider& provider,
            const Profile& profile, asc::LapackTriangularBandView<const T> a,
            asc::DenseBlasMatrixView<const T> b,
            asc::DenseBlasMatrixView<const T> x,
            asc::DenseBlasTranspose operation) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<Real, 4> ferr;
  ferr.fill(Real{-31});
  std::array<Real, 4> berr;
  berr.fill(Real{-33});
  const auto fv = Take(asc::DenseBlasVectorView<Real>::Create(
      ferr.data() + 1, profile.nrhs, 1, {ferr.data(), sizeof(ferr), kHost}));
  const auto bv = Take(asc::DenseBlasVectorView<Real>::Create(
      berr.data() + 1, profile.nrhs, 1, {berr.data(), sizeof(berr), kHost}));
  const auto plan = Take(asc::QueryTbrfsWorkspace(provider, profile.diagonal,
                                                  operation, a, b, x, fv, bv));
  Workspace<T> work(plan);
  asc::LapackReport report;
  {
    const asc_dense_test::AllocationProbe probe;
    ASC_DENSE_TEST_CHECK(
        test, asc::Tbrfs(provider, profile.diagonal, operation, a, b, x, fv, bv,
                         plan, work.value, report)
                  .ok());
    ASC_DENSE_TEST_EQ(test, probe.count(), std::size_t{0});
  }
  const bool active = profile.n != 0 && profile.nrhs != 0;
  ASC_DENSE_TEST_EQ(test, report.called_provider, active);
  ASC_DENSE_TEST_CHECK(test,
                       active ? report.native_info == 0 : !report.native_info);
  for (asc::extent_t j = 0; j < profile.nrhs; ++j) {
    ASC_DENSE_TEST_EQ(test, berr[static_cast<std::size_t>(j + 1)], Real{0});
    const Real f = ferr[static_cast<std::size_t>(j + 1)];
    ASC_DENSE_TEST_CHECK(test, active ? std::isfinite(f) && f >= 0 : f == 0);
  }
  ASC_DENSE_TEST_EQ(test, ferr.front(), Real{-31});
  ASC_DENSE_TEST_EQ(test, berr.front(), Real{-33});
  ASC_DENSE_TEST_CHECK(test,
                       std::all_of(ferr.begin() + 1 + profile.nrhs, ferr.end(),
                                   [](Real f) { return f == Real{-31}; }));
  ASC_DENSE_TEST_CHECK(test,
                       std::all_of(berr.begin() + 1 + profile.nrhs, berr.end(),
                                   [](Real f) { return f == Real{-33}; }));
  work.Guards(test, plan);
}

template <typename T>
class RightHandSide {
 public:
  explicit RightHandSide(TestContext& test) : mapping_(test) {}
  bool Initialize(const Profile& profile, asc::DenseBlasLayout layout) {
    if (!mapping_.Initialize()) {
      return false;
    }
    const auto page = mapping_.page_entries();
    offset_ = layout == kColumn ? page : 2 * page - 2;
    second_ = layout == kColumn ? 3 * page : offset_ + 1;
    active_ = profile.n != 0 && profile.nrhs != 0;
    if (active_) {
      mapping_.data()[offset_] = T{1};
      mapping_.data()[second_] = T{1};
    }
    if (!active_) {
      return mapping_.Protect(0, 5);
    }
    if (layout == kColumn) {
      return mapping_.Protect(0, 1) && mapping_.Protect(2, 1) &&
             mapping_.Protect(4, 1);
    }
    return mapping_.Protect(0, 1) && mapping_.Protect(2, 3);
  }
  auto View(const Profile& profile, asc::DenseBlasLayout layout) {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        mapping_.data() + offset_, profile.n, profile.nrhs, layout,
        static_cast<asc::extent_t>(2 * mapping_.page_entries()),
        {mapping_.data(), mapping_.bytes(), kHost}));
  }
  void RestoreAndCheck(TestContext& test) {
    if (!mapping_.Restore()) {
      return;
    }
    for (std::size_t i = 0; i < mapping_.count(); ++i) {
      const T expected =
          active_ && (i == offset_ || i == second_) ? T{1} : T{19};
      ASC_DENSE_TEST_EQ(test, mapping_.data()[i], expected);
    }
  }

 private:
  Mapping<T> mapping_;
  std::size_t offset_ = 0;
  std::size_t second_ = 0;
  bool active_ = false;
};

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          const Profile& profile, asc::DenseBlasLayout al,
          asc::DenseBlasTriangle triangle, std::size_t& profiles) {
  Mapping<T> matrix(test);
  if (!matrix.Initialize()) {
    return;
  }
  const auto a = Take(asc::LapackTriangularBandView<const T>::Create(
      matrix.data(), profile.n, profile.kd, triangle, al, profile.kd + 1,
      {matrix.data(), matrix.bytes(), kHost}));
  if (!matrix.Protect(0, 5)) {
    return;
  }
  if (profile.n == 0 || (profile.n == 1 && profile.diagonal == kUnit)) {
    for (auto norm : {asc::LapackConditionNorm::kOne,
                      asc::LapackConditionNorm::kInfinity}) {
      Condition(test, provider, profile, a, norm);
      ++profiles;
    }
  }
  for (auto bl : {kColumn, kRow}) {
    for (auto xl : {kColumn, kRow}) {
      RightHandSide<T> b(test);
      RightHandSide<T> x(test);
      if (!b.Initialize(profile, bl) || !x.Initialize(profile, xl)) {
        continue;
      }
      const auto bv = b.View(profile, bl);
      const auto xv = x.View(profile, xl);
      for (auto op :
           {asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kTranspose,
            asc::DenseBlasTranspose::kConjugateTranspose}) {
        Errors(test, provider, profile, a, bv, xv, op);
        ++profiles;
      }
      b.RestoreAndCheck(test);
      x.RestoreAndCheck(test);
    }
  }
  if (matrix.Restore()) {
    ASC_DENSE_TEST_CHECK(
        test, std::all_of(matrix.data(), matrix.data() + matrix.count(),
                          [](T v) { return v == T{19}; }));
  }
}

template <typename T>
void Scalar(TestContext& test, const asc::ReferenceLapackProvider& provider,
            std::size_t& profiles) {
  for (const auto profile :
       {Profile{1, 0, 2, kUnit}, Profile{1, 3, 2, kUnit},
        Profile{0, 3, 2, kNonUnit}, Profile{2, 3, 0, kUnit},
        Profile{2, 3, 0, kNonUnit}}) {
    for (auto al : {kColumn, kRow}) {
      for (auto triangle : {kUpper, kLower}) {
        Case<T>(test, provider, profile, al, triangle, profiles);
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
  ASC_DENSE_TEST_EQ(test, profiles, std::size_t{1056});
  std::printf("Triangular band expert protected-memory profiles: %zu\n",
              profiles);
  return test.Finish();
}
