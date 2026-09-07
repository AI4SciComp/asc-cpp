#ifndef ASC_TESTS_DENSE_LAPACK_LEAST_SQUARES_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_LEAST_SQUARES_TEST_SUPPORT_H_

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
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_least_squares.h"
#include "least_squares_faults.h"

namespace asc_least_squares_test {
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
auto Query(const asc::ReferenceLapackProvider& provider, Routine routine,
           asc::DenseBlasTranspose transpose, asc::DenseBlasMatrixView<T> a,
           asc::DenseBlasMatrixView<T> b, asc::LapackReport& report) {
  switch (routine) {
    case Routine::kGels:
      return asc::QueryGelsWorkspace(provider, transpose, a, b, report);
    case Routine::kGelst:
      return asc::QueryGelstWorkspace(provider, transpose, a, b, report);
    case Routine::kGetsls:
      return asc::QueryGetslsWorkspace(provider, transpose, a, b, report);
  }
  return asc::Result<asc::LapackWorkspacePlan>(
      asc::Status(asc::ErrorCode::kInvalidArgument));
}

template <typename T>
auto Execute(const asc::ReferenceLapackProvider& provider, Routine routine,
             asc::DenseBlasTranspose transpose, asc::DenseBlasMatrixView<T> a,
             asc::DenseBlasMatrixView<T> b,
             const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  switch (routine) {
    case Routine::kGels:
      return asc::Gels(provider, transpose, a, b, plan, workspace, report);
    case Routine::kGelst:
      return asc::Gelst(provider, transpose, a, b, plan, workspace, report);
    case Routine::kGetsls:
      return asc::Getsls(provider, transpose, a, b, plan, workspace, report);
  }
  return asc::Status(asc::ErrorCode::kInvalidArgument);
}

}  // namespace asc_least_squares_test

#endif  // ASC_TESTS_DENSE_LAPACK_LEAST_SQUARES_TEST_SUPPORT_H_
