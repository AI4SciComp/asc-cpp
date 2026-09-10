#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "installed_lu/normal_return_guard.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kLayout;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::Narrow;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::Scratch;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Value;
using asc_tridiagonal_test::Vector;
using asc_tridiagonal_test::Wide;
using asc_tridiagonal_test::WithoutAllocation;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;

template <typename T>
using FactorView = asc::ReferencePositiveDefiniteTridiagonalFactorView<T>;

template <typename T>
struct Matrix {
  using Real = asc::DenseBlasRealType<T>;
  std::array<Real, 66> d{};
  std::array<T, 66> e{};
  asc::extent_t n;
  explicit Matrix(asc::extent_t order) : n(order) {
    d.fill(Real{-83});
    e.fill(Value<T>(-85, 7));
  }
  auto View() {
    return Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
        Vector(d, n), Vector(e, n == 0 ? 0 : n - 1)));
  }
  [[nodiscard]] auto View() const {
    return Take(asc::LapackPositiveDefiniteTridiagonalView<const T>::Create(
        Vector(d, n), Vector(e, n == 0 ? 0 : n - 1)));
  }
  static Wide Multiplier(asc::extent_t i) {
    return ToWide(i % 2 == 0 ? Value<T>(0.25L, 0.125L)
                             : Value<T>(-0.5L, 0.25L));
  }
  static long double Diagonal(asc::extent_t i) {
    constexpr std::array<long double, 3> kValues{2, 3, 5};
    return kValues[static_cast<std::size_t>(i % 3)];
  }
  void Initialize(long double scale) {
    // Original independent L*D*L^H construction; no tested routine is called.
    for (asc::extent_t i = 0; i < n; ++i) {
      const auto at = static_cast<std::size_t>(i + 1);
      d[at] = static_cast<Real>(
          scale *
          (Diagonal(i) +
           (i == 0 ? 0 : Diagonal(i - 1) * std::norm(Multiplier(i - 1)))));
      if (i + 1 < n) {
        e[at] = Narrow<T>(scale * Diagonal(i) * Multiplier(i));
      }
    }
  }
  [[nodiscard]] Wide At(asc::extent_t i, asc::extent_t j) const {
    if (i == j) {
      return {d[static_cast<std::size_t>(i + 1)], 0};
    }
    if (i == j + 1) {
      return ToWide(e[static_cast<std::size_t>(i)]);
    }
    if (j == i + 1) {
      return std::conj(ToWide(e[static_cast<std::size_t>(i + 1)]));
    }
    return {};
  }
  void Guards(TestContext& test) const {
    ASC_DENSE_TEST_EQ(test, d.front(), Real{-83});
    ASC_DENSE_TEST_EQ(test, e.front(), Value<T>(-85, 7));
    for (std::size_t i = static_cast<std::size_t>(n + 1); i < d.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, d[i], Real{-83});
    }
    const auto first_unused = static_cast<std::size_t>(n == 0 ? 1 : n);
    for (std::size_t i = first_unused; i < e.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, e[i], Value<T>(-85, 7));
    }
  }
};

template <typename T>
Wide Exact(asc::extent_t i, asc::extent_t j, int pass) {
  return ToWide(Value<T>(1 + i / 8.0L - j / 16.0L + pass,
                         0.25L + i / 16.0L + j / 8.0L - pass / 4.0L));
}

template <typename T>
void FillRhs(const Matrix<T>& matrix, Rhs<T>& rhs, int pass) {
  for (asc::extent_t j = 0; j < rhs.columns; ++j) {
    for (asc::extent_t i = 0; i < matrix.n; ++i) {
      Wide sum{};
      for (asc::extent_t k = 0; k < matrix.n; ++k) {
        sum += matrix.At(i, k) * Exact<T>(k, j, pass);
      }
      rhs.At(i, j) = Narrow<T>(sum);
    }
  }
}

