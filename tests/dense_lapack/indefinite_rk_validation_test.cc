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

#include "../../src/dense/lapack/internal_indefinite_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite.h"
#include "asc/dense/providers/lapack_indefinite_rook.h"
#include "indefinite_rk_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_rk_test;
using base::TestContext;
constexpr std::size_t kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr std::size_t kPivot =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr std::size_t kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T>
struct Fixture {
  alignas(16) std::array<T, 24> a{};
  alignas(16) std::array<T, 16> e{};
  alignas(16) std::array<asc::index_t, 8> p{};
  base::Scratch<T> scratch;
  asc::LapackReport report;
  Fixture() {
    a.fill(base::Value<T>(-31, 11));
    e.fill(base::Value<T>(-37, 13));
    p.fill(-41);
    report.native_info = 73;
  }
};

template <typename T>
void AlterWorkspace(int bad, Fixture<T>& f, asc::LapackWorkspacePlan& plan,
                    asc::LapackWorkspace& workspace) {
  switch (bad) {
    case 0:
      workspace.regions[kPivot] = {nullptr, 0, base::kHost};
      break;
    case 1:
      workspace.regions[kPivot] = {f.scratch.pivot.data() + 16,
                                   workspace.regions[kPivot].size() - 1,
                                   base::kHost};
      break;
    case 2:
      workspace.regions[kPivot] = {f.scratch.pivot.data() + 17,
                                   workspace.regions[kPivot].size(),
                                   base::kHost};
      break;
    case 3:
      workspace.regions[kPivot] = {
          f.p.data() + 1, workspace.regions[kPivot].size(), base::kHost};
      break;
    case 4:
      workspace.regions[kPivot] = {
          f.e.data() + 1, workspace.regions[kPivot].size(), base::kHost};
      break;
    case 5:
      workspace.regions[kPivot] = {
          f.a.data() + 1, workspace.regions[kPivot].size(), base::kHost};
      break;
    case 6:
      ++plan.regions[kPivot].entry_bytes;
      break;
    case 7:
      ++plan.regions[kPivot].minimum_entries;
      break;
    case 8:
      ++plan.regions[kPivot].preferred_entries;
      break;
    case 9:
      ++plan.regions[kPivot].alignment;
      break;
    case 10:
      plan.total_byte_limit = 1;
      break;
    case 11:
      workspace.regions.back() = workspace.regions[kPivot];
      break;
    default:
      break;
  }
}

template <typename T>
void AlterOperands(int bad, Fixture<T>& f, asc::DenseBlasMatrixView<T>& matrix,
                   asc::DenseBlasVectorView<T>& e,
                   asc::DenseBlasVectorView<asc::index_t>& pivots,
                   asc::DenseBlasTriangle& selected,
                   asc::DenseBlasTriangle triangle,
                   asc::DenseBlasLayout layout) {
  switch (bad) {
    case 12:
      selected = triangle == base::kUpper ? base::kLower : base::kUpper;
      break;
    case 13:
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      selected = static_cast<asc::DenseBlasTriangle>(99);
      break;
    case 14:
      matrix = base::Matrix(f.a, 2, 2, layout, 4);
      break;
    case 15:
      matrix = base::Matrix(f.a, 2, 1, layout, 3);
      break;
    case 16:
      e = base::OffDiagonal(f.e, 1);
      break;
    case 17:
      pivots = base::Pivots(f.p, 1);
      break;
    case 18:
      e = base::Take(asc::DenseBlasVectorView<T>::Create(
          f.e.data() + 1, 2, 2, {f.e.data(), sizeof(f.e), base::kHost}));
      break;
    case 19:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          f.p.data() + 1, 2, 2, {f.p.data(), sizeof(f.p), base::kHost}));
      break;
    case 20:
      e = base::OffDiagonal(f.a, 2);
      break;
    case 21:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          reinterpret_cast<asc::index_t*>(f.a.data() + 2), 2, 1,
          {f.a.data(), sizeof(f.a), base::kHost}));
      break;
    case 22:
      e = base::Take(asc::DenseBlasVectorView<T>::Create(
          reinterpret_cast<T*>(f.p.data() + 1), 2, 1,
          {f.p.data(), sizeof(f.p), base::kHost}));
      break;
    default:
      break;
  }
}

