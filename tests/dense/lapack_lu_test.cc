#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/lu.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
constexpr auto kCpu = asc::MemorySpace::kHost;
constexpr auto kLu = asc::LapackFactorFamily::kLuPartialPivot;
constexpr std::array kLayouts{asc::DenseBlasLayout::kRowMajor,
                              asc::DenseBlasLayout::kColumnMajor};
constexpr std::array kOperations{asc::DenseBlasTranspose::kNone,
                                 asc::DenseBlasTranspose::kTranspose,
                                 asc::DenseBlasTranspose::kConjugateTranspose};

template <typename Value>
Value Take(asc::Result<Value> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}

template <typename Element>
Element Scalar(int real, int imaginary = 0) {
  using Real = asc::DenseBlasRealType<Element>;
  if constexpr (asc::DenseBlasComplex<Element>) {
    return Element(static_cast<Real>(real), static_cast<Real>(imaginary));
  } else {
    return static_cast<Element>(real);
  }
}

template <typename Element>
Wide Widen(Element value) {
  if constexpr (asc::DenseBlasComplex<Element>) {
    return {static_cast<long double>(value.real()),
            static_cast<long double>(value.imag())};
  } else {
    return {static_cast<long double>(value), 0};
  }
}

// The fixed storage includes leading/trailing red zones and leading-dimension
// padding. Tests need no production allocation to preserve independent inputs.
template <typename Element>
class Matrix {
 public:
  Matrix(asc::extent_t rows, asc::extent_t columns, asc::DenseBlasLayout layout)
      : rows_(rows),
        columns_(columns),
        layout_(layout),
        lda_((layout == asc::DenseBlasLayout::kRowMajor ? columns : rows) + 2) {
    storage_.fill(Scalar<Element>(-313, 97));
    for (asc::index_t row = 0; row < rows_; ++row) {
      for (asc::index_t column = 0; column < columns_; ++column) {
        (*this)(row, column) = Element{};
      }
    }
  }
  Element& operator()(asc::index_t row, asc::index_t column) {
    return storage_[Offset(row, column)];
  }
  const Element& operator()(asc::index_t row, asc::index_t column) const {
    return storage_[Offset(row, column)];
  }
  asc::DenseBlasMatrixView<Element> view(asc::MemorySpace placement = kCpu) {
    return Take(asc::DenseBlasMatrixView<Element>::Create(
        storage_.data() + 3, rows_, columns_, layout_, lda_,
        {storage_.data(), sizeof(storage_), placement}));
  }
  [[nodiscard]] asc::DenseBlasMatrixView<const Element> const_view() const {
    return Take(asc::DenseBlasMatrixView<const Element>::Create(
        storage_.data() + 3, rows_, columns_, layout_, lda_,
        {storage_.data(), sizeof(storage_), kCpu}));
  }
  [[nodiscard]] const std::array<Element, 128>& storage() const {
    return storage_;
  }
  [[nodiscard]] asc::extent_t rows() const { return rows_; }
  [[nodiscard]] asc::extent_t columns() const { return columns_; }
  void CheckPadding(TestContext& test) const {
    std::array<bool, 128> logical{};
    for (asc::index_t row = 0; row < rows_; ++row) {
      for (asc::index_t column = 0; column < columns_; ++column) {
        logical[Offset(row, column)] = true;
      }
    }
    for (std::size_t index = 0; index < storage_.size(); ++index) {
      if (!logical[index]) {
        ASC_DENSE_TEST_EQ(test, storage_[index], Scalar<Element>(-313, 97));
      }
    }
  }

 private:
  [[nodiscard]] std::size_t Offset(asc::index_t row,
                                   asc::index_t column) const {
    const auto offset = layout_ == asc::DenseBlasLayout::kRowMajor
                            ? row * lda_ + column
                            : column * lda_ + row;
    return static_cast<std::size_t>(offset) + 3;
  }
  asc::extent_t rows_;
  asc::extent_t columns_;
  asc::DenseBlasLayout layout_;
  asc::stride_t lda_;
  std::array<Element, 128> storage_{};
};

