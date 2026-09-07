#include <array>
#include <cmath>
#include <complex>
#include <concepts>
#include <cstddef>
#include <limits>
#include <span>

#include "../allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/blas.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "test_support.h"

namespace {

using asc_sparse_test::TestContext;

static_assert(sizeof(asc::index_t) == 8);
static_assert(sizeof(asc::nnz_t) == 8);

template <typename Element, std::size_t Size>
asc::ConstMemoryView Storage(const std::array<Element, Size>& values) {
  return {values.data(), values.size() * sizeof(Element),
          asc::MemorySpace::kHost};
}

template <typename Element, std::size_t Size>
asc::MutableMemoryView Storage(std::array<Element, Size>& values) {
  return {values.data(), values.size() * sizeof(Element),
          asc::MemorySpace::kHost};
}

template <typename Element>
double Distance(Element left, Element right) {
  return static_cast<double>(std::abs(left - right));
}

template <typename Element>
void CheckNear(TestContext& test, Element actual, Element expected) {
  constexpr double kTolerance =
      std::same_as<Element, float> || std::same_as<Element, std::complex<float>>
          ? 2.0e-5
          : 2.0e-12;
  ASC_SPARSE_TEST_CHECK(test, Distance(actual, expected) <= kTolerance);
}

template <typename Element>
Element Make(double real, double imaginary = 0.0) {
  if constexpr (asc::SparseBlasComplex<Element>) {
    return Element{static_cast<typename Element::value_type>(real),
                   static_cast<typename Element::value_type>(imaginary)};
  } else {
    static_cast<void>(imaginary);
    return static_cast<Element>(real);
  }
}

template <typename Element>
Element Conjugate(Element value) {
  if constexpr (asc::SparseBlasComplex<Element>) {
    return std::conj(value);
  } else {
    return value;
  }
}

template <typename Element>
void TestLevelOne(TestContext& test) {  // NOLINT(readability-function-size)
  constexpr std::array<asc::index_t, 2> kIndices{0, 2};
  std::array<Element, 2> sparse_values{Make<Element>(1.0, 2.0),
                                       Make<Element>(-2.0, 1.0)};
  std::array<Element, 3> dense_values{Make<Element>(3.0, -1.0),
                                      Make<Element>(9.0, 4.0),
                                      Make<Element>(2.0, 3.0)};
  auto sparse = asc::SparseBlasIndexedVectorView<Element>::Create(
      kIndices.data(), sparse_values.data(), 2, 3, Storage(kIndices),
      Storage(sparse_values));
  auto dense = asc::SparseBlasVectorView<Element>::Create(
      dense_values.data(), 3, 1, Storage(dense_values));
  ASC_SPARSE_TEST_CHECK(test, sparse.ok() && dense.ok());
  if (!sparse.ok() || !dense.ok()) {
    return;
  }
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const asc::SparseBlasIndexedVectorView<const Element> const_sparse(*sparse);
  const asc::SparseBlasVectorView<const Element> const_dense(*dense);
  auto dot = asc::SparseDot(context, asc::SparseBlasConjugation::kConjugated,
                            const_sparse, const_dense);
  ASC_SPARSE_TEST_CHECK(test, dot.ok());
  if (dot.ok()) {
    const Element expected = Conjugate(sparse_values[0]) * dense_values[0] +
                             Conjugate(sparse_values[1]) * dense_values[2];
    CheckNear(test, *dot, expected);
  }

  auto axpy_values = dense_values;
  auto axpy = asc::SparseBlasVectorView<Element>::Create(
      axpy_values.data(), 3, 1, Storage(axpy_values));
  ASC_SPARSE_TEST_CHECK(test, axpy.ok());
  if (axpy.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test,
        asc::SparseAxpy(context, Make<Element>(2.0), const_sparse, *axpy).ok());
    CheckNear(test, axpy_values[0],
              dense_values[0] + Make<Element>(2.0) * sparse_values[0]);
    CheckNear(test, axpy_values[2],
              dense_values[2] + Make<Element>(2.0) * sparse_values[1]);
  }

  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::array<Element, 2> nan_sparse_values{Make<Element>(nan, nan),
                                           Make<Element>(nan, nan)};
  auto nan_sparse = asc::SparseBlasIndexedVectorView<const Element>::Create(
      kIndices.data(), nan_sparse_values.data(), 2, 3, Storage(kIndices),
      Storage(nan_sparse_values));
  auto zero_axpy_values = dense_values;
  auto zero_axpy = asc::SparseBlasVectorView<Element>::Create(
      zero_axpy_values.data(), 3, 1, Storage(zero_axpy_values));
  ASC_SPARSE_TEST_CHECK(test, nan_sparse.ok() && zero_axpy.ok());
  if (nan_sparse.ok() && zero_axpy.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test,
        asc::SparseAxpy(context, Element{}, *nan_sparse, *zero_axpy).ok());
    for (std::size_t position = 0; position < dense_values.size(); ++position) {
      CheckNear(test, zero_axpy_values[position], dense_values[position]);
    }
  }

  std::array<Element, 2> gathered_values{};
  auto gathered = asc::SparseBlasIndexedVectorView<Element>::Create(
      kIndices.data(), gathered_values.data(), 2, 3, Storage(kIndices),
      Storage(gathered_values));
  ASC_SPARSE_TEST_CHECK(test, gathered.ok());
  if (gathered.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::SparseGather(context, const_dense, *gathered).ok());
    CheckNear(test, gathered_values[0], dense_values[0]);
    CheckNear(test, gathered_values[1], dense_values[2]);
  }

  auto gather_zero_dense_values = dense_values;
  std::array<Element, 2> gather_zero_values{};
  auto gather_zero_dense = asc::SparseBlasVectorView<Element>::Create(
      gather_zero_dense_values.data(), 3, 1, Storage(gather_zero_dense_values));
  auto gather_zero_sparse = asc::SparseBlasIndexedVectorView<Element>::Create(
      kIndices.data(), gather_zero_values.data(), 2, 3, Storage(kIndices),
      Storage(gather_zero_values));
  ASC_SPARSE_TEST_CHECK(test,
                        gather_zero_dense.ok() && gather_zero_sparse.ok());
  if (gather_zero_dense.ok() && gather_zero_sparse.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test,
        asc::SparseGatherZero(context, *gather_zero_dense, *gather_zero_sparse)
            .ok());
    CheckNear(test, gather_zero_values[0], dense_values[0]);
    CheckNear(test, gather_zero_values[1], dense_values[2]);
    CheckNear(test, gather_zero_dense_values[0], Element{});
    CheckNear(test, gather_zero_dense_values[2], Element{});
  }

  std::array<Element, 3> scattered_values{};
  auto scattered = asc::SparseBlasVectorView<Element>::Create(
      scattered_values.data(), 3, 1, Storage(scattered_values));
  ASC_SPARSE_TEST_CHECK(test, scattered.ok());
  if (scattered.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::SparseScatter(context, const_sparse, *scattered).ok());
    CheckNear(test, scattered_values[0], sparse_values[0]);
    CheckNear(test, scattered_values[1], Element{});
    CheckNear(test, scattered_values[2], sparse_values[1]);
  }
}

