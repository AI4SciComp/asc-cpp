#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite.h"
#include "asc/dense/providers/lapack_indefinite_rook.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
static_assert(
    !std::is_convertible_v<asc::ReferenceRookFactorView<double>,
                           asc::ReferenceBunchKaufmanFactorView<double>>);
static_assert(
    !std::is_convertible_v<asc::ReferenceBunchKaufmanFactorView<double>,
                           asc::ReferenceRookFactorView<double>>);
using asc_indefinite_rook_test::Factor;
using asc_indefinite_rook_test::kColumn;
using asc_indefinite_rook_test::kHermitian;
using asc_indefinite_rook_test::kHost;
using asc_indefinite_rook_test::kLayout;
using asc_indefinite_rook_test::kLower;
using asc_indefinite_rook_test::kPivot;
using asc_indefinite_rook_test::kRow;
using asc_indefinite_rook_test::kScalar;
using asc_indefinite_rook_test::kSymmetric;
using asc_indefinite_rook_test::kUpper;
using asc_indefinite_rook_test::Matrix;
using asc_indefinite_rook_test::Pivots;
using asc_indefinite_rook_test::QueryFactor;
using asc_indefinite_rook_test::QuerySolve;
using asc_indefinite_rook_test::Raw;
using asc_indefinite_rook_test::Scratch;
using asc_indefinite_rook_test::Solve;
using asc_indefinite_rook_test::Take;
using asc_indefinite_rook_test::TestContext;
using asc_indefinite_rook_test::ToWide;
using asc_indefinite_rook_test::Value;
using asc_indefinite_rook_test::WithoutAllocation;

template <typename T>
struct Fixture {
  bool hermitian;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  std::array<T, 16> a{};
  std::array<asc::index_t, 5> pivots{};
  Scratch<T> scratch;
  asc::LapackReport report;

  Fixture(bool he, asc::DenseBlasTriangle tri, asc::DenseBlasLayout storage)
      : hermitian(he), triangle(tri), layout(storage) {
    a.fill(Value<T>(-37, 17));
    pivots.fill(-43);
    At(0, 0) = T{};
    At(1, 1) = T{};
    At(triangle == kUpper ? 0 : 1, triangle == kUpper ? 1 : 0) = Value<T>(1, 1);
  }
  T& At(int i, int j) {
    return a[1 + (layout == kColumn ? 3 * j + i : 3 * i + j)];
  }
  auto MatrixView() { return Matrix(a, 2, 2, layout, 3); }
  auto PivotView() { return Pivots(pivots, 2); }
  auto Query(const asc::ReferenceLapackProvider& provider, bool blocked) {
    return QueryFactor(provider, triangle, hermitian, blocked, MatrixView(),
                       PivotView());
  }
  auto FactorView(const asc::ReferenceLapackProvider& provider) const {
    return asc::ReferenceRookFactorView<T>::Create(
        provider, Matrix(a, 2, 2, layout, 3), triangle,
        hermitian ? kHermitian : kSymmetric, Raw(pivots, 2), report);
  }
};