class Pivots {
 public:
  explicit Pivots(asc::extent_t count) : count_(count) { storage_.fill(-91); }
  asc::DenseBlasVectorView<asc::index_t> view() {
    return Take(asc::DenseBlasVectorView<asc::index_t>::Create(
        storage_.data() + 1, count_, 1,
        {storage_.data(), sizeof(storage_), kCpu}));
  }
  [[nodiscard]] asc::RawLapackPivotView raw() const {
    return Take(asc::RawLapackPivotView::Create(
        storage_.data() + 1, count_, kLu,
        {storage_.data(), sizeof(storage_), kCpu}));
  }
  void CheckPadding(TestContext& test) const {
    ASC_DENSE_TEST_EQ(test, storage_[0], -91);
    for (std::size_t index = static_cast<std::size_t>(count_) + 1;
         index < storage_.size(); ++index) {
      ASC_DENSE_TEST_EQ(test, storage_[index], -91);
    }
  }

 private:
  asc::extent_t count_;
  std::array<asc::index_t, 12> storage_{};
};

void CheckReport(TestContext& test, const asc::LapackReport& report,
                 asc::LapackOutcome outcome,
                 asc::LapackOutputValidity validity) {
  ASC_DENSE_TEST_EQ(test, report.provider, asc::LapackProviderIdentity{});
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, outcome);
  ASC_DENSE_TEST_EQ(test, report.output_validity, validity);
  ASC_DENSE_TEST_CHECK(test, report.routine[0] != '\0');
}

void CheckMetric(TestContext& test, long double numerator,
                 long double denominator, long double tolerance) {
  // Infinity-norm row sums are accumulated in wider arithmetic, without
  // squaring large double values. A zero denominator cannot mask a nonzero
  // error.
  long double metric = 0;
  if (denominator == 0) {
    metric = numerator == 0 ? 0 : std::numeric_limits<long double>::infinity();
  } else {
    metric = numerator / denominator;
  }
  ASC_DENSE_TEST_CHECK(test, std::isfinite(metric));
  ASC_DENSE_TEST_CHECK(test, metric <= tolerance);
}

template <typename Element>
void EmitExecution(const TestContext& test, const asc::LapackReport& report,
                   const Matrix<Element>& matrix, int rhs_layout = -1,
                   int transpose = -1, int exponent = 0,
                   std::string_view fixture = "rectangular") {
  if (test.Finish() == 0) {
    std::cout << R"({"routine":")" << report.routine.data()
              << R"(","route":"native","rows":)" << matrix.rows()
              << R"(,"columns":)" << matrix.columns() << R"(,"layout":)"
              << static_cast<int>(matrix.const_view().layout())
              << R"(,"rhs_layout":)" << rhs_layout << R"(,"transpose":)"
              << transpose << R"(,"scale_exponent":)" << exponent
              << R"(,"fixture":")" << fixture << "\"}\n";
  }
}

template <typename Element>
void CheckReconstruction(TestContext& test, const Matrix<Element>& original,
                         const Matrix<Element>& packed, const Pivots& pivots) {
  auto permuted = original;
  const auto sequence = pivots.raw().values();
  for (std::size_t index = 0; index < sequence.size(); ++index) {
    for (asc::index_t column = 0; column < original.columns(); ++column) {
      std::swap(permuted(static_cast<asc::index_t>(index), column),
                permuted(sequence[index] - 1, column));
    }
  }
  long double difference = 0;
  long double matrix_norm = 0;
  long double product_norm = 0;
  for (asc::index_t row = 0; row < original.rows(); ++row) {
    long double error_sum = 0;
    long double matrix_sum = 0;
    long double product_sum = 0;
    for (asc::index_t column = 0; column < original.columns(); ++column) {
      Wide product{};
      for (asc::index_t inner = 0;
           inner < std::min(original.rows(), original.columns()); ++inner) {
        Wide lower{};
        if (row == inner) {
          lower = Wide{1};
        } else if (row > inner) {
          lower = Widen(packed(row, inner));
        }
        const Wide upper =
            inner <= column ? Widen(packed(inner, column)) : Wide{};
        product += lower * upper;
      }
      error_sum += std::abs(product - Widen(permuted(row, column)));
      matrix_sum += std::abs(Widen(permuted(row, column)));
      product_sum += std::abs(product);
    }
    difference = std::max(difference, error_sum);
    matrix_norm = std::max(matrix_norm, matrix_sum);
    product_norm = std::max(product_norm, product_sum);
  }
  // Small, independently formed well-scaled fixtures: 32*n epsilon allows
  // accumulated rounding in unblocked elimination, not a fixed precision bound.
  CheckMetric(
      test, difference, matrix_norm + product_norm,
      32 * std::max(original.rows(), original.columns()) *
          std::numeric_limits<asc::DenseBlasRealType<Element>>::epsilon());
  packed.CheckPadding(test);
  pivots.CheckPadding(test);
}

