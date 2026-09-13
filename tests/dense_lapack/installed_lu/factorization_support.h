#ifndef ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_FACTORIZATION_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_FACTORIZATION_SUPPORT_H_

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"

// This installed-consumer support uses only installed public ASC headers.
namespace installed_internal {
using Wide = std::complex<long double>;
inline constexpr auto kHost = asc::MemorySpace::kHost;
inline constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
inline constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;

template <typename T>
T Take(asc::Result<T> value) {
  if (!value.ok()) {
    std::fprintf(stderr, "Installed descriptor failed: %d\n",
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
  // One padding entry per stored major dimension is deliberately reachable.
  std::array<T, (Rows + 1) * (Columns + 1)> data{};
  asc::DenseBlasLayout layout;

  [[nodiscard]] std::size_t Offset(std::size_t row, std::size_t column) const {
    return layout == kColumn ? (Rows + 1) * column + row
                             : (Columns + 1) * row + column;
  }
  T& At(std::size_t row, std::size_t column) {
    return data[Offset(row, column)];
  }
  [[nodiscard]] const T& At(std::size_t row, std::size_t column) const {
    return data[Offset(row, column)];
  }
  auto View() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        data.data(), Rows, Columns, layout,
        layout == kColumn ? Rows + 1 : Columns + 1,
        {data.data(), sizeof(data), kHost}));
  }
  auto ConstView() { return asc::DenseBlasMatrixView<const T>(View()); }
  [[nodiscard]] bool PaddingEquals(
      const std::array<T, (Rows + 1) * (Columns + 1)>& old) const {
    auto expected = old;
    for (std::size_t row = 0; row < Rows; ++row) {
      for (std::size_t column = 0; column < Columns; ++column) {
        expected[Offset(row, column)] = At(row, column);
      }
    }
    return data == expected;
  }
};

template <typename T, std::size_t N>
auto Vector(std::array<T, N>& data) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      data.data(), N, 1, {data.data(), sizeof(data), kHost}));
}

template <typename T, std::size_t N>
auto ConstVector(std::array<T, N>& data) {
  return asc::DenseBlasVectorView<const T>(Vector(data));
}

template <typename T>
struct Scratch {
  std::array<T, 5000> scalar{};
  std::array<T, 64> packing{};
  alignas(std::max_align_t) std::array<std::byte, 64> integers{};
  asc::LapackWorkspace workspace;

  Scratch() {
    workspace.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kScalar)] = {scalar.data(), sizeof(scalar),
                                               kHost};
    workspace.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kLayoutConversion)] = {
        packing.data(), sizeof(packing), kHost};
    workspace.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kInteger)] = {integers.data(),
                                                integers.size(), kHost};
  }
};

inline bool Succeeded(const asc::Status& status,
                      const asc::LapackReport& report) {
  return status.ok() && report.outcome == asc::LapackOutcome::kSuccess &&
         report.output_validity == asc::LapackOutputValidity::kComplete &&
         report.called_provider && report.native_info == 0;
}

template <typename T>
bool Near(Wide actual, Wide expected, long double scale) {
  // Small, well-conditioned fixtures only. Larger/scaled cases have separate
  // production tests; this is not a universal forward-error tolerance.
  return std::isfinite(std::abs(actual)) &&
         std::abs(actual - expected) <=
             128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() *
                 scale;
}
}  // namespace installed_internal

#endif  // ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_FACTORIZATION_SUPPORT_H_