template <typename T>
void Singular(TestContext& test, const asc::ReferenceLapackProvider& provider,
              bool hermitian) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool blocked : {false, true}) {
        for (const int n : {1, 3}) {
          for (const int zero : {0, n - 1}) {
            std::array<T, 24> a{};
            std::array<asc::index_t, 5> pivots{};
            a.fill(Value<T>(-47, 7));
            pivots.fill(-53);
            auto offset = [layout](int i, int j) {
              return 1 + (layout == kColumn ? j * 4 + i : i * 4 + j);
            };
            for (int j = 0; j < n; ++j) {
              for (int i = 0; i < n; ++i) {
                if ((triangle == kUpper && i <= j) ||
                    (triangle == kLower && i >= j)) {
                  a[offset(i, j)] =
                      Value<T>(i == j && i != zero ? 4 : 0,
                               hermitian && i == j
                                   ? std::numeric_limits<double>::quiet_NaN()
                                   : 0);
                }
              }
            }
            const auto matrix = Matrix(a, n, n, layout, 4);
            const auto pivot = Pivots(pivots, n);
            const auto plan = Take(QueryFactor(provider, triangle, hermitian,
                                               blocked, matrix, pivot));
            Scratch<T> scratch;
            const auto workspace = scratch.Workspace(plan);
            asc::LapackReport report;
            const auto status = WithoutAllocation(test, [&] {
              return Factor(provider, triangle, hermitian, blocked, matrix,
                            pivot, plan, workspace, report);
            });
            ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
            ASC_DENSE_TEST_CHECK(test, report.called_provider);
            ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999),
                              zero + 1);
            ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-999),
                              zero);
            ASC_DENSE_TEST_EQ(test, report.outcome,
                              asc::LapackOutcome::kSingular);
            ASC_DENSE_TEST_EQ(test, report.output_validity,
                              asc::LapackOutputValidity::kDocumentedPartial);
            for (int i = 0; i < n; ++i) {
              ASC_DENSE_TEST_EQ(test, pivots[i + 1], i + 1);
            }
            ASC_DENSE_TEST_CHECK(
                test, !asc::ReferenceRookFactorView<T>::Create(
                           provider, Matrix(std::as_const(a), n, n, layout, 4),
                           triangle, hermitian ? kHermitian : kSymmetric,
                           Raw(pivots, n), report)
                           .ok());
            scratch.Guards(test, workspace);
          }
        }
      }
    }
  }
}

template <typename T>
void WorkspaceCases(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    bool hermitian) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool blocked : {false, true}) {
        for (int bad = 0; bad < 12; ++bad) {
          Fixture<T> f(hermitian, triangle, layout);
          auto plan = Take(f.Query(provider, blocked));
          auto workspace = f.scratch.Workspace(plan);
          const auto old_a = f.a;
          const auto old_pivots = f.pivots;
          const auto old_scalar = f.scratch.scalar;
          const auto old_packed = f.scratch.packed;
          const auto old_pivot_bytes = f.scratch.pivot;
          if (bad == 0) {
            workspace.regions[kPivot] = {nullptr, 0, kHost};
          }
          if (bad == 1) {
            workspace.regions[kPivot] = {f.scratch.pivot.data() + 17,
                                         workspace.regions[kPivot].size(),
                                         kHost};
          }
          if (bad == 2) {
            workspace.regions[kPivot] = {
                f.pivots.data() + 1, workspace.regions[kPivot].size(), kHost};
          }
          if (bad == 3) {
            ++plan.regions[kPivot].minimum_entries;
          }
          if (bad == 4) {
            ++plan.regions[kPivot].preferred_entries;
          }
          if (bad == 5) {
            ++plan.regions[kPivot].entry_bytes;
          }
          if (bad == 6) {
            plan.total_byte_limit = 1;
          }
          if (bad == 7) {
            workspace.regions[kPivot] = {workspace.regions[kPivot].data(),
                                         workspace.regions[kPivot].size(),
                                         asc::MemorySpace::kPinnedHost};
          }
          if (bad == 8) {
            workspace.regions.back() = {f.scratch.pivot.data(), 1,
                                        asc::MemorySpace::kPinnedHost};
          }
          if (bad == 9) {
            workspace.regions.back() = workspace.regions[kPivot];
          }
          if (bad == 10) {
            workspace.regions[kPivot] = {
                &f.report, workspace.regions[kPivot].size(), kHost};
            f.report.native_info = 73;
          }
          auto selected = triangle;
          if (bad == 11) {
            selected = triangle == kUpper ? kLower : kUpper;
          }
          const auto status = WithoutAllocation(test, [&] {
            return Factor(provider, selected, hermitian, blocked,
                          f.MatrixView(), f.PivotView(), plan, workspace,
                          f.report);
          });
          ASC_DENSE_TEST_CHECK(test, !status.ok());
          ASC_DENSE_TEST_CHECK(test, !f.report.called_provider);
          ASC_DENSE_TEST_EQ(test, f.report.native_info.has_value(), bad == 10);
          if (bad == 10) {
            ASC_DENSE_TEST_EQ(test, f.report.native_info.value_or(-999), 73);
          }
          ASC_DENSE_TEST_EQ(test, f.a, old_a);
          ASC_DENSE_TEST_EQ(test, f.pivots, old_pivots);
          ASC_DENSE_TEST_EQ(test, f.scratch.scalar, old_scalar);
          ASC_DENSE_TEST_EQ(test, f.scratch.packed, old_packed);
          ASC_DENSE_TEST_EQ(test, f.scratch.pivot, old_pivot_bytes);
        }
      }
    }
  }
}

