#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_tridiagonal.h"
#include "asc/dense/providers/lapack_tridiagonal_driver.h"
#include "installed_lu/normal_return_guard.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_tridiagonal_test::EqualBytes;
using asc_tridiagonal_test::kCapacity;
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kConjugate;
using asc_tridiagonal_test::kNone;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::kScratch;
using asc_tridiagonal_test::kTranspose;
using asc_tridiagonal_test::Pivots;
using asc_tridiagonal_test::Residual;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::RightHandSides;
using asc_tridiagonal_test::Scratch;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Tri;
using asc_tridiagonal_test::Vector;
using asc_tridiagonal_test::Wide;
using asc_tridiagonal_test::WithoutAllocation;

template <typename T>
void Reconstruct(TestContext& test, const Tri<T>& original,
                 const Tri<T>& factor,
                 const std::array<asc::index_t, kCapacity + 2>& pivots) {
  const auto n = original.order;
  std::array<Wide, kCapacity * kCapacity> restored{};
  const auto at = [&](asc::extent_t i, asc::extent_t j) -> Wide& {
    return restored[static_cast<std::size_t>(i) * kCapacity +
                    static_cast<std::size_t>(j)];
  };
  for (asc::extent_t i = 0; i < n; ++i) {
    at(i, i) = ToWide(factor.diagonal[static_cast<std::size_t>(i + 1)]);
    if (i + 1 < n) {
      at(i, i + 1) = ToWide(factor.upper[static_cast<std::size_t>(i + 1)]);
    }
    if (i + 2 < n) {
      at(i, i + 2) = ToWide(factor.second[static_cast<std::size_t>(i + 1)]);
    }
  }
  // Independent reverse elementary-row reconstruction, not a GTTRS call.
  // GTTRF stores successive adjacent swaps and multipliers, not dense L.
  for (asc::extent_t i = n - 2; i >= 0; --i) {
    const Wide multiplier =
        ToWide(factor.lower[static_cast<std::size_t>(i + 1)]);
    for (asc::extent_t j = 0; j < n; ++j) {
      at(i + 1, j) += multiplier * at(i, j);
    }
    const auto pivot = pivots[static_cast<std::size_t>(i + 1)] - 1;
    ASC_DENSE_TEST_CHECK(test, pivot == i || pivot == i + 1);
    if (pivot != i) {
      for (asc::extent_t j = 0; j < n; ++j) {
        std::swap(at(i, j), at(i + 1, j));
      }
    }
  }
  long double norm = 0;
  long double error = 0;
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      norm = std::max(norm, std::abs(original.At(i, j)));
      error = std::max(error, std::abs(at(i, j) - original.At(i, j)));
    }
  }
  constexpr long double kTolerance =
      128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  ASC_DENSE_TEST_CHECK(test, error <= kTolerance * norm);
}

template <typename T>
std::size_t ReusedSolves(TestContext& test,
                         const asc::ReferenceLapackProvider& provider,
                         const Tri<T>& original,
                         asc::ReferenceTridiagonalLuFactorView<T> reused) {
  using Real = asc::DenseBlasRealType<T>;
  const auto n = original.order;
  std::size_t count = 0;
  asc::LapackReport report;
  for (const auto transpose : {kNone, kTranspose, kConjugate}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const int nrhs : {0, 1, 2, 4}) {
        Rhs<T> rhs(n, nrhs, layout);
        RightHandSides(original, transpose, rhs);
        const auto before = rhs;
        const auto plan = Take(WithoutAllocation(test, [&] {
          return asc::QueryGttrsWorkspace(provider, transpose, reused,
                                          rhs.View());
        }));
        Scratch<T> solve_scratch;
        const auto workspace = solve_scratch.Workspace(plan);
        const auto solve_status = WithoutAllocation(test, [&] {
          return asc::Gttrs(provider, transpose, reused, rhs.View(), plan,
                            workspace, report);
        });
        ASC_DENSE_TEST_CHECK(test, solve_status.ok());
        ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0 && nrhs != 0);
        ASC_DENSE_TEST_CHECK(test,
                             Residual(original, transpose, before, rhs) <=
                                 128 * std::numeric_limits<Real>::epsilon());
        rhs.Guards(test);
        solve_scratch.Guards(test, workspace);
        ++count;
      }
    }
  }
  return count;
}