template <typename T>
void CheckSolution(TestContext& test, const Matrix<T>& matrix,
                   const Rhs<T>& input, const Rhs<T>& output, int pass) {
  constexpr long double kTolerance =
      128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (asc::extent_t j = 0; j < input.columns; ++j) {
    for (asc::extent_t i = 0; i < matrix.n; ++i) {
      const auto expected = Exact<T>(i, j, pass);
      ASC_DENSE_TEST_CHECK(test, std::abs(ToWide(output.At(i, j)) - expected) <=
                                     kTolerance * std::abs(expected));
      Wide residual = -ToWide(input.At(i, j));
      long double scale = std::abs(ToWide(input.At(i, j)));
      for (asc::extent_t k = 0; k < matrix.n; ++k) {
        const auto term = matrix.At(i, k) * ToWide(output.At(k, j));
        residual += term;
        scale += std::abs(term);
      }
      ASC_DENSE_TEST_CHECK(test, std::abs(residual) <= kTolerance * scale);
    }
  }
  output.Guards(test);
}

template <typename T>
void CheckFactor(TestContext& test, const Matrix<T>& original,
                 const Matrix<T>& factor, long double scale) {
  constexpr long double kTolerance =
      128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (asc::extent_t i = 0; i < original.n; ++i) {
    const auto at = static_cast<std::size_t>(i + 1);
    const long double expected = scale * Matrix<T>::Diagonal(i);
    ASC_DENSE_TEST_CHECK(
        test, std::abs(factor.d[at] - expected) <= kTolerance * expected);
    const long double diagonal =
        factor.d[at] +
        (i == 0 ? 0 : factor.d[at - 1] * std::norm(ToWide(factor.e[at - 1])));
    ASC_DENSE_TEST_CHECK(test, std::abs(diagonal - original.d[at]) <=
                                   kTolerance * original.d[at]);
    if (i + 1 < original.n) {
      ASC_DENSE_TEST_CHECK(
          test, std::abs(ToWide(factor.e[at]) - Matrix<T>::Multiplier(i)) <=
                    kTolerance);
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(static_cast<long double>(factor.d[at]) *
                                        ToWide(factor.e[at]) -
                                    ToWide(original.e[at])) <=
                               kTolerance * std::abs(ToWide(original.e[at])));
    }
  }
  factor.Guards(test);
}

template <typename T>
void Reuse(TestContext& test, const asc::ReferenceLapackProvider& provider,
           const Matrix<T>& original, const Matrix<T>& factors,
           const asc::LapackReport& factor_report) {
  const auto lower =
      Take(FactorView<T>::Create(provider, factors.View(), factor_report));
  for (const auto triangle : {kLower, kUpper}) {
    Matrix<T> raw = factors;
    if (triangle == kUpper) {
      for (asc::extent_t i = 1; i < raw.n; ++i) {
        const auto at = static_cast<std::size_t>(i);
        raw.e[at] = Narrow<T>(std::conj(ToWide(raw.e[at])));
      }
    }
    const auto factor =
        triangle == kLower
            ? lower
            : Take(FactorView<T>::FromRaw(
                  provider, triangle, Vector(std::as_const(raw.d), raw.n),
                  Vector(std::as_const(raw.e), raw.n == 0 ? 0 : raw.n - 1)));
    for (const auto layout : {kColumn, kRow}) {
      for (const int nrhs : {0, 1, 2, 4}) {
        Rhs<T> rhs(original.n, nrhs, layout);
        const auto plan = Take(WithoutAllocation(test, [&] {
          return asc::QueryPttrsWorkspace(provider, factor, rhs.View());
        }));
        Scratch<T> scratch;
        const auto workspace = scratch.Workspace(plan);
        ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries,
                          layout == kRow ? original.n * nrhs : 0);
        for (const auto& region : plan.regions) {
          if (&region != &plan.regions[kLayout]) {
            ASC_DENSE_TEST_EQ(test, region.minimum_entries, 0);
          }
        }
        for (int pass = 0; pass < 2; ++pass) {
          FillRhs(original, rhs, pass);
          const auto input = rhs;
          asc::LapackReport report;
          const auto status = WithoutAllocation(test, [&] {
            return asc::Pttrs(provider, factor, rhs.View(), plan, workspace,
                              report);
          });
          ASC_DENSE_TEST_CHECK(test, status.ok());
          const bool active = original.n != 0 && nrhs != 0;
          ASC_DENSE_TEST_EQ(test, report.called_provider, active);
          ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active);
          ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
          CheckSolution(test, original, input, rhs, pass);
          scratch.Guards(test, workspace);
        }
      }
    }
  }
}

