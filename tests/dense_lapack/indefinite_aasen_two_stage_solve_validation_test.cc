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
#include "../../src/dense/lapack/internal_indefinite_aasen_two_stage_solve_counts.h"
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
#include "indefinite_aasen_two_stage_solve_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_two_stage_solve_test;
using base::TestContext;
template <typename T>
struct Data {
  alignas(16) std::array<T, 128> a{};
  alignas(16) std::array<T, 128> b{};
  alignas(16) std::array<T, 128> tb{};
  alignas(16) std::array<asc::index_t, 32> p{};
  alignas(16) std::array<asc::index_t, 32> q{};
  Data() {
    a.fill(base::Value<T>(-81, 7));
    b.fill(base::Value<T>(-83, 9));
    tb.fill(T{});
    tb[1] = T{1};
    tb[3] = T{4};
    tb[7] = T{4};
    p.fill(-71);
    q.fill(-73);
    p[1] = 1;
    p[2] = 2;
    q[1] = 1;
    q[2] = 2;
  }
};
template <typename T, std::size_t N>
auto Vector(const std::array<T, N>& values, asc::extent_t count,
            asc::extent_t increment = 1, std::size_t offset = 1,
            asc::MemorySpace space = base::kHost) {
  return base::Take(asc::DenseBlasVectorView<const T>::Create(
      values.data() + offset, count, increment,
      {values.data(), sizeof(values), space}));
}
inline auto Raw(
    const asc::index_t* values, asc::extent_t count,
    asc::ConstMemoryView backing,
    asc::LapackFactorFamily family = asc::LapackFactorFamily::kAasen) {
  return base::Take(
      asc::RawLapackPivotView::Create(values, count, family, backing));
}
template <typename T>
struct Views {
  asc::DenseBlasTriangle tri;
  asc::DenseBlasMatrixView<const T> a;
  asc::DenseBlasVectorView<const T> tb;
  asc::RawLapackPivotView p;
  asc::DenseBlasVectorView<const asc::index_t> q;
  asc::DenseBlasMatrixView<T> b;
  Views(Data<T>& data, asc::DenseBlasTriangle triangle, asc::DenseBlasLayout al,
        asc::DenseBlasLayout bl)
      : tri(triangle),
        a(base::Matrix(std::as_const(data.a), 2, 2, al, 3)),
        tb(Vector(data.tb, 8)),
        p(Raw(data.p.data() + 1, 2,
              {data.p.data(), sizeof(data.p), base::kHost})),
        q(Vector(data.q, 2)),
        b(base::Matrix(data.b, 2, 2, bl, 3)) {}
};
inline void PackingFault(int bad, asc::LapackWorkspacePlan& plan,
                         asc::LapackWorkspace& workspace) {
  const auto pivot = workspace.regions[base::kPivot];
  const auto packed = workspace.regions[base::kLayout];
  switch (bad) {
    case 15:
      if (packed.size() != 0) {
        workspace.regions[base::kLayout] = {nullptr, 0, base::kHost};
      } else {
        plan.regions[base::kLayout].minimum_entries = 1;
      }
      break;
    case 16:
      if (packed.size() != 0) {
        workspace.regions[base::kLayout] = {packed.data(), packed.size() - 1,
                                            base::kHost};
      } else {
        plan.regions[base::kLayout].minimum_entries = 1;
      }
      break;
    case 17:
      if (packed.size() != 0) {
        workspace.regions[base::kPivot] = {packed.data(), pivot.size(),
                                           base::kHost};
      } else {
        plan.regions[base::kLayout].minimum_entries = 1;
      }
      break;
    default:
      break;
  }
}
template <typename T>
void WorkspaceFault(int bad, Data<T>& data, asc::LapackWorkspacePlan& plan,
                    asc::LapackWorkspace& workspace,
                    base::Scratch<T>& scratch) {
  if (bad >= 15) {
    PackingFault(bad, plan, workspace);
    return;
  }
  const auto pivot = workspace.regions[base::kPivot];
  switch (bad) {
    case 0:
      workspace.regions[base::kPivot] = {nullptr, 0, base::kHost};
      break;
    case 1:
      workspace.regions[base::kPivot] = {scratch.pivot.data() + 17,
                                         pivot.size(), base::kHost};
      break;
    case 2:
      workspace.regions[base::kPivot] = {pivot.data(), pivot.size() - 1,
                                         base::kHost};
      break;
    case 3:
      workspace.regions[base::kPivot] = {data.p.data() + 1, pivot.size(),
                                         base::kHost};
      break;
    case 4:
      workspace.regions[base::kPivot] = {data.q.data() + 1, pivot.size(),
                                         base::kHost};
      break;
    case 5:
      workspace.regions[base::kPivot] = {data.tb.data() + 2, pivot.size(),
                                         base::kHost};
      break;
    case 6:
      workspace.regions[base::kPivot] = {data.a.data() + 2, pivot.size(),
                                         base::kHost};
      break;
    case 7:
      workspace.regions[base::kPivot] = {data.b.data() + 2, pivot.size(),
                                         base::kHost};
      break;
    case 8:
      workspace.regions[base::kPivot] = {pivot.data(), pivot.size(),
                                         asc::MemorySpace::kPinnedHost};
      break;
    case 9:
      ++plan.regions[base::kPivot].minimum_entries;
      break;
    case 10:
      ++plan.regions[base::kPivot].preferred_entries;
      break;
    case 11:
      ++plan.regions[base::kPivot].entry_bytes;
      break;
    case 12:
      ++plan.regions[base::kPivot].alignment;
      break;
    case 13:
      plan.total_byte_limit = 1;
      break;
    case 14:
      workspace.regions.back() = pivot;
      break;
    default:
      break;
  }
}
template <typename T>
void MetadataFault(int bad, const asc::ReferenceLapackProvider& provider,
                   asc::LapackWorkspacePlan& plan,
                   asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  const auto bytes = workspace.regions[base::kPivot].size();
  switch (bad) {
    case 18:
      workspace.regions[base::kPivot] = {&report, bytes, base::kHost};
      break;
    case 19:
      workspace.regions[base::kPivot] = {&plan, bytes, base::kHost};
      break;
    case 20:
      workspace.regions[base::kPivot] = {&workspace, bytes, base::kHost};
      break;
    case 21:
      workspace.regions[base::kPivot] = {
          const_cast<asc::ReferenceLapackProvider*>(&provider), bytes,
          base::kHost};
      break;
    default:
      break;
  }
}
template <typename T>
void MatrixFault(int bad, Data<T>& data, Views<T>& views,
                 asc::LapackReport& report) {
  const auto al = views.a.layout();
  const auto bl = views.b.layout();
  switch (bad) {
    case 22:
      views.tri = views.tri == base::kUpper ? base::kLower : base::kUpper;
      break;
    case 23:  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      views.tri = static_cast<asc::DenseBlasTriangle>(99);
      break;
    case 24:
      views.a = base::Matrix(std::as_const(data.a), 2, 1, al, 3);
      break;
    case 25:
      views.a = base::Matrix(std::as_const(data.a), 2, 2, al, 4);
      break;
    case 26:
      views.a = base::Matrix(std::as_const(data.a), 2, 2,
                             al == base::kRow ? base::kColumn : base::kRow, 3);
      break;
    case 27:
      views.a = base::Take(asc::DenseBlasMatrixView<const T>::Create(
          data.a.data() + 1, 2, 2, al, 3,
          {data.a.data(), sizeof(data.a), asc::MemorySpace::kPinnedHost}));
      break;
    case 28:
      views.a = base::Take(asc::DenseBlasMatrixView<const T>::Create(
          reinterpret_cast<const T*>(&report), 2, 2, al, 3,
          {&report, sizeof(report), base::kHost}));
      break;
    case 29:
      views.b = base::Matrix(data.b, 1, 2, bl, 3);
      break;
    case 30:
      views.b = base::Matrix(data.b, 2, 1, bl, 3);
      break;
    case 31:
      views.b = base::Matrix(data.b, 2, 2, bl, 4);
      break;
    case 32:
      views.b = base::Matrix(data.b, 2, 2,
                             bl == base::kRow ? base::kColumn : base::kRow, 3);
      break;
    case 33:
      views.b = base::Take(asc::DenseBlasMatrixView<T>::Create(
          data.b.data() + 1, 2, 2, bl, 3,
          {data.b.data(), sizeof(data.b), asc::MemorySpace::kPinnedHost}));
      break;
    case 34:
      views.b = base::Matrix(data.a, 2, 2, bl, 3);
      break;
    case 35:
      views.b = base::Matrix(data.tb, 2, 2, bl, 3);
      break;
    case 36:
      views.b = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(data.p.data()), 2, 2, bl, 3,
          {data.p.data(), sizeof(data.p), base::kHost}));
      break;
    case 37:
      views.b = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(data.q.data()), 2, 2, bl, 3,
          {data.q.data(), sizeof(data.q), base::kHost}));
      break;
    case 38:
      views.b = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(&report), 2, 2, bl, 3,
          {&report, sizeof(report), base::kHost}));
      break;
    default:
      break;
  }
}
template <typename T>
void BandFault(int bad, Data<T>& data, Views<T>& views,
               asc::LapackReport& report) {
  switch (bad) {
    case 39:
      views.tb = Vector(data.tb, 7);
      break;
    case 40:
      views.tb = Vector(data.tb, 9);
      break;
    case 41:
      views.tb = Vector(data.tb, 8, 2);
      break;
    case 42:
      views.tb = Vector(data.tb, 8, -1, 8);
      break;
    case 43:
      views.tb = Vector(data.tb, 8, 1, 1, asc::MemorySpace::kPinnedHost);
      break;
    case 44:
      views.tb = Vector(data.a, 8);
      break;
    case 45:
      views.tb = Vector(data.b, 8);
      break;
    case 46:
      views.tb = base::Take(asc::DenseBlasVectorView<const T>::Create(
          reinterpret_cast<const T*>(data.p.data()), 8, 1,
          {data.p.data(), sizeof(data.p), base::kHost}));
      break;
    case 47:
      views.tb = base::Take(asc::DenseBlasVectorView<const T>::Create(
          reinterpret_cast<const T*>(data.q.data()), 8, 1,
          {data.q.data(), sizeof(data.q), base::kHost}));
      break;
    case 48:
      views.tb = base::Take(asc::DenseBlasVectorView<const T>::Create(
          reinterpret_cast<const T*>(&report), 8, 1,
          {&report, sizeof(report), base::kHost}));
      break;
    default:
      break;
  }
}
template <typename T>
void OuterFault(int bad, Data<T>& data, Views<T>& views,
                asc::LapackReport& report) {
  switch (bad) {
    case 49:
      views.p = Raw(data.p.data() + 1, 2,
                    {data.p.data(), sizeof(data.p), base::kHost},
                    asc::LapackFactorFamily::kBunchKaufman);
      break;
    case 50:
      views.p = Raw(data.p.data() + 1, 1,
                    {data.p.data(), sizeof(data.p), base::kHost});
      break;
    case 51:
      views.p =
          Raw(data.p.data() + 1, 2,
              {data.p.data(), sizeof(data.p), asc::MemorySpace::kPinnedHost});
      break;
    case 52:
      views.p = Raw(reinterpret_cast<const asc::index_t*>(data.a.data()), 2,
                    {data.a.data(), sizeof(data.a), base::kHost});
      break;
    case 53:
      views.p = Raw(reinterpret_cast<const asc::index_t*>(data.tb.data()), 2,
                    {data.tb.data(), sizeof(data.tb), base::kHost});
      break;
    case 54:
      views.p = Raw(reinterpret_cast<const asc::index_t*>(data.b.data()), 2,
                    {data.b.data(), sizeof(data.b), base::kHost});
      break;
    case 55:
      views.p = Raw(data.q.data() + 1, 2,
                    {data.q.data(), sizeof(data.q), base::kHost});
      break;
    case 56:
      views.p = Raw(reinterpret_cast<const asc::index_t*>(&report), 2,
                    {&report, sizeof(report), base::kHost});
      break;
    default:
      break;
  }
}
template <typename T>
void BandPivotFault(int bad, Data<T>& data, Views<T>& views,
                    asc::LapackReport& report) {
  switch (bad) {
    case 57:
      views.q = Vector(data.q, 1);
      break;
    case 58:
      views.q = Vector(data.q, 2, 2);
      break;
    case 59:
      views.q = Vector(data.q, 2, -1, 2);
      break;
    case 60:
      views.q = Vector(data.q, 2, 1, 1, asc::MemorySpace::kPinnedHost);
      break;
    case 61:
      views.q = Vector(data.p, 2);
      break;
    case 62:
      views.q = base::Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
          reinterpret_cast<const asc::index_t*>(data.a.data()), 2, 1,
          {data.a.data(), sizeof(data.a), base::kHost}));
      break;
    case 63:
      views.q = base::Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
          reinterpret_cast<const asc::index_t*>(data.b.data()), 2, 1,
          {data.b.data(), sizeof(data.b), base::kHost}));
      break;
    case 64:
      views.q = base::Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
          reinterpret_cast<const asc::index_t*>(data.tb.data()), 2, 1,
          {data.tb.data(), sizeof(data.tb), base::kHost}));
      break;
    case 65:
      views.q = base::Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
          reinterpret_cast<const asc::index_t*>(&report), 2, 1,
          {&report, sizeof(report), base::kHost}));
      break;
    default:
      break;
  }
}
template <typename T>
void ValueFault(int bad, Data<T>& data) {
  using Real = asc::DenseBlasRealType<T>;
  switch (bad) {
    case 66:
      data.tb[1] = T{};
      break;
    case 67:
      data.tb[1] = T{-1};
      break;
    case 68:
      data.tb[1] = T{193};
      break;
    case 69:
      data.tb[1] = T{Real{1.5}};
      break;
    case 70:
      data.tb[1] = T{std::numeric_limits<Real>::quiet_NaN()};
      break;
    case 71:
      data.tb[1] = T{std::numeric_limits<Real>::infinity()};
      break;
    case 72:
      if constexpr (asc::DenseBlasComplex<T>) {
        data.tb[1].imag(1);
      } else {
        data.tb[1] = T{2};
      }
      break;
    case 73:
      data.p[1] = 2;
      break;
    case 74:
      data.p[2] = 1;
      break;
    case 75:
      data.p[1] = std::numeric_limits<asc::index_t>::min();
      break;
    case 76:
      data.p[1] = 3;
      break;
    case 77:
      data.q[1] = 0;
      break;
    case 78:
      data.q[2] = 1;
      break;
    case 79:
      data.q[1] = 3;
      break;
    case 80:
      data.q[1] = std::numeric_limits<asc::index_t>::min();
      break;
    case 81:
      data.tb[3] = T{};
      break;
    case 82:
      data.tb[7] = T{};
      break;
    default:
      break;
  }
}
template <typename T>
void Rejection(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool he, asc::DenseBlasTriangle tri, asc::DenseBlasLayout al,
               asc::DenseBlasLayout bl, int bad) {
  Data<T> data;
  Views<T> views(data, tri, al, bl);
  auto plan = base::Take(aa::Query(provider, tri, he, views.a, views.tb,
                                   views.p, views.q, views.b));
  base::Scratch<T> scratch;
  auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 73;
  WorkspaceFault(bad, data, plan, workspace, scratch);
  MetadataFault<T>(bad, provider, plan, workspace, report);
  MatrixFault(bad, data, views, report);
  BandFault(bad, data, views, report);
  OuterFault(bad, data, views, report);
  BandPivotFault(bad, data, views, report);
  ValueFault(bad, data);
  const auto before = data;
  const auto scratch_before = scratch;
  std::array<std::byte, sizeof(report)> report_before{};
  std::memcpy(report_before.data(), &report, sizeof(report));
  const auto status = base::WithoutAllocation(test, [&] {
    return aa::Solve(provider, views.tri, he, views.a, views.tb, views.p,
                     views.q, views.b, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  const bool unsafe = (bad >= 18 && bad <= 21) || bad == 28 || bad == 38 ||
                      bad == 48 || bad == 56 || bad == 65;
  if (unsafe) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(&report, report_before.data(), sizeof(report)));
  } else {
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, report.factor_family,
                      asc::LapackFactorFamily::kAasen);
    if (bad >= 81) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), bad - 81);
    }
  }
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(data.a.data(), before.a.data(), sizeof(data.a)));
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(data.b.data(), before.b.data(), sizeof(data.b)));
  ASC_DENSE_TEST_CHECK(test, base::EqualBytes(data.tb.data(), before.tb.data(),
                                              sizeof(data.tb)));
  ASC_DENSE_TEST_EQ(test, data.p, before.p);
  ASC_DENSE_TEST_EQ(test, data.q, before.q);
  ASC_DENSE_TEST_EQ(test, scratch.scalar, scratch_before.scalar);
  ASC_DENSE_TEST_EQ(test, scratch.packed, scratch_before.packed);
  ASC_DENSE_TEST_EQ(test, scratch.pivot, scratch_before.pivot);
}
template <typename T>
void Unread(TestContext& test, const asc::ReferenceLapackProvider& provider,
            bool he) {
#if defined(__linux__)
  const auto size = sysconf(_SC_PAGESIZE);
  ASC_DENSE_TEST_CHECK(test, size >= 2048);
  if (size < 2048) {
    return;
  }
  const auto bytes = static_cast<std::size_t>(size);
  void* memory =
      mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASC_DENSE_TEST_CHECK(test, memory != MAP_FAILED);
  if (memory == MAP_FAILED) {
    return;
  }
  auto* raw = static_cast<std::byte*>(memory);
  auto* a = reinterpret_cast<T*>(raw);
  auto* tb = reinterpret_cast<T*>(raw + 256);
  auto* p = reinterpret_cast<asc::index_t*>(raw + 512);
  auto* q = reinterpret_cast<asc::index_t*>(raw + 768);
  auto* b = reinterpret_cast<T*>(raw + 1024);
  for (auto tri : {base::kUpper, base::kLower}) {
    for (auto al : {base::kColumn, base::kRow}) {
      for (auto bl : {base::kColumn, base::kRow}) {
        for (const auto shape : {std::array{0, 0}, std::array{0, 2},
                                 std::array{2, 0}, std::array{2, 2}}) {
          const int n = shape[0];
          const int nrhs = shape[1];
          const auto matrix =
              base::Take(asc::DenseBlasMatrixView<const T>::Create(
                  a, n, n, al, 2, {a, 128, base::kHost}));
          const auto band =
              base::Take(asc::DenseBlasVectorView<const T>::Create(
                  tb, 4 * n, 1, {tb, 128, base::kHost}));
          const auto pivots = Raw(p, n, {p, 128, base::kHost});
          const auto band_pivots =
              base::Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
                  q, n, 1, {q, 128, base::kHost}));
          const auto rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
              b, n, nrhs, bl, 2, {b, 128, base::kHost}));
          const auto plan = base::Take(base::WithoutAllocation(test, [&] {
            return aa::Query(provider, tri, he, matrix, band, pivots,
                             band_pivots, rhs);
          }));
          if (n == 0 || nrhs == 0) {
            asc::LapackReport report;
            const auto status = base::WithoutAllocation(test, [&] {
              return aa::Solve(provider, tri, he, matrix, band, pivots,
                               band_pivots, rhs, plan, {}, report);
            });
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
  ASC_DENSE_TEST_EQ(test, munmap(memory, bytes), 0);
#else
  (void)provider;
  (void)he;
  ASC_DENSE_TEST_CHECK(test,
                       false);  // Required protected-memory runner unavailable.
#endif
}
template <typename T>
void OriginalLeading(TestContext& test,
                     const asc::ReferenceLapackProvider& provider, bool he) {
  for (auto tri : {base::kUpper, base::kLower}) {
    for (auto al : {base::kColumn, base::kRow}) {
      for (auto bl : {base::kColumn, base::kRow}) {
        for (bool change_rhs : {false, true}) {
          Data<T> data;
          Views<T> views(data, tri, al, bl);
          views.a = base::Matrix(std::as_const(data.a), 1, 1, al, 1);
          views.b = base::Matrix(data.b, 1, 1, bl, 1);
          views.tb = Vector(data.tb, 4);
          views.p = Raw(data.p.data() + 1, 1,
                        {data.p.data(), sizeof(data.p), base::kHost});
          views.q = Vector(data.q, 1);
          const auto plan = base::Take(aa::Query(
              provider, tri, he, views.a, views.tb, views.p, views.q, views.b));
          const asc::extent_t ld = ASC_LAPACK_INTEGER_BITS == 32
                                       ? asc::extent_t{INT32_MAX} + 1
                                       : asc::extent_t{INT64_MAX};
          if (change_rhs) {
            views.b = base::Matrix(data.b, 1, 1, bl, ld);
          } else {
            views.a = base::Matrix(std::as_const(data.a), 1, 1, al, ld);
          }
          const auto query = aa::Query(provider, tri, he, views.a, views.tb,
                                       views.p, views.q, views.b);
          ASC_DENSE_TEST_EQ(test, query.ok(), ASC_LAPACK_INTEGER_BITS == 64);
          if (!query.ok()) {
            ASC_DENSE_TEST_EQ(test, query.status().code(),
                              asc::ErrorCode::kOverflow);
          }
          base::Scratch<T> scratch;
          const auto workspace = scratch.Workspace(plan);
          const auto before = data;
          const auto old_scratch = scratch;
          asc::LapackReport report;
          const auto status = base::WithoutAllocation(test, [&] {
            return aa::Solve(provider, tri, he, views.a, views.tb, views.p,
                             views.q, views.b, plan, workspace, report);
          });
          ASC_DENSE_TEST_EQ(test, status.code(),
                            ASC_LAPACK_INTEGER_BITS == 32
                                ? asc::ErrorCode::kOverflow
                                : asc::ErrorCode::kInvalidState);
          ASC_DENSE_TEST_CHECK(
              test, !report.called_provider && !report.native_info.has_value());
          ASC_DENSE_TEST_CHECK(
              test,
              base::EqualBytes(data.a.data(), before.a.data(), sizeof(data.a)));
          ASC_DENSE_TEST_CHECK(
              test,
              base::EqualBytes(data.b.data(), before.b.data(), sizeof(data.b)));
          ASC_DENSE_TEST_EQ(test, scratch.scalar, old_scratch.scalar);
          ASC_DENSE_TEST_EQ(test, scratch.packed, old_scratch.packed);
          ASC_DENSE_TEST_EQ(test, scratch.pivot, old_scratch.pivot);
        }
      }
    }
  }
}
void CountCases(TestContext& test) {
  namespace counts = asc::internal_indefinite_aasen_two_stage_solve_counts;
  for (const asc::extent_t limit :
       {asc::extent_t{INT32_MAX}, asc::extent_t{INT64_MAX}}) {
    ASC_DENSE_TEST_CHECK(test, counts::Solve(0, 3, 1, 0, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(2, 0, 2, 8, 1, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Solve(2, 0, 2, 7, 1, limit).code(),
                      asc::ErrorCode::kShape);
    ASC_DENSE_TEST_EQ(test, counts::Solve(2, 1, 1, 8, 2, limit).code(),
                      asc::ErrorCode::kShape);
    ASC_DENSE_TEST_EQ(test, counts::Solve(2, 1, 2, 8, 1, limit).code(),
                      asc::ErrorCode::kShape);
    ASC_DENSE_TEST_CHECK(test, counts::Solve(2, 1, 2, 8, 2, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Solve(-1, 1, 1, 4, 1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, counts::Solve(1, -1, 1, 4, 1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, counts::Solve(1, 1, 0, 4, 1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, counts::Solve(1, 1, 1, -1, 1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, counts::Solve(1, 1, 1, 4, 0, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, counts::Solve(1, 1, 1, 4, 1, limit - 1).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(
        test, counts::Solve(limit / 4 + 1, 0, 1, limit, 1, limit).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, counts::Solve(2, limit, 2, 8, 2, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(
        test, counts::Solve(7, 33, 7, 28, (limit - 1) / 33, limit).ok());
    ASC_DENSE_TEST_EQ(
        test, counts::Solve(7, 33, 7, 28, (limit - 1) / 33 + 1, limit).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, counts::Solve(1, 1, 1, limit, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(2, 0, 2, 8, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(0, limit, 1, 0, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(7, 31, 7, 28, 7, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(7, 32, 7, 28, 7, limit).ok());
  }
}
template <typename T>
int Run(bool he) {
  TestContext test;
  int cases = 0;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto tri : {base::kUpper, base::kLower}) {
    for (auto al : {base::kColumn, base::kRow}) {
      for (auto bl : {base::kColumn, base::kRow}) {
        for (int bad = 0; bad < 83; ++bad) {
          Rejection<T>(test, provider, he, tri, al, bl, bad);
          ++cases;
        }
      }
    }
  }
  CountCases(test);
  Unread<T>(test, provider, he);
  OriginalLeading<T>(test, provider, he);
  std::printf(
      "Two-stage Aasen solve validation cases=%d protected_queries=32 "
      "empty_noncalls=24 original_stride_cases=16 pure_counts=42\n",
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
