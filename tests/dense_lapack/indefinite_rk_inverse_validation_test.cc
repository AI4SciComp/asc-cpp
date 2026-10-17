#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rk_inverse_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_inverse_test_support.h"
#include "indefinite_rook_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_indefinite_test;
using asc_rk_inverse_test::Inverse;
using asc_rk_inverse_test::Query;
using base::TestContext;

template <typename T>
void AlterWorkspace(int bad, asc::LapackWorkspacePlan& plan,
                    asc::LapackWorkspace& workspace,
                    asc_rk_inverse_test::Scratch<T>& scratch,
                    std::array<T, 16>& a, std::array<asc::index_t, 5>& p) {
  switch (bad) {
    case 0:
      workspace.regions[base::kScalar] = {nullptr, 0, base::kHost};
      break;
    case 1:
      workspace.regions[base::kPivot] = {nullptr, 0, base::kHost};
      break;
    case 2:
      workspace.regions[base::kPivot] = {scratch.pivot.data() + 17,
                                         workspace.regions[base::kPivot].size(),
                                         base::kHost};
      break;
    case 3:
      workspace.regions[base::kPivot] = {
          p.data() + 1, workspace.regions[base::kPivot].size(), base::kHost};
      break;
    case 4:
      workspace.regions[base::kScalar] = {a.data() + 1, 2 * sizeof(T),
                                          base::kHost};
      break;
    case 5:
      workspace.regions[base::kScalar] = {p.data() + 1, 2 * sizeof(T),
                                          base::kHost};
      break;
    case 6:
      ++plan.regions[base::kScalar].minimum_entries;
      break;
    case 7:
      ++plan.regions[base::kScalar].preferred_entries;
      break;
    case 8:
      ++plan.regions[base::kPivot].entry_bytes;
      break;
    case 9:
      plan.total_byte_limit = 1;
      break;
    case 10:
      workspace.regions.back() = workspace.regions[base::kPivot];
      break;
    case 26:
      workspace.regions[base::kScalar] = {
          scratch.scalar.data() + 1,
          workspace.regions[base::kScalar].size() - sizeof(T), base::kHost};
      break;
    case 11:
      workspace.regions[base::kScalar] = {scratch.scalar.data() + 1,
                                          2 * sizeof(T),
                                          asc::MemorySpace::kPinnedHost};
      break;
    default:
      break;
  }
}

void RejectedReport(
    TestContext& test, int bad, const asc::Status& status,
    const asc::LapackReport& report,
    const std::array<std::byte, sizeof(asc::LapackReport)>& before) {
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  if (bad == 19 || bad == 20 || bad == 37) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(&report, before.data(), sizeof(report)));
  } else {
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
  }
}

template <typename T>
void AlterOperands(int bad, asc::DenseBlasTriangle triangle,
                   asc::DenseBlasLayout layout, std::array<T, 16>& a,
                   std::array<asc::index_t, 5>& p,
                   asc::DenseBlasMatrixView<T>& matrix,
                   asc::RawLapackPivotView& pivots,
                   asc::LapackWorkspace& workspace, asc::LapackReport& report,
                   asc::DenseBlasTriangle& selected) {
  switch (bad) {
    case 12:
      selected = triangle == base::kUpper ? base::kLower : base::kUpper;
      break;
    case 13:
      matrix = base::Matrix(a, 2, 2, layout, 4);
      break;
    case 14:
      pivots = base::Take(asc::RawLapackPivotView::Create(
          p.data() + 1, 2, asc::LapackFactorFamily::kBunchKaufman,
          {p.data(), sizeof(p), base::kHost}));
      break;
    case 15:
      matrix = base::Matrix(a, 2, 1, layout, 3);
      break;
    case 16:
      pivots = asc_indefinite_rook_test::Raw(p, 1);
      break;
    case 17:
      p[1] = 0;
      break;
    case 18:
      matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
          a.data() + 1, 2, 2, layout, 3,
          {a.data(), sizeof(a), asc::MemorySpace::kPinnedHost}));
      break;
    case 19:
      workspace.regions[base::kPivot] = {
          &report, workspace.regions[base::kPivot].size(), base::kHost};
      break;
    case 20:
      matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(&report), 2, 2, layout, 3,
          {&report, sizeof(report), base::kHost}));
      break;
    case 21:
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      selected = static_cast<asc::DenseBlasTriangle>(99);
      break;
    case 22:
      p[1] = -1;
      break;
    case 23:
      p[2] = 3;
      break;
    case 24:
      p[triangle == base::kUpper ? 1U : 2U] = triangle == base::kUpper ? 2 : 1;
      break;
    case 27:
      // A minimum signed target must be rejected before negation.
      p[1] = -1;
      p[2] = std::numeric_limits<asc::index_t>::min();
      break;
    case 28:
      p[1] = triangle == base::kUpper ? -2 : -1;
      p[2] = p[1];
      break;
    case 25:
      pivots = base::Take(asc::RawLapackPivotView::Create(
          reinterpret_cast<asc::index_t*>(a.data()), 2,
          asc::LapackFactorFamily::kRook, {a.data(), sizeof(a), base::kHost}));
      break;
    default:
      break;
  }
}

