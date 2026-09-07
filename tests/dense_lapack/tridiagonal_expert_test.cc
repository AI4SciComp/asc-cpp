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
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_tridiagonal.h"
#include "asc/dense/providers/lapack_tridiagonal_condition.h"
#include "asc/dense/providers/lapack_tridiagonal_driver.h"
#include "asc/dense/providers/lapack_tridiagonal_refinement.h"
#include "installed_lu/normal_return_guard.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_tridiagonal_test::EqualBytes;
using asc_tridiagonal_test::Exact;
using asc_tridiagonal_test::kCapacity;
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kConjugate;
using asc_tridiagonal_test::kNone;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::kTranspose;
using asc_tridiagonal_test::Narrow;
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

struct Inverse {
  std::array<Wide, kCapacity * kCapacity> data{};
  Wide& At(asc::extent_t i, asc::extent_t j) {
    return data[static_cast<std::size_t>(i) * kCapacity +
                static_cast<std::size_t>(j)];
  }
  [[nodiscard]] Wide At(asc::extent_t i, asc::extent_t j) const {
    return data[static_cast<std::size_t>(i) * kCapacity +
                static_cast<std::size_t>(j)];
  }
};

// Independent long-double Gauss-Jordan oracle: tests alone form a full matrix.
template <typename T>
Inverse Invert(const Tri<T>& original, asc::DenseBlasTranspose transpose) {
  Inverse work;
  Inverse result;
  const auto n = original.order;
  for (asc::extent_t i = 0; i < n; ++i) {
    result.At(i, i) = 1;
    for (asc::extent_t j = 0; j < n; ++j) {
      work.At(i, j) = original.Op(i, j, transpose);
    }
  }
  for (asc::extent_t k = 0; k < n; ++k) {
    asc::extent_t selected = k;
    for (asc::extent_t i = k + 1; i < n; ++i) {
      if (std::abs(work.At(i, k)) > std::abs(work.At(selected, k))) {
        selected = i;
      }
    }
    for (asc::extent_t j = 0; j < n; ++j) {
      std::swap(work.At(k, j), work.At(selected, j));
      std::swap(result.At(k, j), result.At(selected, j));
    }
    const Wide pivot = work.At(k, k);
    if (pivot == Wide{}) {
      std::abort();
    }
    for (asc::extent_t j = 0; j < n; ++j) {
      work.At(k, j) /= pivot;
      result.At(k, j) /= pivot;
    }
    for (asc::extent_t i = 0; i < n; ++i) {
      if (i == k) {
        continue;
      }
      const Wide multiplier = work.At(i, k);
      for (asc::extent_t j = 0; j < n; ++j) {
        work.At(i, j) -= multiplier * work.At(k, j);
        result.At(i, j) -= multiplier * result.At(k, j);
      }
    }
  }
  return result;
}

template <typename T>
long double Norm(const Tri<T>& matrix, asc::LapackConditionNorm norm) {
  long double result = 0;
  for (asc::extent_t i = 0; i < matrix.order; ++i) {
    long double sum = 0;
    for (asc::extent_t j = 0; j < matrix.order; ++j) {
      sum += std::abs(norm == asc::LapackConditionNorm::kOne ? matrix.At(j, i)
                                                             : matrix.At(i, j));
    }
    result = std::max(result, sum);
  }
  return result;
}

inline long double Norm(const Inverse& matrix, asc::extent_t n,
                        asc::LapackConditionNorm norm) {
  long double result = 0;
  for (asc::extent_t i = 0; i < n; ++i) {
    long double sum = 0;
    for (asc::extent_t j = 0; j < n; ++j) {
      sum += std::abs(norm == asc::LapackConditionNorm::kOne ? matrix.At(j, i)
                                                             : matrix.At(i, j));
    }
    result = std::max(result, sum);
  }
  return result;
}

