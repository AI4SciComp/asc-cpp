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
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/triangular_band_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_triangular_band.h"
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
void Execute(TestContext& test, const asc::ReferenceLapackProvider& provider,
             const Profile& profile, asc::LapackTriangularBandView<const T> a,
             asc::DenseBlasLayout layout, asc::DenseBlasTranspose operation) {
  std::array<T, 16> rhs;
  rhs.fill(T{7});
  std::array<T, 32> packing;
  packing.fill(T{23});
  const auto b = Take(asc::DenseBlasMatrixView<T>::Create(
      rhs.data() + 1, profile.n, profile.nrhs, layout, 3,
      {rhs.data(), sizeof(rhs), kHost}));
  asc_dense_test::AllocationProbe allocations;
  const auto plan = Take(
      asc::QueryTbtrsWorkspace(provider, profile.diagonal, operation, a, b));
  const auto count = plan.regions[kLayout].minimum_entries;
  asc::extent_t expected = 0;
  if (a.layout() == kRow &&
      (profile.nrhs != 0 || profile.diagonal == kNonUnit)) {
    expected += profile.n * (profile.kd + 1);
  }
  if (layout == kRow) {
    expected += profile.n * profile.nrhs;
  }
  ASC_DENSE_TEST_EQ(test, count, expected);
  ASC_DENSE_TEST_CHECK(test, count >= 0 && count <= 30);
  if (count < 0 || count > 30) {
    return;
  }
  ASC_DENSE_TEST_CHECK(test, std::all_of(rhs.begin(), rhs.end(), [](T value) {
                         return value == T{7};
                       }));
  ASC_DENSE_TEST_CHECK(test,
                       std::all_of(packing.begin(), packing.end(),
                                   [](T value) { return value == T{23}; }));
  asc::LapackWorkspace workspace;
  if (count != 0) {
    workspace.regions[kLayout] = {
        packing.data() + 1, static_cast<std::size_t>(count) * sizeof(T), kHost};
  }
  asc::LapackReport report;
  const auto status = asc::Tbtrs(provider, profile.diagonal, operation, a, b,
                                 plan, workspace, report);
  ASC_DENSE_TEST_EQ(test, allocations.count(), std::size_t{0});
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && report.called_provider && report.native_info == 0 &&
                report.outcome == asc::LapackOutcome::kSuccess &&
                report.output_validity == asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(test, std::all_of(rhs.begin(), rhs.end(), [](T value) {
                         return value == T{7};
                       }));
  ASC_DENSE_TEST_EQ(test, packing.front(), T{23});
  ASC_DENSE_TEST_CHECK(test,
                       std::all_of(packing.begin() + 1 + count, packing.end(),
                                   [](T value) { return value == T{23}; }));
}

template <typename T>
void ProtectedCase(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   const Profile& profile, asc::DenseBlasLayout layout,
                   asc::DenseBlasTriangle triangle, std::size_t& profiles) {
  Mapping<T> mapping(test);
  if (!mapping.Initialize()) {
    return;
  }
  const bool nonunit = profile.diagonal == kNonUnit;
  const bool ends_at_diagonal = (layout == kColumn) == (triangle == kUpper);
  // With n=2,kd=3 and ld=two pages, both diagonals are readable. The single
  // possible off-diagonal lies in the protected page between them. Row and
  // column encodings put the diagonal at opposite ends of the band slot.
  const auto page = mapping.page_entries();
  const auto first = ends_at_diagonal ? page : page - 1;
  const auto offset = nonunit ? first - (ends_at_diagonal ? 3 : 0) : 0;
  const auto leading =
      nonunit ? static_cast<asc::extent_t>(2 * page) : profile.kd + 2;
  if (nonunit) {
    mapping.data()[first] = T{2};
    mapping.data()[first + 2 * page] = T{3};
  }
  const auto a = Take(asc::LapackTriangularBandView<const T>::Create(
      mapping.data() + offset, profile.n, profile.kd, triangle, layout, leading,
      {mapping.data(), mapping.bytes(), kHost}));
  const bool protected_ok = nonunit
                                ? mapping.Protect(ends_at_diagonal ? 2 : 1, 1)
                                : mapping.Protect(0, 5);
  if (protected_ok) {
    for (auto b_layout : {kColumn, kRow}) {
      for (auto operation :
           {asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kTranspose,
            asc::DenseBlasTranspose::kConjugateTranspose}) {
        Execute(test, provider, profile, a, b_layout, operation);
        ++profiles;
      }
    }
  }
  if (mapping.Restore()) {
    for (std::size_t i = 0; i < mapping.count(); ++i) {
      T expected{19};
      if (nonunit && i == first) {
        expected = T{2};
      } else if (nonunit && i == first + 2 * page) {
        expected = T{3};
      }
      ASC_DENSE_TEST_EQ(test, mapping.data()[i], expected);
    }
  }
}

template <typename T>
void Scalar(TestContext& test, const asc::ReferenceLapackProvider& provider,
            std::size_t& profiles) {
  for (const auto profile : {Profile{1, 0, 2, kUnit}, Profile{2, 3, 0, kUnit},
                             Profile{2, 3, 0, kNonUnit}}) {
    for (auto layout : {kColumn, kRow}) {
      for (auto triangle : {kUpper, kLower}) {
        ProtectedCase<T>(test, provider, profile, layout, triangle, profiles);
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
  ASC_DENSE_TEST_EQ(test, profiles, std::size_t{288});
  std::printf("Triangular band protected-memory profiles: %zu\n", profiles);
  return test.Finish();
}