template <typename T>
void MetadataAliases(TestContext& test,
                     const asc::ReferenceLapackProvider& provider,
                     bool hermitian) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool blocked : {false, true}) {
        for (const bool alias_plan : {false, true}) {
          Fixture<T> f(hermitian, triangle, layout);
          auto plan = Take(f.Query(provider, blocked));
          auto workspace = f.scratch.Workspace(plan);
          const auto old_a = f.a;
          const auto old_pivots = f.pivots;
          const auto old_scalar = f.scratch.scalar;
          const auto old_packed = f.scratch.packed;
          const auto old_pivot_bytes = f.scratch.pivot;
          const auto bytes = workspace.regions[kPivot].size();
          ASC_DENSE_TEST_CHECK(test, bytes <= sizeof(plan));
          ASC_DENSE_TEST_CHECK(test, bytes <= sizeof(workspace));
          // Both aliases use live metadata storage and the exact planned
          // capacity. Neither aliases the report, and neither may be written.
          workspace.regions[kPivot] = {alias_plan
                                           ? static_cast<void*>(&plan)
                                           : static_cast<void*>(&workspace),
                                       bytes, kHost};
          f.report.native_info = 73;
          f.report.called_provider = true;
          f.report.routine.fill('r');
          std::array<std::byte, sizeof(f.report)> report_before{};
          std::array<std::byte, sizeof(plan)> plan_before{};
          std::array<std::byte, sizeof(workspace)> workspace_before{};
          std::memcpy(report_before.data(), &f.report, sizeof(f.report));
          std::memcpy(plan_before.data(), &plan, sizeof(plan));
          std::memcpy(workspace_before.data(), &workspace, sizeof(workspace));
          const auto status = WithoutAllocation(test, [&] {
            return Factor(provider, triangle, hermitian, blocked,
                          f.MatrixView(), f.PivotView(), plan, workspace,
                          f.report);
          });
          ASC_DENSE_TEST_EQ(test, status.code(),
                            asc::ErrorCode::kInvalidArgument);
          // Compare snapshots of the same objects, including padding: this
          // checks that rejection wrote no bytes, not semantic equality.
          std::array<std::byte, sizeof(f.report)> report_after{};
          std::array<std::byte, sizeof(plan)> plan_after{};
          std::array<std::byte, sizeof(workspace)> workspace_after{};
          std::memcpy(report_after.data(), &f.report, sizeof(f.report));
          std::memcpy(plan_after.data(), &plan, sizeof(plan));
          std::memcpy(workspace_after.data(), &workspace, sizeof(workspace));
          ASC_DENSE_TEST_EQ(test, report_before, report_after);
          ASC_DENSE_TEST_EQ(test, plan_before, plan_after);
          ASC_DENSE_TEST_EQ(test, workspace_before, workspace_after);
          ASC_DENSE_TEST_EQ(test, f.a, old_a);
          ASC_DENSE_TEST_EQ(test, f.pivots, old_pivots);
          ASC_DENSE_TEST_EQ(test, f.scratch.scalar, old_scalar);
          ASC_DENSE_TEST_EQ(test, f.scratch.packed, old_packed);
          ASC_DENSE_TEST_EQ(test, f.scratch.pivot, old_pivot_bytes);
        }
      }
    }
  }
}

