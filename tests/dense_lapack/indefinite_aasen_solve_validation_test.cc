#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_aasen_solve_counts.h"
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
#include "indefinite_aasen_solve_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_indefinite_rook_test;
using asc_aasen_solve_test::Query;
using asc_aasen_solve_test::Solve;
using base::TestContext;

template <typename T>
void AlterWorkspace(int bad, asc::LapackWorkspacePlan& plan,
                    asc::LapackWorkspace& workspace, base::Scratch<T>& scratch,
                    std::array<T, 16>& a, std::array<asc::index_t, 5>& p) {
  switch (bad) {
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
  if (bad == 19 || bad == 20 || bad == 40 || bad == 44 || bad == 45 ||
      bad == 46) {
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
                   asc::DenseBlasMatrixView<const T>& matrix,
                   asc::RawLapackPivotView& pivots,
                   asc::LapackWorkspace& workspace, asc::LapackReport& report,
                   asc::DenseBlasTriangle& selected) {
  switch (bad) {
    case 12:
      selected = triangle == base::kUpper ? base::kLower : base::kUpper;
      break;
    case 13:
      matrix = base::Matrix(std::as_const(a), 2, 2, layout, 4);
      break;
    case 14:
      pivots = base::Take(asc::RawLapackPivotView::Create(
          p.data() + 1, 2, asc::LapackFactorFamily::kBunchKaufman,
          {p.data(), sizeof(p), base::kHost}));
      break;
    case 15:
      matrix = base::Matrix(std::as_const(a), 2, 1, layout, 3);
      break;
    case 16:
      pivots = asc_aasen_solve_test::Raw(p, 1);
      break;
    case 17:
      p[1] = 0;
      break;
    case 18:
      matrix = base::Take(asc::DenseBlasMatrixView<const T>::Create(
          a.data() + 1, 2, 2, layout, 3,
          {a.data(), sizeof(a), asc::MemorySpace::kPinnedHost}));
      break;
    case 19:
      workspace.regions[base::kPivot] = {
          &report, workspace.regions[base::kPivot].size(), base::kHost};
      break;
    case 20:
      matrix = base::Take(asc::DenseBlasMatrixView<const T>::Create(
          reinterpret_cast<const T*>(&report), 2, 2, layout, 3,
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
      // Guard negation at the full ASC integer minimum before conversion.
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
          asc::LapackFactorFamily::kAasen, {a.data(), sizeof(a), base::kHost}));
      break;
    default:
      break;
  }
}

template <typename T>
void AlterRhs(int bad, asc::DenseBlasLayout layout, std::array<T, 16>& a,
              std::array<T, 16>& b, std::array<asc::index_t, 5>& p,
              asc::DenseBlasMatrixView<T>& rhs, asc::LapackWorkspace& workspace,
              base::Scratch<T>& scratch, asc::LapackReport& report) {
  switch (bad) {
    case 29:
      workspace.regions[base::kLayout] = {nullptr, 0, base::kHost};
      break;
    case 30:
      workspace.regions[base::kLayout] = {
          scratch.packed.data() + 1,
          workspace.regions[base::kLayout].size() - sizeof(T), base::kHost};
      break;
    case 31:
      rhs = base::Matrix(a, 2, 2, layout, 3);
      break;
    case 32:
      rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(p.data()), 2, 1, base::kColumn, 2,
          {p.data(), sizeof(p), base::kHost}));
      break;
    case 33:
      rhs = base::Matrix(b, 1, 2, layout, 3);
      break;
    case 34:
      rhs = base::Matrix(b, 2, 2, layout, 4);
      break;
    case 35:
      rhs = base::Matrix(b, 2, 1, layout, 3);
      break;
    case 36:
      rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
          b.data() + 1, 2, 2, layout, 3,
          {b.data(), sizeof(b), asc::MemorySpace::kPinnedHost}));
      break;
    case 37:
      workspace.regions[base::kScalar] = {b.data() + 1, 2 * sizeof(T),
                                          base::kHost};
      break;
    case 38:
      workspace.regions[base::kLayout] = {
          a.data() + 1, workspace.regions[base::kLayout].size(), base::kHost};
      break;
    case 39:
      workspace.regions[base::kLayout] = {
          b.data() + 1, workspace.regions[base::kLayout].size(), base::kHost};
      break;
    case 40:
      rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(&report), 2, 2, layout, 3,
          {&report, sizeof(report), base::kHost}));
      break;
    default:
      break;
  }
}

