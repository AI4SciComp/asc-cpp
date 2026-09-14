#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string_view>
#include <tuple>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_helpers.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_helpers_metadata_probe.h"
#include "lu_helpers_test_support.h"

namespace {
namespace support = asc_helpers_test;
namespace probe = asc_helpers_metadata;
using support::Take;
using support::TestContext;
constexpr auto kHost = asc::MemorySpace::kHost;

asc::LapackReport PriorReport() {
  asc::LapackReport report;
  report.routine.fill('Q');
  report.called_provider = true;
  report.native_info = 79;
  report.native_argument = 31;
  report.diagnostic_index = 17;
  report.outcome = asc::LapackOutcome::kSingular;
  report.output_validity = asc::LapackOutputValidity::kUnusable;
  return report;
}
template <typename T>
auto Snapshot(const T& object) {
  std::array<std::byte, sizeof(T)> result{};
  const auto bytes = std::as_bytes(std::span(&object, 1));
  std::copy(bytes.begin(), bytes.end(), result.begin());
  return result;
}
template <typename T>
void Preserved(TestContext& test, const T& object,
               const std::array<std::byte, sizeof(T)>& before) {
  const auto bytes = std::as_bytes(std::span(&object, 1));
  ASC_DENSE_TEST_CHECK(test,
                       std::equal(bytes.begin(), bytes.end(), before.begin()));
}
template <typename Operation, typename... Objects>
auto Preserving(TestContext& test, Operation operation,
                const Objects&... objects) {
  const auto before = std::tuple{Snapshot(objects)...};
  auto result = support::WithoutAllocation(test, operation);
  std::apply(
      [&](const auto&... bytes) { (Preserved(test, objects, bytes), ...); },
      before);
  return result;
}

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  asc::ReferenceLapackProvider provider;
  support::Sample<T> sample;
  asc::DenseBlasMatrixView<T> matrix;
  asc::RawLapackPivotView pivots;
  asc::DenseBlasVectorView<const Real> rows;
  asc::DenseBlasVectorView<const Real> columns;
  bool scale;
  asc::index_t increment;
  asc::LapackWorkspacePlan plan;
  asc::LapackWorkspace workspace;
  asc::LapackReport report = PriorReport();
  Fixture(bool scaling, bool empty, asc::DenseBlasLayout layout, int mode)
      : provider(Take(asc::ReferenceLapackProvider::Create(
            asc::ExecutionContext::Serial()))),
        sample(empty ? 0 : 1, empty ? 0 : 1, layout),
        matrix(sample.Matrix()),
        pivots(sample.Pivots(empty ? 0 : 1)),
        rows(sample.Rows()),
        columns(sample.Columns()),
        scale(scaling),
        increment(mode - 1),
        plan(Prepare(mode)) {
    for (auto role : {support::kInteger, support::kLayout}) {
      const auto& region = plan.regions[role];
      if (region.minimum_entries == 0) {
        continue;
      }
      void* data = role == support::kInteger
                       ? static_cast<void*>(sample.integer.data() + 16)
                       : static_cast<void*>(sample.packed.data() + 1);
      workspace.regions[role] = {
          data,
          static_cast<std::size_t>(region.minimum_entries) * region.entry_bytes,
          kHost};
    }
  }
  asc::LapackWorkspacePlan Prepare(int mode) {
    sample.pivots[1] = 1;
    sample.rows[1] = Real{2};
    sample.columns[1] = Real{3};
    sample.statistics.row_condition =
        mode == 1 || mode == 3 ? Real{0.05} : Real{1};
    sample.statistics.column_condition =
        mode == 2 || mode == 3 ? Real{0.05} : Real{1};
    return scale ? Take(asc::QueryLaqgeWorkspace(provider, matrix, rows,
                                                 columns, sample.statistics,
                                                 sample.applied))
                 : Take(asc::QueryLaswpWorkspace(provider, matrix, 0, sample.m,
                                                 pivots, increment));
  }
  auto Run() {
    return scale
               ? asc::Laqge(provider, matrix, rows, columns, sample.statistics,
                            sample.applied, plan, workspace, report)
               : asc::Laswp(provider, matrix, 0, sample.m, pivots, increment,
                            plan, workspace, report);
  }
};

template <typename T>
auto Objects(Fixture<T>& f) {
  static_assert(alignof(asc::ReferenceLapackProvider) >= alignof(T));
  static_assert(alignof(asc::LapackWorkspacePlan) >= alignof(T));
  static_assert(alignof(asc::LapackWorkspace) >= alignof(T));
  static_assert(alignof(asc::LapackReport) >= alignof(T));
  return std::array<void*, 4>{&f.provider, &f.plan, &f.workspace, &f.report};
}

