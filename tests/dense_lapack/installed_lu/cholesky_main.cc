#include <complex>
#include <cstddef>
#include <cstdio>
#include <initializer_list>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Conjugate;
using installed_internal::kColumn;
using installed_internal::kRow;
using installed_internal::Matrix;
using installed_internal::Near;
using installed_internal::Scratch;
using installed_internal::Succeeded;
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Wide;
using installed_internal::Widen;

template <typename T>
Matrix<T, 2, 2> PositiveMatrix(asc::DenseBlasLayout layout) {
  Matrix<T, 2, 2> a{{}, layout};
  a.data.fill(Value<T>(-91));
  const auto lower = Value<T>(1, 1);
  a.At(0, 0) = Value<T>(4);
  a.At(0, 1) = Value<T>(2) * Conjugate(lower);
  a.At(1, 0) = Value<T>(2) * lower;
  a.At(1, 1) = Value<T>(9) + lower * Conjugate(lower);
  return a;
}

template <typename T>
Matrix<T, 2, 2> RightHandSide(const Matrix<T, 2, 2>& a,
                              asc::DenseBlasLayout layout) {
  Matrix<T, 2, 2> b{{}, layout};
  b.data.fill(Value<T>(-73));
  for (std::size_t row = 0; row < 2; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      b.At(row, column) = a.At(row, 0) * Value<T>(1 + column) +
                          a.At(row, 1) * Value<T>(3 + column);
    }
  }
  return b;
}

template <typename T>
bool Solution(const Matrix<T, 2, 2>& a, const Matrix<T, 2, 2>& b,
              const Matrix<T, 2, 2>& x) {
  for (std::size_t row = 0; row < 2; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      Wide residual = -Widen(b.At(row, column));
      long double scale = std::abs(Widen(b.At(row, column)));
      for (std::size_t inner = 0; inner < 2; ++inner) {
        const auto product =
            Widen(a.At(row, inner)) * Widen(x.At(inner, column));
        residual += product;
        scale += std::abs(product);
      }
      if (!Near<T>(residual, 0, scale) ||
          !Near<T>(Widen(x.At(row, column)), 1 + 2 * row + column, 4)) {
        return false;
      }
    }
  }
  return x.PaddingEquals(b.data);
}

template <typename T>
T SymmetricEntry(const Matrix<T, 2, 2>& a, asc::DenseBlasTriangle triangle,
                 std::size_t row, std::size_t column) {
  const bool selected = triangle == asc::DenseBlasTriangle::kLower
                            ? row >= column
                            : row <= column;
  const auto i = column;
  const auto j = row;
  return selected ? a.At(row, column) : Conjugate(a.At(i, j));
}

template <typename T>
bool Inverse(const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasTriangle triangle, Matrix<T, 2, 2>& factor,
             const Matrix<T, 2, 2>& original, Scratch<T>& scratch) {
  const auto old = factor.data;
  const auto plan = asc::QueryPotriWorkspace(provider, triangle, factor.View());
  asc::LapackReport report;
  if (!plan.ok() || !Succeeded(asc::Potri(provider, triangle, factor.View(),
                                          *plan, scratch.workspace, report),
                               report)) {
    return false;
  }
  for (std::size_t row = 0; row < 2; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      Wide product{};
      for (std::size_t inner = 0; inner < 2; ++inner) {
        product += Widen(original.At(row, inner)) *
                   Widen(SymmetricEntry(factor, triangle, inner, column));
      }
      if (!Near<T>(product, row == column ? 1 : 0, 4)) {
        return false;
      }
    }
  }
  const auto opposite = triangle == asc::DenseBlasTriangle::kLower
                            ? factor.Offset(0, 1)
                            : factor.Offset(1, 0);
  return factor.PaddingEquals(old) && factor.data[opposite] == old[opposite];
}

template <typename T>
auto FactorPlan(const asc::ReferenceLapackProvider& provider, int algorithm,
                asc::DenseBlasTriangle triangle, Matrix<T, 2, 2>& a) {
  if (algorithm == 0) {
    return asc::QueryPotrfWorkspace(provider, triangle, a.View());
  }
  if (algorithm == 1) {
    return asc::QueryPotrf2Workspace(provider, triangle, a.View());
  }
  return asc::QueryPotf2Workspace(provider, triangle, a.View());
}

template <typename T>
auto Factor(const asc::ReferenceLapackProvider& provider, int algorithm,
            asc::DenseBlasTriangle triangle, Matrix<T, 2, 2>& a,
            Scratch<T>& scratch, asc::LapackReport& report) {
  const auto plan = FactorPlan(provider, algorithm, triangle, a);
  if (!plan.ok()) {
    return plan.status();
  }
  if (algorithm == 0) {
    return asc::Potrf(provider, triangle, a.View(), *plan, scratch.workspace,
                      report);
  }
  if (algorithm == 1) {
    return asc::Potrf2(provider, triangle, a.View(), *plan, scratch.workspace,
                       report);
  }
  return asc::Potf2(provider, triangle, a.View(), *plan, scratch.workspace,
                    report);
}