template <typename T>
void Factor(TestContext& test, const asc::ReferenceLapackProvider& provider,
            Tri<T>& factors, std::array<asc::index_t, kCapacity + 2>& pivots) {
  const auto plan = Take(asc::QueryGttrfWorkspace(
      provider, factors.Factors(), Vector(pivots, factors.order)));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Gttrf(provider, factors.Factors(),
                      Vector(pivots, factors.order), plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
}

template <typename T>
void SingularConditions(TestContext& test,
                        const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const int n : {1, 2, 7}) {
    Tri<T> factors(n);
    factors.Initialize(2, 1);
    std::array<asc::index_t, kCapacity + 2> pivots{};
    for (int i = 1; i <= n; ++i) {
      pivots[static_cast<std::size_t>(i)] = i;
    }
    factors.diagonal[static_cast<std::size_t>(n)] = T{};
    Real rcond = -97;
    const auto plan = Take(asc::QueryGtconWorkspace(
        provider, asc::LapackConditionNorm::kOne,
        std::as_const(factors).Factors(), Pivots(pivots, n), Real{1}, rcond));
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Gtcon(provider, asc::LapackConditionNorm::kOne,
                        std::as_const(factors).Factors(), Pivots(pivots, n),
                        Real{1}, rcond, plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
    ASC_DENSE_TEST_EQ(test, rcond, Real{0});
  }
}

template <typename T>
void Condition(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const int exponent = sizeof(Real) == sizeof(float) ? 70 : 650;
  std::size_t count = 0;
  for (const int n : {0, 1, 2, 7}) {
    for (const int mode : {0, 1, 2}) {
      for (const int power : {-exponent, 0, exponent}) {
        Tri<T> original(n);
        original.Initialize(mode, std::ldexp(1.0L, power));
        auto factors = original;
        std::array<asc::index_t, kCapacity + 2> pivots{};
        Factor(test, provider, factors, pivots);
        const auto before = factors;
        const auto inverse = Invert(original, kNone);
        for (const auto norm : {asc::LapackConditionNorm::kOne,
                                asc::LapackConditionNorm::kInfinity}) {
          const auto anorm = static_cast<Real>(Norm(original, norm));
          Real rcond = -97;
          const auto plan = Take(WithoutAllocation(test, [&] {
            return asc::QueryGtconWorkspace(provider, norm,
                                            std::as_const(factors).Factors(),
                                            Pivots(pivots, n), anorm, rcond);
          }));
          Scratch<T> scratch;
          const auto workspace = scratch.Workspace(plan);
          asc::LapackReport report;
          const auto status = WithoutAllocation(test, [&] {
            return asc::Gtcon(provider, norm, std::as_const(factors).Factors(),
                              Pivots(pivots, n), anorm, rcond, plan, workspace,
                              report);
          });
          ASC_DENSE_TEST_CHECK(test, status.ok());
          ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
          const long double exact =
              n == 0 ? 1 : 1 / (Norm(original, norm) * Norm(inverse, n, norm));
          // Norm estimation is not an exact inverse. This independent fixed
          // fixture bound permits underestimation of inverse norm by <=5.
          ASC_DENSE_TEST_CHECK(
              test,
              rcond >=
                  exact * (1 - 128 * std::numeric_limits<Real>::epsilon()));
          ASC_DENSE_TEST_CHECK(test, rcond <= exact * 5);
          if (mode == 2 || n <= 1) {
            ASC_DENSE_TEST_NEAR(test, static_cast<long double>(rcond), exact,
                                0.0L,
                                128.0L * std::numeric_limits<Real>::epsilon());
          }
          scratch.Guards(test, workspace);
          ++count;
        }
        ASC_DENSE_TEST_CHECK(test,
                             EqualBytes(&before, &factors, sizeof(factors)));
      }
    }
  }
  SingularConditions<T>(test, provider);
  std::printf(
      "%zu GTCON independent inverse/analytic norm cases, 3 raw singular "
      "cases\n",
      count);
}

template <typename T>
void Errors(TestContext& test, const Tri<T>& original,
            asc::DenseBlasTranspose transpose, const Rhs<T>& rhs,
            const Rhs<T>& solution,
            const std::array<asc::DenseBlasRealType<T>, 8>& ferr,
            const std::array<asc::DenseBlasRealType<T>, 8>& berr) {
  using Real = asc::DenseBlasRealType<T>;
  const auto inverse = Invert(original, transpose);
  ASC_DENSE_TEST_CHECK(test, Residual(original, transpose, rhs, solution) <=
                                 128 * std::numeric_limits<Real>::epsilon());
  for (asc::extent_t j = 0; j < rhs.columns; ++j) {
    long double error = 0;
    long double scale = 0;
    for (asc::extent_t i = 0; i < original.order; ++i) {
      Wide exact{};
      for (asc::extent_t k = 0; k < original.order; ++k) {
        exact += inverse.At(i, k) * ToWide(rhs.At(k, j));
      }
      error = std::max(error, std::abs(exact - ToWide(solution.At(i, j))));
      scale = std::max(scale, std::abs(ToWide(solution.At(i, j))));
    }
    const auto index = static_cast<std::size_t>(j + 1);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(ferr[index]) && ferr[index] >= 0);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(berr[index]) && berr[index] >= 0);
    ASC_DENSE_TEST_CHECK(
        test, error <= scale * (8 * ferr[index] +
                                32 * std::numeric_limits<Real>::epsilon()));
  }
}