template <typename T>
void AlterExtra(int bad, std::array<T, 8>& e, std::array<T, 16>& a,
                std::array<asc::index_t, 5>& p,
                asc::DenseBlasVectorView<const T>& extra,
                asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  using View = asc::DenseBlasVectorView<const T>;
  if (bad == 33) {
    extra = asc_rk_inverse_test::OffDiagonal(e, 1);
  }
  if (bad == 34) {
    extra = base::Take(
        View::Create(e.data() + 1, 2, 2, {e.data(), sizeof(e), base::kHost}));
  }
  if (bad == 35) {
    extra = base::Take(
        View::Create(a.data() + 1, 2, 1, {a.data(), sizeof(a), base::kHost}));
  }
  if (bad == 36) {
    extra = base::Take(View::Create(reinterpret_cast<const T*>(p.data() + 1), 2,
                                    1, {p.data(), sizeof(p), base::kHost}));
  }
  if (bad == 37) {
    extra = base::Take(View::Create(reinterpret_cast<const T*>(&report), 2, 1,
                                    {&report, sizeof(report), base::kHost}));
  }
  if (bad == 38) {
    workspace.regions[base::kScalar] = {e.data() + 1, 2 * sizeof(T),
                                        base::kHost};
  }
  if (bad == 39) {
    extra = base::Take(
        View::Create(e.data() + 1, 2, 1,
                     {e.data(), sizeof(e), asc::MemorySpace::kPinnedHost}));
  }
}