template <typename Element>
void CheckSolve(TestContext& test, const Matrix<Element>& original,
                const Matrix<Element>& solution,
                const Matrix<Element>& original_rhs,
                const Matrix<Element>& expected,
                asc::DenseBlasTranspose operation) {
  long double difference = 0;
  long double matrix_norm = 0;
  long double solution_norm = 0;
  long double rhs_norm = 0;
  for (asc::index_t row = 0; row < original.rows(); ++row) {
    long double matrix_sum = 0;
    long double solution_sum = 0;
    long double rhs_sum = 0;
    long double error_sum = 0;
    for (asc::index_t inner = 0; inner < original.rows(); ++inner) {
      // The transposed problem intentionally exchanges the original indices.
      // NOLINTNEXTLINE(readability-suspicious-call-argument)
      const auto transposed = original(inner, row);
      matrix_sum += std::abs(Widen(
          operation == kOperations[0] ? original(row, inner) : transposed));
    }
    for (asc::index_t column = 0; column < solution.columns(); ++column) {
      Wide product{};
      for (asc::index_t inner = 0; inner < original.rows(); ++inner) {
        // The transposed problem intentionally exchanges the original indices.
        // NOLINTNEXTLINE(readability-suspicious-call-argument)
        const auto transposed = original(inner, row);
        Wide value = Widen(operation == kOperations[0] ? original(row, inner)
                                                       : transposed);
        if (operation == kOperations[2]) {
          value = std::conj(value);
        }
        product += value * Widen(solution(inner, column));
      }
      error_sum += std::abs(product - Widen(original_rhs(row, column)));
      solution_sum += std::abs(Widen(solution(row, column)));
      rhs_sum += std::abs(Widen(original_rhs(row, column)));
      const long double forward_error =
          std::abs(Widen(solution(row, column)) - Widen(expected(row, column)));
      ASC_DENSE_TEST_CHECK(
          test, forward_error <=
                    256 * std::numeric_limits<
                              asc::DenseBlasRealType<Element>>::epsilon());
    }
    difference = std::max(difference, error_sum);
    matrix_norm = std::max(matrix_norm, matrix_sum);
    solution_norm = std::max(solution_norm, solution_sum);
    rhs_norm = std::max(rhs_norm, rhs_sum);
  }
  CheckMetric(
      test, difference, matrix_norm * solution_norm + rhs_norm,
      32 * original.rows() *
          std::numeric_limits<asc::DenseBlasRealType<Element>>::epsilon());
  solution.CheckPadding(test);
}

template <typename Element>
void TestRectangular(TestContext& test) {
  for (auto layout : kLayouts) {
    for (const auto& shape :
         std::array{std::array{0, 0}, std::array{0, 4}, std::array{4, 0},
                    std::array{1, 1}, std::array{5, 3}, std::array{3, 5}}) {
      Matrix<Element> matrix(shape[0], shape[1], layout);
      for (asc::index_t row = 0; row < matrix.rows(); ++row) {
        for (asc::index_t column = 0; column < matrix.columns(); ++column) {
          matrix(row, column) =
              Scalar<Element>(static_cast<int>((7 * row + 11 * column) % 13) -
                                  6 + (row == column ? 8 : 0),
                              static_cast<int>(row - column));
        }
      }
      const auto original = matrix;
      Pivots pivots(std::min(matrix.rows(), matrix.columns()));
      asc::LapackReport report;
      const auto status = asc::Getrf(asc::ExecutionContext::Serial(),
                                     matrix.view(), pivots.view(), report);
      ASC_DENSE_TEST_CHECK(test, status.ok());
      CheckReport(test, report, asc::LapackOutcome::kSuccess,
                  asc::LapackOutputValidity::kComplete);
      ASC_DENSE_TEST_EQ(test, report.factor_family, kLu);
      CheckReconstruction(test, original, matrix, pivots);
      EmitExecution(test, report, matrix);
    }
  }
}