template <typename T>
struct RefineCase {
  using Real = asc::DenseBlasRealType<T>;
  const Tri<T>& original;
  Tri<T>& factors;
  std::array<asc::index_t, kCapacity + 2>& pivots;
  asc::DenseBlasTranspose transpose;
  bool driver;
  bool factored;
  Rhs<T> rhs;
  Rhs<T> solution;
  std::array<Real, 8> ferr{};
  std::array<Real, 8> berr{};
  Real rcond = -95;
  RefineCase(const Tri<T>& matrix, Tri<T>& factor_storage,
             std::array<asc::index_t, kCapacity + 2>& pivot_storage,
             asc::DenseBlasTranspose trans, bool expert, bool use_factors,
             int nrhs, asc::DenseBlasLayout b_layout,
             asc::DenseBlasLayout x_layout)
      : original(matrix),
        factors(factor_storage),
        pivots(pivot_storage),
        transpose(trans),
        driver(expert),
        factored(use_factors),
        rhs(matrix.order, nrhs, b_layout),
        solution(matrix.order, nrhs, x_layout) {
    RightHandSides(original, transpose, rhs);
    if (!driver) {
      for (int j = 0; j < nrhs; ++j) {
        for (asc::extent_t i = 0; i < matrix.order; ++i) {
          solution.At(i, j) = Narrow<T>(Exact<T>(i, j) * 1.03125L);
        }
      }
    }
    ferr.fill(Real{-97});
    berr.fill(Real{-99});
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    const auto n = original.order;
    const auto nrhs = rhs.columns;

    if (!driver) {
      return asc::QueryGtrfsWorkspace(
          provider, transpose, std::as_const(original).View(),
          std::as_const(factors).Factors(), Pivots(pivots, n),
          std::as_const(rhs).View(), solution.View(), Vector(ferr, nrhs),
          Vector(berr, nrhs));
    }
    if (factored) {
      return asc::QueryGtsvxFactoredWorkspace(
          provider, transpose, std::as_const(original).View(),
          std::as_const(factors).Factors(), Pivots(pivots, n),
          std::as_const(rhs).View(), solution.View(), rcond, Vector(ferr, nrhs),
          Vector(berr, nrhs));
    }
    return asc::QueryGtsvxWorkspace(
        provider, transpose, std::as_const(original).View(), factors.Factors(),
        Vector(pivots, n), std::as_const(rhs).View(), solution.View(), rcond,
        Vector(ferr, nrhs), Vector(berr, nrhs));
  }
  asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                      const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report) {
    const auto n = original.order;
    const auto nrhs = rhs.columns;

    if (!driver) {
      return asc::Gtrfs(provider, transpose, std::as_const(original).View(),
                        std::as_const(factors).Factors(), Pivots(pivots, n),
                        std::as_const(rhs).View(), solution.View(),
                        Vector(ferr, nrhs), Vector(berr, nrhs), plan, workspace,
                        report);
    }
    if (factored) {
      return asc::GtsvxFactored(
          provider, transpose, std::as_const(original).View(),
          std::as_const(factors).Factors(), Pivots(pivots, n),
          std::as_const(rhs).View(), solution.View(), rcond, Vector(ferr, nrhs),
          Vector(berr, nrhs), plan, workspace, report);
    }
    return asc::Gtsvx(
        provider, transpose, std::as_const(original).View(), factors.Factors(),
        Vector(pivots, n), std::as_const(rhs).View(), solution.View(), rcond,
        Vector(ferr, nrhs), Vector(berr, nrhs), plan, workspace, report);
  }
  void Condition(TestContext& test) const {
    const auto n = original.order;
    const auto norm = transpose == kNone ? asc::LapackConditionNorm::kOne
                                         : asc::LapackConditionNorm::kInfinity;
    const auto inverse = Invert(original, kNone);
    const long double exact =
        n == 0 ? 1 : 1 / (Norm(original, norm) * Norm(inverse, n, norm));
    ASC_DENSE_TEST_CHECK(
        test,
        rcond >= exact * (1 - 128 * std::numeric_limits<Real>::epsilon()) &&
            rcond <= exact * 5);
  }
  void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
    const auto n = original.order;
    const auto nrhs = rhs.columns;
    const auto original_before = original;
    const auto rhs_before = rhs;
    const auto factor_before = factors;
    const auto pivots_before = pivots;
    const long double initial = Residual(original, transpose, rhs, solution);
    const auto plan =
        Take(WithoutAllocation(test, [&] { return Query(provider); }));
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    const auto status = WithoutAllocation(
        test, [&] { return Execute(provider, plan, workspace, report); });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.called_provider,
                      n != 0 && (driver || nrhs != 0));
    ASC_DENSE_TEST_EQ(test, report.factor_family.has_value(), false);
    Errors(test, original, transpose, rhs, solution, ferr, berr);
    if (!driver && n != 0 && nrhs != 0) {
      ASC_DENSE_TEST_CHECK(
          test, Residual(original, transpose, rhs, solution) < initial);
    }
    if (driver) {
      Condition(test);
    }
    ASC_DENSE_TEST_CHECK(
        test, EqualBytes(&original, &original_before, sizeof(original)));
    ASC_DENSE_TEST_EQ(test, rhs.data, rhs_before.data);
    if (!driver || factored) {
      ASC_DENSE_TEST_CHECK(
          test, EqualBytes(&factors, &factor_before, sizeof(factors)));
      ASC_DENSE_TEST_EQ(test, pivots, pivots_before);
    }
    solution.Guards(test);
    scratch.Guards(test, workspace);
    ASC_DENSE_TEST_EQ(test, ferr.front(), Real{-97});
    ASC_DENSE_TEST_EQ(test, berr.front(), Real{-99});
    ASC_DENSE_TEST_EQ(test, ferr[static_cast<std::size_t>(nrhs + 1)],
                      Real{-97});
    ASC_DENSE_TEST_EQ(test, berr[static_cast<std::size_t>(nrhs + 1)],
                      Real{-99});
  }
};