template <typename T>
void Rejection(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool hermitian, asc::DenseBlasTriangle triangle,
               asc::DenseBlasLayout layout, int bad) {
  alignas(16) std::array<T, 16> a;
  a.fill(base::Value<T>(std::numeric_limits<double>::quiet_NaN(), 13));
  std::array<asc::index_t, 5> p{-113, 1, 2, -113, -113};
  std::array<T, 8> e;
  e.fill(base::Value<T>(-117, 17));
  auto extra = asc_rk_inverse_test::OffDiagonal(e, 2);
  auto matrix = base::Matrix(a, 2, 2, layout, 3);
  auto pivots = asc_indefinite_rook_test::Raw(p, 2);
  auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return Query(provider, triangle, hermitian, matrix, extra, pivots);
  }));
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kScalar].minimum_entries,
                    asc_rk_inverse_test::Entries<T>(2, hermitian));
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kPivot].minimum_entries, 2);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kLayout].minimum_entries,
                    layout == base::kRow ? 4 : 0);
  asc_rk_inverse_test::Scratch<T> scratch;
  auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 97;
  std::array<std::byte, sizeof(report)> saved_report;
  std::memcpy(saved_report.data(), &report, sizeof(report));
  auto selected = triangle;
  AlterWorkspace(bad, plan, workspace, scratch, a, p);
  AlterOperands(bad, triangle, layout, a, p, matrix, pivots, workspace, report,
                selected);
  AlterExtra(bad, e, a, p, extra, workspace, report);
  const auto before_e = e;
  const auto before_a = a;
  const auto before_p = p;
  const auto before_scratch = scratch;
  const auto original_block = asc_rk_inverse_test::g_block_size;
  if (bad == 29) {
    asc_rk_inverse_test::g_block_size = -1;
  }
  if (bad == 30) {
    asc_rk_inverse_test::g_block_size = std::numeric_limits<lapack_int>::max();
  }
  if (bad == 31) {
    asc_rk_inverse_test::g_block_size = original_block == 1 ? 2 : 1;
  }
  if (bad == 32) {
    asc_rk_inverse_test::g_block_size = original_block == 0 ? 64 : 0;
  }
  const auto status = base::WithoutAllocation(test, [&] {
    return Inverse(provider, selected, hermitian, matrix, extra, pivots, plan,
                   workspace, report);
  });
  asc_rk_inverse_test::g_block_size = original_block;
  RejectedReport(test, bad, status, report, saved_report);
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(a.data(), before_a.data(), sizeof(a)));
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(e.data(), before_e.data(), sizeof(e)));
  ASC_DENSE_TEST_EQ(test, p, before_p);
  ASC_DENSE_TEST_EQ(test, scratch.scalar, before_scratch.scalar);
  ASC_DENSE_TEST_EQ(test, scratch.packed, before_scratch.packed);
  ASC_DENSE_TEST_EQ(test, scratch.pivot, before_scratch.pivot);
}
template <typename T>
void Unread(TestContext& test, const asc::ReferenceLapackProvider& provider,
            bool hermitian) {
#if defined(__linux__)
  const auto page = sysconf(_SC_PAGESIZE);
  ASC_DENSE_TEST_CHECK(test, page > 0);
  if (page <= 0) {
    return;
  }
  const auto bytes = static_cast<std::size_t>(page);
  void* memory =
      mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASC_DENSE_TEST_CHECK(test, memory != MAP_FAILED);
  if (memory == MAP_FAILED) {
    return;
  }
  auto* a = static_cast<T*>(memory);
  auto* e = reinterpret_cast<const T*>(static_cast<std::byte*>(memory) + 256);
  auto* p =
      reinterpret_cast<asc::index_t*>(static_cast<std::byte*>(memory) + 512);
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const asc::extent_t n : {0, 1, 2}) {
        const auto matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
            a, n, n, layout, 3, {a, 128, base::kHost}));
        const auto extra = base::Take(asc::DenseBlasVectorView<const T>::Create(
            e, n, 1, {e, 128, base::kHost}));
        const auto pivots = base::Take(asc::RawLapackPivotView::Create(
            p, n, asc::LapackFactorFamily::kRook, {p, 128, base::kHost}));
        const auto invalid = [&] {
          if constexpr (asc::DenseBlasComplex<T>) {
            if (hermitian) {
              return asc::QueryHetri3xWorkspace(provider, triangle, 0, matrix,
                                                extra, pivots);
            }
          }
          return asc::QuerySytri3xWorkspace(provider, triangle, 0, matrix,
                                            extra, pivots);
        }();
        ASC_DENSE_TEST_CHECK(test, !invalid.ok());
        const auto plan = base::Take(base::WithoutAllocation(test, [&] {
          return Query(provider, triangle, hermitian, matrix, extra, pivots);
        }));
        if (n == 0) {
          asc::LapackReport report;
          const auto status = Inverse(provider, triangle, hermitian, matrix,
                                      extra, pivots, plan, {}, report);
          ASC_DENSE_TEST_CHECK(test, status.ok());
          ASC_DENSE_TEST_CHECK(
              test, !report.called_provider && !report.native_info.has_value());
          ASC_DENSE_TEST_EQ(test, report.output_validity,
                            asc::LapackOutputValidity::kComplete);
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, munmap(memory, bytes), 0);
#else
  (void)test;
  (void)provider;
  (void)hermitian;
#endif
}