template <typename T>
void InvalidReports(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    Fixture<T>& f) {
  const auto good_report = f.report;
  for (int invalid = 0; invalid < 8; ++invalid) {
    f.report = good_report;
    if (invalid == 0) {
      f.report.routine.fill('x');
    }
    if (invalid == 1) {
      f.report.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
    }
    if (invalid == 2) {
      f.report.native_info = 1;
    }
    if (invalid == 3) {
      f.report.native_info.reset();
    }
    if (invalid == 4) {
      f.report.called_provider = false;
    }
    if (invalid == 5) {
      f.report.provider.build_sha256[0] ^= std::byte{1};
    }
    if (invalid == 6) {
      // The classic routine name has the same first six characters.
      f.report.routine[6] = '\0';
    }
    if (invalid == 7) {
      f.report.factor_family = asc::LapackFactorFamily::kBunchKaufman;
    }
    ASC_DENSE_TEST_CHECK(test, !f.FactorView(provider).ok());
  }
  f.report = good_report;
}

template <typename T>
void PivotCases(TestContext& test, const asc::ReferenceLapackProvider& provider,
                bool hermitian) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      Fixture<T> f(hermitian, triangle, layout);
      const auto plan = Take(f.Query(provider, false));
      const auto workspace = f.scratch.Workspace(plan);
      ASC_DENSE_TEST_CHECK(
          test, Factor(provider, triangle, hermitian, false, f.MatrixView(),
                       f.PivotView(), plan, workspace, f.report)
                    .ok());
      const auto factor = Take(f.FactorView(provider));
      ASC_DENSE_TEST_CHECK(test, f.pivots[1] == -1 && f.pivots[2] == -2);
      const auto good_pivots = f.pivots;
      std::array<T, 8> rhs{};
      rhs.fill(Value<T>(2, 1));
      const auto b = Matrix(rhs, 2, 1, kColumn, 4);
      const auto solve_plan = Take(QuerySolve(provider, hermitian, factor, b));
      Scratch<T> solve_scratch;
      const auto solve_workspace = solve_scratch.Workspace(solve_plan);
      for (const auto invalid :
           {std::array<asc::index_t, 2>{0, 0},
            std::array<asc::index_t, 2>{-1, 1},
            std::array<asc::index_t, 2>{3, 1},
            std::array<asc::index_t, 2>{
                std::numeric_limits<asc::index_t>::min(), -1},
            std::array<asc::index_t, 2>{triangle == kUpper ? 2 : 1,
                                        triangle == kUpper ? 2 : 1}}) {
        f.pivots[1] = invalid[0];
        f.pivots[2] = invalid[1];
        ASC_DENSE_TEST_CHECK(test, !f.FactorView(provider).ok());
        asc::LapackReport report;
        const auto before = rhs;
        const auto status = WithoutAllocation(test, [&] {
          return Solve(provider, hermitian, factor, b, solve_plan,
                       solve_workspace, report);
        });
        ASC_DENSE_TEST_CHECK(test, !status.ok());
        ASC_DENSE_TEST_CHECK(
            test, !report.called_provider && !report.native_info.has_value());
        ASC_DENSE_TEST_EQ(test, rhs, before);
      }
      f.pivots = good_pivots;
      InvalidReports(test, provider, f);
      // Paired D entries are consumed as full coefficients. A deliberately
      // invalidated borrowed factor with an exact zero evaluated divisor is
      // rejected before RHS mutation, without inventing a foreign INFO.
      f.At(0, 0) = T{1};
      f.At(1, 1) = T{1};
      f.At(triangle == kUpper ? 0 : 1, triangle == kUpper ? 1 : 0) = T{1};
      asc::LapackReport report;
      const auto before = rhs;
      const auto status = WithoutAllocation(test, [&] {
        return Solve(provider, hermitian, factor, b, solve_plan,
                     solve_workspace, report);
      });
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
      ASC_DENSE_TEST_EQ(test, rhs, before);
    }
  }
}

