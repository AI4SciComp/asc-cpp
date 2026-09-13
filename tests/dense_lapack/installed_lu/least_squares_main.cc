#include <complex>
#include <cstddef>
#include <cstdio>
#include <initializer_list>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_least_squares.h"
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
using Operation = asc::DenseBlasTranspose;

template <typename T, std::size_t Rows, std::size_t Columns>
auto Query(const asc::ReferenceLapackProvider& provider, int algorithm,
           Operation operation, Matrix<T, Rows, Columns>& a,
           Matrix<T, Rows, 2>& b, asc::LapackReport& report) {
  if (algorithm == 0) {
    return asc::QueryGelsWorkspace(provider, operation, a.View(), b.View(),
                                   report);
  }
  if (algorithm == 1) {
    return asc::QueryGelstWorkspace(provider, operation, a.View(), b.View(),
                                    report);
  }
  return asc::QueryGetslsWorkspace(provider, operation, a.View(), b.View(),
                                   report);
}

template <typename T, std::size_t Rows, std::size_t Columns>
auto Execute(const asc::ReferenceLapackProvider& provider, int algorithm,
             Operation operation, Matrix<T, Rows, Columns>& a,
             Matrix<T, Rows, 2>& b, const asc::LapackWorkspacePlan& plan,
             Scratch<T>& scratch, asc::LapackReport& report) {
  if (algorithm == 0) {
    return asc::Gels(provider, operation, a.View(), b.View(), plan,
                     scratch.workspace, report);
  }
  if (algorithm == 1) {
    return asc::Gelst(provider, operation, a.View(), b.View(), plan,
                      scratch.workspace, report);
  }
  return asc::Getsls(provider, operation, a.View(), b.View(), plan,
                     scratch.workspace, report);
}

template <typename T>
bool Check(const Matrix<T, 3, 2>& a, const Matrix<T, 3, 2>& original_b,
           const Matrix<T, 3, 2>& result, bool adjoint) {
  for (std::size_t column = 0; column < 2; ++column) {
    const auto multiplier = static_cast<long double>(column + 1);
    const auto output_rows = adjoint ? 3U : 2U;
    for (std::size_t row = 0; row < output_rows; ++row) {
      const auto expected = (row == 1 ? 2 : 1) * multiplier;
      if (!Near<T>(Widen(result.At(row, column)), expected, 4)) {
        return false;
      }
    }
    if (adjoint) {
      for (std::size_t equation = 0; equation < 2; ++equation) {
        Wide residual = -Widen(original_b.At(equation, column));
        for (std::size_t unknown = 0; unknown < 3; ++unknown) {
          residual += std::conj(Widen(a.At(unknown, equation))) *
                      Widen(result.At(unknown, column));
        }
        if (!Near<T>(residual, 0, 16)) {
          return false;
        }
      }
      // The null space is span([1,0,-1]); orthogonality proves minimum norm.
      if (!Near<T>(Widen(result.At(0, column) - result.At(2, column)), 0, 4)) {
        return false;
      }
    } else {
      for (std::size_t unknown = 0; unknown < 2; ++unknown) {
        Wide gradient{};
        for (std::size_t equation = 0; equation < 3; ++equation) {
          Wide residual = -Widen(original_b.At(equation, column));
          for (std::size_t inner = 0; inner < 2; ++inner) {
            residual +=
                Widen(a.At(equation, inner)) * Widen(result.At(inner, column));
          }
          gradient += std::conj(Widen(a.At(equation, unknown))) * residual;
        }
        if (!Near<T>(gradient, 0, 32)) {
          return false;
        }
      }
    }
  }
  return result.PaddingEquals(original_b.data);
}

