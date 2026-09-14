#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_complete_pivoting.h"
#include "complete_pivoting_faults.h"
#include "complete_pivoting_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace support = asc_complete_pivoting_test;
namespace fault = asc_complete_pivoting_fault;
using support::Take;
using support::TestContext;
using support::Vector;
constexpr auto kHost = asc::MemorySpace::kHost;

asc::LapackReport PriorReport() {
  asc::LapackReport report;
  report.routine.fill('Q');
  report.called_provider = true;
  report.native_info = 79;
  report.native_argument = 31;
  report.diagnostic_index = 1;
  report.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
  report.outcome = asc::LapackOutcome::kSingular;
  report.output_validity = asc::LapackOutputValidity::kUnusable;
  return report;
}

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  using Factor = asc::ReferenceCompletePivotLuFactorView<T>;
  asc::ReferenceLapackProvider provider;
  support::Problem<T> problem;
  asc::DenseBlasMatrixView<T> matrix;
  asc::DenseBlasVectorView<asc::index_t> rows;
  asc::DenseBlasVectorView<asc::index_t> columns;
  asc::DenseBlasVectorView<T> rhs;
  asc::DenseBlasVectorView<Real> scale;
  asc::LapackWorkspacePlan plan;
  support::Scratch<T> scratch;
  asc::LapackWorkspace workspace;
  asc::LapackReport provenance;
  asc::LapackReport report = PriorReport();
  Factor factor;
  bool solve;
  Fixture(TestContext& test, bool solving, asc::DenseBlasLayout layout,
          asc::extent_t n = 3, bool zero = false)
      : provider(Take(asc::ReferenceLapackProvider::Create(
            asc::ExecutionContext::Serial()))),
        problem(n, 2, layout, 0, 0, zero),
        matrix(problem.Matrix()),
        rows(Vector(problem.rows, n)),
        columns(Vector(problem.columns, n)),
        rhs(Vector(problem.rhs, n)),
        scale(Vector(problem.scale, 1)),
        plan(Take(problem.Query(provider))),
        workspace(scratch.Workspace(plan)),
        factor(Prepare(test)),
        solve(solving) {
    if (solve) {
      plan = Take(asc::QueryGesc2Workspace(provider, factor, rhs, scale));
    }
    workspace = scratch.Workspace(plan);
  }
  Factor Prepare(TestContext& test) {
    fault::Reset(fault::Mode::kPass);
    ASC_DENSE_TEST_CHECK(
        test, problem.Factor(provider, plan, workspace, provenance).ok());
    ASC_DENSE_TEST_CHECK(test, fault::BaselinePassed());
    return Take(problem.View(provenance));
  }
  auto Run() {
    return solve ? asc::Gesc2(provider, factor, rhs, scale, plan, workspace,
                              report)
                 : asc::Getc2(provider, matrix, rows, columns, plan, workspace,
                              report);
  }
};

