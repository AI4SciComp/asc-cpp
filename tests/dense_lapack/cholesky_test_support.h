#ifndef ASC_TESTS_DENSE_LAPACK_CHOLESKY_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_CHOLESKY_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "asc/core/contracts.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"

namespace asc_cholesky_test {
using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
using Triangle = asc::DenseBlasTriangle;
using Layout = asc::DenseBlasLayout;
constexpr std::array kTriangles{Triangle::kLower, Triangle::kUpper};
constexpr std::array kLayouts{Layout::kColumnMajor, Layout::kRowMajor};
constexpr auto kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
constexpr std::size_t kCapacity = 4608;

template <typename T>
T Take(asc::Result<T> result) {
  ASC_CHECK(result.ok());
  return std::move(*result);
}

template <typename Operation>
auto WithoutAllocation(TestContext& test, Operation operation) {
  asc_dense_test::AllocationProbe probe;
  auto result = operation();
  const auto calls = probe.count();
  ASC_DENSE_TEST_CHECK(test, asc_test::ProcessAllocationCountMatches(calls, 0));
  return result;
}

template <typename T>
T Narrow(Wide value) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(value.real()), static_cast<Real>(value.imag())};
  } else {
    return static_cast<T>(value.real());
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
T NotANumber() {
  const auto nan = std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN();
  return Narrow<T>({nan, nan});
}

template <typename T>
bool SameBits(T left, T right) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return SameBits(left.real(), right.real()) &&
           SameBits(left.imag(), right.imag());
  } else {
    using Bits = std::conditional_t<sizeof(T) == sizeof(std::uint32_t),
                                    std::uint32_t, std::uint64_t>;
    return std::bit_cast<Bits>(left) == std::bit_cast<Bits>(right);
  }
}

template <typename T>
class Matrix {
 public:
  Matrix(asc::extent_t rows, asc::extent_t columns, Layout layout)
      : rows_(rows),
        columns_(columns),
        layout_(layout),
        leading_((layout == Layout::kRowMajor ? columns : rows) + 2) {
    ASC_CHECK(rows <= 65 && columns <= 65);
    storage_.fill(Narrow<T>({-731, 19}));
  }
  [[nodiscard]] std::size_t Offset(std::size_t row, std::size_t column) const {
    const auto leading = static_cast<std::size_t>(leading_);
    return 3 + (layout_ == Layout::kRowMajor ? row * leading + column
                                             : column * leading + row);
  }
  T& operator()(std::size_t row, std::size_t column) {
    return storage_[Offset(row, column)];
  }
  const T& operator()(std::size_t row, std::size_t column) const {
    return storage_[Offset(row, column)];
  }
  asc::DenseBlasMatrixView<T> view(
      asc::MemorySpace placement = asc::MemorySpace::kHost) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        storage_.data() + 3, rows_, columns_, layout_, leading_,
        {storage_.data(), sizeof(storage_), placement}));
  }
  [[nodiscard]] asc::DenseBlasMatrixView<const T> const_view() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        storage_.data() + 3, rows_, columns_, layout_, leading_,
        {storage_.data(), sizeof(storage_), asc::MemorySpace::kHost}));
  }
  [[nodiscard]] const std::array<T, kCapacity>& bytes() const {
    return storage_;
  }
  void CheckUntouched(TestContext& test, const std::array<T, kCapacity>& before,
                      Triangle triangle, bool all_logical = false) const {
    std::array<bool, kCapacity> writable{};
    for (std::size_t row = 0; row < static_cast<std::size_t>(rows_); ++row) {
      for (std::size_t col = 0; col < static_cast<std::size_t>(columns_);
           ++col) {
        if (all_logical ||
            (triangle == Triangle::kLower ? row >= col : row <= col)) {
          writable[Offset(row, col)] = true;
        }
      }
    }
    for (std::size_t i = 0; i < before.size(); ++i) {
      if (!writable[i]) {
        ASC_DENSE_TEST_CHECK(test, SameBits(storage_[i], before[i]));
      }
    }
  }
  void CheckSame(TestContext& test,
                 const std::array<T, kCapacity>& before) const {
    for (std::size_t i = 0; i < before.size(); ++i) {
      ASC_DENSE_TEST_CHECK(test, SameBits(storage_[i], before[i]));
    }
  }

 private:
  asc::extent_t rows_;
  asc::extent_t columns_;
  Layout layout_;
  asc::stride_t leading_;
  std::array<T, kCapacity> storage_{};
};