template <typename Element>
void TestSolveModes(TestContext& test, asc::DenseBlasLayout factor_layout,
                    asc::DenseBlasLayout rhs_layout, int exponent,
                    bool repeated_swaps = false) {
  using Real = asc::DenseBlasRealType<Element>;
  Matrix<Element> matrix(3, 3, factor_layout);
  constexpr std::array<int, 9> kValues{0, 2, 1, 1, -2, -3, 2, 3, 1};
  constexpr std::array<int, 9> kRepeatedSwaps{1, 8, 2, 2, 1, 9, 8, 2, 3};
  const Real scale = std::ldexp(Real{1}, exponent);
  for (asc::index_t row = 0; row < 3; ++row) {
    for (asc::index_t column = 0; column < 3; ++column) {
      const int imaginary = row == column ? 1 : static_cast<int>(row - column);
      matrix(row, column) =
          scale * Scalar<Element>((repeated_swaps ? kRepeatedSwaps
                                                  : kValues)[3 * row + column],
                                  repeated_swaps ? 0 : imaginary);
    }
  }
  const auto original = matrix;
  Pivots pivots(3);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, asc::Getrf(asc::ExecutionContext::Serial(),
                                        matrix.view(), pivots.view(), report)
                                 .ok());
  CheckReconstruction(test, original, matrix, pivots);
  auto factor = Take(asc::LapackLuFactorView<Element>::Create(
      matrix.const_view(), pivots.raw(), report));
  const auto saved_factors = matrix.storage();
  const auto saved_pivots =
      std::array{pivots.raw().values()[0], pivots.raw().values()[1],
                 pivots.raw().values()[2]};
  Matrix<Element> expected(3, 2, rhs_layout);
  constexpr std::array<int, 6> kSolution{1, 2, -1, 0, 2, -1};
  for (asc::index_t row = 0; row < 3; ++row) {
    for (asc::index_t column = 0; column < 2; ++column) {
      expected(row, column) = Scalar<Element>(kSolution[2 * row + column],
                                              static_cast<int>(row + column));
    }
  }
  for (auto operation : kOperations) {
    Matrix<Element> rhs(3, 2, rhs_layout);
    for (asc::index_t row = 0; row < 3; ++row) {
      for (asc::index_t column = 0; column < 2; ++column) {
        for (asc::index_t inner = 0; inner < 3; ++inner) {
          // The RHS of a transposed problem uses original column coordinates.
          // NOLINTNEXTLINE(readability-suspicious-call-argument)
          const auto transposed = original(inner, row);
          Element value =
              operation == kOperations[0] ? original(row, inner) : transposed;
          if constexpr (asc::DenseBlasComplex<Element>) {
            if (operation == kOperations[2]) {
              value = std::conj(value);
            }
          }
          rhs(row, column) += value * expected(inner, column);
        }
      }
    }
    const auto original_rhs = rhs;
    asc_dense_test::AllocationProbe probe;
    const auto status = asc::Getrs(asc::ExecutionContext::Serial(), operation,
                                   factor, rhs.view(), report);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
    CheckReport(test, report, asc::LapackOutcome::kSuccess,
                asc::LapackOutputValidity::kComplete);
    CheckSolve(test, original, rhs, original_rhs, expected, operation);
    ASC_DENSE_TEST_EQ(test, matrix.storage(), saved_factors);
    ASC_DENSE_TEST_CHECK(test,
                         std::equal(saved_pivots.begin(), saved_pivots.end(),
                                    pivots.raw().values().begin()));
    EmitExecution(test, report, original, static_cast<int>(rhs_layout),
                  static_cast<int>(operation), exponent,
                  repeated_swaps ? "repeated_swaps" : "runbook");
  }
}