template <typename T>
void SingularFactors(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  for (const int n : {1, 2, 7}) {
    for (const int zero : {1, n}) {
      Tri<T> original(n);
      original.Initialize(2, 1);
      original.diagonal[static_cast<std::size_t>(zero)] = T{};
      auto factor = original;
      std::array<asc::index_t, kCapacity + 2> pivots{};
      const auto plan = Take(asc::QueryGttrfWorkspace(
          provider, factor.Factors(), Vector(pivots, n)));
      Scratch<T> scratch;
      const auto workspace = scratch.Workspace(plan);
      asc::LapackReport report;
      const auto status = WithoutAllocation(test, [&] {
        return asc::Gttrf(provider, factor.Factors(), Vector(pivots, n), plan,
                          workspace, report);
      });
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), zero);
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), zero - 1);
      Reconstruct(test, original, factor, pivots);
      ASC_DENSE_TEST_CHECK(test,
                           !asc::ReferenceTridiagonalLuFactorView<T>::Create(
                                provider, std::as_const(factor).Factors(),
                                Pivots(pivots, n), report)
                                .ok());
    }
  }
}

template <typename T>
void FactorSolve(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  std::size_t factors_checked = 0;
  std::size_t solves_checked = 0;
  const int exponent = sizeof(Real) == sizeof(float) ? 70 : 650;
  for (const int n : {0, 1, 2, 3, 7, 41}) {
    for (const int mode : {0, 1, 2}) {
      for (const int power : {-exponent, 0, exponent}) {
        const long double scale = std::ldexp(1.0L, power);
        Tri<T> original(n);
        original.Initialize(mode, scale);
        Tri<T> factor = original;
        std::array<asc::index_t, kCapacity + 2> pivots{};
        pivots.fill(-91);
        auto plan = Take(WithoutAllocation(test, [&] {
          return asc::QueryGttrfWorkspace(provider, factor.Factors(),
                                          Vector(pivots, n));
        }));
        Scratch<T> scratch;
        auto workspace = scratch.Workspace(plan);
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return asc::Gttrf(provider, factor.Factors(), Vector(pivots, n), plan,
                            workspace, report);
        });
        ASC_DENSE_TEST_CHECK(test, status.ok());
        ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
        ASC_DENSE_TEST_EQ(test, report.factor_family.has_value(), false);
        ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), n != 0);
        ASC_DENSE_TEST_EQ(test, pivots.front(), -91);
        ASC_DENSE_TEST_EQ(test, pivots[static_cast<std::size_t>(n + 1)], -91);
        if (n != 0) {
          ASC_DENSE_TEST_EQ(test, pivots[static_cast<std::size_t>(n)], n);
        }
        Reconstruct(test, original, factor, pivots);
        factor.Guards(test);
        scratch.Guards(test, workspace);
        const auto immutable_factors = std::as_const(factor).Factors();
        const auto raw = Pivots(std::as_const(pivots), n);
        const auto reused = Take(WithoutAllocation(test, [&] {
          return asc::ReferenceTridiagonalLuFactorView<T>::Create(
              provider, immutable_factors, raw, report);
        }));
        const auto factor_before = factor;
        const auto pivots_before = pivots;
        solves_checked += ReusedSolves(test, provider, original, reused);
        ASC_DENSE_TEST_CHECK(
            test, EqualBytes(&factor_before, &factor, sizeof(factor)));
        ASC_DENSE_TEST_EQ(test, pivots_before, pivots);
        ++factors_checked;
      }
    }
  }
  SingularFactors<T>(test, provider);
  std::printf(
      "%zu independent GTTRF reconstructions, %zu N/T/C reused solves; "
      "six exact singular-pivot cases\n",
      factors_checked, solves_checked);
}