template <typename T>
bool FactorAndSolve(const asc::ReferenceLapackProvider& provider, int algorithm,
                    asc::DenseBlasTriangle triangle,
                    asc::DenseBlasLayout layout,
                    asc::DenseBlasLayout rhs_layout) {
  auto a = PositiveMatrix<T>(layout);
  const auto original = a;
  auto b = RightHandSide(original, rhs_layout);
  const auto rhs = b;
  Scratch<T> scratch;
  asc::LapackReport report;
  if (!Succeeded(Factor(provider, algorithm, triangle, a, scratch, report),
                 report) ||
      !a.PaddingEquals(original.data)) {
    return false;
  }
  const auto lower = Value<T>(1, 1);
  const auto offdiagonal = triangle == asc::DenseBlasTriangle::kLower
                               ? a.At(1, 0)
                               : Conjugate(a.At(0, 1));
  if (!Near<T>(Widen(a.At(0, 0)), 2, 3) || !Near<T>(Widen(a.At(1, 1)), 3, 3) ||
      !Near<T>(Widen(offdiagonal), Widen(lower), 2)) {
    return false;
  }
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      a.ConstView(), triangle, report));
  const auto factor_bytes = a.data;
  const auto plan = asc::QueryPotrsWorkspace(provider, factor, b.View());
  if (!plan.ok() ||
      !Succeeded(asc::Potrs(provider, factor, b.View(), *plan,
                            scratch.workspace, report),
                 report) ||
      !Solution(original, rhs, b) || a.data != factor_bytes) {
    return false;
  }
  // Reuse the exact same successful factor and plan for a second RHS solve.
  b = rhs;
  if (!Succeeded(asc::Potrs(provider, factor, b.View(), *plan,
                            scratch.workspace, report),
                 report) ||
      !Solution(original, rhs, b) || a.data != factor_bytes) {
    return false;
  }
  return Inverse(provider, triangle, a, original, scratch);
}

template <typename T>
bool DriverAndFailures(const asc::ReferenceLapackProvider& provider,
                       asc::DenseBlasTriangle triangle,
                       asc::DenseBlasLayout layout,
                       asc::DenseBlasLayout rhs_layout) {
  auto a = PositiveMatrix<T>(layout);
  const auto original = a;
  auto b = RightHandSide(original, rhs_layout);
  const auto rhs = b;
  Scratch<T> scratch;
  asc::LapackReport report;
  const auto plan =
      asc::QueryPosvWorkspace(provider, triangle, a.View(), b.View());
  if (!plan.ok() ||
      !Succeeded(asc::Posv(provider, triangle, a.View(), b.View(), *plan,
                           scratch.workspace, report),
                 report) ||
      !Solution(original, rhs, b)) {
    return false;
  }
  for (int algorithm = 0; algorithm < 4; ++algorithm) {
    a.data.fill(Value<T>(1));
    b = rhs;
    const auto status =
        algorithm < 3
            ? Factor(provider, algorithm, triangle, a, scratch, report)
            : asc::Posv(provider, triangle, a.View(), b.View(), *plan,
                        scratch.workspace, report);
    if (status.code() != asc::ErrorCode::kNumerical ||
        report.native_info != 2 || !report.called_provider ||
        report.diagnostic_index != 1 || b.data != rhs.data ||
        asc::LapackCholeskyFactorView<T>::Create(a.ConstView(), triangle,
                                                 report)
            .ok()) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool AllModes(const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : {kRow, kColumn}) {
    for (const auto rhs_layout : {kRow, kColumn}) {
      for (const auto triangle :
           {asc::DenseBlasTriangle::kLower, asc::DenseBlasTriangle::kUpper}) {
        for (int algorithm = 0; algorithm < 3; ++algorithm) {
          if (!FactorAndSolve<T>(provider, algorithm, triangle, layout,
                                 rhs_layout)) {
            return false;
          }
        }
        if (!DriverAndFailures<T>(provider, triangle, layout, rhs_layout)) {
          return false;
        }
      }
    }
  }
  return true;
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const bool passed = AllModes<float>(provider) && AllModes<double>(provider) &&
                      AllModes<std::complex<float>>(provider) &&
                      AllModes<std::complex<double>>(provider);
  std::puts(passed ? "Installed Cholesky: all 24 scalar routes passed."
                   : "Installed Cholesky failed.");
  return passed ? 0 : 1;
}