template <typename T>
void Ordinary(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const int n : {0, 1, 2, 3, 4, 5, 9, 33}) {
    for (const long double scale : {0x1p-40L, 1.0L, 0x1p40L}) {
      Matrix<T> original(n);
      original.Initialize(scale);
      Matrix<T> factor = original;
      const auto plan = Take(WithoutAllocation(test, [&] {
        return asc::QueryPttrfWorkspace(provider, factor.View());
      }));
      for (const auto& region : plan.regions) {
        ASC_DENSE_TEST_EQ(test, region.minimum_entries, 0);
      }
      asc::LapackReport report;
      ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                   return asc::Pttrf(provider, factor.View(),
                                                     plan, {}, report);
                                 }).ok());
      CheckFactor(test, original, factor, scale);
      const auto before = factor;
      Reuse(test, provider, original, factor, report);
      ASC_DENSE_TEST_CHECK(test, factor.d == before.d && factor.e == before.e);
    }
  }
}

template <typename T>
void BadPivots(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  for (const int n : {1, 3, 9}) {
    for (const int position : {1, (n + 1) / 2, n}) {
      for (const int value : {0, -1}) {
        Matrix<T> matrix(n);
        for (int i = 1; i <= n; ++i) {
          matrix.d[static_cast<std::size_t>(i)] = i == position ? value : 2;
          if (i < n) {
            matrix.e[static_cast<std::size_t>(i)] = T{};
          }
        }
        const auto plan =
            Take(asc::QueryPttrfWorkspace(provider, matrix.View()));
        asc::LapackReport report;
        ASC_DENSE_TEST_EQ(
            test, asc::Pttrf(provider, matrix.View(), plan, {}, report).code(),
            asc::ErrorCode::kNumerical);
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), position);
        ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                          position - 1);
        ASC_DENSE_TEST_EQ(test, report.outcome,
                          asc::LapackOutcome::kNotPositiveDefinite);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kDocumentedPartial);
        ASC_DENSE_TEST_CHECK(
            test, !FactorView<T>::Create(provider, std::as_const(matrix).View(),
                                         report)
                       .ok());
        matrix.Guards(test);
      }
    }
  }
}

template <typename T>
void Preflight(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> matrix(3);
  matrix.Initialize(1);
  const auto raw = Take(FactorView<T>::FromRaw(
      provider, kLower, Vector(std::as_const(matrix.d), 3),
      Vector(std::as_const(matrix.e), 2)));
  Rhs<T> rhs(3, 2, kRow);
  const auto initial_rhs = rhs;
  const auto plan = Take(asc::QueryPttrsWorkspace(provider, raw, rhs.View()));
  Scratch<T> scratch;
  auto workspace = scratch.Workspace(plan);
  const auto initial_scratch = scratch;
  asc::LapackReport report;
  const auto bytes = workspace.regions[kLayout].size();
  workspace.regions[kLayout] = {workspace.regions[kLayout].data(), bytes - 1,
                                kHost};
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::Pttrs(provider, raw, rhs.View(), plan, workspace, report).ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_CHECK(test, rhs.data == initial_rhs.data);
  ASC_DENSE_TEST_CHECK(test, scratch.packed == initial_scratch.packed);
  workspace = scratch.Workspace(plan);
  auto wrong = plan;
  ++wrong.regions[kLayout].minimum_entries;
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::Pttrs(provider, raw, rhs.View(), wrong, workspace, report).ok());
  ASC_DENSE_TEST_CHECK(test,
                       rhs.data == initial_rhs.data && !report.called_provider);
  const auto aliased = Take(asc::DenseBlasMatrixView<T>::Create(
      matrix.e.data() + 1, 3, 1, kColumn, 3,
      {matrix.e.data(), sizeof(matrix.e), kHost}));
  ASC_DENSE_TEST_CHECK(test,
                       !asc::QueryPttrsWorkspace(provider, raw, aliased).ok());
  // Deliberately invalid fixed-underlying enum tests checked rejection.
  // NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
  ASC_DENSE_TEST_CHECK(
      test,
      !FactorView<T>::FromRaw(provider, static_cast<asc::DenseBlasTriangle>(99),
                              Vector(std::as_const(matrix.d), 3),
                              Vector(std::as_const(matrix.e), 2))
           .ok());
  // NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)
  matrix.d[1] = std::numeric_limits<Real>::quiet_NaN();
  const auto unread_plan = asc::QueryPttrsWorkspace(provider, raw, rhs.View());
  ASC_DENSE_TEST_CHECK(test, unread_plan.ok());
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::Pttrs(provider, raw, rhs.View(), plan, workspace, report).ok());
  ASC_DENSE_TEST_CHECK(test,
                       !report.called_provider && rhs.data == initial_rhs.data);
  Rhs<T> empty(3, 0, kColumn);
  const auto empty_plan =
      Take(asc::QueryPttrsWorkspace(provider, raw, empty.View()));
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Pttrs(provider, raw, empty.View(), empty_plan, {}, report).ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
}

