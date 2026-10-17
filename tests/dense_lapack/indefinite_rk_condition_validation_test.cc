#include <array>
#include <cmath>
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
#include "../../src/dense/lapack/internal_indefinite_expert_counts.h"
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
#include "indefinite_rk_condition_test_support.h"
#include "indefinite_rk_solve_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_indefinite_rook_test;
using asc_rk_condition_test::Condition;
using asc_rk_condition_test::Query;
using asc_rk_condition_test::Scratch;
using base::TestContext;

template <typename T>
void AlterWorkspace(int bad, asc::LapackWorkspacePlan& plan,
                    asc::LapackWorkspace& workspace, Scratch<T>& scratch,
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
      bad == 46 || bad == 58 || bad == 59 || bad == 60) {
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
      pivots = base::Raw(p, 1);
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
          asc::LapackFactorFamily::kRook, {a.data(), sizeof(a), base::kHost}));
      break;
    default:
      break;
  }
}

template <typename T>
void AlterConditionWorkspace(int bad, std::array<T, 16>& a,
                             std::array<T, 16>& b,
                             asc::LapackWorkspace& workspace,
                             Scratch<T>& scratch) {
  switch (bad) {
    case 29:
      workspace.regions[base::kLayout] = {nullptr, 0, base::kHost};
      break;
    case 30:
      workspace.regions[base::kLayout] = {
          scratch.packed.data() + 1,
          workspace.regions[base::kLayout].size() - sizeof(T), base::kHost};
      break;
    case 37:
      workspace.regions[base::kScalar] = {b.data() + 1, 4 * sizeof(T),
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
    case 52:
      workspace.regions[base::kScalar] = {
          scratch.scalar.data() + 1,
          workspace.regions[base::kScalar].size() - sizeof(T), base::kHost};
      break;
    case 53:
      workspace.regions[base::kScalar] = {
          reinterpret_cast<std::byte*>(scratch.scalar.data() + 1) + 1,
          workspace.regions[base::kScalar].size(), base::kHost};
      break;
    case 54:
      workspace.regions[base::kPivot] = {
          scratch.pivot.data() + 16,
          workspace.regions[base::kPivot].size() - sizeof(lapack_int),
          base::kHost};
      break;
    case 55:
      workspace.regions[base::kScalar] = workspace.regions[base::kPivot];
      break;
    default:
      break;
  }
}
template <typename T>
void AlterCondition(int bad, const asc::ReferenceLapackProvider& provider,
                    std::array<T, 16>& a, std::array<T, 5>& e,
                    std::array<asc::index_t, 5>& p,
                    asc::DenseBlasRealType<T>*& condition,
                    asc::DenseBlasRealType<T>& norm,
                    asc::LapackWorkspacePlan& plan,
                    asc::LapackWorkspace& workspace, Scratch<T>& scratch,
                    asc::LapackReport& report) {
  using Real = asc::DenseBlasRealType<T>;
  switch (bad) {
    case 31:
      condition = reinterpret_cast<Real*>(a.data() + 1);
      break;
    case 32:
      condition = reinterpret_cast<Real*>(p.data() + 1);
      break;
    case 33:
      norm = 0;
      break;
    case 34:
      norm = -1;
      break;
    case 35:
      norm = std::numeric_limits<Real>::quiet_NaN();
      break;
    case 36:
      norm = std::numeric_limits<Real>::infinity();
      break;
    case 40:
      condition = reinterpret_cast<Real*>(&report);
      break;
    case 56:
      condition = reinterpret_cast<Real*>(scratch.scalar.data() + 1);
      break;
    case 57:
      condition = reinterpret_cast<Real*>(e.data() + 1);
      break;
    case 58:
      condition = reinterpret_cast<Real*>(
          const_cast<asc::ReferenceLapackProvider*>(&provider));
      break;
    case 59:
      condition = reinterpret_cast<Real*>(&plan);
      break;
    case 60:
      condition = reinterpret_cast<Real*>(&workspace);
      break;
    default:
      break;
  }
}
template <typename T>
void AlterExtra(int bad, const asc::ReferenceLapackProvider& provider,
                std::array<T, 5>& e, std::array<T, 16>& a, std::array<T, 16>& b,
                std::array<asc::index_t, 5>& p,
                asc::DenseBlasVectorView<const T>& extra,
                asc::LapackWorkspacePlan& plan, asc::LapackWorkspace& workspace,
                asc::LapackReport& report) {
  switch (bad) {
    case 0:
      extra = base::Take(asc::DenseBlasVectorView<const T>::Create(
          e.data() + 1, 2, 2, {e.data(), sizeof(e), base::kHost}));
      break;
    case 26:
      extra = asc_rk_solve_test::OffDiagonal(e, 1);
      break;
    case 41:
      extra = asc_rk_solve_test::OffDiagonal(a, 2);
      break;
    case 42:
      extra = asc_rk_solve_test::OffDiagonal(b, 2);
      break;
    case 43:
      extra = base::Take(asc::DenseBlasVectorView<const T>::Create(
          reinterpret_cast<const T*>(p.data() + 1), 2, 1,
          {p.data(), sizeof(p), base::kHost}));
      break;
    case 44:
      extra = base::Take(asc::DenseBlasVectorView<const T>::Create(
          reinterpret_cast<const T*>(&report), 2, 1,
          {&report, sizeof(report), base::kHost}));
      break;
    case 45:
      extra = base::Take(asc::DenseBlasVectorView<const T>::Create(
          reinterpret_cast<const T*>(&provider), 2, 1,
          {&provider, sizeof(provider), base::kHost}));
      break;
    case 46:
      extra = base::Take(asc::DenseBlasVectorView<const T>::Create(
          reinterpret_cast<const T*>(&plan), 2, 1,
          {&plan, sizeof(plan), base::kHost}));
      break;
    case 47:
      workspace.regions[base::kPivot] = {
          e.data() + 1, workspace.regions[base::kPivot].size(), base::kHost};
      break;
    case 48:
      workspace.regions[base::kScalar] = {e.data() + 1, 2 * sizeof(T),
                                          base::kHost};
      break;
    case 49:
      extra = base::Take(asc::DenseBlasVectorView<const T>::Create(
          e.data() + 1, 2, 1,
          {e.data(), sizeof(e), asc::MemorySpace::kPinnedHost}));
      break;
    case 50:
      extra = asc_rk_solve_test::OffDiagonal(e, 3);
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
               asc::DenseBlasLayout layout, int bad) {
  alignas(16) std::array<T, 16> a;
  a.fill(base::Value<T>(std::numeric_limits<double>::quiet_NaN(), 13));
  alignas(16) std::array<asc::index_t, 5> p{-113, 1, 2, -113, -113};
  std::array<T, 5> e;
  e.fill(base::Value<T>(std::numeric_limits<double>::quiet_NaN(), 19));
  auto extra = asc_rk_solve_test::OffDiagonal(e, 2);
  std::array<T, 16> b;
  b.fill(base::Value<T>(std::numeric_limits<double>::quiet_NaN(), 17));
  using Real = asc::DenseBlasRealType<T>;
  Real* condition = reinterpret_cast<Real*>(b.data() + 1);
  Real norm = 1;
  auto matrix = base::Matrix(std::as_const(a), 2, 2, layout, 3);
  auto pivots = base::Raw(p, 2);
  auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return Query(provider, triangle, hermitian, matrix, extra, pivots, norm,
                 *condition);
  }));
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kScalar].minimum_entries, 4);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kPivot].minimum_entries,
                    asc::DenseBlasComplex<T> ? 2 : 4);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kLayout].minimum_entries,
                    (layout == base::kRow ? 4 : 0));
  Scratch<T> scratch;
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
  AlterConditionWorkspace(bad, a, b, workspace, scratch);
  AlterCondition(bad, provider, a, e, p, condition, norm, plan, workspace,
                 scratch, report);
  AlterExtra(bad, provider, e, a, b, p, extra, plan, workspace, report);
  const auto before_e = e;
  const auto before_b = b;
  const auto before_a = a;
  const auto before_p = p;
  const auto before_scratch = scratch;
  const auto status = base::WithoutAllocation(test, [&] {
    return Condition(provider, selected, hermitian, matrix, extra, pivots, norm,
                     *condition, plan, workspace, report);
  });
  RejectedReport(test, bad, status, report, saved_report);
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(a.data(), before_a.data(), sizeof(a)));
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(b.data(), before_b.data(), sizeof(b)));
  ASC_DENSE_TEST_EQ(test, p, before_p);
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(e.data(), before_e.data(), sizeof(e)));
  ASC_DENSE_TEST_EQ(test, scratch.scalar, before_scratch.scalar);
  ASC_DENSE_TEST_EQ(test, scratch.packed, before_scratch.packed);
  ASC_DENSE_TEST_EQ(test, scratch.pivot, before_scratch.pivot);
}
template <typename T>
int OriginalStrides(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    bool hermitian, const T* a, const T* e,
                    const asc::index_t* p,
                    const asc::DenseBlasRealType<T>& condition) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr auto kLimit = asc::internal_indefinite::kIntegerLimit;
  const auto extra = base::Take(asc::DenseBlasVectorView<const T>::Create(
      e, 1, 1, {e, sizeof(T), base::kHost}));
  const auto pivots = base::Take(
      asc::RawLapackPivotView::Create(p, 1, asc::LapackFactorFamily::kRook,
                                      {p, sizeof(asc::index_t), base::kHost}));
  int checks = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      const auto query = [&](asc::extent_t stride) {
        const auto matrix =
            base::Take(asc::DenseBlasMatrixView<const T>::Create(
                a, 1, 1, layout, stride, {a, sizeof(T), base::kHost}));
        return base::WithoutAllocation(test, [&] {
          return Query(provider, triangle, hermitian, matrix, extra, pivots,
                       Real{1}, condition);
        });
      };
      ASC_DENSE_TEST_CHECK(test, query(kLimit).ok());
      ++checks;
      if constexpr (kLimit < std::numeric_limits<asc::extent_t>::max()) {
        const auto outside =
            static_cast<asc::extent_t>(static_cast<std::uint64_t>(kLimit) + 1U);
        ASC_DENSE_TEST_EQ(test, query(outside).status().code(),
                          asc::ErrorCode::kOverflow);
        ++checks;
      }
    }
  }
  return checks;
}
template <typename T>
void Unread(TestContext& test, const asc::ReferenceLapackProvider& provider,
            bool hermitian) {
#if defined(__linux__)
  using Real = asc::DenseBlasRealType<T>;
  const auto page = sysconf(_SC_PAGESIZE);
  ASC_DENSE_TEST_CHECK(test, page >= 2048);
  if (page < 2048) {
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
  auto* e = reinterpret_cast<T*>(static_cast<std::byte*>(memory) + 1024);
  const auto* protected_condition =
      reinterpret_cast<const Real*>(static_cast<std::byte*>(memory) + 1536);
  int queries = 0;
  int empty = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const asc::extent_t n : {0, 1, 2}) {
        for (const Real norm : {Real{}, Real{1}}) {
          const auto matrix =
              base::Take(asc::DenseBlasMatrixView<const T>::Create(
                  a, n, n, layout, 2, {a, 128, base::kHost}));
          const auto extra =
              base::Take(asc::DenseBlasVectorView<const T>::Create(
                  e, n, 1, {e, 128, base::kHost}));
          const auto pivots = base::Take(asc::RawLapackPivotView::Create(
              p, n, asc::LapackFactorFamily::kRook, {p, 128, base::kHost}));
          const auto plan = base::Take(base::WithoutAllocation(test, [&] {
            return Query(provider, triangle, hermitian, matrix, extra, pivots,
                         norm, *protected_condition);
          }));
          ++queries;
          if (n == 0 || norm == 0) {
            Real condition = -13;
            asc::LapackReport report;
            const auto status = base::WithoutAllocation(test, [&] {
              return Condition(provider, triangle, hermitian, matrix, extra,
                               pivots, norm, condition, plan, {}, report);
            });
            ASC_DENSE_TEST_CHECK(test, status.ok());
            ASC_DENSE_TEST_CHECK(test, !report.called_provider &&
                                           !report.native_info.has_value());
            ASC_DENSE_TEST_EQ(test, condition, n == 0 ? Real{1} : Real{});
            ASC_DENSE_TEST_EQ(test, report.factor_family,
                              asc::LapackFactorFamily::kRook);
            ASC_DENSE_TEST_EQ(test, report.output_validity,
                              asc::LapackOutputValidity::kComplete);
            ++empty;
          }
        }
      }
    }
  }
  const auto strides =
      OriginalStrides(test, provider, hermitian, a, e, p, *protected_condition);
  std::printf(
      "RK condition protected_queries=%d empty_noncalls=%d "
      "original_stride_checks=%d\n",
      queries, empty, strides);
  ASC_DENSE_TEST_EQ(test, queries, 24);
  ASC_DENSE_TEST_EQ(test, empty, 16);
  ASC_DENSE_TEST_EQ(test, munmap(memory, bytes), 0);