template <typename Element>
void TestPivotAndFailure(TestContext& test) {
  for (auto layout : kLayouts) {
    Matrix<Element> matrix(3, 3, layout);
    constexpr std::array<int, 9> kValues{1, 8, 2, 2, 1, 9, 8, 2, 3};
    for (asc::index_t row = 0; row < 3; ++row) {
      for (asc::index_t column = 0; column < 3; ++column) {
        matrix(row, column) = Scalar<Element>(kValues[3 * row + column]);
      }
    }
    const auto original = matrix;
    Pivots pivots(3);
    asc::LapackReport report;
    asc_dense_test::AllocationProbe probe;
    ASC_DENSE_TEST_CHECK(test, asc::Getrf(asc::ExecutionContext::Serial(),
                                          matrix.view(), pivots.view(), report)
                                   .ok());
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
    for (asc::index_t pivot : pivots.raw().values()) {
      ASC_DENSE_TEST_EQ(test, pivot, 3);
    }
    CheckReconstruction(test, original, matrix, pivots);
    for (int size : {1, 2, 3}) {
      Matrix<Element> singular(size, size, layout);
      for (int diagonal = 0; diagonal < size - 1; ++diagonal) {
        singular(diagonal, diagonal) = Element{1};
      }
      Pivots singular_pivots(size);
      const auto status =
          asc::Getrf(asc::ExecutionContext::Serial(), singular.view(),
                     singular_pivots.view(), report);
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      CheckReport(test, report, asc::LapackOutcome::kSingular,
                  asc::LapackOutputValidity::kDocumentedPartial);
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), size - 1);
      ASC_DENSE_TEST_CHECK(
          test, !asc::LapackLuFactorView<Element>::Create(
                     singular.const_view(), singular_pivots.raw(), report)
                     .ok());
      singular.CheckPadding(test);
      singular_pivots.CheckPadding(test);
    }
    Matrix<Element> dependent(2, 2, layout);
    dependent(0, 0) = Element{1};
    dependent(0, 1) = Element{2};
    dependent(1, 0) = Element{2};
    dependent(1, 1) = Element{4};
    Pivots dependent_pivots(2);
    const auto dependent_original = dependent;
    ASC_DENSE_TEST_EQ(
        test,
        asc::Getrf(asc::ExecutionContext::Serial(), dependent.view(),
                   dependent_pivots.view(), report)
            .code(),
        asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), 1);
    CheckReconstruction(test, dependent_original, dependent, dependent_pivots);
    Matrix<Element> zero(3, 3, layout);
    Pivots zero_pivots(3);
    ASC_DENSE_TEST_EQ(test,
                      asc::Getrf(asc::ExecutionContext::Serial(), zero.view(),
                                 zero_pivots.view(), report)
                          .code(),
                      asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), 0);
    const auto sequence = zero_pivots.raw().values();
    ASC_DENSE_TEST_EQ(test, sequence[0], 1);
    ASC_DENSE_TEST_EQ(test, sequence[1], 2);
    ASC_DENSE_TEST_EQ(test, sequence[2], 3);
  }
}

template <typename Element>
void TestSubnormal(TestContext& test) {
  using Real = asc::DenseBlasRealType<Element>;
  Matrix<Element> matrix(2, 2, kLayouts[0]);
  const Real tiny = std::numeric_limits<Real>::denorm_min();
  matrix(0, 0) = Element{tiny * Real{2}};
  matrix(1, 0) = Element{tiny};
  matrix(1, 1) = Element{1};
  Pivots pivots(2);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, asc::Getrf(asc::ExecutionContext::Serial(),
                                        matrix.view(), pivots.view(), report)
                                 .ok());
  ASC_DENSE_TEST_EQ(test, matrix(1, 0), Element{Real{0.5}});
  Matrix<Element> rhs(2, 1, kLayouts[1]);
  rhs(0, 0) = Element{tiny * Real{2}};
  rhs(1, 0) = Element{1};
  auto factor = Take(asc::LapackLuFactorView<Element>::Create(
      matrix.const_view(), pivots.raw(), report));
  ASC_DENSE_TEST_CHECK(
      test, asc::Getrs(asc::ExecutionContext::Serial(), kOperations[0], factor,
                       rhs.view(), report)
                .ok());
  ASC_DENSE_TEST_EQ(test, rhs(0, 0), Element{1});
  ASC_DENSE_TEST_EQ(test, rhs(1, 0), Element{1});
}

template <typename Real>
void TestComplexPivot(TestContext& test) {
  using Element = std::complex<Real>;
  Matrix<Element> matrix(2, 1, kLayouts[0]);
  Pivots pivots(1);
  asc::LapackReport report;
  matrix(0, 0) = Element{5, 0};
  matrix(1, 0) = Element{3, 3};
  ASC_DENSE_TEST_CHECK(test, asc::Getrf(asc::ExecutionContext::Serial(),
                                        matrix.view(), pivots.view(), report)
                                 .ok());
  ASC_DENSE_TEST_EQ(test, pivots.raw().values()[0], 2);
  matrix(0, 0) = Element{4, 1};
  matrix(1, 0) = Element{3, 2};
  ASC_DENSE_TEST_CHECK(test, asc::Getrf(asc::ExecutionContext::Serial(),
                                        matrix.view(), pivots.view(), report)
                                 .ok());
  ASC_DENSE_TEST_EQ(test, pivots.raw().values()[0], 1);
}

