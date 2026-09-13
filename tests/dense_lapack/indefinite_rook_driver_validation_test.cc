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
#include "../../src/dense/lapack/internal_indefinite_expert_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rook_driver_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_indefinite_rook_test;
using asc_rook_driver_test::Driver;
using asc_rook_driver_test::Query;
using base::TestContext;

template <typename T>
void AlterWorkspace(int bad, asc::LapackWorkspacePlan& plan,
                    asc::LapackWorkspace& workspace, base::Scratch<T>& scratch,
                    std::array<T, 16>& a, std::array<T, 16>& b,
                    std::array<asc::index_t, 5>& p) {
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
      workspace.regions[base::kScalar] = {a.data() + 1, sizeof(T), base::kHost};
      break;
    case 5:
      workspace.regions[base::kScalar] = {b.data() + 1, sizeof(T), base::kHost};
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
      workspace.regions[base::kScalar] = {scratch.scalar.data() + 1, sizeof(T),
                                          asc::MemorySpace::kPinnedHost};
      break;
    default:
      break;
  }
}

void RejectedReport(
    TestContext& test, int bad, const asc::Status& status,
    const asc::LapackReport& report,
    const std::array<std::byte, sizeof(asc::LapackReport)>& saved_report) {
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  if (bad == 19 || bad == 20) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(&report, saved_report.data(), sizeof(report)));
  } else {
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
  }
}

template <typename T>
void Rejection(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool hermitian, asc::DenseBlasTriangle triangle,
               asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout,
               int bad) {
  std::array<T, 16> a;
  std::array<T, 16> b;
  std::array<asc::index_t, 5> p;
  a.fill(base::Value<T>(std::numeric_limits<double>::quiet_NaN(), 13));
  b.fill(base::Value<T>(-17, 19));
  p.fill(std::numeric_limits<asc::index_t>::min());
  auto matrix = base::Matrix(a, 2, 2, layout, 3);
  auto rhs = base::Matrix(b, 2, 2, rhs_layout, 3);
  auto pivots = base::Pivots(p, 2);
  auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return Query(provider, triangle, hermitian, matrix, pivots, rhs);
  }));
  base::Scratch<T> scratch;
  auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 97;
  std::array<std::byte, sizeof(report)> saved_report;
  std::memcpy(saved_report.data(), &report, sizeof(report));
  auto selected = triangle;
  AlterWorkspace(bad, plan, workspace, scratch, a, b, p);
  switch (bad) {
    case 12:
      selected = triangle == base::kUpper ? base::kLower : base::kUpper;
      break;
    case 13:
      rhs = base::Matrix(b, 2, 1, rhs_layout, 3);
      break;
    case 14:
      rhs = base::Matrix(a, 2, 2, layout, 3);
      break;
    case 15:
      matrix = base::Matrix(a, 2, 1, layout, 3);
      break;
    case 16:
      pivots = base::Pivots(p, 1);
      break;
    case 17:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          p.data(), 2, 2, {p.data(), sizeof(p), base::kHost}));
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
      // Invalid fixed-underlying public enum must be rejected without reads.
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      selected = static_cast<asc::DenseBlasTriangle>(99);
      break;
    default:
      break;
  }
  const auto before_a = a;
  const auto before_b = b;
  const auto before_p = p;
  const auto before_scratch = scratch;
  const auto status = base::WithoutAllocation(test, [&] {
    return Driver(provider, selected, hermitian, matrix, pivots, rhs, plan,
                  workspace, report);
  });
  RejectedReport(test, bad, status, report, saved_report);
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(a.data(), before_a.data(), sizeof(a)));
  ASC_DENSE_TEST_EQ(test, b, before_b);
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
  auto* b = reinterpret_cast<T*>(static_cast<std::byte*>(memory) + 256);
  auto* p =
      reinterpret_cast<asc::index_t*>(static_cast<std::byte*>(memory) + 512);
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const asc::extent_t n : {0, 1}) {
        const auto matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
            a, n, n, layout, 1, {a, 128, base::kHost}));
        const auto rhs = base::Take(asc::DenseBlasMatrixView<T>::Create(
            b, n, 1, layout, 1, {b, 128, base::kHost}));
        const auto pivots =
            base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
                p, n, 1, {p, 128, base::kHost}));
        const auto plan = base::Take(base::WithoutAllocation(test, [&] {
          return Query(provider, triangle, hermitian, matrix, pivots, rhs);
        }));
        if (n == 0) {
          asc::LapackReport report;
          const auto status = Driver(provider, triangle, hermitian, matrix,
                                     pivots, rhs, plan, {}, report);
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
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const auto rhs_layout : {base::kColumn, base::kRow}) {
        for (int bad = 0; bad < 22; ++bad) {
          Rejection<T>(test, provider, hermitian, triangle, layout, rhs_layout,
                       bad);
          ++cases;
        }
      }
    }
  }
  using Real = asc::DenseBlasRealType<T>;
  constexpr asc::extent_t kLimit = std::numeric_limits<lapack_int>::max();
  const auto safe_order = kLimit / 128;
  ASC_DENSE_TEST_CHECK(
      test,
      asc::internal_indefinite_expert_counts::Driver<Real>(safe_order, kLimit)
          .ok());
  ASC_DENSE_TEST_EQ(test,
                    asc::internal_indefinite_expert_counts::Driver<Real>(
                        kLimit / 64 + 1, kLimit)
                        .status()
                        .code(),
                    asc::ErrorCode::kOverflow);
  Unread<T>(test, provider, hermitian);
  std::printf("rook driver rejection cases=%d\n", cases);
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