template <typename T>
bool Solve(const asc::ReferenceLapackProvider& provider, int algorithm,
           asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout,
           bool adjoint) {
  Matrix<T, 3, 2> a{{}, layout};
  Matrix<T, 3, 2> b{{}, rhs_layout};
  a.data.fill(Value<T>(-71));
  b.data.fill(Value<T>(-73));
  const auto phase = Value<T>(1, 0.5);
  for (std::size_t row = 0; row < 3; ++row) {
    a.At(row, 0) = row == 1 ? T{} : phase;
    a.At(row, 1) = row == 1 ? Value<T>(2) : T{};
  }
  for (std::size_t column = 0; column < 2; ++column) {
    const auto factor = Value<T>(static_cast<double>(column + 1));
    b.At(0, column) = adjoint ? Value<T>(2) * Conjugate(phase) * factor
                              : phase * factor + Value<T>(1);
    b.At(1, column) = Value<T>(4) * factor;
    b.At(2, column) = adjoint ? Value<T>(-97) : phase * factor - Value<T>(1);
  }
  const auto original_a = a;
  const auto original_b = b;
  const auto transposed = asc::DenseBlasComplex<T>
                              ? Operation::kConjugateTranspose
                              : Operation::kTranspose;
  const auto operation = adjoint ? transposed : Operation::kNone;
  asc::LapackReport report;
  const auto plan = Query(provider, algorithm, operation, a, b, report);
  if (!plan.ok() || !report.called_provider || report.native_info != 0 ||
      a.data != original_a.data || b.data != original_b.data) {
    return false;
  }
  Scratch<T> scratch;
  const auto status =
      Execute(provider, algorithm, operation, a, b, *plan, scratch, report);
  if (!Succeeded(status, report) || report.factor_family.has_value() ||
      !a.PaddingEquals(original_a.data) ||
      !Check(original_a, original_b, b, adjoint)) {
    return false;
  }
  return algorithm != 2 || adjoint ||
         (b.At(2, 0) == original_b.At(2, 0) &&
          b.At(2, 1) == original_b.At(2, 1));
}

template <typename T>
bool Singular(const asc::ReferenceLapackProvider& provider, int algorithm,
              asc::DenseBlasLayout layout) {
  Matrix<T, 2, 2> a{{}, layout};
  Matrix<T, 2, 2> b{{}, layout};
  a.At(0, 0) = Value<T>(1);
  b.At(0, 0) = Value<T>(2);
  b.At(1, 1) = Value<T>(3);
  const auto original_a = a.data;
  const auto original_b = b.data;
  asc::LapackReport report;
  const auto plan = Query(provider, algorithm, Operation::kNone, a, b, report);
  if (!plan.ok()) {
    return false;
  }
  Scratch<T> scratch;
  const auto status = Execute(provider, algorithm, Operation::kNone, a, b,
                              *plan, scratch, report);
  return status.code() == asc::ErrorCode::kNumerical &&
         report.called_provider && report.native_info == 2 &&
         report.diagnostic_index == 1 &&
         report.outcome == asc::LapackOutcome::kSingular &&
         report.output_validity ==
             asc::LapackOutputValidity::kDocumentedPartial &&
         !report.factor_family.has_value() && a.PaddingEquals(original_a) &&
         b.PaddingEquals(original_b);
}

template <typename T>
bool All(const asc::ReferenceLapackProvider& provider) {
  for (const int algorithm : {0, 1, 2}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const auto rhs_layout : {kColumn, kRow}) {
        for (const bool adjoint : {false, true}) {
          if (!Solve<T>(provider, algorithm, layout, rhs_layout, adjoint)) {
            return false;
          }
        }
      }
      if (!Singular<T>(provider, algorithm, layout)) {
        return false;
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
  if (!All<float>(provider) || !All<double>(provider) ||
      !All<std::complex<float>>(provider) ||
      !All<std::complex<double>>(provider)) {
    std::fprintf(stderr, "Installed least-squares consumer failed.\n");
    return 1;
  }
  std::puts(
      "Installed GELS/GELST/GETSLS: all four scalars, both operations and "
      "layouts, optimality/minimum-norm and singular INFO passed.");
  return 0;
}