template <typename Element>
// Level-two and level-three operations share one sparse matrix oracle.
// NOLINTNEXTLINE(readability-function-size)
void TestLevelTwoAndThree(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape{2, 2};
  constexpr std::array<asc::nnz_t, 3> kOffsets{0, 1, 3};
  constexpr std::array<asc::index_t, 3> kIndices{0, 0, 1};
  std::array<Element, 3> values{Make<Element>(2.0, 1.0),
                                Make<Element>(1.0, -1.0),
                                Make<Element>(3.0, 0.5)};
  auto matrix = asc::CsrView<const Element>::Create(
      kOffsets.data(), kIndices.data(), values.data(), kShape, 3,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, matrix.ok());
  if (!matrix.ok()) {
    return;
  }
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  std::array<Element, 2> input_values{Make<Element>(1.0, 2.0),
                                      Make<Element>(-1.0, 1.0)};
  std::array<Element, 2> output_values{};
  auto input = asc::SparseBlasVectorView<const Element>::Create(
      input_values.data(), 2, 1, Storage(input_values));
  auto output = asc::SparseBlasVectorView<Element>::Create(
      output_values.data(), 2, 1, Storage(output_values));
  ASC_SPARSE_TEST_CHECK(test, input.ok() && output.ok());
  if (input.ok() && output.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::Spmv(context, asc::SparseBlasTranspose::kNone,
                        Make<Element>(1.0), *matrix, *input, *output)
                  .ok());
    CheckNear(test, output_values[0], values[0] * input_values[0]);
    CheckNear(test, output_values[1],
              values[1] * input_values[0] + values[2] * input_values[1]);
  }

  constexpr std::array<asc::nnz_t, 3> kCscOffsets{0, 2, 3};
  constexpr std::array<asc::index_t, 3> kCscIndices{0, 1, 1};
  std::array<Element, 3> csc_values{values[0], values[1], values[2]};
  auto csc = asc::CscView<const Element>::Create(
      kCscOffsets.data(), kCscIndices.data(), csc_values.data(), kShape, 3,
      asc::MemorySpace::kHost);
  std::array<Element, 2> csc_output_values{};
  auto csc_output = asc::SparseBlasVectorView<Element>::Create(
      csc_output_values.data(), 2, 1, Storage(csc_output_values));
  ASC_SPARSE_TEST_CHECK(test, csc.ok() && csc_output.ok());
  if (csc.ok() && input.ok() && csc_output.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::Spmv(context, asc::SparseBlasTranspose::kNone,
                        Make<Element>(1.0), *csc, *input, *csc_output)
                  .ok());
    CheckNear(test, csc_output_values[0], values[0] * input_values[0]);
    CheckNear(test, csc_output_values[1],
              values[1] * input_values[0] + values[2] * input_values[1]);
  }

  std::array<Element, 2> transposed_output_values{};
  auto transposed_output = asc::SparseBlasVectorView<Element>::Create(
      transposed_output_values.data(), 2, 1, Storage(transposed_output_values));
  ASC_SPARSE_TEST_CHECK(test, transposed_output.ok());
  if (input.ok() && transposed_output.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::Spmv(context, asc::SparseBlasTranspose::kConjugateTranspose,
                        Make<Element>(1.0), *matrix, *input, *transposed_output)
                  .ok());
    const Element first = Conjugate(values[0]) * input_values[0] +
                          Conjugate(values[1]) * input_values[1];
    const Element second = Conjugate(values[2]) * input_values[1];
    CheckNear(test, transposed_output_values[0], first);
    CheckNear(test, transposed_output_values[1], second);
  }

  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::array<Element, 3> nan_values{Make<Element>(nan, nan),
                                    Make<Element>(nan, nan),
                                    Make<Element>(nan, nan)};
  auto nan_matrix = asc::CsrView<const Element>::Create(
      kOffsets.data(), kIndices.data(), nan_values.data(), kShape, 3,
      asc::MemorySpace::kHost);
  std::array<Element, 2> zero_spmv_values{Make<Element>(7.0),
                                          Make<Element>(9.0)};
  auto zero_spmv = asc::SparseBlasVectorView<Element>::Create(
      zero_spmv_values.data(), 2, 1, Storage(zero_spmv_values));
  ASC_SPARSE_TEST_CHECK(test, nan_matrix.ok() && zero_spmv.ok());
  if (nan_matrix.ok() && input.ok() && zero_spmv.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::Spmv(context, asc::SparseBlasTranspose::kNone, Element{},
                        *nan_matrix, *input, *zero_spmv)
                  .ok());
    CheckNear(test, zero_spmv_values[0], Make<Element>(7.0));
    CheckNear(test, zero_spmv_values[1], Make<Element>(9.0));
  }

  std::array<Element, 4> rhs_values{Make<Element>(1.0), Make<Element>(2.0),
                                    Make<Element>(3.0), Make<Element>(4.0)};
  std::array<Element, 4> product_values{};
  auto rhs = asc::SparseBlasMatrixView<const Element>::Create(
      rhs_values.data(), 2, 2, asc::SparseBlasLayout::kRowMajor, 2,
      Storage(rhs_values));
  auto product = asc::SparseBlasMatrixView<Element>::Create(
      product_values.data(), 2, 2, asc::SparseBlasLayout::kRowMajor, 2,
      Storage(product_values));
  ASC_SPARSE_TEST_CHECK(test, rhs.ok() && product.ok());
  if (rhs.ok() && product.ok()) {
    ASC_SPARSE_TEST_CHECK(test,
                          asc::Spmm(context, asc::SparseBlasTranspose::kNone,
                                    Make<Element>(1.0), *matrix, *rhs, *product)
                              .ok());
    CheckNear(test, product_values[0], values[0] * rhs_values[0]);
    CheckNear(test, product_values[1], values[0] * rhs_values[1]);
    CheckNear(test, product_values[2],
              values[1] * rhs_values[0] + values[2] * rhs_values[2]);
    CheckNear(test, product_values[3],
              values[1] * rhs_values[1] + values[2] * rhs_values[3]);
  }

  std::array<Element, 4> column_rhs_values{
      Make<Element>(1.0), Make<Element>(3.0), Make<Element>(2.0),
      Make<Element>(4.0)};
  std::array<Element, 4> column_product_values{};
  auto column_rhs = asc::SparseBlasMatrixView<const Element>::Create(
      column_rhs_values.data(), 2, 2, asc::SparseBlasLayout::kColumnMajor, 2,
      Storage(column_rhs_values));
  auto column_product = asc::SparseBlasMatrixView<Element>::Create(
      column_product_values.data(), 2, 2, asc::SparseBlasLayout::kColumnMajor,
      2, Storage(column_product_values));
  ASC_SPARSE_TEST_CHECK(test, column_rhs.ok() && column_product.ok());
  if (csc.ok() && column_rhs.ok() && column_product.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::Spmm(context, asc::SparseBlasTranspose::kConjugateTranspose,
                        Make<Element>(1.0), *csc, *column_rhs, *column_product)
                  .ok());
    CheckNear(test, column_product_values[0],
              Conjugate(values[0]) * column_rhs_values[0] +
                  Conjugate(values[1]) * column_rhs_values[1]);
    CheckNear(test, column_product_values[1],
              Conjugate(values[2]) * column_rhs_values[1]);
    CheckNear(test, column_product_values[2],
              Conjugate(values[0]) * column_rhs_values[2] +
                  Conjugate(values[1]) * column_rhs_values[3]);
    CheckNear(test, column_product_values[3],
              Conjugate(values[2]) * column_rhs_values[3]);
  }
  std::array<Element, 4> zero_spmm_values{
      Make<Element>(5.0), Make<Element>(6.0), Make<Element>(7.0),
      Make<Element>(8.0)};
  auto zero_spmm = asc::SparseBlasMatrixView<Element>::Create(
      zero_spmm_values.data(), 2, 2, asc::SparseBlasLayout::kRowMajor, 2,
      Storage(zero_spmm_values));
  ASC_SPARSE_TEST_CHECK(test, zero_spmm.ok());
  if (nan_matrix.ok() && rhs.ok() && zero_spmm.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::Spmm(context, asc::SparseBlasTranspose::kNone, Element{},
                        *nan_matrix, *rhs, *zero_spmm)
                  .ok());
    for (std::size_t position = 0; position < zero_spmm_values.size();
         ++position) {
      CheckNear(test, zero_spmm_values[position],
                Make<Element>(5.0 + static_cast<double>(position)));
    }
  }

  auto triangular =
      asc::SparseBlasTriangularView<Element,
                                    asc::SparseCompressedFormat::kCsr>::
          Create(*matrix, asc::SparseBlasTriangle::kLower,
                 asc::SparseBlasDiagonal::kNonUnit);
  ASC_SPARSE_TEST_CHECK(test, triangular.ok());
  if (!triangular.ok()) {
    return;
  }
  std::array<Element, 2> solution{
      values[0] * Make<Element>(2.0),
      values[1] * Make<Element>(2.0) + values[2] * Make<Element>(-1.0)};
  auto solution_view = asc::SparseBlasVectorView<Element>::Create(
      solution.data(), 2, 1, Storage(solution));
  ASC_SPARSE_TEST_CHECK(test, solution_view.ok());
  if (solution_view.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::SparseTriangularSolve(
                  context, asc::SparseBlasTranspose::kNone, Make<Element>(1.0),
                  *triangular, *solution_view)
                  .ok());
    CheckNear(test, solution[0], Make<Element>(2.0));
    CheckNear(test, solution[1], Make<Element>(-1.0));
  }

  if (csc.ok()) {
    auto csc_triangular =
        asc::SparseBlasTriangularView<Element,
                                      asc::SparseCompressedFormat::kCsc>::
            Create(*csc, asc::SparseBlasTriangle::kLower,
                   asc::SparseBlasDiagonal::kNonUnit);
    std::array<Element, 2> transposed_solution{
        Conjugate(values[0]) * Make<Element>(2.0) - Conjugate(values[1]),
        -Conjugate(values[2])};
    auto transposed_solution_view = asc::SparseBlasVectorView<Element>::Create(
        transposed_solution.data(), 2, 1, Storage(transposed_solution));
    ASC_SPARSE_TEST_CHECK(test,
                          csc_triangular.ok() && transposed_solution_view.ok());
    if (csc_triangular.ok() && transposed_solution_view.ok()) {
      ASC_SPARSE_TEST_CHECK(
          test,
          asc::SparseTriangularSolve(
              context, asc::SparseBlasTranspose::kConjugateTranspose,
              Make<Element>(1.0), *csc_triangular, *transposed_solution_view)
              .ok());
      CheckNear(test, transposed_solution[0], Make<Element>(2.0));
      CheckNear(test, transposed_solution[1], Make<Element>(-1.0));
    }
  }

  std::array<Element, 4> multiple{
      values[0] * Make<Element>(2.0), values[0] * Make<Element>(1.0),
      values[1] * Make<Element>(2.0) + values[2] * Make<Element>(-1.0),
      values[1] * Make<Element>(1.0) + values[2] * Make<Element>(3.0)};
  auto multiple_view = asc::SparseBlasMatrixView<Element>::Create(
      multiple.data(), 2, 2, asc::SparseBlasLayout::kRowMajor, 2,
      Storage(multiple));
  ASC_SPARSE_TEST_CHECK(test, multiple_view.ok());
  if (multiple_view.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::SparseTriangularSolveMultiple(
                  context, asc::SparseBlasTranspose::kNone, Make<Element>(1.0),
                  *triangular, *multiple_view)
                  .ok());
    CheckNear(test, multiple[0], Make<Element>(2.0));
    CheckNear(test, multiple[1], Make<Element>(1.0));
    CheckNear(test, multiple[2], Make<Element>(-1.0));
    CheckNear(test, multiple[3], Make<Element>(3.0));
  }

  std::array<Element, 3> zero_solve_values{
      Make<Element>(2.0), Make<Element>(nan, nan), Make<Element>(3.0)};
  auto zero_solve_matrix = asc::CsrView<const Element>::Create(
      kOffsets.data(), kIndices.data(), zero_solve_values.data(), kShape, 3,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, zero_solve_matrix.ok());
  if (zero_solve_matrix.ok()) {
    auto zero_solve_triangular =
        asc::SparseBlasTriangularView<Element,
                                      asc::SparseCompressedFormat::kCsr>::
            Create(*zero_solve_matrix, asc::SparseBlasTriangle::kLower,
                   asc::SparseBlasDiagonal::kNonUnit);
    std::array<Element, 2> zero_solution_values{Make<Element>(4.0),
                                                Make<Element>(5.0)};
    auto zero_solution = asc::SparseBlasVectorView<Element>::Create(
        zero_solution_values.data(), 2, 1, Storage(zero_solution_values));
    ASC_SPARSE_TEST_CHECK(test,
                          zero_solve_triangular.ok() && zero_solution.ok());
    if (zero_solve_triangular.ok() && zero_solution.ok()) {
      ASC_SPARSE_TEST_CHECK(
          test, asc::SparseTriangularSolve(
                    context, asc::SparseBlasTranspose::kNone, Element{},
                    *zero_solve_triangular, *zero_solution)
                    .ok());
      CheckNear(test, zero_solution_values[0], Element{});
      CheckNear(test, zero_solution_values[1], Element{});
    }
  }
}