template <typename Element>
void TestFactorValidation(TestContext& test) {
  Matrix<Element> matrix(2, 2, kLayouts[0]);
  matrix(0, 0) = Element{1};
  matrix(1, 1) = Element{1};
  const auto saved = matrix.storage();
  std::array<asc::index_t, 8> pivot_values{};
  const asc::ConstMemoryView backing{pivot_values.data(), sizeof(pivot_values),
                                     kCpu};
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 91;
  report.native_argument = 2;
  report.factor_family = asc::LapackFactorFamily::kBunchKaufman;
  for (const auto& spec : std::array{std::array{1, 1}, std::array{3, 1},
                                     std::array{2, 2}, std::array{2, -1}}) {
    const auto pivots = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
        pivot_values.data() + 2, spec[0], spec[1], backing));
    asc_dense_test::AllocationProbe probe;
    const auto status = asc::Getrf(asc::ExecutionContext::Serial(),
                                   matrix.view(), pivots, report);
    ASC_DENSE_TEST_EQ(test, status.code(),
                      spec[0] == 2 ? asc::ErrorCode::kInvalidArgument
                                   : asc::ErrorCode::kShape);
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
    CheckReport(test, report, asc::LapackOutcome::kNotRun,
                asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    ASC_DENSE_TEST_EQ(test, matrix.storage(), saved);
    ASC_DENSE_TEST_CHECK(
        test, std::all_of(pivot_values.begin(), pivot_values.end(),
                          [](asc::index_t value) { return value == 0; }));
  }
  for (auto placement : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged,
                         asc::MemorySpace::kPinnedHost}) {
    const auto pivots = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
        pivot_values.data(), 2, 1, backing));
    ASC_DENSE_TEST_EQ(test,
                      asc::Getrf(asc::ExecutionContext::Serial(),
                                 matrix.view(placement), pivots, report)
                          .code(),
                      asc::ErrorCode::kMemoryAccess);
    const auto placed_pivots =
        Take(asc::DenseBlasVectorView<asc::index_t>::Create(
            pivot_values.data(), 2, 1,
            {pivot_values.data(), sizeof(pivot_values), placement}));
    ASC_DENSE_TEST_EQ(test,
                      asc::Getrf(asc::ExecutionContext::Serial(), matrix.view(),
                                 placed_pivots, report)
                          .code(),
                      asc::ErrorCode::kMemoryAccess);
    ASC_DENSE_TEST_EQ(test, matrix.storage(), saved);
    CheckReport(test, report, asc::LapackOutcome::kNotRun,
                asc::LapackOutputValidity::kUnchanged);
  }
}