template <typename T>
void Ownership(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  Matrix<T> matrix(3);
  matrix.Initialize(1);
  const auto original = matrix;
  const auto plan = Take(asc::QueryPttrfWorkspace(provider, matrix.View()));
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, asc::Pttrf(provider, matrix.View(), plan, {}, report).ok());
  const auto factor = Take(
      FactorView<T>::Create(provider, std::as_const(matrix).View(), report));
  asc::HostMemoryResource resource;
  for (const auto triangle : {kLower, kUpper}) {
    auto owner =
        Take(asc::ReferencePositiveDefiniteTridiagonalFactor<T>::CopyFrom(
            provider, factor, triangle, resource));
    auto moved = std::move(owner);
    // The documented moved-from query must return invalid-state.
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    ASC_DENSE_TEST_CHECK(test, !owner.view(provider).ok());
    const auto owned = Take(moved.view(provider));
    ASC_DENSE_TEST_CHECK(test,
                         owned.diagonal().data() != factor.diagonal().data());
    ASC_DENSE_TEST_CHECK(
        test, owned.off_diagonal().data() != factor.off_diagonal().data());
    for (int pass = 0; pass < 2; ++pass) {
      Rhs<T> rhs(3, 2, kRow);
      FillRhs(original, rhs, pass);
      const auto before = rhs;
      const auto solve =
          Take(asc::QueryPttrsWorkspace(provider, owned, rhs.View()));
      Scratch<T> scratch;
      ASC_DENSE_TEST_CHECK(test, asc::Pttrs(provider, owned, rhs.View(), solve,
                                            scratch.Workspace(solve), report)
                                     .ok());
      CheckSolution(test, original, before, rhs, pass);
    }
  }
}

// No allocation audit or synthetic native callback is active in this test.
// Each worker owns context, report, B and scratch, sharing only immutable
// factor bytes and the metadata-only plan. The linked provider is the real
// static ABI.
template <typename T>
void Concurrent(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  Matrix<T> matrix(9);
  matrix.Initialize(1);
  const auto original = matrix;
  const auto factor_plan =
      Take(asc::QueryPttrfWorkspace(provider, matrix.View()));
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, asc::Pttrf(provider, matrix.View(), factor_plan, {}, report).ok());
  const auto factor = Take(
      FactorView<T>::Create(provider, std::as_const(matrix).View(), report));
  for (const auto layout : {kRow, kColumn}) {
    Rhs<T> shape(9, 2, layout);
    const auto plan =
        Take(asc::QueryPttrsWorkspace(provider, factor, shape.View()));
    std::array<int, 4> results{};
    std::array<std::thread, 4> workers;
    for (std::size_t worker = 0; worker < workers.size(); ++worker) {
      workers[worker] = std::thread([&, worker] {
        const auto local = Take(asc::ReferenceLapackProvider::Create(
            asc::ExecutionContext::Serial()));
        TestContext checks;
        Rhs<T> rhs(9, 2, layout);
        Scratch<T> scratch;
        const auto workspace = scratch.Workspace(plan);
        for (int pass = 0; pass < 8; ++pass) {
          const int fixture = pass + static_cast<int>(worker);
          FillRhs(original, rhs, fixture);
          const auto input = rhs;
          asc::LapackReport local_report;
          // A rejected call in one worker must not contaminate any next call.
          if (worker == 0) {
            auto wrong = plan;
            ++wrong.regions[kLayout].minimum_entries;
            ASC_DENSE_TEST_CHECK(
                checks, !asc::Pttrs(local, factor, rhs.View(), wrong, workspace,
                                    local_report)
                             .ok());
            ASC_DENSE_TEST_CHECK(checks, !local_report.called_provider);
          }
          ASC_DENSE_TEST_CHECK(checks, asc::Pttrs(local, factor, rhs.View(),
                                                  plan, workspace, local_report)
                                           .ok());
          ASC_DENSE_TEST_CHECK(checks, local_report.called_provider &&
                                           local_report.native_info == 0);
          CheckSolution(checks, original, input, rhs, fixture);
        }
        results[worker] = checks.Finish();
      });
    }
    for (auto& worker : workers) {
      worker.join();
    }
    for (const int result : results) {
      ASC_DENSE_TEST_EQ(test, result, 0);
    }
  }
}

