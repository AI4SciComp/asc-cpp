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
#include "indefinite_packed_solve_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"

namespace {
namespace solve = asc_packed_solve_test;
namespace base = asc_indefinite_test;
using base::TestContext;

template <typename T>
void WorkspaceFault(int fault, solve::Fixture<T>& sample,
                    asc::LapackWorkspacePlan& plan, asc::LapackWorkspace& work,
                    base::Scratch<T>& scratch) {
  const auto pivot = work.regions[base::kPivot];
  const auto layout = work.regions[base::kLayout];
  switch (fault) {
    case 0:
      work.regions[base::kPivot] = {pivot.data(), pivot.size() - 1,
                                    base::kHost};
      break;
    case 1:
      work.regions[base::kPivot] = {scratch.pivot.data() + 17, pivot.size(),
                                    base::kHost};
      break;
    case 2:
      work.regions[base::kPivot] = {pivot.data(), pivot.size(),
                                    asc::MemorySpace::kPinnedHost};
      break;
    case 3:
      work.regions[base::kPivot] = {sample.a.a.data() + 1, pivot.size(),
                                    base::kHost};
      break;
    case 4:
      work.regions[base::kPivot] = {sample.a.pivots.data() + 1, pivot.size(),
                                    base::kHost};
      break;
    case 5:
      work.regions[base::kPivot] = {sample.rhs.data() + 1, pivot.size(),
                                    base::kHost};
      break;
    case 6:
      work.regions.back() = pivot;
      break;
    case 7:
      ++plan.regions[base::kPivot].minimum_entries;
      break;
    case 8:
      ++plan.regions[base::kPivot].preferred_entries;
      break;
    case 9:
      ++plan.regions[base::kPivot].entry_bytes;
      break;
    case 10:
      ++plan.regions[base::kPivot].alignment;
      break;
    case 11:
      plan.total_byte_limit = 1;
      break;
    case 12:
      if (layout.size() != 0) {
        work.regions[base::kLayout] = {layout.data(), layout.size() - 1,
                                       base::kHost};
      } else {
        ++plan.regions[base::kLayout].minimum_entries;
      }
      break;
    case 13:
      work.regions[base::kLayout] = pivot;
      break;
    case 14:
      work.regions[base::kScalar] = {sample.rhs.data() + 1, sizeof(T),
                                     base::kHost};
      break;
    default:
      break;
  }
}

template <typename T>
void OperandFault(int fault, solve::Fixture<T>& sample,
                  asc::DenseBlasTriangle& triangle,
                  asc::DenseBlasPackedMatrixView<const T>& factors,
                  asc::RawLapackPivotView& pivots,
                  asc::DenseBlasMatrixView<T>& rhs) {
  auto& a = sample.a;
  switch (fault) {
    case 15:
      triangle = a.triangle == solve::kUpper ? solve::kLower : solve::kUpper;
      break;
    case 16:
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      triangle = static_cast<asc::DenseBlasTriangle>(99);
      break;
    case 17:
      factors = solve::Take(asc::DenseBlasPackedMatrixView<const T>::Create(
          a.a.data() + 1, a.n,
          a.layout == solve::kColumn ? solve::kRow : solve::kColumn,
          {a.a.data(), sizeof(a.a), solve::kHost}));
      break;
    case 18:
      factors = solve::Take(asc::DenseBlasPackedMatrixView<const T>::Create(
          a.a.data() + 1, a.n, a.layout,
          {a.a.data(), sizeof(a.a), asc::MemorySpace::kPinnedHost}));
      break;
    case 19:
      pivots = solve::Take(asc::RawLapackPivotView::Create(
          a.pivots.data() + 1, a.n, asc::LapackFactorFamily::kRook,
          {a.pivots.data(), sizeof(a.pivots), solve::kHost}));
      break;
    case 20:
      pivots = solve::Take(asc::RawLapackPivotView::Create(
          a.pivots.data() + 1, a.n - 1, asc::LapackFactorFamily::kBunchKaufman,
          {a.pivots.data(), sizeof(a.pivots), solve::kHost}));
      break;
    case 21:
      rhs = solve::Take(asc::DenseBlasMatrixView<T>::Create(
          sample.rhs.data() + 1, a.n, sample.nrhs, sample.rhs_layout,
          sample.Leading() + 1,
          {sample.rhs.data(), sizeof(sample.rhs), solve::kHost}));
      break;
    case 22:
      rhs = solve::Take(asc::DenseBlasMatrixView<T>::Create(
          sample.rhs.data() + 1, a.n, sample.nrhs - 1, sample.rhs_layout,
          sample.Leading(),
          {sample.rhs.data(), sizeof(sample.rhs), solve::kHost}));
      break;
    case 23:
      rhs = solve::Take(asc::DenseBlasMatrixView<T>::Create(
          a.a.data() + 1, a.n, sample.nrhs, sample.rhs_layout, sample.Leading(),
          {a.a.data(), sizeof(a.a), solve::kHost}));
      break;
    case 24:
      rhs = solve::Take(asc::DenseBlasMatrixView<T>::Create(
          sample.rhs.data() + 1, a.n - 1, sample.nrhs, sample.rhs_layout,
          sample.Leading(),
          {sample.rhs.data(), sizeof(sample.rhs), solve::kHost}));
      break;
    case 25:
      rhs = solve::Take(asc::DenseBlasMatrixView<T>::Create(
          sample.rhs.data() + 1, a.n, sample.nrhs, sample.rhs_layout,
          sample.Leading(),
          {sample.rhs.data(), sizeof(sample.rhs),
           asc::MemorySpace::kPinnedHost}));
      break;
    case 40:
      rhs = solve::Take(asc::DenseBlasMatrixView<T>::Create(
          sample.rhs.data() + 1, a.n, sample.nrhs,
          sample.rhs_layout == solve::kColumn ? solve::kRow : solve::kColumn,
          sample.Leading(),
          {sample.rhs.data(), sizeof(sample.rhs), solve::kHost}));
      break;
    default:
      break;
  }
}

template <typename T>
void PivotFault(int fault, solve::Fixture<T>& sample) {
  auto& a = sample.a;
  switch (fault) {
    case 26:
      a.pivots[1] = 0;
      break;
    case 27:
      a.pivots[1] = a.n + 1;
      break;
    case 28:
      a.pivots[1] = std::numeric_limits<asc::index_t>::min();
      break;
    case 29:
      a.pivots[1] = -1;
      a.pivots[2] = -2;
      break;
    case 30:
      a.pivots[a.triangle == solve::kUpper ? 1 : a.n] =
          a.triangle == solve::kUpper ? a.n : 1;
      break;
    case 31:
      a.pivots[a.n] = -1;
      break;
    case 32:
      a.pivots[1] = a.triangle == solve::kUpper ? -2 : -1;
      a.pivots[2] = a.pivots[1];
      break;
    case 33:
      a.a[a.Offset(1, 1)] = T{};
      break;
    case 38:
    case 39:
      a.pivots[1] = a.triangle == solve::kUpper ? -1 : -2;
      a.pivots[2] = a.pivots[1];
      a.a[a.Offset(0, 0)] = T{1};
      a.a[a.Offset(1, 1)] = T{1};
      a.a[a.triangle == solve::kUpper ? a.Offset(0, 1) : a.Offset(1, 0)] =
          fault == 38 ? T{} : T{1};
      break;
    default:
      break;
  }
}

template <typename T>
void Reject(TestContext& test, const asc::ReferenceLapackProvider& provider,
            solve::Fixture<T> sample, int fault) {
  sample.Prepare(test, provider);
  auto factors = sample.a.ConstView();
  auto pivots = sample.Pivots();
  auto rhs = sample.Rhs();
  auto triangle = sample.a.triangle;
  auto plan = solve::Take(solve::Query(provider, triangle, sample.a.hermitian,
                                       factors, pivots, rhs));
  base::Scratch<T> scratch;
  auto work = scratch.Workspace(plan);
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 271;
  WorkspaceFault(fault, sample, plan, work, scratch);
  OperandFault(fault, sample, triangle, factors, pivots, rhs);
  PivotFault(fault, sample);
  if (fault == 34) {
    work.regions[base::kPivot] = {&report, work.regions[base::kPivot].size(),
                                  base::kHost};
  }
  if (fault == 35) {
    work.regions[base::kPivot] = {&plan, work.regions[base::kPivot].size(),
                                  base::kHost};
  }
  if (fault == 36) {
    work.regions[base::kPivot] = {&work, work.regions[base::kPivot].size(),
                                  base::kHost};
  }
  if (fault == 37) {
    factors = solve::Take(asc::DenseBlasPackedMatrixView<const T>::Create(
        reinterpret_cast<const T*>(&report), 2, sample.a.layout,
        {&report, sizeof(report), base::kHost}));
  }
  const auto before_a = sample.a.a;
  const auto before_p = sample.a.pivots;
  const auto before_b = sample.rhs;
  const auto before_scratch = scratch;
  std::array<std::byte, sizeof(report)> before_report;
  std::memcpy(before_report.data(), &report, sizeof(report));
  const auto status = base::WithoutAllocation(test, [&] {
    return solve::Solve(provider, triangle, sample.a.hermitian, factors, pivots,
                        rhs, plan, work, report);
  });
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  if (fault >= 34 && fault <= 37) {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(&report, before_report.data(), sizeof(report)));
  } else {
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    if (fault == 33 || fault == 38 || fault == 39) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index, fault == 33 ? 1 : 0);
    }
  }
  ASC_DENSE_TEST_CHECK(
      test,
      base::EqualBytes(sample.a.a.data(), before_a.data(), sizeof(before_a)));
  ASC_DENSE_TEST_EQ(test, sample.a.pivots, before_p);
  ASC_DENSE_TEST_CHECK(
      test,
      base::EqualBytes(sample.rhs.data(), before_b.data(), sizeof(before_b)));
  ASC_DENSE_TEST_EQ(test, scratch.scalar, before_scratch.scalar);
  ASC_DENSE_TEST_EQ(test, scratch.packed, before_scratch.packed);
  ASC_DENSE_TEST_EQ(test, scratch.pivot, before_scratch.pivot);
}