template <typename T>
void AlterPlacement(int bad, Fixture<T>& f, asc::DenseBlasMatrixView<T>& matrix,
                    asc::DenseBlasVectorView<T>& e,
                    asc::DenseBlasVectorView<asc::index_t>& pivots,
                    asc::LapackWorkspace& workspace,
                    asc::DenseBlasLayout layout) {
  switch (bad) {
    case 23:
      matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
          f.a.data() + 1, 2, 2, layout, 3,
          {f.a.data(), sizeof(f.a), asc::MemorySpace::kPinnedHost}));
      break;
    case 24:
      e = base::Take(asc::DenseBlasVectorView<T>::Create(
          f.e.data() + 1, 2, 1,
          {f.e.data(), sizeof(f.e), asc::MemorySpace::kPinnedHost}));
      break;
    case 25:
      pivots = base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          f.p.data() + 1, 2, 1,
          {f.p.data(), sizeof(f.p), asc::MemorySpace::kPinnedHost}));
      break;
    case 26:
      workspace.regions[kPivot] = {workspace.regions[kPivot].data(),
                                   workspace.regions[kPivot].size(),
                                   asc::MemorySpace::kPinnedHost};
      break;
    default:
      break;
  }
}

template <typename T>
void AlterMetadata(int bad, Fixture<T>& f, asc::DenseBlasMatrixView<T>& matrix,
                   asc::DenseBlasVectorView<T>& e,
                   asc::LapackWorkspacePlan& plan,
                   asc::LapackWorkspace& workspace, bool& selected_blocked,
                   bool blocked, asc::DenseBlasLayout layout) {
  switch (bad) {
    case 27:
      workspace.regions[kPivot] = {&f.report, workspace.regions[kPivot].size(),
                                   base::kHost};
      break;
    case 28:
      matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(&f.report), 2, 2, layout, 3,
          {&f.report, sizeof(f.report), base::kHost}));
      break;
    case 29:
      e = base::Take(asc::DenseBlasVectorView<T>::Create(
          reinterpret_cast<T*>(&f.report), 2, 1,
          {&f.report, sizeof(f.report), base::kHost}));
      break;
    case 30:
      selected_blocked = !blocked;
      break;
    case 31:
      e = base::Take(asc::DenseBlasVectorView<T>::Create(
          reinterpret_cast<T*>(&plan), 2, 1,
          {&plan, sizeof(plan), base::kHost}));
      break;
    case 32:
      e = base::Take(asc::DenseBlasVectorView<T>::Create(
          reinterpret_cast<T*>(&workspace), 2, 1,
          {&workspace, sizeof(workspace), base::kHost}));
      break;
    case 33:
      matrix = base::Matrix(
          f.a, 2, 2, layout == base::kColumn ? base::kRow : base::kColumn, 3);
      break;
    case 34:
      workspace.regions[kPivot] = {&workspace, workspace.regions[kPivot].size(),
                                   base::kHost};
      break;
    default:
      break;
  }
}

template <typename T>
void AlterStorage(int bad, Fixture<T>& f, asc::LapackWorkspace& workspace) {
  switch (bad) {
    case 35:
      workspace.regions[kScalar] = {nullptr, 0, base::kHost};
      break;
    case 36:
      workspace.regions[kScalar] = {f.scratch.scalar.data() + 1, sizeof(T) - 1,
                                    base::kHost};
      break;
    case 37:
      workspace.regions[kScalar] = {
          reinterpret_cast<std::byte*>(f.scratch.scalar.data()) + 1,
          workspace.regions[kScalar].size(), base::kHost};
      break;
    case 38:
      workspace.regions[kScalar] = {f.e.data() + 1, sizeof(T), base::kHost};
      break;
    case 39:
      workspace.regions[kLayout] = {nullptr, 0, base::kHost};
      break;
    case 40:
      workspace.regions[kLayout] = {
          f.scratch.packed.data() + 1,
          workspace.regions[kLayout].size() - sizeof(T), base::kHost};
      break;
    case 41:
      workspace.regions[kLayout] = {
          reinterpret_cast<std::byte*>(f.scratch.packed.data()) + 1,
          workspace.regions[kLayout].size(), base::kHost};
      break;
    case 42:
      workspace.regions[kLayout] = {
          f.a.data(), workspace.regions[kLayout].size(), base::kHost};
      break;
    default:
      break;
  }
}