// Validation and rollback checks intentionally share their fixture state.
// NOLINTNEXTLINE(readability-function-size)
void TestInvalidAndEdgeCases(TestContext& test) {
  constexpr std::array<asc::index_t, 2> kUnsorted{2, 1};
  std::array<double, 2> values{1.0, 2.0};
  auto invalid = asc::SparseBlasIndexedVectorView<double>::Create(
      kUnsorted.data(), values.data(), 2, 3, Storage(kUnsorted),
      Storage(values));
  ASC_SPARSE_TEST_CHECK(test, !invalid.ok());
  if (!invalid.ok()) {
    ASC_SPARSE_TEST_EQ(test, invalid.status().code(), asc::ErrorCode::kIndex);
  }

  constexpr std::array<asc::index_t, 1> kOneIndex{0};
  std::array<double, 1> one_sparse_value{2.0};
  std::array<double, 1> one_dense_value{3.0};
  auto one_sparse = asc::SparseBlasIndexedVectorView<const double>::Create(
      kOneIndex.data(), one_sparse_value.data(), 1, 1, Storage(kOneIndex),
      Storage(one_sparse_value));
  auto one_dense = asc::SparseBlasVectorView<const double>::Create(
      one_dense_value.data(), 1, 1, Storage(one_dense_value));
  ASC_SPARSE_TEST_CHECK(test, one_sparse.ok() && one_dense.ok());
  if (one_sparse.ok() && one_dense.ok()) {
    const auto invalid_conjugation_value =
        static_cast<asc::SparseBlasConjugation>(
            255);  // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    auto invalid_conjugation =
        asc::SparseDot(asc::ExecutionContext::Serial(),
                       invalid_conjugation_value, *one_sparse, *one_dense);
    ASC_SPARSE_TEST_CHECK(test, !invalid_conjugation.ok());
    if (!invalid_conjugation.ok()) {
      ASC_SPARSE_TEST_EQ(test, invalid_conjugation.status().code(),
                         asc::ErrorCode::kInvalidArgument);
    }
  }

  constexpr std::array<asc::extent_t, 2> kShape{2, 2};
  constexpr std::array<asc::nnz_t, 3> kOffsets{0, 1, 2};
  constexpr std::array<asc::index_t, 2> kIndices{0, 1};
  std::array<double, 2> singular_values{1.0, 0.0};
  auto singular = asc::CsrView<const double>::Create(
      kOffsets.data(), kIndices.data(), singular_values.data(), kShape, 2,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, singular.ok());
  if (singular.ok()) {
    // The invalid enumerator is the input under test.
    // NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
    const auto invalid_triangle_value =
        static_cast<asc::SparseBlasTriangle>(255);
    // NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)
    auto invalid_triangle =
        asc::SparseBlasTriangularView<double,
                                      asc::SparseCompressedFormat::kCsr>::
            Create(*singular, invalid_triangle_value,
                   asc::SparseBlasDiagonal::kUnit);
    ASC_SPARSE_TEST_CHECK(test, !invalid_triangle.ok());
    const auto invalid_diagonal_value = static_cast<asc::SparseBlasDiagonal>(
        255);  // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    auto invalid_diagonal =
        asc::SparseBlasTriangularView<double,
                                      asc::SparseCompressedFormat::kCsr>::
            Create(*singular, asc::SparseBlasTriangle::kLower,
                   invalid_diagonal_value);
    ASC_SPARSE_TEST_CHECK(test, !invalid_diagonal.ok());

    auto triangular =
        asc::SparseBlasTriangularView<double,
                                      asc::SparseCompressedFormat::kCsr>::
            Create(*singular, asc::SparseBlasTriangle::kLower,
                   asc::SparseBlasDiagonal::kNonUnit);
    ASC_SPARSE_TEST_CHECK(test, !triangular.ok());

    std::array<double, 2> input_values{1.0, 2.0};
    std::array<double, 2> output_values{7.0, 9.0};
    auto input = asc::SparseBlasVectorView<const double>::Create(
        input_values.data(), 2, 1, Storage(input_values));
    auto output = asc::SparseBlasVectorView<double>::Create(
        output_values.data(), 2, 1, Storage(output_values));
    ASC_SPARSE_TEST_CHECK(test, input.ok() && output.ok());
    if (input.ok() && output.ok()) {
      const auto invalid_transpose_value =
          static_cast<asc::SparseBlasTranspose>(
              255);  // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
      const asc::Status invalid_transpose =
          asc::Spmv(asc::ExecutionContext::Serial(), invalid_transpose_value,
                    1.0, *singular, *input, *output);
      ASC_SPARSE_TEST_CHECK(test, !invalid_transpose.ok());
      ASC_SPARSE_TEST_EQ(test, invalid_transpose.code(),
                         asc::ErrorCode::kInvalidArgument);
      ASC_SPARSE_TEST_EQ(test, output_values[0], 7.0);
      ASC_SPARSE_TEST_EQ(test, output_values[1], 9.0);
    }
  }

  constexpr std::array<asc::index_t, 0> kNoIndices{};
  std::array<double, 0> no_values{};
  std::array<double, 0> no_dense{};
  auto empty_sparse = asc::SparseBlasIndexedVectorView<const double>::Create(
      kNoIndices.data(), no_values.data(), 0, 0, Storage(kNoIndices),
      Storage(no_values));
  auto empty_dense = asc::SparseBlasVectorView<const double>::Create(
      no_dense.data(), 0, -1, Storage(no_dense));
  ASC_SPARSE_TEST_CHECK(test, empty_sparse.ok() && empty_dense.ok());
  if (empty_sparse.ok() && empty_dense.ok()) {
    auto result = asc::SparseDot(asc::ExecutionContext::Serial(),
                                 asc::SparseBlasConjugation::kUnconjugated,
                                 *empty_sparse, *empty_dense);
    ASC_SPARSE_TEST_CHECK(test, result.ok() && *result == 0.0);
  }

  constexpr std::array<asc::index_t, 1> kMiddleIndex{1};
  std::array<double, 1> middle_value{2.0};
  std::array<double, 3> reversed_storage{3.0, 5.0, 7.0};
  auto middle = asc::SparseBlasIndexedVectorView<const double>::Create(
      kMiddleIndex.data(), middle_value.data(), 1, 3, Storage(kMiddleIndex),
      Storage(middle_value));
  auto reversed = asc::SparseBlasVectorView<const double>::Create(
      reversed_storage.data() + 2, 3, -1, Storage(reversed_storage));
  ASC_SPARSE_TEST_CHECK(test, middle.ok() && reversed.ok());
  if (middle.ok() && reversed.ok()) {
    auto result = asc::SparseDot(asc::ExecutionContext::Serial(),
                                 asc::SparseBlasConjugation::kUnconjugated,
                                 *middle, *reversed);
    ASC_SPARSE_TEST_CHECK(test, result.ok());
    if (result.ok()) {
      ASC_SPARSE_TEST_EQ(test, *result, 10.0);
    }
  }
}