template <typename T>
auto Snapshot(const T& object) {
  std::array<std::byte, sizeof(T)> bytes{};
  const auto representation = std::as_bytes(std::span(&object, 1));
  std::copy(representation.begin(), representation.end(), bytes.begin());
  return bytes;
}
template <typename T>
void Preserved(TestContext& test, const T& object,
               const std::array<std::byte, sizeof(T)>& before) {
  const auto actual = std::as_bytes(std::span(&object, 1));
  ASC_DENSE_TEST_CHECK(
      test, std::equal(actual.begin(), actual.end(), before.begin()));
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
void Reject(TestContext& test, Fixture<T>& fixture, asc::ErrorCode expected) {
  auto& p = fixture.problem;
  fixture.report = PriorReport();
  fault::Reset(fault::Mode::kPass);
  const auto status = Preserving(
      test, [&] { return fixture.Run(); }, p.dense, p.rhs, p.rows, p.columns,
      p.scale, fixture.scratch, fixture.plan, fixture.workspace,
      fixture.provider);
  ASC_DENSE_TEST_EQ(test, status.code(), expected);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
  ASC_DENSE_TEST_CHECK(test, !fixture.report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !fixture.report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !fixture.report.native_argument.has_value());
}

template <typename T>
auto Objects(Fixture<T>& f) {
  static_assert(std::is_same_v<asc::index_t, asc::extent_t>);
  static_assert(alignof(asc::ReferenceLapackProvider) >= alignof(T));
  static_assert(alignof(asc::LapackWorkspacePlan) >= alignof(T));
  static_assert(alignof(asc::LapackWorkspace) >= alignof(T));
  static_assert(alignof(asc::LapackReport) >= alignof(T));
  return std::array<void*, 4>{&f.provider, &f.plan, &f.workspace, &f.report};
}

template <typename T>
void AliasOperand(Fixture<T>& f, std::size_t object, std::size_t target) {
  const auto objects = Objects(f);
  const asc::MutableMemoryView alias{objects[object], sizeof(T), kHost};
  if (target == 0) {
    if (f.solve) {
      const auto matrix = Take(asc::DenseBlasMatrixView<const T>::Create(
          static_cast<const T*>(alias.data()), 1, 1, f.problem.layout,
          f.problem.leading, alias));
      f.factor = Take(Fixture<T>::Factor::Create(matrix, f.factor.row_pivots(),
                                                 f.factor.column_pivots(),
                                                 f.provenance));
    } else {
      f.matrix = Take(asc::DenseBlasMatrixView<T>::Create(
          static_cast<T*>(alias.data()), 1, 1, f.problem.layout,
          f.problem.leading, alias));
    }
  } else if (target == 1 || target == 2) {
    if (f.solve) {
      // Factor construction reads pivots: use a real live index subobject.
      f.plan.regions[support::kScratch].minimum_entries = 1;
      if (!f.report.diagnostic_index.has_value()) {
        return;
      }
      const asc::index_t* value =
          object == 1 ? &f.plan.regions[support::kScratch].minimum_entries
                      : &*f.report.diagnostic_index;
      const auto pivot =
          Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
              value, 1, 1, {value, sizeof(*value), kHost}));
      f.factor = Take(Fixture<T>::Factor::Create(
          f.factor.factors(), target == 1 ? pivot : f.factor.row_pivots(),
          target == 2 ? pivot : f.factor.column_pivots(), f.provenance));
    } else {
      const auto pivot = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          static_cast<asc::index_t*>(objects[object]), 1, 1,
          {objects[object], sizeof(asc::index_t), kHost}));
      (target == 1 ? f.rows : f.columns) = pivot;
    }
  } else if (target == 3) {
    f.rhs = Take(asc::DenseBlasVectorView<T>::Create(
        static_cast<T*>(alias.data()), 1, 1, alias));
  } else {
    using Real = typename Fixture<T>::Real;
    f.scale = Take(asc::DenseBlasVectorView<Real>::Create(
        static_cast<Real*>(alias.data()), 1, 1, alias));
  }
}

template <typename T>
void Alias(TestContext& test, bool solve, bool empty,
           asc::DenseBlasLayout layout, std::size_t object,
           std::size_t target) {
  Fixture<T> f(test, solve, layout, empty ? 0 : 1);
  if (target < 5) {
    AliasOperand(f, object, target);
  } else {
    f.workspace.regions[target - 5] = {Objects(f)[object], sizeof(T), kHost};
  }
  fault::Reset(fault::Mode::kPass);
  const auto status = Preserving(
      test, [&] { return f.Run(); }, f.provider, f.plan, f.workspace, f.report,
      f.problem.dense, f.problem.rhs, f.problem.rows, f.problem.columns,
      f.problem.scale, f.scratch);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
}