template <typename T>
struct Scratch {
  std::array<T, 2 * kCapacity> values{};
  asc::LapackWorkspace view(const asc::LapackWorkspacePlan& plan) {
    const auto count =
        static_cast<std::size_t>(plan.regions[kPacking].minimum_entries);
    ASC_CHECK(count <= values.size());
    asc::LapackWorkspace workspace;
    workspace.regions[kPacking] = {values.data(), count * sizeof(T),
                                   asc::MemorySpace::kHost};
    return workspace;
  }
};

// The n=3 tables are independent integer fixtures. The n=65 fixture is the
// closed-form product of bidiagonal L with diagonal 2 and subdiagonal q,
// forcing the pinned ILAENV block size 64 to take the blocked branch.
template <typename T>
Wide Coefficient(std::size_t n, std::size_t row, std::size_t column) {
  if (n == 3) {
    if constexpr (asc::DenseBlasComplex<T>) {
      constexpr std::array<Wide, 9> kA{Wide{4, 0},  Wide{2, -2}, Wide{-2, -4},
                                       Wide{2, 2},  Wide{11, 0}, Wide{7, 0},
                                       Wide{-2, 4}, Wide{7, 0},  Wide{14, 0}};
      return kA[row * 3 + column];
    } else {
      constexpr std::array<int, 9> kA{4, 2, -2, 2, 10, 5, -2, 5, 9};
      return {static_cast<long double>(kA[row * 3 + column]), 0};
    }
  }
  const Wide sub =
      asc::DenseBlasComplex<T> ? Wide{0.125L, 0.0625L} : Wide{0.125L, 0};
  if (row == column) {
    return {row == 0 ? 4 : 4 + std::norm(sub), 0};
  }
  if (row == column + 1) {
    return 2.L * sub;
  }
  if (column == row + 1) {
    return 2.L * std::conj(sub);
  }
  return {};
}

