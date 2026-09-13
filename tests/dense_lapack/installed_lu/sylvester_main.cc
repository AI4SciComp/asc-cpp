#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_sylvester.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::kColumn;
using installed_internal::kRow;
using installed_internal::Matrix;
using installed_internal::Scratch;
using installed_internal::Succeeded;
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Wide;
using installed_internal::Widen;
using Operation = asc::DenseBlasTranspose;
using Sign = asc::LapackSylvesterSign;
constexpr std::size_t kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T>
bool Valid(Operation operation) {
  return !asc::DenseBlasComplex<T> || operation != Operation::kTranspose;
}

template <typename T, std::size_t N>
Wide SchurEntry(const Matrix<T, N, N>& matrix, std::size_t row,
                std::size_t column, Operation operation) {
  if (operation != Operation::kNone) {
    const auto saved = row;
    row = column;
    column = saved;
  }
  const bool meaningful =
      row <= column || (!asc::DenseBlasComplex<T> && row == column + 1);
  const Wide value = meaningful ? Widen(matrix.At(row, column)) : Wide{};
  return operation == Operation::kConjugateTranspose ? std::conj(value) : value;
}

template <typename T>
void Initialize(Matrix<T, 3, 3>& a, Matrix<T, 2, 2>& b) {
  a.data.fill(T{919});
  b.data.fill(T{919});
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = i; j < 3; ++j) {
      a.At(i, j) = i == j ? Value<T>(i == 2 ? 3 : 2, 0.5) : Value<T>(-3, 0.25);
    }
  }
  b.At(0, 0) = Value<T>(4, -0.25);
  b.At(0, 1) = Value<T>(0.5, 0.75);
  b.At(1, 1) = Value<T>(6, 0.25);
  if constexpr (!asc::DenseBlasComplex<T>) {
    a.At(1, 0) = 1;
    a.At(2, 1) = 0;
    b.At(1, 0) = 0;
  }
}

template <typename T>
Wide Known(std::size_t row, std::size_t column) {
  return Widen(
      Value<T>(1 + static_cast<double>(row) - 0.5 * static_cast<double>(column),
               0.25 * static_cast<double>(1 + row + column)));
}

template <typename T>
Wide Product(const Matrix<T, 3, 3>& a, const Matrix<T, 2, 2>& b,
             const Matrix<T, 3, 2>* x, std::size_t row, std::size_t column,
             Operation operation_a, Operation operation_b, Sign sign) {
  Wide result{};
  for (std::size_t k = 0; k < 3; ++k) {
    const Wide value =
        x == nullptr ? Known<T>(k, column) : Widen(x->At(k, column));
    result += SchurEntry(a, row, k, operation_a) * value;
  }
  for (std::size_t k = 0; k < 2; ++k) {
    const Wide value = x == nullptr ? Known<T>(row, k) : Widen(x->At(row, k));
    result += static_cast<long double>(sign) * value *
              SchurEntry(b, k, column, operation_b);
  }
  return result;
}