template <typename T>
void Counts(TestContext& test, bool hermitian) {
  using Real = asc::DenseBlasRealType<T>;
  namespace counts = asc::internal_indefinite_rk_inverse_counts;
  constexpr asc::extent_t kLimit = std::numeric_limits<lapack_int>::max();
  ASC_DENSE_TEST_EQ(test, *counts::Preferred<Real>(12, kLimit), 12);
  ASC_DENSE_TEST_CHECK(test, !counts::Preferred<Real>(0, kLimit).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Preferred<Real>(-1, kLimit).ok());
  ASC_DENSE_TEST_EQ(test, *counts::Preferred<Real>(67108868, kLimit),
                    sizeof(Real) == 4 ? 67108872 : 67108868);
  ASC_DENSE_TEST_EQ(test,
                    counts::Preferred<Real>((kLimit / 4) * 4, kLimit).ok(),
                    sizeof(lapack_int) == 4 && sizeof(Real) == 8);
  for (const bool upper : {false, true}) {
    auto count = [hermitian, upper](asc::extent_t n, asc::extent_t lda,
                                    asc::extent_t nb, bool explicit_block) {
      return counts::Workspace(n, lda, {hermitian, explicit_block, nb}, upper,
                               kLimit);
    };
    ASC_DENSE_TEST_EQ(test, *count(1, 1, 1, true), 12);
    ASC_DENSE_TEST_EQ(test, *count(2, 2, 64, false), 4489);
    ASC_DENSE_TEST_EQ(test, *count(0, 1, kLimit, true), 0);
    ASC_DENSE_TEST_CHECK(test, !count(1, 1, 0, true).ok());
    ASC_DENSE_TEST_CHECK(test, !count(1, 1, -1, true).ok());
    ASC_DENSE_TEST_CHECK(test, !count(-1, 1, 1, true).ok());
    ASC_DENSE_TEST_CHECK(test, !count(1, 0, 1, true).ok());
    ASC_DENSE_TEST_CHECK(test, !count(1, 1, kLimit - 2, true).ok());
    ASC_DENSE_TEST_CHECK(test, !count(kLimit - 1, 1, 1, true).ok());
    ASC_DENSE_TEST_CHECK(test, count(3, kLimit - 1, 1, true).ok());
    ASC_DENSE_TEST_EQ(test, count(3, kLimit, 1, true).ok(), hermitian && upper);
    // Explicit TRI_3X has no native INTEGER LWORK product. This LP64 example
    // exceeds INTEGER in total entries while every native dimension fits.
    if constexpr (sizeof(lapack_int) == 4) {
      ASC_DENSE_TEST_CHECK(test, count(1, 1, 65536, true).ok());
      ASC_DENSE_TEST_CHECK(test, !count(1, 1, 65536, false).ok());
    }
    ASC_DENSE_TEST_EQ(test, count(kLimit - 2, 1, 1, true).ok(),
                      !upper && sizeof(lapack_int) == 4);
  }
}
template <typename T>
void OriginalStrides(TestContext& test,
                     const asc::ReferenceLapackProvider& provider,
                     bool hermitian) {
  constexpr asc::extent_t kLimit = std::numeric_limits<lapack_int>::max();
  std::array<T, 3> a{};
  std::array<T, 3> e{};
  std::array<asc::index_t, 3> p{};
  int queries = 0;
  for (const auto tri : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const asc::extent_t order : {0, 1}) {
        const auto extra = asc_rk_inverse_test::OffDiagonal(e, order);
        const auto pivots = asc_indefinite_rook_test::Raw(p, order);
        const auto matrix = base::Matrix(a, order, order, layout, kLimit);
        ASC_DENSE_TEST_CHECK(
            test, Query(provider, tri, hermitian, matrix, extra, pivots).ok());
        ++queries;
        if constexpr (sizeof(lapack_int) == 4) {
          const auto oversized =
              base::Matrix(a, order, order, layout, kLimit + 1);
          const auto result =
              Query(provider, tri, hermitian, oversized, extra, pivots);
          ASC_DENSE_TEST_CHECK(
              test, !result.ok() &&
                        result.status().code() == asc::ErrorCode::kOverflow);
          ++queries;
        }
      }
    }
  }
  std::printf("original_stride_queries=%d\n", queries);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (int bad = 0; bad < 40; ++bad) {
        Rejection<T>(test, provider, hermitian, triangle, layout, bad);
        ++cases;
      }
    }
  }
  Counts<T>(test, hermitian);
  Unread<T>(test, provider, hermitian);
  OriginalStrides<T>(test, provider, hermitian);
  std::printf("RK inverse rejection cases=%d\n", cases);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3 || !asc_rk_inverse_test::Select(argv[2])) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false);
  }
  if (scalar == "d") {
    return Run<double>(false);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true);
  }
  return 2;
}