template <typename T>
void Driver(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const int exponent = sizeof(Real) == sizeof(float) ? 70 : 650;
  std::size_t count = 0;
  for (const int n : {0, 1, 2, 3, 7, 41}) {
    for (const int nrhs : {0, 1, 2, 4}) {
      for (const auto layout : {kColumn, kRow}) {
        for (const int mode : {0, 1, 2}) {
          for (const int power : {-exponent, 0, exponent}) {
            Tri<T> original(n);
            original.Initialize(mode, std::ldexp(1.0L, power));
            auto matrix = original;
            Rhs<T> rhs(n, nrhs, layout);
            RightHandSides(original, kNone, rhs);
            const auto before = rhs;
            const auto plan = Take(WithoutAllocation(test, [&] {
              return asc::QueryGtsvWorkspace(provider, matrix.View(),
                                             rhs.View());
            }));
            ASC_DENSE_TEST_EQ(
                test, plan.regions[kScratch].minimum_entries,
                asc::DenseBlasReal<T> && n != 0 && nrhs == 0 ? n : 0);
            Scratch<T> scratch;
            const auto workspace = scratch.Workspace(plan);
            asc::LapackReport report;
            const auto status = WithoutAllocation(test, [&] {
              return asc::Gtsv(provider, matrix.View(), rhs.View(), plan,
                               workspace, report);
            });
            ASC_DENSE_TEST_CHECK(test, status.ok());
            ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
            ASC_DENSE_TEST_CHECK(
                test, Residual(original, kNone, before, rhs) <=
                          128 * std::numeric_limits<Real>::epsilon());
            matrix.Guards(test);
            rhs.Guards(test);
            scratch.Guards(test, workspace);
            if (nrhs == 0) {
              ASC_DENSE_TEST_EQ(test, rhs.data, before.data);
            }
            ++count;
          }
        }
      }
    }
  }
  for (const int n : {1, 2, 7}) {
    for (const int zero : {1, n}) {
      for (const auto layout : {kColumn, kRow}) {
        for (const int nrhs : {0, 1, 4}) {
          Tri<T> matrix(n);
          matrix.Initialize(2, 1);
          matrix.diagonal[static_cast<std::size_t>(zero)] = T{};
          Rhs<T> rhs(n, nrhs, layout);
          const auto before = rhs;
          const auto plan = Take(
              asc::QueryGtsvWorkspace(provider, matrix.View(), rhs.View()));
          Scratch<T> scratch;
          const auto workspace = scratch.Workspace(plan);
          asc::LapackReport report;
          const auto status = WithoutAllocation(test, [&] {
            return asc::Gtsv(provider, matrix.View(), rhs.View(), plan,
                             workspace, report);
          });
          ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
          ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), zero);
          ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                            zero - 1);
          ASC_DENSE_TEST_EQ(test, rhs.data, before.data);
          scratch.Guards(test, workspace);
        }
      }
    }
  }
  std::printf("%zu GTSV scale/layout/RHS cases; 36 singular/zero-RHS cases\n",
              count);
}

}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar = argv[1];
  const std::string_view operation = argv[2];
  const auto run = [&]<typename T>() {
    if (operation == "factor") {
      FactorSolve<T>(test, provider);
    } else if (operation == "driver") {
      Driver<T>(test, provider);
    } else {
      std::abort();
    }
  };
  if (scalar == "s") {
    run.template operator()<float>();
  } else if (scalar == "d") {
    run.template operator()<double>();
  } else if (scalar == "c") {
    run.template operator()<std::complex<float>>();
  } else if (scalar == "z") {
    run.template operator()<std::complex<double>>();
  } else {
    return 2;
  }
  return test.Finish();
}