template <typename T>
void SetAlias(Fixture<T>& f, std::size_t object, std::size_t target) {
  const auto pointer = Objects(f)[object];
  const asc::MutableMemoryView bytes{pointer, sizeof(T), kHost};
  if (target == 0) {
    f.matrix = Take(asc::DenseBlasMatrixView<T>::Create(
        static_cast<T*>(pointer), 1, 1, f.sample.layout, f.sample.Ld(), bytes));
  } else if (target < 3 && f.scale) {
    using Real = typename Fixture<T>::Real;
    const auto vector = Take(asc::DenseBlasVectorView<const Real>::Create(
        static_cast<const Real*>(pointer), 1, 1, bytes));
    (target == 1 ? f.rows : f.columns) = vector;
  } else if (target == 1) {
    f.pivots = Take(asc::RawLapackPivotView::Create(
        static_cast<const asc::index_t*>(pointer), 1, f.pivots.family(),
        {pointer, sizeof(asc::index_t), kHost}));
  } else {
    f.workspace.regions[target - 3] = bytes;
  }
}

template <typename T>
void Alias(TestContext& test, bool scale, bool empty,
           asc::DenseBlasLayout layout, int mode, std::size_t object,
           std::size_t target) {
  Fixture<T> f(scale, empty, layout, mode);
  // Descriptors refer to live metadata bytes, which must not be interpreted as
  // numerical values. No protected pages or invalid native arguments are used.
  SetAlias(f, object, target);
  probe::Reset();
  const auto status = Preserving(
      test, [&] { return f.Run(); }, f.provider, f.plan, f.workspace, f.report,
      f.sample.a, f.sample.rows, f.sample.columns, f.sample.pivots,
      f.sample.statistics, f.sample.applied, f.sample.packed, f.sample.integer);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, probe::Read().swap, std::size_t{0});
  ASC_DENSE_TEST_EQ(test, probe::Read().scale, std::size_t{0});
}

template <typename T>
void Control(TestContext& test, bool scale, bool empty,
             asc::DenseBlasLayout layout, int mode) {
  Fixture<T> f(scale, empty, layout, mode);
  const auto before = f.sample;
  probe::Reset();
  const auto status = support::WithoutAllocation(test, [&] { return f.Run(); });
  const bool entered = !empty && (scale || f.increment != 0);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, probe::Read().swap,
                    !scale && entered ? std::size_t{1} : std::size_t{0});
  ASC_DENSE_TEST_EQ(test, probe::Read().scale,
                    scale && entered ? std::size_t{1} : std::size_t{0});
  ASC_DENSE_TEST_EQ(test, f.report.called_provider, entered);
  ASC_DENSE_TEST_CHECK(test, !f.report.native_info.has_value() &&
                                 !f.report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, f.report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  if (!empty) {
    int multiplier = 1;
    if (scale && (mode == 1 || mode == 3)) {
      multiplier *= 2;
    }
    if (scale && (mode == 2 || mode == 3)) {
      multiplier *= 3;
    }
    const auto i = f.sample.Offset(0, 0);
    ASC_DENSE_TEST_EQ(
        test, f.sample.a[i],
        before.a[i] * static_cast<typename Fixture<T>::Real>(multiplier));
  }
  support::Padding(test, f.sample, before);
  ++f.plan.regions[support::kInteger].minimum_entries;
  f.report = PriorReport();
  probe::Reset();
  const auto rejected = Preserving(
      test, [&] { return f.Run(); }, f.sample.a, f.sample.pivots, f.sample.rows,
      f.sample.columns, f.sample.statistics, f.sample.applied, f.sample.packed,
      f.sample.integer);
  ASC_DENSE_TEST_EQ(test, rejected.code(), asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_EQ(test, probe::Read().swap, std::size_t{0});
  ASC_DENSE_TEST_EQ(test, probe::Read().scale, std::size_t{0});
  ASC_DENSE_TEST_CHECK(
      test, !f.report.called_provider && !f.report.native_info.has_value());
}

template <typename T>
void Run(TestContext& test) {
  std::size_t aliases = 0;
  std::size_t controls = 0;
  for (auto layout : {support::kRow, support::kColumn}) {
    for (bool scale : {false, true}) {
      for (bool empty : {false, true}) {
        for (int mode = 0; mode < (scale ? 4 : 3); ++mode) {
          Control<T>(test, scale, empty, layout, mode);
          ++controls;
          for (std::size_t object = 0; object < 4; ++object) {
            for (std::size_t target = 0;
                 target < 3 + asc::LapackWorkspace{}.regions.size(); ++target) {
              if ((empty && target < 3) || (!scale && target == 2)) {
                continue;
              }
              Alias<T>(test, scale, empty, layout, mode, object, target);
              ++aliases;
            }
          }
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, aliases, std::size_t{1040});
  ASC_DENSE_TEST_EQ(test, controls, std::size_t{28});
  std::printf("HELPERS_METADATA aliases=%zu valid/stale controls=%zu\n",
              aliases, 2 * controls);
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test);
  } else if (scalar == "d") {
    Run<double>(test);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test);
  } else {
    return 2;
  }
  return test.Finish();
}
