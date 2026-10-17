#ifndef ASC_TESTS_DENSE_LAPACK_QR_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_QR_TEST_SUPPORT_H_

#include <algorithm>
#include <bit>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "asc/core/contracts.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/workspace.h"

namespace asc_qr_test {
using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
using Layout = asc::DenseBlasLayout;
constexpr std::size_t kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr std::size_t kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T>
T Take(asc::Result<T> result) {
  ASC_CHECK(result.ok());
  return std::move(*result);
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
bool SameBits(T a, T b) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return SameBits(a.real(), b.real()) && SameBits(a.imag(), b.imag());
  } else {
    using Bits =
        std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
    return std::bit_cast<Bits>(a) == std::bit_cast<Bits>(b);
  }
}

template <typename Operation>
auto WithoutAllocation(TestContext& test, Operation operation) {
  asc_dense_test::AllocationProbe probe;
  auto result = operation();
  const auto count = probe.count();
  ASC_DENSE_TEST_CHECK(test, asc_test::ProcessAllocationCountMatches(count, 0));
  return result;
}

template <typename T>
class Matrix {
 public:
  Matrix(asc::extent_t rows, asc::extent_t columns, Layout layout)
      : rows_(rows),
        columns_(columns),
        layout_(layout),
        leading_((layout == Layout::kRowMajor ? columns : rows) + 2),
        data_(static_cast<std::size_t>(
                  (layout == Layout::kRowMajor ? rows : columns) * leading_) +
                  7,
              Narrow<T>({-713, 21})) {}
  [[nodiscard]] std::size_t Offset(asc::extent_t row,
                                   asc::extent_t column) const {
    return 3 + static_cast<std::size_t>(layout_ == Layout::kRowMajor
                                            ? row * leading_ + column
                                            : column * leading_ + row);
  }
  T& operator()(asc::extent_t row, asc::extent_t column) {
    return data_[Offset(row, column)];
  }
  const T& operator()(asc::extent_t row, asc::extent_t column) const {
    return data_[Offset(row, column)];
  }
  auto view(asc::MemorySpace space = asc::MemorySpace::kHost) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        data_.data() + 3, rows_, columns_, layout_, leading_,
        {data_.data(), data_.size() * sizeof(T), space}));
  }
  [[nodiscard]] auto const_view() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        data_.data() + 3, rows_, columns_, layout_, leading_,
        {data_.data(), data_.size() * sizeof(T), asc::MemorySpace::kHost}));
  }
  [[nodiscard]] const std::vector<T>& bytes() const { return data_; }
  void CheckSame(TestContext& test, const std::vector<T>& before) const {
    for (std::size_t i = 0; i < data_.size(); ++i) {
      ASC_DENSE_TEST_CHECK(test, SameBits(data_[i], before[i]));
    }
  }
  void CheckPadding(TestContext& test, const std::vector<T>& before) const {
    std::vector<bool> logical(data_.size());
    for (asc::extent_t row = 0; row < rows_; ++row) {
      for (asc::extent_t col = 0; col < columns_; ++col) {
        logical[Offset(row, col)] = true;
      }
    }
    for (std::size_t i = 0; i < data_.size(); ++i) {
      if (!logical[i]) {
        ASC_DENSE_TEST_CHECK(test, SameBits(data_[i], before[i]));
      }
    }
  }

 private:
  asc::extent_t rows_;
  asc::extent_t columns_;
  Layout layout_;
  asc::stride_t leading_;
  std::vector<T> data_;
};

template <typename T>
auto Vector(std::vector<T>& values, asc::extent_t count) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data(), count, 1,
      {values.data(), values.size() * sizeof(T), asc::MemorySpace::kHost}));
}

template <typename T>
auto ConstVector(const std::vector<T>& values, asc::extent_t count) {
  return Take(asc::DenseBlasVectorView<const T>::Create(
      values.data(), count, 1,
      {values.data(), values.size() * sizeof(T), asc::MemorySpace::kHost}));
}

template <typename T>
class Scratch {
 public:
  Scratch(const asc::LapackWorkspacePlan& plan, bool preferred)
      : scalar_(static_cast<std::size_t>(
            preferred ? plan.regions[kScalar].preferred_entries
                      : plan.regions[kScalar].minimum_entries)),
        packing_(
            static_cast<std::size_t>(plan.regions[kPacking].minimum_entries)) {}
  auto view() {
    asc::LapackWorkspace result;
    result.regions[kScalar] = {scalar_.data(), scalar_.size() * sizeof(T),
                               asc::MemorySpace::kHost};
    result.regions[kPacking] = {packing_.data(), packing_.size() * sizeof(T),
                                asc::MemorySpace::kHost};
    return result;
  }

 private:
  std::vector<T> scalar_;
  std::vector<T> packing_;
};

template <typename T>
void Fill(Matrix<T>& matrix, int fixture) {
  const auto view = matrix.const_view();
  for (asc::extent_t i = 0; i < view.rows(); ++i) {
    for (asc::extent_t j = 0; j < view.columns(); ++j) {
      const auto column = fixture == 1 && j == 1 ? 0 : j;
      const long double real =
          static_cast<long double>((i * 3 + column * 5 + 2) % 17 - 8) / 7;
      const long double imag =
          static_cast<long double>((i + column * 2) % 5 - 2) / 9;
      matrix(i, j) = fixture == 2 ? T{} : Narrow<T>({real, imag});
    }
  }
}

template <typename T>
std::vector<Wide> ExplicitQ(const Matrix<T>& factors, const std::vector<T>& tau,
                            asc::extent_t k) {
  const auto m = factors.const_view().rows();
  std::vector<Wide> q(static_cast<std::size_t>(m * m));
  for (asc::extent_t i = 0; i < m; ++i) {
    q[static_cast<std::size_t>(i * m + i)] = 1;
  }
  // Independent widened right multiplication by H_j=I-tau_j*v_j*v_j^H.
  // It does not call ASC's Q generation/application or LAPACK/BLAS.
  for (asc::extent_t j = 0; j < k; ++j) {
    for (asc::extent_t row = 0; row < m; ++row) {
      Wide dot = q[static_cast<std::size_t>(row * m + j)];
      for (asc::extent_t col = j + 1; col < m; ++col) {
        dot +=
            q[static_cast<std::size_t>(row * m + col)] * Widen(factors(col, j));
      }
      dot *= Widen(tau[static_cast<std::size_t>(j)]);
      q[static_cast<std::size_t>(row * m + j)] -= dot;
      for (asc::extent_t col = j + 1; col < m; ++col) {
        q[static_cast<std::size_t>(row * m + col)] -=
            dot * std::conj(Widen(factors(col, j)));
      }
    }
  }
  return q;
}

template <typename T>
void Near(TestContext& test, Wide actual, Wide expected, asc::extent_t order) {
  const auto bound =
      120 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() *
      std::max<asc::extent_t>(1, order) * (1 + std::abs(expected));
  ASC_DENSE_TEST_CHECK(test, std::isfinite(std::abs(actual)));
  ASC_DENSE_TEST_CHECK(test, std::abs(actual - expected) <= bound);
}
}  // namespace asc_qr_test

#endif  // ASC_TESTS_DENSE_LAPACK_QR_TEST_SUPPORT_H_