template <typename T>
void MetadataAliases(TestContext& test, asc::DenseBlasLayout layout) {
  std::size_t cases = 0;
  for (bool solve : {false, true}) {
    for (bool empty : {false, true}) {
      for (std::size_t object = 0; object < 4; ++object) {
        for (std::size_t target = 0;
             target < 5 + asc::LapackWorkspace{}.regions.size(); ++target) {
          if ((!solve && (target == 3 || target == 4)) ||
              (empty && target < 4) || (empty && !solve && target == 4) ||
              (solve && (target == 1 || target == 2) && object != 1 &&
               object != 3)) {
            continue;
          }
          Alias<T>(test, solve, empty, layout, object, target);
          ++cases;
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, std::size_t{160});
}

template <typename T>
void LocalValues(TestContext& test, asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  for (bool solve : {false, true}) {
    for (Real bad : {std::numeric_limits<Real>::quiet_NaN(),
                     std::numeric_limits<Real>::infinity(),
                     -std::numeric_limits<Real>::infinity()}) {
      for (bool imaginary : {false, true}) {
        if (imaginary && !asc::DenseBlasComplex<T>) {
          continue;
        }
        const auto value =
            imaginary ? support::Value<T>(1, bad) : support::Value<T>(bad, 1);
        Fixture<T> matrix(test, solve, layout);
        matrix.problem.At(0, 1) = value;
        Reject(test, matrix, asc::ErrorCode::kNumerical);
        if (solve) {
          Fixture<T> rhs(test, true, layout);
          rhs.problem.rhs[2] = value;
          Reject(test, rhs, asc::ErrorCode::kNumerical);
        }
      }
    }
  }
  for (bool column : {false, true}) {
    for (std::size_t i = 1; i <= 3; ++i) {
      for (asc::index_t bad :
           {asc::index_t{-1}, static_cast<asc::index_t>(i - 1), asc::index_t{4},
            std::numeric_limits<asc::index_t>::max()}) {
        Fixture<T> f(test, true, layout);
        (column ? f.problem.columns : f.problem.rows)[i] = bad;
        Reject(test, f, asc::ErrorCode::kIndex);
      }
    }
  }
  for (asc::extent_t i = 0; i < 3; ++i) {
    Fixture<T> f(test, true, layout);
    f.problem.At(i, i) = T{};
    Reject(test, f, asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, f.report.outcome, asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, f.report.diagnostic_index, i);
  }
}

template <typename T>
void Plans(TestContext& test, asc::DenseBlasLayout layout) {
  for (bool solve : {false, true}) {
    for (std::size_t role = 0; role < asc::LapackWorkspace{}.regions.size();
         ++role) {
      for (int field = 0; field < 4; ++field) {
        Fixture<T> f(test, solve, layout);
        auto& region = f.plan.regions[role];
        if (field == 0) {
          ++region.minimum_entries;
        }
        if (field == 1) {
          ++region.preferred_entries;
        }
        if (field == 2) {
          ++region.entry_bytes;
        }
        if (field == 3) {
          ++region.alignment;
        }
        Reject(test, f, asc::ErrorCode::kInvalidState);
      }
    }
    Fixture<T> bytes(test, solve, layout);
    --bytes.plan.total_byte_limit;
    Reject(test, bytes, asc::ErrorCode::kInvalidState);
    Fixture<T> wrong_routine(test, solve, layout);
    wrong_routine.plan =
        solve ? Take(wrong_routine.problem.Query(wrong_routine.provider))
              : Take(asc::QueryGesc2Workspace(
                    wrong_routine.provider, wrong_routine.factor,
                    wrong_routine.rhs, wrong_routine.scale));
    Reject(test, wrong_routine, asc::ErrorCode::kInvalidState);
  }
  Fixture<T> normal(test, true, layout);
  Fixture<T> perturbed(test, true, layout, 3, true);
  normal.plan = perturbed.plan;
  Reject(test, normal, asc::ErrorCode::kInvalidState);
}

template <typename T>
void Workspaces(TestContext& test, asc::DenseBlasLayout layout) {
  for (bool solve : {false, true}) {
    for (auto role : {support::kScratch, support::kInteger}) {
      for (int kind = 0; kind < 6; ++kind) {
        Fixture<T> f(test, solve, layout);
        auto& region = f.workspace.regions[role];
        auto expected = asc::ErrorCode::kInvalidArgument;
        if (kind == 0) {
          region = {nullptr, 0, kHost};
        }
        if (kind == 1) {
          region = {region.data(), region.size() - 1, kHost};
        }
        if (kind == 2) {
          region = {static_cast<std::byte*>(region.data()) + 1, region.size(),
                    kHost};
        }
        if (kind == 3) {
          region = {region.data(), region.size(), asc::MemorySpace::kDevice};
          expected = asc::ErrorCode::kMemoryAccess;
        }
        if (kind == 4) {
          region = {f.problem.dense.data() + 1, region.size(), kHost};
        }
        if (kind == 5) {
          f.workspace.regions[static_cast<std::size_t>(
              asc::LapackWorkspaceKind::kReal)] = region;
        }
        Reject(test, f, expected);
      }
    }
  }
}

template <typename T>
void Factory(TestContext& test, asc::DenseBlasLayout layout) {
  for (bool empty : {false, true}) {
    Fixture<T> f(test, true, layout, empty ? 0 : 3);
    for (int kind = 0; kind < 11; ++kind) {
      auto report = f.provenance;
      if (kind == 0) {
        report.routine[0] = 'x';
      }
      if (kind == 1) {
        report.routine.back() = 'x';
      }
      if (kind == 2) {
        report.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
      }
      if (kind == 3) {
        report.output_validity = asc::LapackOutputValidity::kUnusable;
      }
      if (kind == 4) {
        report.called_provider = empty;
      }
      if (kind == 5) {
        report.native_info = -1;
      }
      if (kind == 6) {
        report.native_info = 4;
      }
      if (kind == 7) {
        report.outcome = asc::LapackOutcome::kAccuracyWarning;
      }
      if (kind == 8) {
        report.native_argument = 1;
      }
      if (kind == 9) {
        report.diagnostic_index = 1;
      }
      if (kind == 10) {
        report.provider.kind = asc::LapackProviderKind::kNative;
      }
      fault::Reset(fault::Mode::kPass);
      const auto result = Preserving(
          test, [&] { return f.problem.View(report); }, f.problem.dense,
          f.problem.rows, f.problem.columns, report);
      ASC_DENSE_TEST_EQ(test, result.status().code(),
                        asc::ErrorCode::kInvalidState);
      ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
    }
  }
  Fixture<T> f(test, true, layout, 3, true);
  ASC_DENSE_TEST_CHECK(test, f.factor.perturbed());
  auto report = f.provenance;
  report.diagnostic_index = 0;
  ASC_DENSE_TEST_EQ(test, f.problem.View(report).status().code(),
                    asc::ErrorCode::kInvalidState);
}

template <typename T>
void Descriptors(TestContext& test, asc::DenseBlasLayout layout) {
  for (bool column : {false, true}) {
    for (asc::extent_t size : {2, 4}) {
      Fixture<T> f(test, false, layout);
      (column ? f.columns : f.rows) =
          Vector(column ? f.problem.columns : f.problem.rows, size);
      Reject(test, f, asc::ErrorCode::kShape);
    }
    Fixture<T> f(test, false, layout);
    auto& values = column ? f.problem.columns : f.problem.rows;
    (column ? f.columns : f.rows) =
        Take(asc::DenseBlasVectorView<asc::index_t>::Create(
            values.data() + 1, 3, 2, {values.data(), sizeof(values), kHost}));
    Reject(test, f, asc::ErrorCode::kShape);
  }
  Fixture<T> rectangular(test, false, layout);
  rectangular.matrix = Take(asc::DenseBlasMatrixView<T>::Create(
      rectangular.problem.dense.data() + 1, 3, 2, layout,
      rectangular.problem.leading,
      {rectangular.problem.dense.data(), sizeof(rectangular.problem.dense),
       kHost}));
  Reject(test, rectangular, asc::ErrorCode::kShape);
  for (asc::extent_t size : {2, 4}) {
    Fixture<T> f(test, true, layout);
    f.rhs = Vector(f.problem.rhs, size);
    Reject(test, f, asc::ErrorCode::kShape);
  }
  Fixture<T> strided(test, true, layout);
  strided.rhs = Take(asc::DenseBlasVectorView<T>::Create(
      strided.problem.rhs.data() + 1, 3, 2,
      {strided.problem.rhs.data(), sizeof(strided.problem.rhs), kHost}));
  Reject(test, strided, asc::ErrorCode::kShape);
  for (asc::extent_t size : {0, 2}) {
    Fixture<T> f(test, true, layout);
    f.scale = Vector(f.problem.scale, size);
    Reject(test, f, asc::ErrorCode::kShape);
  }
}

template <typename T>
void Placement(TestContext& test, asc::DenseBlasLayout layout) {
  for (auto space : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
    Fixture<T> f(test, false, layout);
    f.matrix = Take(asc::DenseBlasMatrixView<T>::Create(
        f.problem.dense.data() + 1, 3, 3, layout, f.problem.leading,
        {f.problem.dense.data(), sizeof(f.problem.dense), space}));
    Reject(test, f, asc::ErrorCode::kMemoryAccess);
    const auto matrix = Take(asc::DenseBlasMatrixView<const T>::Create(
        f.problem.dense.data() + 1, 3, 3, layout, f.problem.leading,
        {f.problem.dense.data(), sizeof(f.problem.dense), space}));
    ASC_DENSE_TEST_EQ(
        test,
        Fixture<T>::Factor::Create(matrix, f.factor.row_pivots(),
                                   f.factor.column_pivots(), f.provenance)
            .status()
            .code(),
        asc::ErrorCode::kMemoryAccess);
    for (bool column : {false, true}) {
      Fixture<T> pivot(test, false, layout);
      auto& values = column ? pivot.problem.columns : pivot.problem.rows;
      (column ? pivot.columns : pivot.rows) =
          Take(asc::DenseBlasVectorView<asc::index_t>::Create(
              values.data() + 1, 3, 1, {values.data(), sizeof(values), space}));
      Reject(test, pivot, asc::ErrorCode::kMemoryAccess);
    }
    Fixture<T> rhs(test, true, layout);
    rhs.rhs = Take(asc::DenseBlasVectorView<T>::Create(
        rhs.problem.rhs.data() + 1, 3, 1,
        {rhs.problem.rhs.data(), sizeof(rhs.problem.rhs), space}));
    Reject(test, rhs, asc::ErrorCode::kMemoryAccess);
    Fixture<T> scale(test, true, layout);
    scale.scale =
        Take(asc::DenseBlasVectorView<typename Fixture<T>::Real>::Create(
            scale.problem.scale.data() + 1, 1, 1,
            {scale.problem.scale.data(), sizeof(scale.problem.scale), space}));
    Reject(test, scale, asc::ErrorCode::kMemoryAccess);
  }
}

template <typename T>
void Empty(TestContext& test, asc::DenseBlasLayout layout) {
  for (bool solve : {false, true}) {
    Fixture<T> f(test, solve, layout, 0);
    f.problem.rows.fill(std::numeric_limits<asc::index_t>::min());
    f.problem.columns.fill(std::numeric_limits<asc::index_t>::min());
    f.problem.dense.fill(support::Value<T>(
        std::numeric_limits<typename Fixture<T>::Real>::quiet_NaN()));
    f.problem.rhs.fill(support::Value<T>(
        std::numeric_limits<typename Fixture<T>::Real>::quiet_NaN()));
    fault::Reset(fault::Mode::kPass);
    const auto status = Preserving(
        test, [&] { return f.Run(); }, f.problem.dense, f.problem.rhs,
        f.problem.rows, f.problem.columns, f.scratch);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
    ASC_DENSE_TEST_CHECK(
        test, !f.report.called_provider && !f.report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, f.report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_EQ(test, f.problem.scale[1], solve ? 1 : -73);
    ASC_DENSE_TEST_EQ(test, f.problem.scale.front(), -71);
    ASC_DENSE_TEST_EQ(test, f.problem.scale.back(), -79);
  }
}

template <typename T>
void Run(TestContext& test) {
  for (auto layout : {support::kRow, support::kColumn}) {
    MetadataAliases<T>(test, layout);
    LocalValues<T>(test, layout);
    Plans<T>(test, layout);
    Workspaces<T>(test, layout);
    Factory<T>(test, layout);
    Descriptors<T>(test, layout);
    Placement<T>(test, layout);
    Empty<T>(test, layout);
  }
  std::puts(
      "COMPLETE_PIVOTING metadata aliases=320; local rejection and factory "
      "checks completed");
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