template <typename Element>
void TestSolveValidation(TestContext& test) {
  Matrix<Element> matrix(2, 2, kLayouts[0]);
  matrix(0, 0) = Element{1};
  matrix(1, 1) = Element{1};
  Pivots pivots(2);
  asc::LapackReport factor_report;
  ASC_DENSE_TEST_CHECK(
      test, asc::Getrf(asc::ExecutionContext::Serial(), matrix.view(),
                       pivots.view(), factor_report)
                .ok());
  const auto factor = Take(asc::LapackLuFactorView<Element>::Create(
      matrix.const_view(), pivots.raw(), factor_report));
  Matrix<Element> rhs(2, 2, kLayouts[0]);
  rhs(0, 0) = Element{3};
  const auto original = rhs.storage();
  const auto packed = matrix.storage();
  asc::LapackReport report = factor_report;
  auto reject = [&](asc::DenseBlasTranspose operation,
                    asc::LapackLuFactorView<Element> input_factor,
                    asc::DenseBlasMatrixView<Element> output,
                    asc::ErrorCode code) {
    asc_dense_test::AllocationProbe probe;
    const auto status = asc::Getrs(asc::ExecutionContext::Serial(), operation,
                                   input_factor, output, report);
    ASC_DENSE_TEST_EQ(test, status.code(), code);
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
    CheckReport(test, report, asc::LapackOutcome::kNotRun,
                asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, rhs.storage(), original);
    ASC_DENSE_TEST_EQ(test, matrix.storage(), packed);
  };
  // Fixed underlying storage permits a deliberate invalid-option input.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  reject(static_cast<asc::DenseBlasTranspose>(255), factor, rhs.view(),
         asc::ErrorCode::kInvalidArgument);
  reject(kOperations[0], factor, matrix.view(),
         asc::ErrorCode::kInvalidArgument);
  Matrix<Element> wrong_shape(3, 1, kLayouts[1]);
  reject(kOperations[0], factor, wrong_shape.view(), asc::ErrorCode::kShape);
  reject(kOperations[0], factor, rhs.view(asc::MemorySpace::kDevice),
         asc::ErrorCode::kMemoryAccess);
  auto foreign_report = factor_report;
  foreign_report.provider.kind = asc::LapackProviderKind::kReference;
  const auto foreign_factor = Take(asc::LapackLuFactorView<Element>::Create(
      matrix.const_view(), pivots.raw(), foreign_report));
  reject(kOperations[0], foreign_factor, rhs.view(),
         asc::ErrorCode::kInvalidState);
  // Deliberate borrowed-buffer invalidation exercises defensive revalidation;
  // these are not valid uses of a successful factor for numerical work.
  for (asc::index_t bad : {-1, 0, 3}) {
    pivots.view().data()[0] = bad;
    reject(kOperations[0], factor, rhs.view(), asc::ErrorCode::kIndex);
  }
  pivots.view().data()[0] = 1;
  pivots.view().data()[1] = 1;
  reject(kOperations[0], factor, rhs.view(), asc::ErrorCode::kIndex);
  pivots.view().data()[1] = 2;
  matrix(1, 1) = Element{0};
  ASC_DENSE_TEST_EQ(test,
                    asc::Getrs(asc::ExecutionContext::Serial(), kOperations[0],
                               factor, rhs.view(), report)
                        .code(),
                    asc::ErrorCode::kNumerical);
  CheckReport(test, report, asc::LapackOutcome::kSingular,
              asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), 1);
  ASC_DENSE_TEST_EQ(test, rhs.storage(), original);
}

template <typename Element>
void TestEmptyAndRectangularSolve(TestContext& test) {
  Matrix<Element> rectangular(2, 3, kLayouts[0]);
  rectangular(0, 0) = Element{1};
  rectangular(1, 1) = Element{1};
  Pivots pivots(2);
  asc::LapackReport factor_report;
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, asc::Getrf(asc::ExecutionContext::Serial(), rectangular.view(),
                       pivots.view(), factor_report)
                .ok());
  const auto rectangular_factor = Take(asc::LapackLuFactorView<Element>::Create(
      rectangular.const_view(), pivots.raw(), factor_report));
  Matrix<Element> rhs(2, 0, kLayouts[1]);
  ASC_DENSE_TEST_EQ(test,
                    asc::Getrs(asc::ExecutionContext::Serial(), kOperations[0],
                               rectangular_factor, rhs.view(), report)
                        .code(),
                    asc::ErrorCode::kShape);
  Matrix<Element> square(2, 2, kLayouts[0]);
  square(0, 0) = Element{1};
  square(1, 1) = Element{1};
  ASC_DENSE_TEST_CHECK(
      test, asc::Getrf(asc::ExecutionContext::Serial(), square.view(),
                       pivots.view(), factor_report)
                .ok());
  const auto square_factor = Take(asc::LapackLuFactorView<Element>::Create(
      square.const_view(), pivots.raw(), factor_report));
  for (auto operation : kOperations) {
    ASC_DENSE_TEST_CHECK(test,
                         asc::Getrs(asc::ExecutionContext::Serial(), operation,
                                    square_factor, rhs.view(), report)
                             .ok());
  }
  rhs.CheckPadding(test);
}

