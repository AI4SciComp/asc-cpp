#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/cholesky.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/qr.h"
#include "asc/dense/lapack/report.h"

namespace {
using Wide = std::complex<long double>;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;

template <typename T>
T Take(asc::Result<T> value) {
  if (!value.ok()) {
    std::fprintf(stderr, "Descriptor failed: %d\n",
                 static_cast<int>(value.status().code()));
    std::abort();
  }
  return std::move(*value);
}

template <typename T>
T Value(double real, double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
Wide Widen(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}

template <typename T>
T Conjugate(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return std::conj(value);
  } else {
    return value;
  }
}

template <typename T, std::size_t Rows, std::size_t Columns>
struct Matrix {
  std::array<T, Rows * Columns> data{};
  asc::DenseBlasLayout layout;

  T& At(std::size_t row, std::size_t column) {
    return data[layout == kColumn ? Rows * column + row
                                  : Columns * row + column];
  }
  auto View() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        data.data(), Rows, Columns, layout, layout == kColumn ? Rows : Columns,
        {data.data(), sizeof(data), kHost}));
  }
  auto ConstView() { return asc::DenseBlasMatrixView<const T>(View()); }
};

template <typename T, std::size_t N>
auto Vector(std::array<T, N>& data) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      data.data(), N, 1, {data.data(), sizeof(data), kHost}));
}

bool Succeeded(const asc::Status& status, const asc::LapackReport& report) {
  return status.ok() && report.outcome == asc::LapackOutcome::kSuccess &&
         report.output_validity == asc::LapackOutputValidity::kComplete &&
         !report.called_provider && !report.native_info.has_value();
}

template <typename T>
bool Near(Wide actual, Wide expected, long double scale) {
  return std::isfinite(std::abs(actual)) &&
         std::abs(actual - expected) <=
             128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() *
                 scale;
}

template <typename T>
bool Cholesky(asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout,
              asc::DenseBlasTriangle triangle) {
  const auto cpu = asc::ExecutionContext::Serial();
  Matrix<T, 2, 2> a{{}, layout};
  Matrix<T, 2, 2> b{{}, rhs_layout};
  const auto lower = Value<T>(1, 1);
  a.At(0, 0) = Value<T>(4);
  a.At(0, 1) = Value<T>(2) * Conjugate(lower);
  a.At(1, 0) = Value<T>(2) * lower;
  a.At(1, 1) = Value<T>(9) + lower * Conjugate(lower);
  for (std::size_t row = 0; row < 2; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      b.At(row, column) = a.At(row, 0) * Value<T>(1 + column) +
                          a.At(row, 1) * Value<T>(3 + column);
    }
  }
  asc::LapackReport report;
  if (!Succeeded(asc::Potrf(cpu, triangle, a.View(), report), report)) {
    return false;
  }
  const auto factor = Take(asc::LapackCholeskyFactorView<T>::Create(
      a.ConstView(), triangle, report));
  const auto factor_bytes = a.data;
  if (!Near<T>(Widen(a.At(0, 0)), 2, 3) || !Near<T>(Widen(a.At(1, 1)), 3, 3) ||
      !Succeeded(asc::Potrs(cpu, factor, b.View(), report), report)) {
    return false;
  }
  for (std::size_t row = 0; row < 2; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      if (!Near<T>(Widen(b.At(row, column)), 1 + 2 * row + column, 4)) {
        return false;
      }
    }
  }
  // Factor reuse is a separate solve, not another factorization.
  b.data.fill(T{});
  if (!Succeeded(asc::Potrs(cpu, factor, b.View(), report), report) ||
      b.data != std::array<T, 4>{} || a.data != factor_bytes) {
    return false;
  }
  a.data.fill(Value<T>(1));
  const auto failure = asc::Potrf(cpu, triangle, a.View(), report);
  return failure.code() == asc::ErrorCode::kNumerical &&
         report.outcome == asc::LapackOutcome::kNotPositiveDefinite &&
         report.diagnostic_index == 1 && !report.native_info.has_value() &&
         !report.called_provider &&
         !asc::LapackCholeskyFactorView<T>::Create(a.ConstView(), triangle,
                                                   report)
              .ok();
}