template <typename T>
void Rejection(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool hermitian, asc::DenseBlasTriangle triangle,
               asc::DenseBlasLayout layout, bool blocked, int bad) {
  Fixture<T> f;
  auto matrix = base::Matrix(f.a, 2, 2, layout, 3);
  auto e = base::OffDiagonal(f.e, 2);
  auto pivots = base::Pivots(f.p, 2);
  auto plan = base::Take(base::QueryFactor(provider, triangle, hermitian,
                                           blocked, matrix, e, pivots));
  auto workspace = f.scratch.Workspace(plan);
  auto selected = triangle;
  bool selected_blocked = blocked;
  AlterWorkspace(bad, f, plan, workspace);
  AlterOperands(bad, f, matrix, e, pivots, selected, triangle, layout);
  AlterPlacement(bad, f, matrix, e, pivots, workspace, layout);
  AlterMetadata(bad, f, matrix, e, plan, workspace, selected_blocked, blocked,
                layout);
  AlterStorage(bad, f, workspace);
  const auto old_a = f.a;
  const auto old_e = f.e;
  const auto old_p = f.p;
  const auto old_scalar = f.scratch.scalar;
  const auto old_packed = f.scratch.packed;
  const auto old_bytes = f.scratch.pivot;
  std::array<std::byte, sizeof(asc::LapackReport)> old_report{};
  std::memcpy(old_report.data(), &f.report, sizeof(f.report));
  const auto status = base::WithoutAllocation(test, [&] {
    return base::Factor(provider, selected, hermitian, selected_blocked, matrix,
                        e, pivots, plan, workspace, f.report);
  });
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  const bool metadata = bad == 27 || bad == 28 || bad == 29 || bad == 31 ||
                        bad == 32 || bad == 34;
  if (metadata) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(&f.report, old_report.data(), sizeof(f.report)));
  } else {
    ASC_DENSE_TEST_CHECK(test,
                         !f.report.called_provider && !f.report.native_info);
  }
  ASC_DENSE_TEST_EQ(test, f.a, old_a);
  ASC_DENSE_TEST_EQ(test, f.e, old_e);
  ASC_DENSE_TEST_EQ(test, f.p, old_p);
  ASC_DENSE_TEST_EQ(test, f.scratch.scalar, old_scalar);
  ASC_DENSE_TEST_EQ(test, f.scratch.packed, old_packed);
  ASC_DENSE_TEST_EQ(test, f.scratch.pivot, old_bytes);
}