template <typename T>
void WideStride(TestContext& test, const asc::ReferenceLapackProvider& provider,
                bool hermitian) {
  for (const auto layout : {kColumn, kRow}) {
    for (const bool blocked : {false, true}) {
      std::array<T, 3> a{T{}, T{4}, T{}};
      std::array<T, 3> rhs{T{}, T{8}, T{}};
      std::array<asc::index_t, 3> pivots{};
      constexpr auto kWide = std::numeric_limits<asc::extent_t>::max();
      const auto matrix = Matrix(a, 1, 1, layout, kWide);
      const auto pivot = Pivots(pivots, 1);
      const auto plan = Take(
          QueryFactor(provider, kUpper, hermitian, blocked, matrix, pivot));
      Scratch<T> scratch;
      auto workspace = scratch.Workspace(plan);
      workspace.regions.back() = {nullptr, 0, asc::MemorySpace::kPinnedHost};
      asc::LapackReport report;
      ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                   return Factor(provider, kUpper, hermitian,
                                                 blocked, matrix, pivot, plan,
                                                 workspace, report);
                                 }).ok());
      const auto factor = Take(asc::ReferenceRookFactorView<T>::Create(
          provider, Matrix(std::as_const(a), 1, 1, layout, kWide), kUpper,
          hermitian ? kHermitian : kSymmetric, Raw(pivots, 1), report));
      const auto classic_tag = Take(asc::RawLapackPivotView::Create(
          pivots.data() + 1, 1, asc::LapackFactorFamily::kBunchKaufman,
          {pivots.data(), sizeof(pivots), kHost}));
      // The scalar pivot value is legal for both algorithms; rejection here
      // establishes nominal provenance, independently of pivot encoding.
      ASC_DENSE_TEST_EQ(
          test,
          asc::ReferenceRookFactorView<T>::Create(
              provider, Matrix(std::as_const(a), 1, 1, layout, kWide), kUpper,
              hermitian ? kHermitian : kSymmetric, classic_tag, report)
              .status()
              .code(),
          asc::ErrorCode::kInvalidArgument);
      const auto b = Matrix(rhs, 1, 1, layout, kWide);
      const auto solve_plan = Take(QuerySolve(provider, hermitian, factor, b));
      const auto solve_workspace = scratch.Workspace(solve_plan);
      ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                   return Solve(provider, hermitian, factor, b,
                                                solve_plan, solve_workspace,
                                                report);
                                 }).ok());
      ASC_DENSE_TEST_EQ(test, rhs[1], T{2});
    }
  }
}