template <typename T>
void AlterExtra(int bad, const asc::ReferenceLapackProvider& provider,
                std::array<T, 16>& a, std::array<T, 16>& b,
                std::array<asc::index_t, 5>& p,
                asc::DenseBlasMatrixView<const T>& matrix,
                asc::RawLapackPivotView& pivots, asc::LapackWorkspacePlan& plan,
                asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  switch (bad) {
    case 0:
      workspace.regions[base::kScalar] = {
          workspace.regions[base::kScalar].data(), 4 * sizeof(T) - 1,
          base::kHost};
      break;
    case 26:
      p[1] = 2;
      break;
    case 41:
      matrix = base::Matrix(std::as_const(b), 2, 2, base::kColumn, 3);
      break;
    case 42:
      workspace.regions[base::kScalar] = {
          workspace.regions[base::kLayout].data(), 4 * sizeof(T), base::kHost};
      break;
    case 43:
      workspace.regions[base::kPivot] = {
          a.data() + 1, workspace.regions[base::kPivot].size(), base::kHost};
      break;
    case 44:
      pivots = base::Take(asc::RawLapackPivotView::Create(
          reinterpret_cast<asc::index_t*>(&report), 2,
          asc::LapackFactorFamily::kAasen,
          {&report, sizeof(report), base::kHost}));
      break;
    case 45:
      matrix = base::Take(asc::DenseBlasMatrixView<const T>::Create(
          reinterpret_cast<const T*>(&provider), 2, 2, base::kColumn, 2,
          {&provider, sizeof(provider), base::kHost}));
      break;
    case 46:
      matrix = base::Take(asc::DenseBlasMatrixView<const T>::Create(
          reinterpret_cast<const T*>(&plan), 2, 2, base::kColumn, 2,
          {&plan, sizeof(plan), base::kHost}));
      break;
    case 47:
      workspace.regions[base::kPivot] = {
          b.data() + 1, workspace.regions[base::kPivot].size(), base::kHost};
      break;
    case 48:
      workspace.regions[base::kScalar] = {
          reinterpret_cast<std::byte*>(
              workspace.regions[base::kScalar].data()) +
              1,
          workspace.regions[base::kScalar].size(), base::kHost};
      break;
    case 49:
      pivots = base::Take(asc::RawLapackPivotView::Create(
          p.data() + 1, 2, asc::LapackFactorFamily::kAasen,
          {p.data(), sizeof(p), asc::MemorySpace::kPinnedHost}));
      break;
    case 50:
      pivots = asc_aasen_solve_test::Raw(p, 3);
      break;
    case 51:
      plan.regions[base::kPivot].alignment *= 2;
      break;
    default:
      break;
  }
}