#if defined(__linux__)
template <typename T>
void Protected(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool hermitian) {
  const auto page = sysconf(_SC_PAGESIZE);
  ASC_DENSE_TEST_CHECK(test, page > 0);
  if (page <= 0) {
    return;
  }
  const auto size = static_cast<std::size_t>(page);
  void* memory =
      mmap(nullptr, 3 * size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASC_DENSE_TEST_CHECK(test, memory != MAP_FAILED);
  if (memory == MAP_FAILED) {
    return;
  }
  auto* bytes = static_cast<std::byte*>(memory);
  for (auto layout : {solve::kColumn, solve::kRow}) {
    for (auto triangle : {solve::kUpper, solve::kLower}) {
      const auto a =
          solve::Take(asc::DenseBlasPackedMatrixView<const T>::Create(
              reinterpret_cast<const T*>(bytes), 2, layout,
              {bytes, size, base::kHost}));
      const auto pivots = solve::Take(asc::RawLapackPivotView::Create(
          reinterpret_cast<const asc::index_t*>(bytes + size), 2,
          asc::LapackFactorFamily::kBunchKaufman,
          {bytes + size, size, base::kHost}));
      const auto rhs = solve::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(bytes + 2 * size), 2, 2, layout, 2,
          {bytes + 2 * size, size, base::kHost}));
      const auto plan = solve::Take(base::WithoutAllocation(test, [&] {
        return solve::Query(provider, triangle, hermitian, a, pivots, rhs);
      }));
      ASC_DENSE_TEST_EQ(test, plan.regions[base::kPivot].minimum_entries, 2);
      const auto empty_rhs = solve::Take(asc::DenseBlasMatrixView<T>::Create(
          reinterpret_cast<T*>(bytes + 2 * size), 2, 0, layout, 2,
          {bytes + 2 * size, size, base::kHost}));
      const auto empty_plan = solve::Take(
          solve::Query(provider, triangle, hermitian, a, pivots, empty_rhs));
      asc::LapackReport report;
      ASC_DENSE_TEST_CHECK(test, base::WithoutAllocation(test, [&] {
                                   return solve::Solve(
                                       provider, triangle, hermitian, a, pivots,
                                       empty_rhs, empty_plan, {}, report);
                                 }).ok());
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
      // Missing scratch is rejected before any numerical operand is observed.
      ASC_DENSE_TEST_CHECK(test, !solve::Solve(provider, triangle, hermitian, a,
                                               pivots, rhs, plan, {}, report)
                                      .ok());
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
    }
  }
  ASC_DENSE_TEST_EQ(test, munmap(memory, 3 * size), 0);
}
#endif

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = solve::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (auto triangle : {solve::kUpper, solve::kLower}) {
    for (auto al : {solve::kColumn, solve::kRow}) {
      for (auto bl : {solve::kColumn, solve::kRow}) {
        for (int fault = 0; fault < 41; ++fault) {
          std::printf("Packed solve rejection tri=%d al=%d bl=%d fault=%d\n",
                      static_cast<int>(triangle), static_cast<int>(al),
                      static_cast<int>(bl), fault);
          Reject(test, provider,
                 solve::Fixture<T>(3, 2, hermitian, triangle, al, bl, 0, 0),
                 fault);
          ++cases;
        }
      }
    }
  }
#if defined(__linux__)
  Protected<T>(test, provider, hermitian);
#endif
  ASC_DENSE_TEST_EQ(test, cases, 328);
  std::printf("Packed solve rejection cases=%d\n", cases);
  return test.Finish();
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
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