template <typename T>
void Extreme(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  // Analytic finite scalar A=B=a, exact X=1. No extended-range oracle needed.
  for (const Real a :
       {std::numeric_limits<Real>::denorm_min(),
        2 * std::numeric_limits<Real>::denorm_min(),
        std::numeric_limits<Real>::min(), std::numeric_limits<Real>::max()}) {
    Matrix<T> matrix(1);
    matrix.d[1] = a;
    const auto plan = Take(asc::QueryPttrfWorkspace(provider, matrix.View()));
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(
        test, asc::Pttrf(provider, matrix.View(), plan, {}, report).ok());
    const auto factor = Take(
        FactorView<T>::Create(provider, std::as_const(matrix).View(), report));
    for (const auto layout : {kColumn, kRow}) {
      Rhs<T> rhs(1, 2, layout);
      rhs.At(0, 0) = Value<T>(a);
      rhs.At(0, 1) = Value<T>(a);
      const auto solve_plan =
          Take(asc::QueryPttrsWorkspace(provider, factor, rhs.View()));
      Scratch<T> scratch;
      const auto status = asc::Pttrs(provider, factor, rhs.View(), solve_plan,
                                     scratch.Workspace(solve_plan), report);
      std::printf(
          "PT scalar=%zu a=%La layout=%d x=(%La,%La) INFO=%lld status=%d\n",
          sizeof(T), static_cast<long double>(a), static_cast<int>(layout),
          ToWide(rhs.At(0, 0)).real(), ToWide(rhs.At(0, 0)).imag(),
          static_cast<long long>(report.native_info.value_or(-99)),
          static_cast<int>(status.code()));
      ASC_DENSE_TEST_CHECK(test, status.ok());
      for (asc::extent_t j = 0; j < 2; ++j) {
        ASC_DENSE_TEST_CHECK(test,
                             std::abs(ToWide(rhs.At(0, j)) - Wide{1, 0}) <=
                                 16 * std::numeric_limits<Real>::epsilon());
      }
    }
  }
}

template <typename T>
int Scalar(const asc::ReferenceLapackProvider& provider, bool extreme) {
  TestContext test;
  if (extreme) {
    Extreme<T>(test, provider);
  } else {
    Ordinary<T>(test, provider);
    BadPivots<T>(test, provider);
    Preflight<T>(test, provider);
    Ownership<T>(test, provider);
    Concurrent<T>(test, provider);
  }
  return test.Finish();
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
  if (argc != 2 && argc != 3) {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const bool extreme = argc == 3 && std::string_view(argv[2]) == "extreme";
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Scalar<float>(provider, extreme);
  }
  if (scalar == "d") {
    return Scalar<double>(provider, extreme);
  }
  if (scalar == "c") {
    return Scalar<std::complex<float>>(provider, extreme);
  }
  if (scalar == "z") {
    return Scalar<std::complex<double>>(provider, extreme);
  }
  return 2;
}
