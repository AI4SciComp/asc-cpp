#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "../../src/dense/lapack/internal_indefinite_aasen_two_stage_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_driver_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_two_stage_driver_test;
using base::TestContext;
template <typename T>
void WorkspaceFault(int bad, asc::LapackWorkspacePlan& plan,
                    asc::LapackWorkspace& workspace, base::Scratch<T>& scratch,
                    std::array<T, 64>& a, std::array<asc::index_t, 16>& p) {
  const auto scalar = workspace.regions[base::kScalar];
  const auto pivot = workspace.regions[base::kPivot];
  switch (bad) {
    case 0:
      workspace.regions[base::kScalar] = {scalar.data(), scalar.size() - 1,
                                          base::kHost};
      break;
    case 1:
      workspace.regions[base::kScalar] = {
          reinterpret_cast<std::byte*>(scalar.data()) + 1, scalar.size(),
          base::kHost};
      break;
    case 2:
      workspace.regions[base::kPivot] = {pivot.data(), pivot.size() - 1,
                                         base::kHost};
      break;
    case 3:
      workspace.regions[base::kPivot] = {scratch.pivot.data() + 17,
                                         pivot.size(), base::kHost};
      break;
    case 4:
      workspace.regions[base::kScalar] = {a.data() + 1, scalar.size(),
                                          base::kHost};
      break;
    case 5:
      workspace.regions[base::kPivot] = {p.data() + 1, pivot.size(),
                                         base::kHost};
      break;
    case 6:
      workspace.regions[base::kPivot] = {scalar.data(), pivot.size(),
                                         base::kHost};
      break;
    case 7:
      workspace.regions[base::kScalar] = {scalar.data(), scalar.size(),
                                          asc::MemorySpace::kPinnedHost};
      break;
    case 8:
      ++plan.regions[base::kScalar].minimum_entries;
      break;
    case 9:
      ++plan.regions[base::kScalar].preferred_entries;
      break;
    case 10:
      ++plan.regions[base::kPivot].entry_bytes;
      break;
    case 11:
      ++plan.regions[base::kPivot].alignment;
      break;
    case 12:
      plan.total_byte_limit = 1;
      break;
    case 13:
      workspace.regions.back() = pivot;
      break;
    case 14:
      if (plan.regions[base::kLayout].minimum_entries != 0) {
        workspace.regions[base::kLayout] = {nullptr, 0, base::kHost};
      } else {
        plan.regions[base::kLayout].minimum_entries = 1;
      }
      break;
    default:
      break;
  }
}
template <typename T>
void OperandFault(int bad, asc::DenseBlasTriangle tri,
                  asc::DenseBlasLayout layout, std::array<T, 64>& a,
                  std::array<asc::index_t, 16>& p,
                  asc::DenseBlasMatrixView<T>& matrix,
                  asc::DenseBlasVectorView<asc::index_t>& pivots,
                  asc::DenseBlasTriangle& selected,
                  asc::LapackWorkspacePlan& plan,
                  asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  switch (bad) {
    case 15:
      selected = tri == base::kUpper ? base::kLower : base::kUpper;
      break;
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    case 16:
      selected = static_cast<asc::DenseBlasTriangle>(99);
      break;
    case 17:
      matrix = base::Matrix(a, 2, 1, layout, 3);
      break;
    case 18:
      matrix = base::Matrix(a, 2, 2, layout, 4);
      break;
    case 19:
      pivots = base::Pivots(p, 1);
      break;
    case 20:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          p.data() + 1, 2, 2, {p.data(), sizeof(p), base::kHost}));
      break;
    case 21:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          reinterpret_cast<asc::index_t*>(a.data() + 2), 2, 1,
          {a.data(), sizeof(a), base::kHost}));
      break;
    case 22:
      matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
          a.data() + 1, 2, 2, layout, 3,
          {a.data(), sizeof(a), asc::MemorySpace::kPinnedHost}));
      break;
    case 23:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          p.data() + 1, 2, 1,
          {p.data(), sizeof(p), asc::MemorySpace::kPinnedHost}));
      break;
    case 24:
      matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(&report), 2, 2, layout, 3,
          {&report, sizeof(report), base::kHost}));
      break;
    case 25:
      workspace.regions[base::kScalar] = {
          &report, workspace.regions[base::kScalar].size(), base::kHost};
      break;
    case 26:
      workspace.regions[base::kPivot] = {
          &plan, workspace.regions[base::kPivot].size(), base::kHost};
      break;
    default:
      break;
  }
}
template <typename T>
void ExtraOperandFault(int bad, asc::DenseBlasLayout layout,
                       std::array<T, 64>& a, std::array<T, 128>& tb,
                       std::array<asc::index_t, 16>& p,
                       std::array<asc::index_t, 16>& q,
                       asc::DenseBlasMatrixView<T>& matrix,
                       asc::DenseBlasVectorView<T>& band,
                       asc::DenseBlasVectorView<asc::index_t>& pivots,
                       asc::DenseBlasVectorView<asc::index_t>& band_pivots) {
  switch (bad) {
    case 27:
      band = base::Take(asc::DenseBlasVectorView<T>::Create(
          tb.data() + 1, 7, 1, {tb.data(), sizeof(tb), base::kHost}));
      break;
    case 28:
      band = base::Take(asc::DenseBlasVectorView<T>::Create(
          tb.data() + 1, 8, 2, {tb.data(), sizeof(tb), base::kHost}));
      break;
    case 29:
      band = base::Take(asc::DenseBlasVectorView<T>::Create(
          a.data() + 2, 8, 1, {a.data(), sizeof(a), base::kHost}));
      break;
    case 30:
      band_pivots = base::Pivots(p, 2);
      break;
    case 31:
      band_pivots = base::Pivots(q, 1);
      break;
    case 32:
      band_pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          q.data() + 1, 2, 2, {q.data(), sizeof(q), base::kHost}));
      break;
    case 33:
      band_pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          q.data() + 1, 2, 1,
          {q.data(), sizeof(q), asc::MemorySpace::kPinnedHost}));
      break;
    case 34:
      band = base::Take(asc::DenseBlasVectorView<T>::Create(
          tb.data() + 1, 8, 1,
          {tb.data(), sizeof(tb), asc::MemorySpace::kPinnedHost}));
      break;
    case 35:
      band_pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          reinterpret_cast<asc::index_t*>(a.data() + 2), 2, 1,
          {a.data(), sizeof(a), base::kHost}));
      break;
    case 36:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          reinterpret_cast<asc::index_t*>(tb.data() + 2), 2, 1,
          {tb.data(), sizeof(tb), base::kHost}));
      break;
    case 37:
      band_pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          reinterpret_cast<asc::index_t*>(tb.data() + 2), 2, 1,
          {tb.data(), sizeof(tb), base::kHost}));
      break;
    case 38:
      band = base::Take(asc::DenseBlasVectorView<T>::Create(
          tb.data() + 1, 10, 1, {tb.data(), sizeof(tb), base::kHost}));
      break;
    case 39:
      band_pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          q.data() + 2, 2, -1, {q.data(), sizeof(q), base::kHost}));
      break;
    case 40:
      band = base::Take(asc::DenseBlasVectorView<T>::Create(
          tb.data() + 8, 8, -1, {tb.data(), sizeof(tb), base::kHost}));
      break;
    case 41:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          p.data() + 2, 2, -1, {p.data(), sizeof(p), base::kHost}));
      break;
    case 42:
      matrix = base::Matrix(
          a, 2, 2, layout == base::kRow ? base::kColumn : base::kRow, 3);
      break;
    default:
      break;
  }
}
template <typename T>
void ExtraAliasFault(int bad, std::array<T, 64>& a, std::array<T, 128>& tb,
                     std::array<asc::index_t, 16>& p,
                     std::array<asc::index_t, 16>& q,
                     asc::DenseBlasVectorView<T>& band,
                     asc::DenseBlasVectorView<asc::index_t>& pivots,
                     asc::DenseBlasVectorView<asc::index_t>& band_pivots,
                     asc::LapackWorkspacePlan& plan,
                     asc::LapackWorkspace& workspace,
                     asc::LapackReport& report) {
  const auto scalar = workspace.regions[base::kScalar];
  const auto pivot = workspace.regions[base::kPivot];
  switch (bad) {
    case 43:
      workspace.regions[base::kPivot] = {q.data() + 1, pivot.size(),
                                         base::kHost};
      break;
    case 44:
      workspace.regions[base::kScalar] = {tb.data() + 1, scalar.size(),
                                          base::kHost};
      break;
    case 45:
      workspace.regions[base::kScalar] = {q.data() + 1, scalar.size(),
                                          base::kHost};
      break;
    case 46:
      workspace.regions[base::kLayout] = {tb.data() + 1, 4 * sizeof(T),
                                          base::kHost};
      break;
    case 47:
      workspace.regions[base::kLayout] = {a.data() + 1, 4 * sizeof(T),
                                          base::kHost};
      break;
    case 48:
      workspace.regions[base::kLayout] = {p.data() + 1, 4 * sizeof(T),
                                          base::kHost};
      break;
    case 49:
      workspace.regions[base::kLayout] = {q.data() + 1, 4 * sizeof(T),
                                          base::kHost};
      break;
    case 50:
      workspace.regions[base::kLayout] = scalar;
      break;
    case 51:
      workspace.regions[base::kLayout] = pivot;
      break;
    case 52:
      band = base::Take(asc::DenseBlasVectorView<T>::Create(
          reinterpret_cast<T*>(&report), 8, 1,
          {&report, sizeof(report), base::kHost}));
      break;
    case 53:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          reinterpret_cast<asc::index_t*>(&report), 2, 1,
          {&report, sizeof(report), base::kHost}));
      break;
    case 54:
      band_pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          reinterpret_cast<asc::index_t*>(&report), 2, 1,
          {&report, sizeof(report), base::kHost}));
      break;
    case 55:
      band = base::Take(asc::DenseBlasVectorView<T>::Create(
          reinterpret_cast<T*>(&plan), 8, 1,
          {&plan, sizeof(plan), base::kHost}));
      break;
    case 56:
      workspace.regions[base::kScalar] = {&workspace, scalar.size(),
                                          base::kHost};
      break;
    case 57:
      band_pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          reinterpret_cast<asc::index_t*>(&plan), 2, 1,
          {&plan, sizeof(plan), base::kHost}));
      break;
    default:
      break;
  }
}
template <typename T>
void RhsWorkspaceFault(int bad, const asc::ReferenceLapackProvider& provider,
                       asc::DenseBlasLayout layout, std::array<T, 64>& b,
                       asc::DenseBlasMatrixView<T>& rhs,
                       asc::LapackWorkspacePlan& plan,
                       asc::LapackWorkspace& workspace,
                       asc::LapackReport& report) {
  const auto scalar = workspace.regions[base::kScalar];
  const auto pivot = workspace.regions[base::kPivot];
  const auto packed = workspace.regions[base::kLayout];
  switch (bad) {
    case 67:
      workspace.regions[base::kScalar] = {b.data() + 1, scalar.size(),
                                          base::kHost};
      break;
    case 68:
      workspace.regions[base::kPivot] = {b.data() + 1, pivot.size(),
                                         base::kHost};
      break;
    case 69:
      workspace.regions[base::kLayout] = {b.data() + 1, 4 * sizeof(T),
                                          base::kHost};
      break;
    case 70:
      if (packed.size() != 0) {
        workspace.regions[base::kLayout] = {packed.data(), packed.size() - 1,
                                            base::kHost};
      } else {
        ++plan.regions[base::kLayout].minimum_entries;
      }
      break;
    case 71:
      if (packed.size() != 0) {
        workspace.regions[base::kLayout] = {
            static_cast<std::byte*>(packed.data()) + 1, packed.size(),
            base::kHost};
      } else {
        ++plan.regions[base::kLayout].minimum_entries;
      }
      break;
    case 72:
      rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(&report), 2, 2, layout, 3,
          {&report, sizeof(report), base::kHost}));
      break;
    case 73:
      rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(&plan), 2, 2, layout, 3,
          {&plan, sizeof(plan), base::kHost}));
      break;
    case 74:
      rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(&workspace), 2, 2, layout, 3,
          {&workspace, sizeof(workspace), base::kHost}));
      break;
    case 75:
      workspace.regions[base::kScalar] = {
          const_cast<asc::ReferenceLapackProvider*>(&provider), scalar.size(),
          base::kHost};
      break;
    default:
      break;
  }
}
template <typename T>
void RhsFault(int bad, const asc::ReferenceLapackProvider& provider,
              asc::DenseBlasLayout layout, std::array<T, 64>& a,
              std::array<T, 64>& b, std::array<T, 128>& tb,
              std::array<asc::index_t, 16>& p, std::array<asc::index_t, 16>& q,
              asc::DenseBlasMatrixView<T>& rhs, asc::LapackWorkspacePlan& plan,
              asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  if (bad >= 67) {
    RhsWorkspaceFault(bad, provider, layout, b, rhs, plan, workspace, report);
    return;
  }
  switch (bad) {
    case 58:
      rhs = base::Matrix(b, 1, 2, layout, 3);
      break;
    case 59:
      rhs = base::Matrix(b, 2, 1, layout, 3);
      break;
    case 60:
      rhs = base::Matrix(b, 2, 2, layout, 4);
      break;
    case 61:
      rhs = base::Matrix(b, 2, 2,
                         layout == base::kRow ? base::kColumn : base::kRow, 3);
      break;
    case 62:
      rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
          b.data() + 1, 2, 2, layout, 3,
          {b.data(), sizeof(b), asc::MemorySpace::kPinnedHost}));
      break;
    case 63:
      rhs = base::Matrix(a, 2, 2, layout, 3);
      break;
    case 64:
      rhs = base::Matrix(tb, 2, 2, layout, 3);
      break;
    case 65:
      rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(p.data() + 2), 2, 2, layout, 3,
          {p.data(), sizeof(p), base::kHost}));
      break;
    case 66:
      rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(q.data() + 2), 2, 2, layout, 3,
          {q.data(), sizeof(q), base::kHost}));
      break;
    default:
      break;
  }
}
template <typename T>
void Rejection(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool he, asc::DenseBlasTriangle tri, asc::DenseBlasLayout layout,
               asc::DenseBlasLayout rhs_layout, int bad) {
  alignas(16) std::array<T, 64> a;
  alignas(16) std::array<T, 64> b;
  std::array<asc::index_t, 16> p;
  std::array<asc::index_t, 16> q;
  alignas(16) std::array<T, 128> tb;
  a.fill(base::Value<T>(-31, 11));
  b.fill(base::Value<T>(-33, 9));
  p.fill(-41);
  q.fill(-43);
  tb.fill(base::Value<T>(-47, 13));
  auto matrix = base::Matrix(a, 2, 2, layout, 3);
  auto rhs = base::Matrix(b, 2, 2, rhs_layout, 3);
  auto pivots = base::Pivots(p, 2);
  auto band_pivots = base::Pivots(q, 2);
  auto band = base::Take(asc::DenseBlasVectorView<T>::Create(
      tb.data() + 1, 8, 1, {tb.data(), sizeof(tb), base::kHost}));
  auto plan = base::Take(
      aa::Query(provider, tri, he, matrix, band, pivots, band_pivots, rhs));
  base::Scratch<T> scratch;
  auto workspace =
      scratch.Workspace(plan, plan.regions[base::kScalar].minimum_entries);
  asc::LapackReport report;
  report.native_info = 73;
  auto selected = tri;
  WorkspaceFault(bad, plan, workspace, scratch, a, p);
  OperandFault(bad, tri, layout, a, p, matrix, pivots, selected, plan,
               workspace, report);
  ExtraOperandFault(bad, layout, a, tb, p, q, matrix, band, pivots,
                    band_pivots);
  ExtraAliasFault(bad, a, tb, p, q, band, pivots, band_pivots, plan, workspace,
                  report);
  RhsFault(bad, provider, rhs_layout, a, b, tb, p, q, rhs, plan, workspace,
           report);
  const auto before_b = b;
  const auto before_tb = tb;
  const auto before_q = q;
  const auto before_a = a;
  const auto before_p = p;
  const auto before_scratch = scratch;
  std::array<std::byte, sizeof(report)> before_report;
  std::memcpy(before_report.data(), &report, sizeof(report));
  const auto status = base::WithoutAllocation(test, [&] {
    return aa::Driver(provider, selected, he, matrix, band, pivots, band_pivots,
                      rhs, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  if ((bad >= 24 && bad <= 26) || (bad >= 52 && bad <= 57) || bad >= 72) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(&report, before_report.data(), sizeof(report)));
  } else {
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
  }
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(a.data(), before_a.data(), sizeof(a)));
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(b.data(), before_b.data(), sizeof(b)));
  ASC_DENSE_TEST_EQ(test, p, before_p);
  ASC_DENSE_TEST_EQ(test, q, before_q);
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(tb.data(), before_tb.data(), sizeof(tb)));
  ASC_DENSE_TEST_EQ(test, scratch.scalar, before_scratch.scalar);
  ASC_DENSE_TEST_EQ(test, scratch.packed, before_scratch.packed);
  ASC_DENSE_TEST_EQ(test, scratch.pivot, before_scratch.pivot);
}
template <typename T>
void Unread(TestContext& test, const asc::ReferenceLapackProvider& provider,
            bool he) {
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
  auto* tb = reinterpret_cast<T*>(static_cast<std::byte*>(memory) + 256);
  auto* p =
      reinterpret_cast<asc::index_t*>(static_cast<std::byte*>(memory) + 512);
  auto* q =
      reinterpret_cast<asc::index_t*>(static_cast<std::byte*>(memory) + 768);
  auto* b = reinterpret_cast<T*>(static_cast<std::byte*>(memory) + 1024);
  for (const auto tri : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const auto rhs_layout : {base::kColumn, base::kRow}) {
        for (const asc::extent_t nrhs : {0, 1, 2}) {
          for (const asc::extent_t n : {0, 1, 2}) {
            const auto matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
                a, n, n, layout, 2, {a, 128, base::kHost}));
            const auto rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
                b, n, nrhs, rhs_layout, 2, {b, 128, base::kHost}));
            const auto pivots =
                base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
                    p, n, 1, {p, 128, base::kHost}));
            const auto band = base::Take(asc::DenseBlasVectorView<T>::Create(
                tb, 4 * n, 1, {tb, 128, base::kHost}));
            const auto band_pivots =
                base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
                    q, n, 1, {q, 128, base::kHost}));
            const auto plan = base::Take(base::WithoutAllocation(test, [&] {
              return aa::Query(provider, tri, he, matrix, band, pivots,
                               band_pivots, rhs);
            }));
            if (n == 0) {
              asc::LapackReport report;
              const auto status =
                  aa::Driver(provider, tri, he, matrix, band, pivots,
                             band_pivots, rhs, plan, {}, report);
              ASC_DENSE_TEST_CHECK(test, status.ok());
              ASC_DENSE_TEST_CHECK(test, !report.called_provider &&
                                             !report.native_info.has_value());
            }
          }
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, munmap(memory, bytes), 0);
#else
  (void)test;
  (void)provider;
  (void)he;