template <typename T>
void Rejection(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool hermitian, asc::DenseBlasTriangle triangle,
               asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout,
               int bad) {
  alignas(16) std::array<T, 16> a;
  a.fill(base::Value<T>(std::numeric_limits<double>::quiet_NaN(), 13));
  alignas(16) std::array<asc::index_t, 5> p{-113, 1, 2, -113, -113};
  std::array<T, 16> b;
  b.fill(base::Value<T>(std::numeric_limits<double>::quiet_NaN(), 17));
  auto rhs = base::Matrix(b, 2, 2, rhs_layout, 3);
  auto matrix = base::Matrix(std::as_const(a), 2, 2, layout, 3);
  auto pivots = asc_aasen_solve_test::Raw(p, 2);
  auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return Query(provider, hermitian, triangle, matrix, pivots, rhs);
  }));
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kScalar].minimum_entries, 4);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kPivot].minimum_entries, 2);
  ASC_DENSE_TEST_EQ(
      test, plan.regions[base::kLayout].minimum_entries,
      (layout == base::kRow ? 4 : 0) + (rhs_layout == base::kRow ? 4 : 0));
  base::Scratch<T> scratch;
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
  AlterRhs(bad, rhs_layout, a, b, p, rhs, workspace, scratch, report);
  AlterExtra(bad, provider, a, b, p, matrix, pivots, plan, workspace, report);
  const auto before_b = b;
  const auto before_a = a;
  const auto before_p = p;
  const auto before_scratch = scratch;
  const auto status = base::WithoutAllocation(test, [&] {
    return Solve(provider, hermitian, selected, matrix, pivots, rhs, plan,
                 workspace, report);
  });
  RejectedReport(test, bad, status, report, saved_report);
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(a.data(), before_a.data(), sizeof(a)));
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(b.data(), before_b.data(), sizeof(b)));
  ASC_DENSE_TEST_EQ(test, p, before_p);
  ASC_DENSE_TEST_EQ(test, scratch.scalar, before_scratch.scalar);
  ASC_DENSE_TEST_EQ(test, scratch.packed, before_scratch.packed);
  ASC_DENSE_TEST_EQ(test, scratch.pivot, before_scratch.pivot);
}
template <typename T>
int OriginalStrides(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    bool hermitian, const T* a, const asc::index_t* p, T* b) {
  constexpr auto kLimit = asc::internal_indefinite::kIntegerLimit;
  const auto pivots = base::Take(
      asc::RawLapackPivotView::Create(p, 1, asc::LapackFactorFamily::kAasen,
                                      {p, sizeof(asc::index_t), base::kHost}));
  int checks = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const auto rhs_layout : {base::kColumn, base::kRow}) {
        for (const bool large_a : {false, true}) {
          const auto query = [&](asc::extent_t original_stride) {
            const auto matrix =
                base::Take(asc::DenseBlasMatrixView<const T>::Create(
                    a, 1, 1, layout, large_a ? original_stride : 1,
                    {a, sizeof(T), base::kHost}));
            const auto rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
                b, 1, 1, rhs_layout, large_a ? 1 : original_stride,
                {b, sizeof(T), base::kHost}));
            return base::WithoutAllocation(test, [&] {
              return Query(provider, hermitian, triangle, matrix, pivots, rhs);
            });
          };
          // A one-column descriptor uses compact effective LD=1 in the
          // reviewed shared packing helper. Original LD remains range checked.
          ASC_DENSE_TEST_CHECK(test, query(kLimit).ok());
          ++checks;
          if constexpr (kLimit < std::numeric_limits<asc::extent_t>::max()) {
            const auto outside = static_cast<asc::extent_t>(
                static_cast<std::uint64_t>(kLimit) + 1U);
            ASC_DENSE_TEST_EQ(test, query(outside).status().code(),
                              asc::ErrorCode::kOverflow);
            ++checks;
          }
        }
      }
    }
  }
  return checks;
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
  auto* p =
      reinterpret_cast<asc::index_t*>(static_cast<std::byte*>(memory) + 512);
  auto* b = reinterpret_cast<T*>(static_cast<std::byte*>(memory) + 1024);
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const auto rhs_layout : {base::kColumn, base::kRow}) {
        for (const asc::extent_t n : {0, 1}) {
          for (const asc::extent_t nrhs : {0, 1}) {
            const auto matrix =
                base::Take(asc::DenseBlasMatrixView<const T>::Create(
                    a, n, n, layout, 1, {a, 128, base::kHost}));
            const auto pivots = base::Take(asc::RawLapackPivotView::Create(
                p, n, asc::LapackFactorFamily::kAasen, {p, 128, base::kHost}));
            const auto rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
                b, n, nrhs, rhs_layout, 1, {b, 128, base::kHost}));
            const auto plan = base::Take(base::WithoutAllocation(test, [&] {
              return Query(provider, hermitian, triangle, matrix, pivots, rhs);
            }));
            if (n == 0 || nrhs == 0) {
              asc::LapackReport report;
              const auto status = Solve(provider, hermitian, triangle, matrix,
                                        pivots, rhs, plan, {}, report);
              ASC_DENSE_TEST_CHECK(test, status.ok());
              ASC_DENSE_TEST_CHECK(test, !report.called_provider &&
                                             !report.native_info.has_value());
              ASC_DENSE_TEST_EQ(test, report.output_validity,
                                asc::LapackOutputValidity::kComplete);
            }
          }
        }
      }
    }
  }
  const auto stride_checks =
      OriginalStrides(test, provider, hermitian, a, p, b);
  std::printf("protected_original_stride_checks=%d\n", stride_checks);
  ASC_DENSE_TEST_EQ(test, munmap(memory, bytes), 0);
