#ifndef ASC_TESTS_NATIVE_ALIAS_SUPPORT_H_
#define ASC_TESTS_NATIVE_ALIAS_SUPPORT_H_

#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <span>
#include <type_traits>
#include <utility>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "test_support.h"

namespace asc_native_test {
using asc_dense_test::TestContext;
using Layout = asc::DenseBlasLayout;
using Op = asc::DenseBlasTranspose;
using Triangle = asc::DenseBlasTriangle;
using Wide = std::complex<long double>;
constexpr std::array kLayouts{Layout::kRowMajor, Layout::kColumnMajor};
constexpr std::array kOperations{Op::kNone, Op::kTranspose,
                                 Op::kConjugateTranspose};
constexpr std::array kTriangles{Triangle::kLower, Triangle::kUpper};
constexpr auto kHost = asc::MemorySpace::kHost;

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}

template <typename T>
T Scalar(long double real, long double imaginary = 0) {
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
struct Matrix {
  std::array<T, 40> values;
  asc::extent_t rows;
  asc::extent_t columns;
  Layout layout;
  asc::stride_t leading;

  Matrix(asc::extent_t m, asc::extent_t n, Layout order)
      : rows(m),
        columns(n),
        layout(order),
        leading((order == Layout::kRowMajor ? n : m) + 2) {
    values.fill(Scalar<T>(-79, 31));
  }
  [[nodiscard]] std::size_t Offset(asc::index_t i, asc::index_t j) const {
    return 3 + static_cast<std::size_t>(layout == Layout::kRowMajor
                                            ? i * leading + j
                                            : j * leading + i);
  }
  T& At(asc::index_t i, asc::index_t j) { return values[Offset(i, j)]; }
  auto View(asc::MemorySpace space = kHost) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        values.data() + 3, rows, columns, layout, leading,
        {values.data(), sizeof(values), space}));
  }
  [[nodiscard]] auto ConstView() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        values.data() + 3, rows, columns, layout, leading,
        {values.data(), sizeof(values), kHost}));
  }
  void CheckPadding(TestContext& test, const std::array<T, 40>& before) const {
    std::array<bool, 40> logical{};
    for (asc::index_t i = 0; i < rows; ++i) {
      for (asc::index_t j = 0; j < columns; ++j) {
        logical[Offset(i, j)] = true;
      }
    }
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (!logical[i]) {
        ASC_DENSE_TEST_EQ(test, values[i], before[i]);
      }
    }
  }
};

template <typename T, std::size_t N>
auto Vector(std::array<T, N>& values, asc::extent_t size) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data() + 1, size, 1, {values.data(), sizeof(values), kHost}));
}

template <typename Function>
asc::Status Observe(TestContext& test, Function function) {
  asc::Status status;
  std::size_t count = 0;
  {
    const asc_dense_test::AllocationProbe probe;
    status = function();
    count = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test, asc_test::ProcessAllocationCountMatches(count, 0));
  return status;
}

inline void Report(
    TestContext& test, const asc::LapackReport& report,
    asc::LapackOutcome outcome = asc::LapackOutcome::kSuccess,
    asc::LapackOutputValidity validity = asc::LapackOutputValidity::kComplete) {
  ASC_DENSE_TEST_EQ(test, report.provider, asc::LapackProviderIdentity{});
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, outcome);
  ASC_DENSE_TEST_EQ(test, report.output_validity, validity);
}

template <typename T>
auto Bytes(const T& value) {
  std::array<std::byte, sizeof(T)> result{};
  const auto bytes = std::as_bytes(std::span(&value, 1));
  std::ranges::copy(bytes, result.begin());
  return result;
}

// Rejected descriptors cover live T elements on each side of a live foreign
// object. No T is overlaid onto that object. These descriptors are used only
// for metadata rejection, before any numerical element access.
template <typename T, typename Middle>
struct GappedMatrix {
  std::array<T, 2> first{T{1}, T{0}};
  alignas(std::max(alignof(Middle), sizeof(T))) Middle middle{};
  alignas(sizeof(T)) std::array<T, 2> last{T{0}, T{1}};

  auto View(Layout layout) {
    using Storage = GappedMatrix<T, Middle>;
    static_assert(std::is_standard_layout_v<Storage>);
    static_assert(offsetof(Storage, last) % sizeof(T) == 0);
    constexpr auto kLeading =
        static_cast<asc::stride_t>(offsetof(Storage, last) / sizeof(T));
    return Take(asc::DenseBlasMatrixView<T>::Create(
        first.data(), 2, 2, layout, kLeading, {this, sizeof(*this), kHost}));
  }
};

inline void RejectedReport(TestContext& test, const asc::LapackReport& report) {
  Report(test, report, asc::LapackOutcome::kNotRun,
         asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
}

}  // namespace asc_native_test

#endif  // ASC_TESTS_NATIVE_ALIAS_SUPPORT_H_