void TestBoundsAndAlias(TestContext& test) {
  std::array<double, 4> values{1, 0, 0, 1};
  const asc::ConstMemoryView backing{values.data(), sizeof(values), kCpu};
  ASC_DENSE_TEST_EQ(test,
                    asc::DenseBlasMatrixView<double>::Create(
                        values.data(), 2, 2, kLayouts[0], 1, backing)
                        .status()
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_CHECK(test, !asc::DenseBlasMatrixView<double>::Create(
                                  values.data(), 2, 2, kLayouts[0], 2,
                                  {values.data(), sizeof(double), kCpu})
                                  .ok());
  ASC_DENSE_TEST_EQ(test,
                    asc::DenseBlasMatrixView<double>::Create(
                        values.data(), 2, 2, kLayouts[0],
                        std::numeric_limits<asc::stride_t>::max(), backing)
                        .status()
                        .code(),
                    asc::ErrorCode::kOverflow);
  auto matrix = Take(asc::DenseBlasMatrixView<double>::Create(
      values.data(), 2, 2, kLayouts[0], 2, backing));
  // Only the checked reachable spans are inspected: no differently typed
  // aliased element is read or written during the rejected call.
  auto aliases = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      reinterpret_cast<asc::index_t*>(values.data()), 2, 1, backing));
  const auto original = values;
  asc::LapackReport report;
  ASC_DENSE_TEST_EQ(
      test,
      asc::Getrf(asc::ExecutionContext::Serial(), matrix, aliases, report)
          .code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, values, original);
  CheckReport(test, report, asc::LapackOutcome::kNotRun,
              asc::LapackOutputValidity::kUnchanged);
  Pivots separate_pivots(2);
  ASC_DENSE_TEST_CHECK(test, asc::Getrf(asc::ExecutionContext::Serial(), matrix,
                                        separate_pivots.view(), report)
                                 .ok());
  const auto reusable = Take(asc::LapackLuFactorView<double>::Create(
      matrix, separate_pivots.raw(), report));
  auto alias_rhs = Take(asc::DenseBlasMatrixView<double>::Create(
      reinterpret_cast<double*>(separate_pivots.view().data()), 2, 1,
      kLayouts[0], 1, separate_pivots.raw().reachable_storage()));
  ASC_DENSE_TEST_EQ(test,
                    asc::Getrs(asc::ExecutionContext::Serial(), kOperations[0],
                               reusable, alias_rhs, report)
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, separate_pivots.raw().values()[0], 1);
  ASC_DENSE_TEST_EQ(test, separate_pivots.raw().values()[1], 2);
}

void TestEmptyHuge(TestContext& test) {
  asc::LapackReport report;
  const auto empty = Take(asc::DenseBlasMatrixView<double>::Create(
      nullptr, 0, 0, kLayouts[0], 1, {nullptr, 0, kCpu}));
  const auto empty_pivots = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      nullptr, 0, 1, {nullptr, 0, kCpu}));
  ASC_DENSE_TEST_CHECK(test, asc::Getrf(asc::ExecutionContext::Serial(), empty,
                                        empty_pivots, report)
                                 .ok());
  const auto raw = Take(
      asc::RawLapackPivotView::Create(nullptr, 0, kLu, {nullptr, 0, kCpu}));
  const auto factor =
      Take(asc::LapackLuFactorView<double>::Create(empty, raw, report));
  auto huge_empty = Take(asc::DenseBlasMatrixView<double>::Create(
      nullptr, 0, std::numeric_limits<asc::extent_t>::max(), kLayouts[1], 1,
      {nullptr, 0, kCpu}));
  ASC_DENSE_TEST_CHECK(test, asc::Getrf(asc::ExecutionContext::Serial(),
                                        huge_empty, empty_pivots, report)
                                 .ok());
  for (auto operation : kOperations) {
    ASC_DENSE_TEST_CHECK(test, asc::Getrs(asc::ExecutionContext::Serial(),
                                          operation, factor, huge_empty, report)
                                   .ok());
  }
}

template <typename Element>
void TestScalar(TestContext& test) {
  TestRectangular<Element>(test);
  TestPivotAndFailure<Element>(test);
  TestSubnormal<Element>(test);
  TestFactorValidation<Element>(test);
  TestSolveValidation<Element>(test);
  TestEmptyAndRectangularSolve<Element>(test);
  using Real = asc::DenseBlasRealType<Element>;
  constexpr int kScale = std::numeric_limits<Real>::max_exponent - 20;
  for (auto factor_layout : kLayouts) {
    for (auto rhs_layout : kLayouts) {
      for (int exponent : {-kScale, 0, kScale}) {
        TestSolveModes<Element>(test, factor_layout, rhs_layout, exponent);
      }
      TestSolveModes<Element>(test, factor_layout, rhs_layout, 0, true);
    }
  }
}

}  // namespace

int main() {
  TestContext test;
  TestScalar<float>(test);
  TestScalar<double>(test);
  TestScalar<std::complex<float>>(test);
  TestScalar<std::complex<double>>(test);
  TestComplexPivot<float>(test);
  TestComplexPivot<double>(test);
  TestBoundsAndAlias(test);
  TestEmptyHuge(test);
  return test.Finish();
}
