#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string_view>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
#include "tests/dense/test_support.h"
namespace {
namespace packed = asc_packed_indefinite_test;
namespace base = asc_indefinite_test;
using asc_dense_test::TestContext;
template <typename T, std::size_t Size>
auto Matrix(std::array<T, Size>& a, asc::extent_t n,
            asc::DenseBlasLayout layout, asc::MemorySpace space = base::kHost) {
  return base::Take(asc::DenseBlasPackedMatrixView<T>::Create(
      a.data() + 1, n, layout, {a.data(), sizeof(a), space}));
}
template <typename T>
void WorkspaceFault(int bad, asc::LapackWorkspacePlan& plan,
                    asc::LapackWorkspace& workspace, base::Scratch<T>& scratch,
                    std::array<T, 64>& a, std::array<asc::index_t, 16>& p) {
  const auto pivot = workspace.regions[base::kPivot];
  switch (bad) {
    case 0:
      workspace.regions[base::kPivot] = {pivot.data(), pivot.size() - 1,
                                         base::kHost};
      break;
    case 1:
      workspace.regions[base::kPivot] = {scratch.pivot.data() + 17,
                                         pivot.size(), base::kHost};
      break;
    case 2:
      workspace.regions[base::kPivot] = {pivot.data(), pivot.size(),
                                         asc::MemorySpace::kPinnedHost};
      break;
    case 3:
      workspace.regions[base::kPivot] = {a.data() + 1, pivot.size(),
                                         base::kHost};
      break;
    case 4:
      workspace.regions[base::kPivot] = {p.data() + 1, pivot.size(),
                                         base::kHost};
      break;
    case 5:
      workspace.regions[base::kLayout] = pivot;
      break;
    case 6:
      ++plan.regions[base::kPivot].minimum_entries;
      break;
    case 7:
      ++plan.regions[base::kPivot].preferred_entries;
      break;
    case 8:
      ++plan.regions[base::kPivot].entry_bytes;
      break;
    case 9:
      ++plan.regions[base::kPivot].alignment;
      break;
    case 10:
      plan.total_byte_limit = 1;
      break;
    case 11:
      workspace.regions.back() = pivot;
      break;
    case 12:
      if (plan.regions[base::kLayout].minimum_entries != 0) {
        workspace.regions[base::kLayout] = {nullptr, 0, base::kHost};
      } else {
        plan.regions[base::kLayout].minimum_entries = 1;
      }
      break;
    case 13:
      workspace.regions[base::kLayout] = {a.data() + 1, 3 * sizeof(T),
                                          base::kHost};
      break;
    default:
      break;
  }
}
template <typename T>
void OperandFault(int bad, asc::DenseBlasTriangle& tri,
                  asc::DenseBlasLayout layout, std::array<T, 64>& a,
                  std::array<asc::index_t, 16>& p,
                  asc::DenseBlasPackedMatrixView<T>& matrix,
                  asc::DenseBlasVectorView<asc::index_t>& pivots,
                  asc::LapackWorkspacePlan& plan) {
  switch (bad) {
    case 14:
      tri = tri == base::kUpper ? base::kLower : base::kUpper;
      break;
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    case 15:
      tri = static_cast<asc::DenseBlasTriangle>(99);
      break;
    case 16:
      matrix = Matrix(a, 3, layout);
      break;
    case 17:
      pivots = base::Pivots(p, 1);
      break;
    case 18:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          p.data() + 1, 2, 2, {p.data(), sizeof(p), base::kHost}));
      break;
    case 19:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          reinterpret_cast<asc::index_t*>(a.data() + 2), 2, 1,
          {a.data(), sizeof(a), base::kHost}));
      break;
    case 20:
      matrix = Matrix(a, 2, layout, asc::MemorySpace::kPinnedHost);
      break;
    case 21:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          p.data() + 1, 2, 1,
          {p.data(), sizeof(p), asc::MemorySpace::kPinnedHost}));
      break;
    case 22:
      matrix =
          Matrix(a, 2, layout == base::kColumn ? base::kRow : base::kColumn);
      break;
    case 23:
      ++plan.regions[base::kScalar].minimum_entries;
      break;
    default:
      break;
  }
}
template <typename T>
void AliasFault(int bad, const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasLayout layout,
                asc::DenseBlasPackedMatrixView<T>& matrix,
                asc::DenseBlasVectorView<asc::index_t>& pivots,
                asc::LapackWorkspacePlan& plan, asc::LapackWorkspace& workspace,
                asc::LapackReport& report) {
  const auto pivot = workspace.regions[base::kPivot];
  switch (bad) {
    case 24:
      matrix = base::Take(asc::DenseBlasPackedMatrixView<T>::Create(
          reinterpret_cast<T*>(&report), 2, layout,
          {&report, sizeof(report), base::kHost}));
      break;
    case 25:
      workspace.regions[base::kPivot] = {&report, pivot.size(), base::kHost};
      break;
    case 26:
      workspace.regions[base::kPivot] = {&plan, pivot.size(), base::kHost};
      break;
    case 27:
      workspace.regions[base::kPivot] = {&workspace, pivot.size(), base::kHost};
      break;
    case 28:
      matrix = base::Take(asc::DenseBlasPackedMatrixView<T>::Create(
          reinterpret_cast<T*>(&plan), 2, layout,
          {&plan, sizeof(plan), base::kHost}));
      break;
    case 29:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          reinterpret_cast<asc::index_t*>(&report), 2, 1,
          {&report, sizeof(report), base::kHost}));
      break;
    case 30:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          reinterpret_cast<asc::index_t*>(&plan), 2, 1,
          {&plan, sizeof(plan), base::kHost}));
      break;
    case 31:
      matrix = base::Take(asc::DenseBlasPackedMatrixView<T>::Create(
          reinterpret_cast<T*>(
              const_cast<asc::ReferenceLapackProvider*>(&provider)),
          2, layout, {&provider, sizeof(provider), base::kHost}));
      break;
    default:
      break;
  }
}
template <typename T>
void Reject(TestContext& test, const asc::ReferenceLapackProvider& provider,
            bool he, asc::DenseBlasTriangle tri, asc::DenseBlasLayout layout,
            int bad) {
  alignas(16) std::array<T, 64> a;
  std::array<asc::index_t, 16> p;
  a.fill(base::Value<T>(-31, 11));
  p.fill(-41);
  auto matrix = Matrix(a, 2, layout);
  auto pivots = base::Pivots(p, 2);
  auto plan = base::Take(packed::Query(provider, tri, he, matrix, pivots));
  base::Scratch<T> scratch;
  auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  report.native_info = 73;
  WorkspaceFault(bad, plan, workspace, scratch, a, p);
  OperandFault(bad, tri, layout, a, p, matrix, pivots, plan);
  AliasFault(bad, provider, layout, matrix, pivots, plan, workspace, report);
  const auto before_a = a;
  const auto before_p = p;
  const auto before_scratch = scratch;
  std::array<std::byte, sizeof(report)> before_report;
  std::memcpy(before_report.data(), &report, sizeof(report));
  const auto status = base::WithoutAllocation(test, [&] {
    return packed::Factor(provider, tri, he, matrix, pivots, plan, workspace,
                          report);
  });
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  if (bad >= 24) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(&report, before_report.data(), sizeof(report)));
  } else {
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
  }
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(a.data(), before_a.data(), sizeof(a)));
  ASC_DENSE_TEST_EQ(test, p, before_p);
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
  auto* p =
      reinterpret_cast<asc::index_t*>(static_cast<std::byte*>(memory) + 512);
  for (auto tri : {base::kUpper, base::kLower}) {
    for (auto layout : {base::kColumn, base::kRow}) {
      for (const asc::extent_t n : {0, 1, 2, 3}) {
        const auto matrix =
            base::Take(asc::DenseBlasPackedMatrixView<T>::Create(
                a, n, layout, {a, 128, base::kHost}));
        const auto pivots =
            base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
                p, n, 1, {p, 128, base::kHost}));
        const auto plan = base::Take(base::WithoutAllocation(test, [&] {
          return packed::Query(provider, tri, he, matrix, pivots);
        }));
        if (n == 0) {
          asc::LapackReport report;
          const auto status = packed::Factor(provider, tri, he, matrix, pivots,
                                             plan, {}, report);
          ASC_DENSE_TEST_CHECK(test, status.ok());
          ASC_DENSE_TEST_CHECK(
              test, !report.called_provider && !report.native_info.has_value());
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, munmap(memory, bytes), 0);
#else
  ASC_DENSE_TEST_CHECK(
      test, false);  // Required protected nonaccess proof unavailable.
  (void)provider;
  (void)he;
#endif
}
template <typename T>
void PackedLimits(TestContext& test,
                  const asc::ReferenceLapackProvider& provider, bool he) {
#if defined(__linux__)
  constexpr asc::extent_t kLast = 46342;
  constexpr auto kMatrixBytes =
      static_cast<std::size_t>(kLast * (kLast + 1) / 2) * sizeof(T);
  constexpr auto kPivotBytes =
      static_cast<std::size_t>(kLast) * sizeof(asc::index_t);
  constexpr auto kPivotOffset = (kMatrixBytes + 15) / 16 * 16;
  constexpr auto kBytes = kPivotOffset + kPivotBytes;
  void* memory =
      mmap(nullptr, kBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASC_DENSE_TEST_CHECK(test, memory != MAP_FAILED);
  if (memory == MAP_FAILED) {
    return;
  }
  auto* a = static_cast<T*>(memory);
  auto* p = reinterpret_cast<asc::index_t*>(static_cast<std::byte*>(memory) +
                                            kPivotOffset);
  for (auto tri : {base::kUpper, base::kLower}) {
    for (auto layout : {base::kColumn, base::kRow}) {
      for (const asc::extent_t n : {46340, 46341, 46342}) {
        const auto matrix =
            base::Take(asc::DenseBlasPackedMatrixView<T>::Create(
                a, n, layout, {a, kMatrixBytes, base::kHost}));
        const auto pivots =
            base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
                p, n, 1, {p, kPivotBytes, base::kHost}));
        const auto plan = base::WithoutAllocation(test, [&] {
          return packed::Query(provider, tri, he, matrix, pivots);
        });
        const bool admitted = ASC_LAPACK_INTEGER_BITS == 64 ||
                              n <= (tri == base::kUpper ? 46341 : 46340);
        ASC_DENSE_TEST_EQ(test, plan.ok(), admitted);
        if (!admitted) {
          ASC_DENSE_TEST_EQ(test, plan.status().code(),
                            asc::ErrorCode::kOverflow);
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, munmap(memory, kBytes), 0);
#else
  ASC_DENSE_TEST_CHECK(
      test, false);  // Required protected boundary proof unavailable.
  (void)provider;
  (void)he;
#endif
}
template <typename T>
int Run(bool he) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (auto tri : {base::kUpper, base::kLower}) {
    for (auto layout : {base::kColumn, base::kRow}) {
      for (int bad = 0; bad < 32; ++bad) {
        Reject<T>(test, provider, he, tri, layout, bad);
        ++cases;
      }
    }
  }
  Unread<T>(test, provider, he);
  PackedLimits<T>(test, provider, he);
  std::printf("Packed validation rejection cases=%d protected_queries=28\n",
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