void TestAllocationFree(TestContext& test) {
  constexpr std::array<asc::index_t, 1> kIndices{0};
  std::array<double, 1> sparse_values{2.0};
  std::array<double, 1> dense_values{3.0};
  auto sparse = asc::SparseBlasIndexedVectorView<const double>::Create(
      kIndices.data(), sparse_values.data(), 1, 1, Storage(kIndices),
      Storage(sparse_values));
  auto dense = asc::SparseBlasVectorView<const double>::Create(
      dense_values.data(), 1, 1, Storage(dense_values));
  ASC_SPARSE_TEST_CHECK(test, sparse.ok() && dense.ok());
  if (!sparse.ok() || !dense.ok()) {
    return;
  }
  std::size_t allocations = 0;
  {
    asc_sparse_test::AllocationProbe probe;
    auto result = asc::SparseDot(asc::ExecutionContext::Serial(),
                                 asc::SparseBlasConjugation::kUnconjugated,
                                 *sparse, *dense);
    ASC_SPARSE_TEST_CHECK(test, result.ok());
    allocations = probe.count();
  }
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(allocations, 0));
}

}  // namespace

int main() {
  TestContext test;
  TestLevelOne<float>(test);
  TestLevelOne<double>(test);
  TestLevelOne<std::complex<float>>(test);
  TestLevelOne<std::complex<double>>(test);
  TestLevelTwoAndThree<float>(test);
  TestLevelTwoAndThree<double>(test);
  TestLevelTwoAndThree<std::complex<float>>(test);
  TestLevelTwoAndThree<std::complex<double>>(test);
  TestInvalidAndEdgeCases(test);
  TestAllocationFree(test);
  return test.Finish();
}
