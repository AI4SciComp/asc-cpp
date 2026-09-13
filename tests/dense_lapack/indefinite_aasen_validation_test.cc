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
#include "../../src/dense/lapack/internal_indefinite_aasen_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_test;
using base::TestContext;
template <typename T>
void WorkspaceFault(int bad, asc::LapackWorkspacePlan& plan,
                    asc::LapackWorkspace& workspace, base::Scratch<T>& scratch,
                    std::array<T, 24>& a, std::array<asc::index_t, 8>& p) {
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
                  asc::DenseBlasLayout layout, std::array<T, 24>& a,
                  std::array<asc::index_t, 8>& p,
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
void Rejection(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool he, asc::DenseBlasTriangle tri, asc::DenseBlasLayout layout,
               int bad) {
  alignas(16) std::array<T, 24> a;
  std::array<asc::index_t, 8> p;
  a.fill(base::Value<T>(-31, 11));
  p.fill(-41);
  auto matrix = base::Matrix(a, 2, 2, layout, 3);
  auto pivots = base::Pivots(p, 2);
  auto plan = base::Take(aa::Query(provider, tri, he, matrix, pivots));
  base::Scratch<T> scratch;
  auto workspace =
      scratch.Workspace(plan, plan.regions[base::kScalar].minimum_entries);
  asc::LapackReport report;
  report.native_info = 73;
  auto selected = tri;
  WorkspaceFault(bad, plan, workspace, scratch, a, p);
  OperandFault(bad, tri, layout, a, p, matrix, pivots, selected, plan,
               workspace, report);
  const auto before_a = a;
  const auto before_p = p;
  const auto before_scratch = scratch;
  std::array<std::byte, sizeof(report)> before_report;
  std::memcpy(before_report.data(), &report, sizeof(report));
  const auto status = base::WithoutAllocation(test, [&] {
    return aa::Factor(provider, selected, he, matrix, pivots, plan, workspace,
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
      reinterpret_cast<asc::index_t*>(static_cast<std::byte*>(memory) + 256);
  for (const auto tri : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const asc::extent_t n : {0, 1, 2}) {
        const auto matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
            a, n, n, layout, 2, {a, 128, base::kHost}));
        const auto pivots =
            base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
                p, n, 1, {p, 128, base::kHost}));
        const auto plan = base::Take(base::WithoutAllocation(test, [&] {
          return aa::Query(provider, tri, he, matrix, pivots);
        }));
        if (n == 0) {
          asc::LapackReport report;
          const auto status =
              aa::Factor(provider, tri, he, matrix, pivots, plan, {}, report);
          ASC_DENSE_TEST_CHECK(test, status.ok());
          ASC_DENSE_TEST_CHECK(
              test, !report.called_provider && !report.native_info.has_value());
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
int Run(bool he) {
  TestContext test;
  int cases = 0;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (const auto tri : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (int bad = 0; bad < 27; ++bad) {
        Rejection<T>(test, provider, he, tri, layout, bad);
        ++cases;
      }
    }
  }
  namespace counts = asc::internal_indefinite_aasen_counts;
  using Real = asc::DenseBlasRealType<T>;
  for (const asc::extent_t limit :
       {asc::extent_t{INT32_MAX}, asc::extent_t{INT64_MAX}}) {
    ASC_DENSE_TEST_CHECK(test, counts::Factor(2, (limit - 1) / 2, limit).ok());
    ASC_DENSE_TEST_EQ(test,
                      counts::Factor(2, (limit - 1) / 2 + 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(
        test, counts::Preferred<Real>(limit / 130, true, limit).ok());
    ASC_DENSE_TEST_EQ(
        test,
        counts::Preferred<Real>(limit / 65 + 1, true, limit).status().code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, base::Take(counts::Preferred<Real>(1, true, limit)),
                      65);
    ASC_DENSE_TEST_EQ(test,
                      base::Take(counts::Preferred<Real>(1, false, limit)), 1);
  }
  Unread<T>(test, provider, he);
  std::printf("Aasen validation rejections=%d\n", cases);
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
