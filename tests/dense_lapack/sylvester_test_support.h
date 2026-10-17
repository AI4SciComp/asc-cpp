#ifndef ASC_TESTS_DENSE_LAPACK_SYLVESTER_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_SYLVESTER_TEST_SUPPORT_H_

#include <bit>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "allocation_audit.h"
#include "asc/core/contracts.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/workspace.h"

namespace asc_sylvester_test {
using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
using Layout = asc::DenseBlasLayout;
using Operation = asc::DenseBlasTranspose;
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

template <typename Function>
auto WithoutAllocation(TestContext& test, Function function) {
  asc_dense_test::AllocationProbe probe;
  asc_lapack_test::BeginAllocationAudit();
  auto result = function();
  const auto libc_count = asc_lapack_test::EndAllocationAudit();
  const auto cpp_count = probe.count();
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(cpp_count, 0));
  ASC_DENSE_TEST_EQ(test, libc_count, std::size_t{0});
  return result;
}

template <typename T>
class Matrix {
 public:
  Matrix(asc::extent_t rows, asc::extent_t columns, Layout layout)
      : rows_(rows),
        columns_(columns),
        layout_(layout),
        leading_((layout == Layout::kRowMajor ? columns : rows) + 3),
        storage_(
            static_cast<std::size_t>(
                (layout == Layout::kRowMajor ? rows : columns) * leading_) +
                9,
            Narrow<T>({-715, 37})) {}
  [[nodiscard]] std::size_t Offset(asc::extent_t row,
                                   asc::extent_t column) const {
    return 4 + static_cast<std::size_t>(layout_ == Layout::kRowMajor
                                            ? row * leading_ + column
                                            : column * leading_ + row);
  }
  T& operator()(asc::extent_t row, asc::extent_t column) {
    return storage_[Offset(row, column)];
  }
  const T& operator()(asc::extent_t row, asc::extent_t column) const {
    return storage_[Offset(row, column)];
  }
  auto view(asc::MemorySpace space = asc::MemorySpace::kHost) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        storage_.data() + 4, rows_, columns_, layout_, leading_,
        {storage_.data(), storage_.size() * sizeof(T), space}));
  }
  [[nodiscard]] auto const_view() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        storage_.data() + 4, rows_, columns_, layout_, leading_,
        {storage_.data(), storage_.size() * sizeof(T),
         asc::MemorySpace::kHost}));
  }
  [[nodiscard]] asc::extent_t rows() const { return rows_; }
  [[nodiscard]] asc::extent_t columns() const { return columns_; }
  [[nodiscard]] const std::vector<T>& bytes() const { return storage_; }
  void CheckSame(TestContext& test, const std::vector<T>& before) const {
    for (std::size_t i = 0; i < storage_.size(); ++i) {
      ASC_DENSE_TEST_CHECK(test, SameBits(storage_[i], before[i]));
    }
  }
  void CheckPadding(TestContext& test, const std::vector<T>& before) const {
    std::vector<bool> logical(storage_.size());
    for (asc::extent_t i = 0; i < rows_; ++i) {
      for (asc::extent_t j = 0; j < columns_; ++j) {
        logical[Offset(i, j)] = true;
      }
    }
    for (std::size_t i = 0; i < storage_.size(); ++i) {
      if (!logical[i]) {
        ASC_DENSE_TEST_CHECK(test, SameBits(storage_[i], before[i]));
      }
    }
  }

 private:
  asc::extent_t rows_;
  asc::extent_t columns_;
  Layout layout_;
  asc::stride_t leading_;
  std::vector<T> storage_;
};

template <typename T>
class Scratch {
 public:
  explicit Scratch(const asc::LapackWorkspacePlan& plan)
      : storage_(
            static_cast<std::size_t>(plan.regions[kPacking].minimum_entries) +
                2,
            Narrow<T>({-619, 43})) {
    workspace.regions[kPacking] = asc::MutableMemoryView(
        storage_.data() + 1, (storage_.size() - 2) * sizeof(T),
        asc::MemorySpace::kHost);
  }
  void CheckGuards(TestContext& test) const {
    ASC_DENSE_TEST_CHECK(test,
                         SameBits(storage_.front(), Narrow<T>({-619, 43})));
    ASC_DENSE_TEST_CHECK(test,
                         SameBits(storage_.back(), Narrow<T>({-619, 43})));
  }
  [[nodiscard]] const std::vector<T>& bytes() const { return storage_; }
  void CheckSame(TestContext& test, const std::vector<T>& before) const {
    for (std::size_t i = 0; i < storage_.size(); ++i) {
      ASC_DENSE_TEST_CHECK(test, SameBits(storage_[i], before[i]));
    }
  }
  asc::LapackWorkspace workspace;

 private:
  std::vector<T> storage_;
};

}  // namespace asc_sylvester_test

#endif  // ASC_TESTS_DENSE_LAPACK_SYLVESTER_TEST_SUPPORT_H_
