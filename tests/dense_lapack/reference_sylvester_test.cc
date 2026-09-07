#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_sylvester.h"
#include "sylvester_test_support.h"

namespace {
using asc::extent_t;
using asc_sylvester_test::Layout;
using asc_sylvester_test::Matrix;
using asc_sylvester_test::Narrow;
using asc_sylvester_test::NotANumber;
using asc_sylvester_test::Operation;
using asc_sylvester_test::Scratch;
using asc_sylvester_test::Take;
using asc_sylvester_test::TestContext;
using asc_sylvester_test::Wide;
using asc_sylvester_test::Widen;
using asc_sylvester_test::WithoutAllocation;
using Sign = asc::LapackSylvesterSign;

template <typename T>
void FillSchur(Matrix<T>& a, long double diagonal, bool blocks) {
  for (extent_t i = 0; i < a.rows(); ++i) {
    for (extent_t j = 0; j < a.columns(); ++j) {
      if (i == j) {
        a(i, j) = Narrow<T>({diagonal + i, 0.25L * (i + 1)});
      } else if (i < j) {
        a(i, j) = Narrow<T>({0.125L * (i + j + 1), 0.25L * (j - i)});
      } else if (!asc::DenseBlasComplex<T> && i == j + 1) {
        a(i, j) = T{};
      } else {
        a(i, j) = NotANumber<T>();
      }
    }
  }
  if constexpr (!asc::DenseBlasComplex<T>) {
    if (blocks) {
      for (extent_t i = 0; i + 1 < a.rows(); i += 2) {
        a(i + 1, i + 1) = a(i, i);
        a(i, i + 1) = Narrow<T>({2});
        a(i + 1, i) = Narrow<T>({-1});
      }
    }
  }
}

template <typename T>
Wide Coefficient(const Matrix<T>& a, Operation operation, extent_t i,
                 extent_t j) {
  if (operation != Operation::kNone) {
    std::swap(i, j);
  }
  Wide value{};
  if (i <= j || (!asc::DenseBlasComplex<T> && i == j + 1)) {
    value = Widen(a(i, j));
  }
  return operation == Operation::kConjugateTranspose ? std::conj(value) : value;
}

template <typename T>
Wide ChosenSolution(extent_t i, extent_t j) {
  return Widen(
      Narrow<T>({0.25L * (i + 1) - 0.5L * (j + 1), 0.125L * (i + j + 1)}));
}

template <typename T>
void CheckResidual(TestContext& test, const Matrix<T>& a, const Matrix<T>& b,
                   const Matrix<T>& original_c, const Matrix<T>& x,
                   Operation operation_a, Operation operation_b, Sign sign,
                   asc::DenseBlasRealType<T> scale) {
  long double a_squared = 0;
  long double b_squared = 0;
  long double x_squared = 0;
  long double c_squared = 0;
  long double residual_squared = 0;
  const long double signed_term = static_cast<int>(sign);
  for (extent_t i = 0; i < a.rows(); ++i) {
    for (extent_t j = 0; j < a.rows(); ++j) {
      a_squared += std::norm(Coefficient(a, operation_a, i, j));
    }
  }
  for (extent_t i = 0; i < b.rows(); ++i) {
    for (extent_t j = 0; j < b.rows(); ++j) {
      b_squared += std::norm(Coefficient(b, operation_b, i, j));
    }
  }
  for (extent_t i = 0; i < x.rows(); ++i) {
    for (extent_t j = 0; j < x.columns(); ++j) {
      x_squared += std::norm(Widen(x(i, j)));
      c_squared += std::norm(Widen(original_c(i, j)));
      Wide residual =
          -static_cast<long double>(scale) * Widen(original_c(i, j));
      for (extent_t k = 0; k < a.rows(); ++k) {
        residual += Coefficient(a, operation_a, i, k) * Widen(x(k, j));
      }
      for (extent_t k = 0; k < b.rows(); ++k) {
        residual +=
            signed_term * Widen(x(i, k)) * Coefficient(b, operation_b, k, j);
      }
      residual_squared += std::norm(residual);
    }
  }
  const long double denominator =
      (std::sqrt(a_squared) + std::sqrt(b_squared)) * std::sqrt(x_squared) +
      static_cast<long double>(scale) * std::sqrt(c_squared);
  const long double tolerance =
      64 * static_cast<long double>(a.rows() + b.rows()) *
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  ASC_DENSE_TEST_CHECK(test, std::isfinite(residual_squared));
  ASC_DENSE_TEST_CHECK(test,
                       std::sqrt(residual_squared) <= tolerance * denominator);
}

template <typename T>
void CheckProblem(TestContext& test,
                  const asc::ReferenceLapackProvider& provider, extent_t m,
                  extent_t n, Layout layout_a, Layout layout_b, Layout layout_c,
                  Operation operation_a, Operation operation_b, Sign sign,
                  bool blocks) {
  Matrix<T> a(m, m, layout_a);
  Matrix<T> b(n, n, layout_b);
  Matrix<T> c(m, n, layout_c);
  FillSchur(a, 3, blocks);
  FillSchur(b, 17, blocks);
  for (extent_t i = 0; i < m; ++i) {
    for (extent_t j = 0; j < n; ++j) {
      Wide value{};
      for (extent_t k = 0; k < m; ++k) {
        value += Coefficient(a, operation_a, i, k) * ChosenSolution<T>(k, j);
      }
      for (extent_t k = 0; k < n; ++k) {
        value += static_cast<long double>(static_cast<int>(sign)) *
                 ChosenSolution<T>(i, k) * Coefficient(b, operation_b, k, j);
      }
      c(i, j) = Narrow<T>(value);
    }
  }
  const auto old_a = a.bytes();
  const auto old_b = b.bytes();
  const auto original_c = c;
  asc::LapackReport report;
  const auto plan = WithoutAllocation(test, [&] {
    return asc::QueryTrsylWorkspace(provider, operation_a, operation_b, sign,
                                    a.const_view(), b.const_view(), c.view(),
                                    report);
  });
  ASC_DENSE_TEST_CHECK(test, plan.ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  a.CheckSame(test, old_a);
  b.CheckSame(test, old_b);
  c.CheckSame(test, original_c.bytes());
  if (!plan.ok()) {
    return;
  }
  Scratch<T> scratch(*plan);
  asc::DenseBlasRealType<T> scale = -73;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Trsyl(provider, operation_a, operation_b, sign, a.const_view(),
                      b.const_view(), c.view(), scale, *plan, scratch.workspace,
                      report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, scale, 1);
  CheckResidual(test, a, b, original_c, c, operation_a, operation_b, sign,
                scale);
  const long double tolerance =
      64 * static_cast<long double>(m + n) *
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (extent_t i = 0; i < m; ++i) {
    for (extent_t j = 0; j < n; ++j) {
      ASC_DENSE_TEST_CHECK(
          test,
          std::abs(Widen(c(i, j)) - ChosenSolution<T>(i, j)) <=
              tolerance * std::max(1.0L, std::abs(ChosenSolution<T>(i, j))));
    }
  }
  a.CheckSame(test, old_a);
  b.CheckSame(test, old_b);
  c.CheckPadding(test, original_c.bytes());
  scratch.CheckGuards(test);
}

template <typename T>
void CheckScaled(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Layout layout) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(1, 1, layout);
  Matrix<T> b(1, 1, layout);
  Matrix<T> c(1, 1, layout);
  const Real tiny = std::is_same_v<Real, float> ? static_cast<Real>(1e-20L)
                                                : static_cast<Real>(1e-200L);
  a(0, 0) = Narrow<T>({tiny});
  b(0, 0) = Narrow<T>({tiny});
  c(0, 0) = Narrow<T>({std::numeric_limits<Real>::max() / 16.0L,
                       std::numeric_limits<Real>::max() / 32.0L});
  const auto original_c = c;
  asc::LapackReport report;
  const auto plan = Take(asc::QueryTrsylWorkspace(
      provider, Operation::kNone, Operation::kNone, Sign::kPlus, a.const_view(),
      b.const_view(), c.view(), report));
  Scratch<T> scratch(plan);
  Real scale = -1;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Trsyl(provider, Operation::kNone, Operation::kNone, Sign::kPlus,
                      a.const_view(), b.const_view(), c.view(), scale, plan,
                      scratch.workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_CHECK(test, scale > 0);
  ASC_DENSE_TEST_CHECK(test, scale < 1);
  CheckResidual(test, a, b, original_c, c, Operation::kNone, Operation::kNone,
                Sign::kPlus, scale);
  const Wide expected = static_cast<long double>(scale) *
                        Widen(original_c(0, 0)) /
                        (2.0L * static_cast<long double>(tiny));
  ASC_DENSE_TEST_CHECK(
      test, std::abs(Widen(c(0, 0)) - expected) <=
                32 * std::numeric_limits<Real>::epsilon() * std::abs(expected));
  scratch.CheckGuards(test);
}

template <typename T>
void CheckPerturbed(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    Layout layout) {
  Matrix<T> a(1, 1, layout);
  Matrix<T> b(1, 1, layout);
  Matrix<T> c(1, 1, layout);
  a(0, 0) = T{1};
  b(0, 0) = T{1};
  c(0, 0) = T{2};
  asc::LapackReport report;
  const auto plan = Take(asc::QueryTrsylWorkspace(
      provider, Operation::kNone, Operation::kNone, Sign::kMinus,
      a.const_view(), b.const_view(), c.view(), report));
  Scratch<T> scratch(plan);
  asc::DenseBlasRealType<T> scale = -1;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Trsyl(provider, Operation::kNone, Operation::kNone,
                      Sign::kMinus, a.const_view(), b.const_view(), c.view(),
                      scale, plan, scratch.workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 1);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kAccuracyWarning);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  ASC_DENSE_TEST_CHECK(test, scale > 0 && scale <= 1);
  ASC_DENSE_TEST_CHECK(test, std::isfinite(std::abs(Widen(c(0, 0)))));
  ASC_DENSE_TEST_CHECK(test, std::abs(Widen(c(0, 0))) > 2);
  // This unperturbed equation is inconsistent: its left side is identically
  // zero for every X, so INFO=1 must never be credited as its solution.
  ASC_DENSE_TEST_CHECK(test, 2 * static_cast<long double>(scale) > 0);
  scratch.CheckGuards(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  constexpr std::array<Operation, 3> kOperations{
      Operation::kNone, Operation::kTranspose, Operation::kConjugateTranspose};
  constexpr std::array<std::array<extent_t, 2>, 6> kShapes{
      std::array<extent_t, 2>{1, 1}, {1, 2}, {2, 1}, {2, 2}, {5, 4}, {4, 5}};
  std::size_t executed = 0;
  for (const auto shape : kShapes) {
    for (const auto layout_a : {Layout::kColumnMajor, Layout::kRowMajor}) {
      for (const auto layout_b : {Layout::kColumnMajor, Layout::kRowMajor}) {
        for (const auto layout_c : {Layout::kColumnMajor, Layout::kRowMajor}) {
          for (const auto operation_a : kOperations) {
            if (asc::DenseBlasComplex<T> &&
                operation_a == Operation::kTranspose) {
              continue;
            }
            for (const auto operation_b : kOperations) {
              if (asc::DenseBlasComplex<T> &&
                  operation_b == Operation::kTranspose) {
                continue;
              }
              for (const auto sign : {Sign::kPlus, Sign::kMinus}) {
                for (const bool blocks : {false, true}) {
                  if (asc::DenseBlasComplex<T> && blocks) {
                    continue;
                  }
                  CheckProblem<T>(test, provider, shape[0], shape[1], layout_a,
                                  layout_b, layout_c, operation_a, operation_b,
                                  sign, blocks);
                  ++executed;
                }
              }
            }
          }
        }
      }
    }
  }
  for (const auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
    CheckScaled<T>(test, provider, layout);
    CheckPerturbed<T>(test, provider, layout);
    executed += 2;
  }
  ASC_DENSE_TEST_EQ(test, executed, asc::DenseBlasComplex<T> ? 388U : 1732U);
  std::cout << executed << " actual nonempty TRSYL cases; formula queries are "
            << "not foreign execution evidence\n";
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