template <typename T>
void Refine(TestContext& test, const asc::ReferenceLapackProvider& provider,
            bool driver) {
  using Real = asc::DenseBlasRealType<T>;
  const int exponent = sizeof(Real) == sizeof(float) ? 60 : 600;
  std::size_t count = 0;
  for (const int n : {0, 1, 2, 7}) {
    for (const int mode : {0, 1, 2}) {
      for (const int power : {-exponent, 0, exponent}) {
        Tri<T> original(n);
        original.Initialize(mode, std::ldexp(1.0L, power));
        auto factors = original;
        std::array<asc::index_t, kCapacity + 2> pivots{};
        Factor(test, provider, factors, pivots);
        for (const auto transpose : {kNone, kTranspose, kConjugate}) {
          for (const auto b_layout : {kColumn, kRow}) {
            for (const auto x_layout : {kColumn, kRow}) {
              for (const int nrhs : {0, 1, 3}) {
                const auto run = [&](bool factored) {
                  RefineCase<T> data(original, factors, pivots, transpose,
                                     driver, factored, nrhs, b_layout,
                                     x_layout);
                  data.Run(test, provider);
                  ++count;
                };
                run(true);
                if (driver) {
                  run(false);
                }
              }
            }
          }
        }
      }
    }
  }
  std::printf("%zu %s independent residual/refinement/error/condition cases\n",
              count, driver ? "GTSVX N/F" : "GTRFS");
}

template <typename T>
void ColdExpert(TestContext& test, const asc::ReferenceLapackProvider& provider,
                bool factored) {
  Tri<T> original(7);
  original.Initialize(1, 1);
  auto factors = original;
  std::array<asc::index_t, kCapacity + 2> pivots{};
  if (factored) {
    Factor(test, provider, factors, pivots);
  }
  RefineCase<T> data(original, factors, pivots, kConjugate, true, factored, 3,
                     kRow, kColumn);
  data.Run(test, provider);
  std::printf(
      "First GTSVX FACT=%c query/execution observed with independent checks\n",
      factored ? 'F' : 'N');
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
    if (operation == "expert-cold-n" || operation == "expert-cold-f") {
      ColdExpert<T>(test, provider, operation == "expert-cold-f");
    } else if (operation == "condition") {
      Condition<T>(test, provider);
    } else if (operation == "refine" || operation == "expert") {
      Refine<T>(test, provider, operation == "expert");
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