#else
  (void)test;
  (void)provider;
  (void)hermitian;
#endif
}

void Counts(TestContext& test) {
  namespace counts = asc::internal_indefinite_aasen_solve_counts;
  for (const asc::extent_t limit : {static_cast<asc::extent_t>(INT32_MAX),
                                    static_cast<asc::extent_t>(INT64_MAX)}) {
    const auto count = [limit](asc::extent_t n, asc::extent_t nrhs,
                               asc::extent_t lda, asc::extent_t ldb) {
      return counts::Solve(n, nrhs, lda, ldb, limit);
    };
    ASC_DENSE_TEST_CHECK(test, count(limit / 3, 1, limit - 1, 1).ok());
    ASC_DENSE_TEST_EQ(test, count(limit / 3 + 1, 1, 1, 1).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, count(limit, 0, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, count(0, limit, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, count(1, limit - 1, 1, limit).ok());
    ASC_DENSE_TEST_EQ(test, count(1, limit, 1, 1).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, count(1, 1, limit, 1).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, count(2, 1, 2, limit - 1).ok());
    ASC_DENSE_TEST_EQ(test, count(2, 1, 2, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, count(2, (limit - 1) / 3, 2, 3).ok());
    ASC_DENSE_TEST_EQ(test, count(2, (limit - 1) / 3 + 1, 2, 3).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, count(-1, 1, 1, 1).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, count(1, -1, 1, 1).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, count(1, 1, 0, 1).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, count(1, 1, 1, 0).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, *counts::Preferred<float>(1, limit), 1);
    ASC_DENSE_TEST_EQ(test, *counts::Preferred<double>(1, limit), 1);
    ASC_DENSE_TEST_EQ(test, counts::Preferred<float>(0, limit).status().code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(
        test, counts::Preferred<double>(limit / 3 + 1, limit).status().code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test, counts::Preferred<float>(limit / 3, limit).status().code(),
        asc::ErrorCode::kOverflow);
    const auto n = (asc::extent_t{1} << 24) / 3 + 2;
    ASC_DENSE_TEST_CHECK(test,
                         *counts::Preferred<float>(n, limit) >= 3 * n - 2);
  }
}
template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const auto rhs_layout : {base::kColumn, base::kRow}) {
        for (int bad = 0; bad < 52; ++bad) {
          // No layout workspace exists when both arrays are column-major.
          if (layout == base::kColumn && rhs_layout == base::kColumn &&
              (bad == 29 || bad == 30 || bad == 38 || bad == 39)) {
            continue;
          }
          Rejection<T>(test, provider, hermitian, triangle, layout, rhs_layout,
                       bad);
          ++cases;
        }
      }
    }
  }
  Counts(test);
  Unread<T>(test, provider, hermitian);
  std::printf(
      "Aasen solve rejection cases=%d protected_queries=32 empty_noncalls=24 "
      "count_checks=42\n",
      cases);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
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