template <typename T>
void Unread(TestContext& test, const asc::ReferenceLapackProvider& provider,
            bool hermitian) {
#if defined(__linux__)
  const auto page = sysconf(_SC_PAGESIZE);
  ASC_DENSE_TEST_CHECK(test, page >= 4096);
  if (page < 4096) {
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
  auto* e = reinterpret_cast<T*>(static_cast<std::byte*>(memory) + 512);
  auto* p =
      reinterpret_cast<asc::index_t*>(static_cast<std::byte*>(memory) + 1024);
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (bool blocked : {false, true}) {
        for (const asc::extent_t n : {0, 1, 3}) {
          const auto matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
              a, n, n, layout, 4, {a, 512, base::kHost}));
          const auto extra = base::Take(asc::DenseBlasVectorView<T>::Create(
              e, n, 1, {e, 512, base::kHost}));
          const auto pivots =
              base::Take(asc::DenseBlasVectorView<asc::index_t>::Create(
                  p, n, 1, {p, 512, base::kHost}));
          const auto plan = base::Take(base::WithoutAllocation(test, [&] {
            return base::QueryFactor(provider, triangle, hermitian, blocked,
                                     matrix, extra, pivots);
          }));
          if (n == 0) {
            asc::LapackReport report;
            const auto status = base::WithoutAllocation(test, [&] {
              return base::Factor(provider, triangle, hermitian, blocked,
                                  matrix, extra, pivots, plan, {}, report);
            });
            ASC_DENSE_TEST_CHECK(test, status.ok() && !report.called_provider &&
                                           !report.native_info);
            ASC_DENSE_TEST_EQ(test, report.output_validity,
                              asc::LapackOutputValidity::kComplete);
          }
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

void Counts(TestContext& test) {
  namespace counts = asc::internal_indefinite_counts;
  for (const asc::extent_t limit : {static_cast<asc::extent_t>(INT32_MAX),
                                    static_cast<asc::extent_t>(INT64_MAX)}) {
    ASC_DENSE_TEST_CHECK(test, counts::Factor(0, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Factor(2, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Factor(limit - 1, 1, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Factor(limit, 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, counts::Factor(3, (limit - 1) / 2, limit).ok());
    ASC_DENSE_TEST_EQ(test,
                      counts::Factor(3, (limit - 1) / 2 + 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, counts::Factor(-1, 1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, counts::Factor(1, 0, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(test,
                         counts::Preferred<float>(limit / 128, limit).ok());
    ASC_DENSE_TEST_CHECK(test,
                         counts::Preferred<double>(limit / 128, limit).ok());
    ASC_DENSE_TEST_EQ(
        test, counts::Preferred<float>(limit / 64 + 1, limit).status().code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test, counts::Preferred<double>(limit / 64 + 1, limit).status().code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, counts::Preferred<float>(-1, limit).status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
void Provenance(TestContext& test, const asc::ReferenceLapackProvider& provider,
                bool hermitian) {
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (bool blocked : {false, true}) {
        Fixture<T> f;
        f.a[1] = T{4};
        const auto a = base::Matrix(f.a, 1, 1, layout, 3);
        const auto e = base::OffDiagonal(f.e, 1);
        const auto p = base::Pivots(f.p, 1);
        const auto plan = base::Take(
            base::QueryFactor(provider, triangle, hermitian, blocked, a, e, p));
        ASC_DENSE_TEST_CHECK(
            test, base::Factor(provider, triangle, hermitian, blocked, a, e, p,
                               plan, f.scratch.Workspace(plan), f.report)
                      .ok());
        const auto raw = asc_indefinite_rook_test::Raw(f.p, 1);
        const auto matrix = asc::DenseBlasMatrixView<const T>::Create(
            f.a.data() + 1, 1, 1, layout, 3,
            {f.a.data(), sizeof(f.a), base::kHost});
        ASC_DENSE_TEST_CHECK(test, matrix.ok());
        const auto view = asc::ReferenceRookFactorView<T>::Create(
            provider, *matrix, triangle,
            hermitian ? asc::LapackBunchKaufmanSymmetry::kHermitian
                      : asc::LapackBunchKaufmanSymmetry::kSymmetric,
            raw, f.report);
        ASC_DENSE_TEST_CHECK(test, !view.ok());
        ASC_DENSE_TEST_EQ(test, view.status().code(),
                          asc::ErrorCode::kInvalidState);
      }
    }
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
      for (bool blocked : {false, true}) {
        for (int bad = 0; bad < 43; ++bad) {
          if (bad >= 35 && bad <= 38 && !blocked) {
            continue;
          }
          if (bad >= 39 && !hermitian && layout == base::kColumn) {
            continue;
          }
          Rejection<T>(test, provider, hermitian, triangle, layout, blocked,
                       bad);
          ++cases;
        }
      }
    }
  }
  Counts(test);
  Unread<T>(test, provider, hermitian);
  Provenance<T>(test, provider, hermitian);
  std::printf(
      "RK rejection cases=%d protected_queries=24 empty_noncalls=8 "
      "provenance_rejections=8 count_checks=26\n",
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