template <typename T>
bool CheckQr(Matrix<T, 3, 2>& original, Matrix<T, 3, 2>& packed,
             Matrix<T, 3, 3>& q) {
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      Wide dot{};
      const auto first_column = row;
      for (std::size_t inner = 0; inner < 3; ++inner) {
        dot += std::conj(Widen(q.At(inner, first_column))) *
               Widen(q.At(inner, column));
      }
      if (!Near<T>(dot, row == column ? 1 : 0, 1)) {
        return false;
      }
    }
    for (std::size_t column = 0; column < 2; ++column) {
      Wide product{};
      for (std::size_t inner = 0; inner <= column; ++inner) {
        product += Widen(q.At(row, inner)) * Widen(packed.At(inner, column));
      }
      if (!Near<T>(product, Widen(original.At(row, column)), 2)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool Qr(asc::DenseBlasLayout layout, asc::DenseBlasLayout output_layout) {
  const auto cpu = asc::ExecutionContext::Serial();
  Matrix<T, 3, 2> a{{}, layout};
  a.At(0, 0) = Value<T>(1, 0.25);
  a.At(1, 1) = Value<T>(1, -0.125);
  a.At(2, 0) = a.At(2, 1) = Value<T>(1);
  auto original = a;
  Matrix<T, 3, 3> q{{}, output_layout};
  std::array<T, 2> tau{};
  std::array<T, 3> scratch{};
  asc::LapackReport report;
  if (!Succeeded(
          asc::Geqrf(cpu, a.View(), Vector(tau), Vector(scratch), report),
          report)) {
    return false;
  }
  const auto factor = Take(asc::LapackHouseholderQrFactorView<T>::Create(
      a.ConstView(), asc::DenseBlasVectorView<const T>(Vector(tau)), report));
  const auto factor_bytes = a.data;
  const auto tau_bytes = tau;
  if (!Succeeded(
          asc::FormHouseholderQ(cpu, factor, q.View(), Vector(scratch), report),
          report) ||
      !CheckQr(original, a, q)) {
    return false;
  }
  Matrix<T, 3, 2> c = original;
  if (!Succeeded(asc::ApplyHouseholderQ(cpu, asc::DenseBlasSide::kLeft,
                                        asc::DenseBlasTranspose::kNone, factor,
                                        c.View(), Vector(scratch), report),
                 report)) {
    return false;
  }
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      Wide expected{};
      for (std::size_t inner = 0; inner < 3; ++inner) {
        expected += Widen(q.At(row, inner)) * Widen(original.At(inner, column));
      }
      if (!Near<T>(Widen(c.At(row, column)), expected, 3)) {
        return false;
      }
    }
  }
  return a.data == factor_bytes && tau == tau_bytes;
}

template <typename T>
bool AllModes() {
  for (const auto layout : {kRow, kColumn}) {
    for (const auto output_layout : {kRow, kColumn}) {
      if (!Qr<T>(layout, output_layout)) {
        return false;
      }
      for (const auto triangle :
           {asc::DenseBlasTriangle::kLower, asc::DenseBlasTriangle::kUpper}) {
        if (!Cholesky<T>(layout, output_layout, triangle)) {
          return false;
        }
      }
    }
  }
  return true;
}
}  // namespace

int main() {
  const bool passed = AllModes<float>() && AllModes<double>() &&
                      AllModes<std::complex<float>>() &&
                      AllModes<std::complex<double>>();
  std::puts(passed ? "Native Cholesky reuse/failure and QR "
                     "reconstruction/application passed."
                   : "Native factorization example failed.");
  return passed ? 0 : 1;
}