#else
  (void)test;
  (void)provider;
  (void)hermitian;
#endif
}
void Counts(TestContext& test) {
  for (const asc::extent_t limit : {static_cast<asc::extent_t>(INT32_MAX),
                                    static_cast<asc::extent_t>(INT64_MAX)}) {
    const auto count = [limit](asc::extent_t n) {
      return asc::internal_indefinite_expert_counts::Estimator(n, limit);
    };
    ASC_DENSE_TEST_CHECK(test, count(0).ok());
    ASC_DENSE_TEST_CHECK(test, count(limit / 3).ok());
    ASC_DENSE_TEST_EQ(test, count(limit / 3 + 1).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, count(limit).code(), asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, count(-1).code(), asc::ErrorCode::kInvalidArgument);
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
      for (int bad = 0; bad < 61; ++bad) {
        // Column A requires no layout region; test these missing/short/alias
        // regions on row A.
        if (layout == base::kColumn &&
            (bad == 29 || bad == 30 || bad == 38 || bad == 39)) {
          continue;
        }
        Rejection<T>(test, provider, hermitian, triangle, layout, bad);
        ++cases;
      }
    }
  }
  Counts(test);
  Unread<T>(test, provider, hermitian);
  std::printf("RK condition rejection cases=%d count_checks=10\n", cases);
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