template <typename T>
void Placement(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool hermitian) {
  Fixture<T> f(hermitian, kLower, kRow);
  const auto plan = Take(f.Query(provider, true));
  const auto workspace = f.scratch.Workspace(plan);
  const auto before = f.a;
  const auto before_pivots = f.pivots;
  const auto pinned_a = Take(asc::DenseBlasMatrixView<T>::Create(
      f.a.data() + 1, 2, 2, kRow, 3,
      {f.a.data(), sizeof(f.a), asc::MemorySpace::kPinnedHost}));
  const auto pinned_pivots =
      Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          f.pivots.data() + 1, 2, 1,
          {f.pivots.data(), sizeof(f.pivots), asc::MemorySpace::kPinnedHost}));
  for (const bool matrix_pinned : {true, false}) {
    const auto status = WithoutAllocation(test, [&] {
      return Factor(provider, kLower, hermitian, true,
                    matrix_pinned ? pinned_a : f.MatrixView(),
                    matrix_pinned ? f.PivotView() : pinned_pivots, plan,
                    workspace, f.report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
    ASC_DENSE_TEST_CHECK(
        test, !f.report.called_provider && !f.report.native_info.has_value());
  }
  for (const auto role : {kScalar, kLayout}) {
    auto short_workspace = workspace;
    short_workspace.regions[role] = {nullptr, 0, kHost};
    const auto status = WithoutAllocation(test, [&] {
      return Factor(provider, kLower, hermitian, true, f.MatrixView(),
                    f.PivotView(), plan, short_workspace, f.report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_CHECK(test, !f.report.called_provider);
  }
  ASC_DENSE_TEST_EQ(test, f.a, before);
  ASC_DENSE_TEST_EQ(test, f.pivots, before_pivots);
  ASC_DENSE_TEST_CHECK(test,
                       Factor(provider, kLower, hermitian, true, f.MatrixView(),
                              f.PivotView(), plan, workspace, f.report)
                           .ok());
  const auto factor = Take(f.FactorView(provider));
  std::array<T, 8> rhs{};
  rhs.fill(T{2});
  const auto b = Matrix(rhs, 2, 1, kRow, 3);
  const auto solve_plan = Take(QuerySolve(provider, hermitian, factor, b));
  Scratch<T> scratch;
  const auto solve_workspace = scratch.Workspace(solve_plan);
  const auto pinned_b = Take(asc::DenseBlasMatrixView<T>::Create(
      rhs.data() + 1, 2, 1, kRow, 3,
      {rhs.data(), sizeof(rhs), asc::MemorySpace::kPinnedHost}));
  asc::LapackReport report;
  ASC_DENSE_TEST_EQ(
      test,
      WithoutAllocation(test,
                        [&] {
                          return Solve(provider, hermitian, factor, pinned_b,
                                       solve_plan, solve_workspace, report);
                        })
          .code(),
      asc::ErrorCode::kMemoryAccess);
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  for (const auto value : rhs) {
    ASC_DENSE_TEST_EQ(test, value, T{2});
  }
}

template <typename T>
void SelectedNan(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, bool hermitian) {
  // Direct calls in both admitted GNU integer ABIs return INFO=1 here.
  // MAX(NaN,0) follows the compiled provider behavior, despite no DISNAN.
  using Real = asc::DenseBlasRealType<T>;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool blocked : {false, true}) {
        std::array<T, 3> a{
            T{7}, Value<T>(std::numeric_limits<Real>::quiet_NaN()), T{7}};
        std::array<asc::index_t, 3> pivots{-11, -11, -11};
        const auto matrix = Matrix(a, 1, 1, layout, 1);
        const auto pivot = Pivots(pivots, 1);
        const auto plan = Take(
            QueryFactor(provider, triangle, hermitian, blocked, matrix, pivot));
        Scratch<T> scratch;
        const auto workspace = scratch.Workspace(plan);
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return Factor(provider, triangle, hermitian, blocked, matrix, pivot,
                        plan, workspace, report);
        });
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 1);
        ASC_DENSE_TEST_EQ(test, report.outcome,
                          asc::LapackOutcome::kPartialResult);
        ASC_DENSE_TEST_EQ(test, pivots[1], 1);
        ASC_DENSE_TEST_CHECK(test, std::isnan(ToWide(a[1]).real()));
        ASC_DENSE_TEST_EQ(test, a.front(), T{7});
        ASC_DENSE_TEST_EQ(test, a.back(), T{7});
      }
    }
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         bool hermitian) {
  Singular<T>(test, provider, hermitian);
  WorkspaceCases<T>(test, provider, hermitian);
  MetadataAliases<T>(test, provider, hermitian);
  PivotCases<T>(test, provider, hermitian);
  WideStride<T>(test, provider, hermitian);
  Placement<T>(test, provider, hermitian);
  SelectedNan<T>(test, provider, hermitian);
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view mode = argv[1];
  if (mode == "s") {
    Run<float>(test, provider, false);
  } else if (mode == "d") {
    Run<double>(test, provider, false);
  } else if (mode == "c") {
    Run<std::complex<float>>(test, provider, false);
  } else if (mode == "z") {
    Run<std::complex<double>>(test, provider, false);
  } else if (mode == "ch") {
    Run<std::complex<float>>(test, provider, true);
  } else if (mode == "zh") {
    Run<std::complex<double>>(test, provider, true);
  } else {
    return 2;
  }
  return test.Finish();
}