template <typename T>
void Fill(Matrix<T>& matrix, std::size_t n, Triangle triangle,
          long double scale) {
  for (std::size_t row = 0; row < n; ++row) {
    for (std::size_t column = 0; column < n; ++column) {
      const bool selected =
          triangle == Triangle::kLower ? row >= column : row <= column;
      matrix(row, column) =
          selected ? Narrow<T>(Coefficient<T>(n, row, column) * scale)
                   : NotANumber<T>();
      if constexpr (asc::DenseBlasComplex<T>) {
        if (row == column) {
          matrix(row, column)
              .imag(
                  std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
        }
      }
    }
  }
}

inline void CheckRatio(TestContext& test, long double numerator,
                       long double denominator, long double tolerance) {
  long double ratio = 0;
  if (denominator != 0) {
    ratio = numerator / denominator;
  } else if (numerator != 0) {
    ratio = std::numeric_limits<long double>::infinity();
  }
  ASC_DENSE_TEST_CHECK(test, std::isfinite(ratio) && ratio <= tolerance);
}

template <typename T>
Wide Lower(const Matrix<T>& matrix, Triangle triangle, std::size_t row,
           std::size_t column) {
  if (row < column) {
    return {};
  }
  if (triangle == Triangle::kLower) {
    return Widen(matrix(row, column));
  }
  const auto stored_row = column;
  const auto stored_column = row;
  return std::conj(Widen(matrix(stored_row, stored_column)));
}

template <typename T>
void CheckFactor(TestContext& test, const Matrix<T>& matrix, std::size_t n,
                 Triangle triangle, long double scale) {
  long double error = 0;
  long double norm = 0;
  for (std::size_t row = 0; row < n; ++row) {
    long double row_error = 0;
    long double row_norm = 0;
    for (std::size_t col = 0; col < n; ++col) {
      Wide product{};
      for (std::size_t k = 0; k < n; ++k) {
        product += Lower(matrix, triangle, row, k) *
                   std::conj(Lower(matrix, triangle, col, k));
      }
      const auto expected =
          Widen(Narrow<T>(Coefficient<T>(n, row, col) * scale));
      row_error += std::abs(product - expected);
      row_norm += std::abs(expected);
    }
    error = std::max(error, row_error);
    norm = std::max(norm, row_norm);
    const auto diagonal = Widen(matrix(row, row));
    ASC_DENSE_TEST_CHECK(test, diagonal.real() > 0 && diagonal.imag() == 0);
  }
  CheckRatio(test, error, norm,
             256 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon());
}

template <typename T>
Wide Solution(std::size_t row, std::size_t column) {
  const auto real = static_cast<long double>((row + column) % 5) - 2;
  const auto imaginary = static_cast<long double>((row * 3 + column) % 3) - 1;
  return {real, asc::DenseBlasComplex<T> ? imaginary : 0};
}

template <typename T>
void FillRhs(Matrix<T>& rhs, std::size_t n, std::size_t nrhs,
             long double scale) {
  for (std::size_t row = 0; row < n; ++row) {
    for (std::size_t column = 0; column < nrhs; ++column) {
      Wide sum{};
      for (std::size_t k = 0; k < n; ++k) {
        sum += Widen(Narrow<T>(Coefficient<T>(n, row, k) * scale)) *
               Solution<T>(k, column);
      }
      rhs(row, column) = Narrow<T>(sum);
    }
  }
}

template <typename T>
void CheckSolution(TestContext& test, const Matrix<T>& solution,
                   const Matrix<T>& original, std::size_t n, std::size_t nrhs,
                   long double scale) {
  long double residual = 0;
  long double anorm = 0;
  long double xnorm = 0;
  long double bnorm = 0;
  const long double tolerance =
      512 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (std::size_t row = 0; row < n; ++row) {
    long double ar = 0;
    long double xr = 0;
    long double br = 0;
    long double rr = 0;
    for (std::size_t k = 0; k < n; ++k) {
      ar += std::abs(Widen(Narrow<T>(Coefficient<T>(n, row, k) * scale)));
    }
    for (std::size_t col = 0; col < nrhs; ++col) {
      Wide sum{};
      for (std::size_t k = 0; k < n; ++k) {
        sum += Widen(Narrow<T>(Coefficient<T>(n, row, k) * scale)) *
               Widen(solution(k, col));
      }
      rr += std::abs(sum - Widen(original(row, col)));
      xr += std::abs(Widen(solution(row, col)));
      br += std::abs(Widen(original(row, col)));
      CheckRatio(test,
                 std::abs(Widen(solution(row, col)) - Solution<T>(row, col)), 1,
                 tolerance * 4);
    }
    residual = std::max(residual, rr);
    anorm = std::max(anorm, ar);
    xnorm = std::max(xnorm, xr);
    bnorm = std::max(bnorm, br);
  }
  CheckRatio(test, residual, anorm * xnorm + bnorm, tolerance);
}

template <typename T>
Wide SymmetricEntry(const Matrix<T>& inverse, Triangle triangle,
                    std::size_t row, std::size_t column) {
  if (triangle == Triangle::kLower ? row >= column : row <= column) {
    return Widen(inverse(row, column));
  }
  const auto stored_row = column;
  const auto stored_column = row;
  return std::conj(Widen(inverse(stored_row, stored_column)));
}

template <typename T>
void CheckInverse(TestContext& test, const Matrix<T>& inverse, std::size_t n,
                  Triangle triangle, long double scale) {
  long double residual = 0;
  long double anorm = 0;
  long double inorm = 0;
  for (std::size_t row = 0; row < n; ++row) {
    long double rr = 0;
    long double ar = 0;
    long double ir = 0;
    for (std::size_t col = 0; col < n; ++col) {
      Wide product{};
      for (std::size_t k = 0; k < n; ++k) {
        product += Widen(Narrow<T>(Coefficient<T>(n, row, k) * scale)) *
                   SymmetricEntry(inverse, triangle, k, col);
      }
      rr += std::abs(product - Wide{row == col ? 1.L : 0.L, 0});
      ar += std::abs(Widen(Narrow<T>(Coefficient<T>(n, row, col) * scale)));
      ir += std::abs(SymmetricEntry(inverse, triangle, row, col));
    }
    residual = std::max(residual, rr);
    anorm = std::max(anorm, ar);
    inorm = std::max(inorm, ir);
  }
  CheckRatio(test, residual, anorm * inorm,
             512 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon());
}
}  // namespace asc_cholesky_test

#endif  // ASC_TESTS_DENSE_LAPACK_CHOLESKY_TEST_SUPPORT_H_