template <typename T>
bool One(const asc::ReferenceLapackProvider& provider,
         const std::array<asc::DenseBlasLayout, 3>& layouts,
         Operation operation_a, Operation operation_b, Sign sign) {
  Matrix<T, 3, 3> a{{}, layouts[0]};
  Matrix<T, 2, 2> b{{}, layouts[1]};
  Matrix<T, 3, 2> c{{}, layouts[2]};
  Initialize(a, b);
  c.data.fill(T{719});
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      const auto value =
          Product(a, b, static_cast<const Matrix<T, 3, 2>*>(nullptr), i, j,
                  operation_a, operation_b, sign);
      c.At(i, j) = Value<T>(static_cast<double>(value.real()),
                            static_cast<double>(value.imag()));
    }
  }
  const auto original_a = a.data;
  const auto original_b = b.data;
  const auto original_c = c.data;
  asc::LapackReport report;
  const auto plan = Take(
      asc::QueryTrsylWorkspace(provider, operation_a, operation_b, sign,
                               a.ConstView(), b.ConstView(), c.View(), report));
  if (report.called_provider || report.native_info || c.data != original_c) {
    return false;
  }
  Scratch<T> scratch;
  auto short_workspace = scratch.workspace;
  short_workspace.regions[kPacking] = asc::MutableMemoryView(
      scratch.packing.data(),
      static_cast<std::size_t>(plan.regions[kPacking].minimum_entries - 1) *
          sizeof(T),
      asc::MemorySpace::kHost);
  asc::DenseBlasRealType<T> scale = 71;
  const auto rejected =
      asc::Trsyl(provider, operation_a, operation_b, sign, a.ConstView(),
                 b.ConstView(), c.View(), scale, plan, short_workspace, report);
  if (rejected.ok() || report.called_provider || scale != 71 ||
      c.data != original_c || a.data != original_a || b.data != original_b) {
    return false;
  }
  const auto status = asc::Trsyl(provider, operation_a, operation_b, sign,
                                 a.ConstView(), b.ConstView(), c.View(), scale,
                                 plan, scratch.workspace, report);
  if (!Succeeded(status, report) || scale <= 0 || scale > 1 ||
      a.data != original_a || b.data != original_b ||
      !c.PaddingEquals(original_c)) {
    return false;
  }
  long double residual = 0;
  long double denominator = 0;
  long double forward = 0;
  long double reference = 0;
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      const auto actual =
          Product(a, b, &c, i, j, operation_a, operation_b, sign);
      const Wide target =
          static_cast<long double>(scale) * Widen(original_c[c.Offset(i, j)]);
      residual += std::abs(actual - target);
      denominator += std::abs(actual) + std::abs(target);
      const Wide expected = static_cast<long double>(scale) * Known<T>(i, j);
      forward += std::abs(Widen(c.At(i, j)) - expected);
      reference += std::abs(expected);
    }
  }
  // This fixture has disjoint well-separated spectra and nonzero X. Both
  // metrics use their actual positive scales, never an additive one.
  const auto tolerance =
      256 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  return denominator > 0 && reference > 0 &&
         residual / denominator <= tolerance &&
         forward / reference <= tolerance;
}

template <typename T>
bool Warnings(const asc::ReferenceLapackProvider& provider) {
  Matrix<T, 1, 1> a{{}, kRow};
  Matrix<T, 1, 1> b{{}, kColumn};
  Matrix<T, 1, 1> c{{}, kRow};
  a.At(0, 0) = b.At(0, 0) = 1;
  c.At(0, 0) = 2;
  Scratch<T> scratch;
  asc::LapackReport report;
  const auto plan = Take(asc::QueryTrsylWorkspace(
      provider, Operation::kNone, Operation::kNone, Sign::kMinus, a.ConstView(),
      b.ConstView(), c.View(), report));
  asc::DenseBlasRealType<T> scale = 71;
  const auto status = asc::Trsyl(
      provider, Operation::kNone, Operation::kNone, Sign::kMinus, a.ConstView(),
      b.ConstView(), c.View(), scale, plan, scratch.workspace, report);
  return status.code() == asc::ErrorCode::kNumerical &&
         report.called_provider && report.native_info == 1 &&
         report.outcome == asc::LapackOutcome::kAccuracyWarning &&
         report.output_validity ==
             asc::LapackOutputValidity::kDocumentedPartial &&
         scale > 0 && scale <= 1 && std::isfinite(std::abs(c.At(0, 0)));
}

template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  for (const auto a : {kColumn, kRow}) {
    for (const auto b : {kColumn, kRow}) {
      for (const auto c : {kColumn, kRow}) {
        for (const auto op_a : {Operation::kNone, Operation::kTranspose,
                                Operation::kConjugateTranspose}) {
          for (const auto op_b : {Operation::kNone, Operation::kTranspose,
                                  Operation::kConjugateTranspose}) {
            if (!Valid<T>(op_a) || !Valid<T>(op_b)) {
              continue;
            }
            for (const auto sign : {Sign::kPlus, Sign::kMinus}) {
              if (!One<T>(provider, {a, b, c}, op_a, op_b, sign)) {
                return false;
              }
            }
          }
        }
      }
    }
  }
  return Warnings<T>(provider);
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (!Run<float>(provider) || !Run<double>(provider) ||
      !Run<std::complex<float>>(provider) ||
      !Run<std::complex<double>>(provider)) {
    std::fputs("Installed Sylvester contract failed\n", stderr);
    return 1;
  }
  std::puts(
      "416 independent Sylvester solves and four actual warning calls passed");
  return 0;
}