#endif
}
template <typename T>
void OriginalLeading(TestContext& test,
                     const asc::ReferenceLapackProvider& provider, bool he) {
  std::array<T, 16> a{};
  std::array<T, 16> b{};
  std::array<T, 16> tb{};
  std::array<asc::index_t, 4> p{};
  std::array<asc::index_t, 4> q{};
  for (const auto tri : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (bool bad_rhs : {false, true}) {
        auto rhs = base::Matrix(b, 1, 1, layout, 1);
        auto matrix = base::Matrix(a, 1, 1, layout, 1);
        const auto band = base::Take(asc::DenseBlasVectorView<T>::Create(
            tb.data() + 1, 4, 1, {tb.data(), sizeof(tb), base::kHost}));
        const auto pivots = base::Pivots(p, 1);
        const auto band_pivots = base::Pivots(q, 1);
        const auto plan = base::Take(aa::Query(provider, tri, he, matrix, band,
                                               pivots, band_pivots, rhs));
        const asc::extent_t leading = ASC_LAPACK_INTEGER_BITS == 32
                                          ? asc::extent_t{INT32_MAX} + 1
                                          : asc::extent_t{INT64_MAX};
        if (bad_rhs) {
          rhs = base::Matrix(b, 1, 1, layout, leading);
        } else {
          matrix = base::Matrix(a, 1, 1, layout, leading);
        }
        const auto changed = base::WithoutAllocation(test, [&] {
          return aa::Query(provider, tri, he, matrix, band, pivots, band_pivots,
                           rhs);
        });
        ASC_DENSE_TEST_EQ(test, changed.ok(), ASC_LAPACK_INTEGER_BITS == 64);
        if (!changed.ok()) {
          ASC_DENSE_TEST_EQ(test, changed.status().code(),
                            asc::ErrorCode::kOverflow);
        }
        base::Scratch<T> scratch;
        auto workspace = scratch.Workspace(plan, 1);
        const auto before = scratch;
        asc::LapackReport report;
        const auto status = base::WithoutAllocation(test, [&] {
          return aa::Driver(provider, tri, he, matrix, band, pivots,
                            band_pivots, rhs, plan, workspace, report);
        });
        ASC_DENSE_TEST_EQ(test, status.code(),
                          ASC_LAPACK_INTEGER_BITS == 32
                              ? asc::ErrorCode::kOverflow
                              : asc::ErrorCode::kInvalidState);
        ASC_DENSE_TEST_CHECK(
            test, !report.called_provider && !report.native_info.has_value());
        ASC_DENSE_TEST_EQ(test, scratch.scalar, before.scalar);
        ASC_DENSE_TEST_EQ(test, scratch.packed, before.packed);
        ASC_DENSE_TEST_EQ(test, scratch.pivot, before.pivot);
        for (const auto value : a) {
          ASC_DENSE_TEST_EQ(test, value, T{});
        }
        for (const auto value : b) {
          ASC_DENSE_TEST_EQ(test, value, T{});
        }
        for (const auto value : tb) {
          ASC_DENSE_TEST_EQ(test, value, T{});
        }
        for (const auto value : p) {
          ASC_DENSE_TEST_EQ(test, value, 0);
        }
        for (const auto value : q) {
          ASC_DENSE_TEST_EQ(test, value, 0);
        }
      }
    }
  }
}
template <typename T>
int Run(bool he) {
  TestContext test;
  int cases = 0;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (const auto tri : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const auto rhs_layout : {base::kColumn, base::kRow}) {
        for (int bad = 0; bad < 76; ++bad) {
          Rejection<T>(test, provider, he, tri, layout, rhs_layout, bad);
          ++cases;
        }
      }
    }
  }
  namespace counts = asc::internal_indefinite_aasen_two_stage_counts;
  using Real = asc::DenseBlasRealType<T>;
  for (const asc::extent_t limit :
       {asc::extent_t{INT32_MAX}, asc::extent_t{INT64_MAX}}) {
    ASC_DENSE_TEST_CHECK(test, counts::Factor(0, 1, 0, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Factor(1, 1, 4, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Factor(1, 1, 3, limit).code(),
                      asc::ErrorCode::kShape);
    ASC_DENSE_TEST_EQ(test, counts::Factor(-1, 1, 4, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, counts::Factor(1, 0, 4, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, counts::Factor(1, 1, -1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, counts::Factor(1, 1, 4, limit - 1).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, counts::Factor(1, 1, limit, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, counts::Factor(2, limit - 1, 8, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Factor(2, limit, 8, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test,
                         counts::Factor(3, (limit - 1) / 2, 12, limit).ok());
    ASC_DENSE_TEST_EQ(test,
                      counts::Factor(3, (limit - 1) / 2 + 1, 12, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test,
        counts::Factor(limit / 192 + 1, limit / 192 + 1, limit - 1, limit)
            .code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test,
                         counts::Preferred<Real>(limit / 384, limit).ok());
    ASC_DENSE_TEST_EQ(
        test, counts::Preferred<Real>(limit / 192 + 1, limit).status().code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, base::Take(counts::Preferred<Real>(0, limit)), 0);
    ASC_DENSE_TEST_EQ(test, base::Take(counts::Preferred<Real>(1, limit)), 192);
    ASC_DENSE_TEST_EQ(test, counts::Preferred<Real>(-1, limit).status().code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test,
                      counts::Preferred<Real>(1, limit - 1).status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  ASC_DENSE_TEST_EQ(test, counts::BlockWidth(3, 12, 3), 1);
  ASC_DENSE_TEST_EQ(test, counts::BlockWidth(3, 1731, 3), 1);
  ASC_DENSE_TEST_EQ(test, counts::BlockWidth(3, 30, 576), 3);
  ASC_DENSE_TEST_EQ(test, counts::BlockWidth(3, 1731, 576), 192);
  Unread<T>(test, provider, he);
  OriginalLeading<T>(test, provider, he);
  std::printf("Two-stage driver validation rejections=%d\n", cases);
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
