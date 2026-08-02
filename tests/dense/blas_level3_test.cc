#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <span>
#include <type_traits>

#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;

template <typename Element>
using Real = asc::DenseBlasRealType<Element>;

template <typename Element>
Element Value(Real<Element> real, Real<Element> imaginary = Real<Element>{0}) {
  if constexpr (asc::DenseBlasComplex<Element>) {
    return Element{real, imaginary};
  } else {
    static_cast<void>(imaginary);
    return real;
  }
}

template <typename Element, std::size_t Extent>
asc::ConstMemoryView Storage(std::array<Element, Extent>& storage,
                             asc::MemorySpace space = asc::MemorySpace::kHost) {
  return asc::ConstMemoryView(storage.data(), sizeof(storage), space);
}

template <typename Element>
asc::DenseBlasMatrixView<Element> MakeMatrix(Element* data, asc::extent_t rows,
                                             asc::extent_t columns,
                                             asc::DenseBlasLayout layout,
                                             asc::stride_t leading_dimension,
                                             asc::ConstMemoryView storage) {
  auto result = asc::DenseBlasMatrixView<Element>::Create(
      data, rows, columns, layout, leading_dimension, storage);
  if (!result.ok()) {
    std::abort();
  }
  return *result;
}

template <typename Element>
Element& At(asc::DenseBlasMatrixView<Element> matrix, asc::index_t row,
            asc::index_t column) {
  const asc::stride_t offset =
      matrix.layout() == asc::DenseBlasLayout::kColumnMajor
          ? column * matrix.leading_dimension() + row
          : row * matrix.leading_dimension() + column;
  return matrix.data()[offset];
}

template <typename Element>
Element Conjugate(Element value) {
  if constexpr (asc::DenseBlasComplex<Element>) {
    return std::conj(value);
  } else {
    return value;
  }
}

template <typename Element>
Element OpAt(asc::DenseBlasMatrixView<const Element> matrix,
             asc::DenseBlasTranspose transpose, asc::index_t row,
             asc::index_t column) {
  if (transpose == asc::DenseBlasTranspose::kNone) {
    return At(matrix, row, column);
  }
  const Element value = At(matrix, column, row);
  return transpose == asc::DenseBlasTranspose::kConjugateTranspose
             ? Conjugate(value)
             : value;
}

template <typename Element>
bool Near(Element actual, Element expected,
          Real<Element> scale = Real<Element>{1}) {
  const Real<Element> tolerance =
      Real<Element>{768} * std::numeric_limits<Real<Element>>::epsilon() *
      scale * (Real<Element>{1} + std::abs(expected));
  return std::abs(actual - expected) <= tolerance;
}

template <typename Element>
void Fill(asc::DenseBlasMatrixView<Element> matrix, Real<Element> seed) {
  for (asc::index_t row = 0; row < matrix.rows(); ++row) {
    for (asc::index_t column = 0; column < matrix.columns(); ++column) {
      At(matrix, row, column) =
          Value<Element>(seed + Real<Element>{2} * row + column,
                         Real<Element>{0.25} * (row - column));
    }
  }
}

template <typename Element>
void CheckMatrix(TestContext& test, asc::DenseBlasMatrixView<Element> actual,
                 asc::DenseBlasMatrixView<const Element> expected,
                 Real<Element> scale = Real<Element>{1}) {
  for (asc::index_t row = 0; row < actual.rows(); ++row) {
    for (asc::index_t column = 0; column < actual.columns(); ++column) {
      ASC_DENSE_TEST_CHECK(test, Near(At(actual, row, column),
                                      At(expected, row, column), scale));
    }
  }
}

template <typename Element>
void TestGemm(TestContext& test) {
  const auto context = asc::ExecutionContext::Serial();
  for (asc::DenseBlasLayout layout :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    for (asc::DenseBlasTranspose left_operation :
         {asc::DenseBlasTranspose::kNone,
          asc::DenseBlasTranspose::kConjugateTranspose}) {
      for (asc::DenseBlasTranspose right_operation :
           {asc::DenseBlasTranspose::kNone,
            asc::DenseBlasTranspose::kConjugateTranspose}) {
        const asc::extent_t left_rows =
            left_operation == asc::DenseBlasTranspose::kNone ? 2 : 3;
        const asc::extent_t left_columns =
            left_operation == asc::DenseBlasTranspose::kNone ? 3 : 2;
        std::array<Element, 32> left_storage{};
        std::array<Element, 32> right_storage{};
        std::array<Element, 32> output_storage{};
        std::array<Element, 32> expected_storage{};
        auto left = MakeMatrix(left_storage.data(), left_rows, left_columns,
                               layout, 5, Storage(left_storage));
        const asc::extent_t right_rows =
            right_operation == asc::DenseBlasTranspose::kNone ? 3 : 4;
        const asc::extent_t right_columns =
            right_operation == asc::DenseBlasTranspose::kNone ? 4 : 3;
        auto right = MakeMatrix(right_storage.data(), right_rows, right_columns,
                                layout, 5, Storage(right_storage));
        auto output = MakeMatrix(output_storage.data(), 2, 4, layout, 6,
                                 Storage(output_storage));
        auto expected = MakeMatrix(expected_storage.data(), 2, 4, layout, 6,
                                   Storage(expected_storage));
        Fill(left, Real<Element>{1});
        Fill(right, Real<Element>{-2});
        Fill(output, Real<Element>{3});
        Fill(expected, Real<Element>{3});
        for (asc::index_t row = 0; row < 2; ++row) {
          for (asc::index_t column = 0; column < 4; ++column) {
            Element product{};
            for (asc::index_t inner = 0; inner < 3; ++inner) {
              product += OpAt(asc::DenseBlasMatrixView<const Element>(left),
                              left_operation, row, inner) *
                         OpAt(asc::DenseBlasMatrixView<const Element>(right),
                              right_operation, inner, column);
            }
            At(expected, row, column) =
                Value<Element>(Real<Element>{1.5}) * product -
                Value<Element>(Real<Element>{0.5}) * At(expected, row, column);
          }
        }
        ASC_DENSE_TEST_CHECK(
            test, asc::Gemm(context, left_operation, right_operation,
                            Value<Element>(Real<Element>{1.5}),
                            asc::DenseBlasMatrixView<const Element>(left),
                            asc::DenseBlasMatrixView<const Element>(right),
                            Value<Element>(Real<Element>{-0.5}), output)
                      .ok());
        CheckMatrix(test, output,
                    asc::DenseBlasMatrixView<const Element>(expected),
                    Real<Element>{20});
      }
    }
  }
}

template <typename Element>
Element StructuredAt(asc::DenseBlasMatrixView<const Element> matrix,
                     asc::DenseBlasTriangle triangle, bool hermitian,
                     asc::index_t row, asc::index_t column) {
  if (row == column && hermitian) {
    return Value<Element>(std::real(At(matrix, row, column)));
  }
  const bool stored = triangle == asc::DenseBlasTriangle::kUpper
                          ? row <= column
                          : row >= column;
  const Element value =
      stored ? At(matrix, row, column) : At(matrix, column, row);
  return !stored && hermitian ? Conjugate(value) : value;
}

template <typename Element>
void TestStructured(TestContext& test) {
  const auto context = asc::ExecutionContext::Serial();
  for (asc::DenseBlasLayout layout :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    for (asc::DenseBlasSide side :
         {asc::DenseBlasSide::kLeft, asc::DenseBlasSide::kRight}) {
      constexpr asc::DenseBlasTriangle kTriangle =
          asc::DenseBlasTriangle::kUpper;
      const asc::extent_t order = side == asc::DenseBlasSide::kLeft ? 2 : 3;
      std::array<Element, 32> structured_storage{};
      std::array<Element, 32> other_storage{};
      std::array<Element, 32> output_storage{};
      std::array<Element, 32> expected_storage{};
      auto structured = MakeMatrix(structured_storage.data(), order, order,
                                   layout, 4, Storage(structured_storage));
      auto other = MakeMatrix(other_storage.data(), 2, 3, layout, 5,
                              Storage(other_storage));
      auto output = MakeMatrix(output_storage.data(), 2, 3, layout, 5,
                               Storage(output_storage));
      auto expected = MakeMatrix(expected_storage.data(), 2, 3, layout, 5,
                                 Storage(expected_storage));
      Fill(structured, Real<Element>{2});
      Fill(other, Real<Element>{-1});
      Fill(output, Real<Element>{0.5});
      Fill(expected, Real<Element>{0.5});
      auto calculate_expected = [&](bool hermitian) {
        for (asc::index_t row = 0; row < 2; ++row) {
          for (asc::index_t column = 0; column < 3; ++column) {
            Element sum{};
            for (asc::index_t inner = 0; inner < order; ++inner) {
              sum +=
                  side == asc::DenseBlasSide::kLeft
                      ? StructuredAt(
                            asc::DenseBlasMatrixView<const Element>(structured),
                            kTriangle, hermitian, row, inner) *
                            At(other, inner, column)
                      : At(other, row, inner) *
                            StructuredAt(
                                asc::DenseBlasMatrixView<const Element>(
                                    structured),
                                kTriangle, hermitian, inner, column);
            }
            At(expected, row, column) =
                Value<Element>(Real<Element>{1.25}) * sum +
                Value<Element>(Real<Element>{0.5}) * At(expected, row, column);
          }
        }
      };
      calculate_expected(false);
      ASC_DENSE_TEST_CHECK(
          test, asc::Symm(context, side, kTriangle,
                          Value<Element>(Real<Element>{1.25}),
                          asc::DenseBlasMatrixView<const Element>(structured),
                          asc::DenseBlasMatrixView<const Element>(other),
                          Value<Element>(Real<Element>{0.5}), output)
                    .ok());
      CheckMatrix(test, output,
                  asc::DenseBlasMatrixView<const Element>(expected),
                  Real<Element>{15});
      if constexpr (asc::DenseBlasComplex<Element>) {
        Fill(output, Real<Element>{0.5});
        Fill(expected, Real<Element>{0.5});
        calculate_expected(true);
        ASC_DENSE_TEST_CHECK(
            test, asc::Hemm(context, side, kTriangle,
                            Value<Element>(Real<Element>{1.25}),
                            asc::DenseBlasMatrixView<const Element>(structured),
                            asc::DenseBlasMatrixView<const Element>(other),
                            Value<Element>(Real<Element>{0.5}), output)
                      .ok());
        CheckMatrix(test, output,
                    asc::DenseBlasMatrixView<const Element>(expected),
                    Real<Element>{15});
      }
    }
  }
}

template <typename Element>
void TestRankK(TestContext& test) {
  const auto context = asc::ExecutionContext::Serial();
  for (asc::DenseBlasLayout layout :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    for (asc::DenseBlasTranspose transpose :
         {asc::DenseBlasTranspose::kNone,
          asc::DenseBlasTranspose::kConjugateTranspose}) {
      const asc::extent_t rows =
          transpose == asc::DenseBlasTranspose::kNone ? 3 : 2;
      const asc::extent_t columns =
          transpose == asc::DenseBlasTranspose::kNone ? 2 : 3;
      std::array<Element, 32> left_storage{};
      std::array<Element, 32> right_storage{};
      std::array<Element, 32> output_storage{};
      auto left = MakeMatrix(left_storage.data(), rows, columns, layout, 5,
                             Storage(left_storage));
      auto right = MakeMatrix(right_storage.data(), rows, columns, layout, 5,
                              Storage(right_storage));
      auto output = MakeMatrix(output_storage.data(), 3, 3, layout, 5,
                               Storage(output_storage));
      Fill(left, Real<Element>{1});
      Fill(right, Real<Element>{-2});
      Fill(output, Real<Element>{0.25});
      const auto triangle = layout == asc::DenseBlasLayout::kColumnMajor
                                ? asc::DenseBlasTriangle::kUpper
                                : asc::DenseBlasTriangle::kLower;
      std::array<Element, 9> original{};
      for (asc::index_t row = 0; row < 3; ++row) {
        for (asc::index_t column = 0; column < 3; ++column) {
          original[static_cast<std::size_t>(row * 3 + column)] =
              At(output, row, column);
        }
      }
      const auto effective_transpose =
          transpose == asc::DenseBlasTranspose::kConjugateTranspose
              ? asc::DenseBlasTranspose::kTranspose
              : transpose;
      ASC_DENSE_TEST_CHECK(
          test, asc::Syr2k(context, triangle, effective_transpose,
                           Value<Element>(Real<Element>{0.75}),
                           asc::DenseBlasMatrixView<const Element>(left),
                           asc::DenseBlasMatrixView<const Element>(right),
                           Value<Element>(Real<Element>{-0.5}), output)
                    .ok());
      for (asc::index_t row = 0; row < 3; ++row) {
        for (asc::index_t column = 0; column < 3; ++column) {
          const bool stored = triangle == asc::DenseBlasTriangle::kUpper
                                  ? row <= column
                                  : row >= column;
          if (!stored) {
            ASC_DENSE_TEST_EQ(
                test, At(output, row, column),
                original[static_cast<std::size_t>(row * 3 + column)]);
            continue;
          }
          Element sum{};
          for (asc::index_t inner = 0; inner < 2; ++inner) {
            sum += OpAt(asc::DenseBlasMatrixView<const Element>(left),
                        effective_transpose, row, inner) *
                       OpAt(asc::DenseBlasMatrixView<const Element>(right),
                            effective_transpose, column, inner) +
                   OpAt(asc::DenseBlasMatrixView<const Element>(right),
                        effective_transpose, row, inner) *
                       OpAt(asc::DenseBlasMatrixView<const Element>(left),
                            effective_transpose, column, inner);
          }
          const Element expected =
              Value<Element>(Real<Element>{0.75}) * sum -
              Value<Element>(Real<Element>{0.5}) *
                  original[static_cast<std::size_t>(row * 3 + column)];
          ASC_DENSE_TEST_CHECK(
              test, Near(At(output, row, column), expected, Real<Element>{20}));
        }
      }

      Fill(output, Real<Element>{0.25});
      ASC_DENSE_TEST_CHECK(
          test, asc::Syrk(context, triangle, effective_transpose,
                          Value<Element>(Real<Element>{0.75}),
                          asc::DenseBlasMatrixView<const Element>(left),
                          Value<Element>(Real<Element>{0.25}), output)
                    .ok());

      if constexpr (asc::DenseBlasComplex<Element>) {
        Fill(output, Real<Element>{0.25});
        ASC_DENSE_TEST_CHECK(
            test, asc::Herk(context, triangle, transpose, Real<Element>{0.75},
                            asc::DenseBlasMatrixView<const Element>(left),
                            Real<Element>{0.25}, output)
                      .ok());
        for (asc::index_t diagonal = 0; diagonal < 3; ++diagonal) {
          ASC_DENSE_TEST_EQ(test, std::imag(At(output, diagonal, diagonal)),
                            Real<Element>{0});
        }
        Fill(output, Real<Element>{0.25});
        ASC_DENSE_TEST_CHECK(
            test,
            asc::Her2k(context, triangle, transpose,
                       Value<Element>(Real<Element>{0.75}, Real<Element>{0.25}),
                       asc::DenseBlasMatrixView<const Element>(left),
                       asc::DenseBlasMatrixView<const Element>(right),
                       Real<Element>{0.25}, output)
                .ok());
        for (asc::index_t diagonal = 0; diagonal < 3; ++diagonal) {
          ASC_DENSE_TEST_EQ(test, std::imag(At(output, diagonal, diagonal)),
                            Real<Element>{0});
        }
      }
    }
  }
}

template <typename Element>
Element TriangularAt(asc::DenseBlasMatrixView<const Element> matrix,
                     asc::DenseBlasTriangle triangle,
                     asc::DenseBlasTranspose transpose,
                     asc::DenseBlasDiagonal diagonal, asc::index_t row,
                     asc::index_t column) {
  asc::index_t stored_row = row;
  asc::index_t stored_column = column;
  if (transpose != asc::DenseBlasTranspose::kNone) {
    stored_row = column;
    stored_column = row;
  }
  const bool stored = triangle == asc::DenseBlasTriangle::kUpper
                          ? stored_row <= stored_column
                          : stored_row >= stored_column;
  if (!stored) {
    return Element{};
  }
  if (row == column && diagonal == asc::DenseBlasDiagonal::kUnit) {
    return Element{1};
  }
  Element value = At(matrix, stored_row, stored_column);
  if (transpose == asc::DenseBlasTranspose::kConjugateTranspose) {
    value = Conjugate(value);
  }
  return value;
}

template <typename Element>
void TestTriangular(TestContext& test) {
  const auto context = asc::ExecutionContext::Serial();
  for (asc::DenseBlasLayout layout :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    for (asc::DenseBlasSide side :
         {asc::DenseBlasSide::kLeft, asc::DenseBlasSide::kRight}) {
      const asc::extent_t matrix_rows =
          side == asc::DenseBlasSide::kLeft ? 3 : 2;
      const asc::extent_t matrix_columns =
          side == asc::DenseBlasSide::kLeft ? 2 : 3;
      const asc::extent_t order =
          side == asc::DenseBlasSide::kLeft ? matrix_rows : matrix_columns;
      const auto triangle = side == asc::DenseBlasSide::kLeft
                                ? asc::DenseBlasTriangle::kUpper
                                : asc::DenseBlasTriangle::kLower;
      const auto transpose = layout == asc::DenseBlasLayout::kColumnMajor
                                 ? asc::DenseBlasTranspose::kNone
                                 : asc::DenseBlasTranspose::kConjugateTranspose;
      const auto diagonal = side == asc::DenseBlasSide::kLeft
                                ? asc::DenseBlasDiagonal::kNonUnit
                                : asc::DenseBlasDiagonal::kUnit;
      std::array<Element, 32> triangular_storage{};
      std::array<Element, 32> matrix_storage{};
      std::array<Element, 32> original_storage{};
      std::array<Element, 32> expected_storage{};
      auto triangular = MakeMatrix(triangular_storage.data(), order, order,
                                   layout, 5, Storage(triangular_storage));
      auto matrix =
          MakeMatrix(matrix_storage.data(), matrix_rows, matrix_columns, layout,
                     5, Storage(matrix_storage));
      auto original =
          MakeMatrix(original_storage.data(), matrix_rows, matrix_columns,
                     layout, 5, Storage(original_storage));
      auto expected =
          MakeMatrix(expected_storage.data(), matrix_rows, matrix_columns,
                     layout, 5, Storage(expected_storage));
      Fill(triangular, Real<Element>{2});
      Fill(matrix, Real<Element>{-1});
      Fill(original, Real<Element>{-1});
      Fill(expected, Real<Element>{0});
      for (asc::index_t row = 0; row < matrix_rows; ++row) {
        for (asc::index_t column = 0; column < matrix_columns; ++column) {
          Element sum{};
          for (asc::index_t inner = 0; inner < order; ++inner) {
            sum +=
                side == asc::DenseBlasSide::kLeft
                    ? TriangularAt(
                          asc::DenseBlasMatrixView<const Element>(triangular),
                          triangle, transpose, diagonal, row, inner) *
                          At(original, inner, column)
                    : At(original, row, inner) *
                          TriangularAt(asc::DenseBlasMatrixView<const Element>(
                                           triangular),
                                       triangle, transpose, diagonal, inner,
                                       column);
          }
          At(expected, row, column) = Value<Element>(Real<Element>{0.5}) * sum;
        }
      }
      ASC_DENSE_TEST_CHECK(
          test,
          asc::Trmm(context, side, triangle, transpose, diagonal,
                    Value<Element>(Real<Element>{0.5}),
                    asc::DenseBlasMatrixView<const Element>(triangular), matrix)
              .ok());
      CheckMatrix(test, matrix,
                  asc::DenseBlasMatrixView<const Element>(expected),
                  Real<Element>{20});
      ASC_DENSE_TEST_CHECK(
          test,
          asc::Trsm(context, side, triangle, transpose, diagonal,
                    Value<Element>(Real<Element>{2}),
                    asc::DenseBlasMatrixView<const Element>(triangular), matrix)
              .ok());
      CheckMatrix(test, matrix,
                  asc::DenseBlasMatrixView<const Element>(original),
                  Real<Element>{30});
    }
  }
}

template <typename Element>
void TestScalarEdges(TestContext& test) {
  const auto context = asc::ExecutionContext::Serial();
  std::array<Element, 4> left_storage{};
  std::array<Element, 4> right_storage{};
  std::array<Element, 4> output_storage{};
  auto left =
      MakeMatrix(left_storage.data(), 1, 1, asc::DenseBlasLayout::kColumnMajor,
                 1, Storage(left_storage));
  auto right =
      MakeMatrix(right_storage.data(), 1, 1, asc::DenseBlasLayout::kColumnMajor,
                 1, Storage(right_storage));
  auto output = MakeMatrix(output_storage.data(), 1, 1,
                           asc::DenseBlasLayout::kColumnMajor, 1,
                           Storage(output_storage));
  At(left, 0, 0) = Element{1};
  At(right, 0, 0) = Element{1};
  const Real<Element> denormal =
      std::numeric_limits<Real<Element>>::denorm_min();
  const Real<Element> large = std::numeric_limits<Real<Element>>::max() / 4;
  const std::array<Element, 7> coefficients{
      Element{0},
      Element{1},
      Element{-1},
      Value<Element>(denormal),
      Value<Element>(large),
      Value<Element>(std::numeric_limits<Real<Element>>::infinity()),
      Value<Element>(std::numeric_limits<Real<Element>>::quiet_NaN())};
  for (Element alpha : coefficients) {
    At(output, 0, 0) = Element{1};
    const Element expected = alpha * Element{1} * Element{1} + Element{1};
    ASC_DENSE_TEST_CHECK(
        test, asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                        asc::DenseBlasTranspose::kNone, alpha,
                        asc::DenseBlasMatrixView<const Element>(left),
                        asc::DenseBlasMatrixView<const Element>(right),
                        Element{1}, output)
                  .ok());
    if constexpr (asc::DenseBlasComplex<Element>) {
      const Element actual = At(output, 0, 0);
      const bool real_matches =
          actual.real() == expected.real() ||
          (std::isnan(actual.real()) && std::isnan(expected.real()));
      const bool imaginary_matches =
          actual.imag() == expected.imag() ||
          (std::isnan(actual.imag()) && std::isnan(expected.imag()));
      ASC_DENSE_TEST_CHECK(test, real_matches && imaginary_matches);
    } else if (std::isnan(expected)) {
      ASC_DENSE_TEST_CHECK(test, std::isnan(At(output, 0, 0)));
    } else {
      ASC_DENSE_TEST_EQ(test, At(output, 0, 0), expected);
    }
  }
}

void TestValidationAndEdges(TestContext& test) {
  const auto context = asc::ExecutionContext::Serial();
  std::array<float, 32> a_storage{};
  std::array<float, 32> b_storage{};
  std::array<float, 32> c_storage{};
  auto a =
      MakeMatrix(a_storage.data(), 2, 2, asc::DenseBlasLayout::kColumnMajor, 3,
                 Storage(a_storage));
  auto b =
      MakeMatrix(b_storage.data(), 2, 2, asc::DenseBlasLayout::kColumnMajor, 3,
                 Storage(b_storage));
  auto c =
      MakeMatrix(c_storage.data(), 2, 2, asc::DenseBlasLayout::kColumnMajor, 3,
                 Storage(c_storage));
  Fill(a, 1.0F);
  Fill(b, 2.0F);
  c_storage.fill(std::numeric_limits<float>::quiet_NaN());
  a_storage.fill(std::numeric_limits<float>::quiet_NaN());
  b_storage.fill(std::numeric_limits<float>::quiet_NaN());
  ASC_DENSE_TEST_CHECK(
      test, asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                      asc::DenseBlasTranspose::kNone, 0.0F,
                      asc::DenseBlasMatrixView<const float>(a),
                      asc::DenseBlasMatrixView<const float>(b), 0.0F, c)
                .ok());
  for (asc::index_t row = 0; row < 2; ++row) {
    for (asc::index_t column = 0; column < 2; ++column) {
      ASC_DENSE_TEST_CHECK(test, At(c, row, column) == 0.0F &&
                                     !std::signbit(At(c, row, column)));
    }
  }

  auto mismatched =
      MakeMatrix(b_storage.data(), 2, 2, asc::DenseBlasLayout::kRowMajor, 3,
                 Storage(b_storage));
  auto bad_layout = asc::Gemm(
      context, asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kNone,
      1.0F, asc::DenseBlasMatrixView<const float>(a),
      asc::DenseBlasMatrixView<const float>(mismatched), 0.0F, c);
  ASC_DENSE_TEST_CHECK(test, !bad_layout.ok());
  ASC_DENSE_TEST_EQ(test, bad_layout.code(), asc::ErrorCode::kInvalidArgument);
  auto bad_enum =
      asc::Trsm(context, static_cast<asc::DenseBlasSide>(255),
                asc::DenseBlasTriangle::kUpper, asc::DenseBlasTranspose::kNone,
                asc::DenseBlasDiagonal::kNonUnit, 1.0F,
                asc::DenseBlasMatrixView<const float>(a), c);
  ASC_DENSE_TEST_CHECK(test, !bad_enum.ok());
  ASC_DENSE_TEST_EQ(test, bad_enum.code(), asc::ErrorCode::kInvalidArgument);
  auto overlap = asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                           asc::DenseBlasTranspose::kNone, 1.0F,
                           asc::DenseBlasMatrixView<const float>(a),
                           asc::DenseBlasMatrixView<const float>(b), 0.0F, a);
  ASC_DENSE_TEST_CHECK(test, !overlap.ok());
  ASC_DENSE_TEST_EQ(test, overlap.code(), asc::ErrorCode::kInvalidArgument);

  auto device =
      MakeMatrix(a_storage.data(), 2, 2, asc::DenseBlasLayout::kColumnMajor, 3,
                 Storage(a_storage, asc::MemorySpace::kDevice));
  auto wrong_backend = asc::Gemm(
      context, asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kNone,
      1.0F, asc::DenseBlasMatrixView<const float>(device),
      asc::DenseBlasMatrixView<const float>(b), 0.0F, c);
  ASC_DENSE_TEST_CHECK(test, !wrong_backend.ok());
  ASC_DENSE_TEST_EQ(test, wrong_backend.code(), asc::ErrorCode::kMemoryAccess);

  auto empty = asc::DenseBlasMatrixView<const float>::Create(
      nullptr, 0, 2, asc::DenseBlasLayout::kColumnMajor, 1,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost));
  auto empty_output = asc::DenseBlasMatrixView<float>::Create(
      nullptr, 0, 2, asc::DenseBlasLayout::kColumnMajor, 1,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost));
  ASC_DENSE_TEST_CHECK(test, empty.ok() && empty_output.ok());
  if (empty.ok() && empty_output.ok()) {
    ASC_DENSE_TEST_CHECK(
        test,
        asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                  asc::DenseBlasTranspose::kNone, 1.0F, *empty,
                  asc::DenseBlasMatrixView<const float>(b), 0.0F, *empty_output)
            .ok());
  }

  std::size_t allocation_count = 0;
  {
    asc_dense_test::AllocationProbe probe;
    auto status = asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                            asc::DenseBlasTranspose::kNone, 0.0F,
                            asc::DenseBlasMatrixView<const float>(a),
                            asc::DenseBlasMatrixView<const float>(b), 0.0F, c);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    allocation_count = probe.count();
  }
  ASC_DENSE_TEST_EQ(test, allocation_count, std::size_t{0});
}

}  // namespace

int main() {
  TestContext test;
  TestGemm<float>(test);
  TestGemm<double>(test);
  TestGemm<std::complex<float>>(test);
  TestGemm<std::complex<double>>(test);
  TestStructured<float>(test);
  TestStructured<double>(test);
  TestStructured<std::complex<float>>(test);
  TestStructured<std::complex<double>>(test);
  TestRankK<float>(test);
  TestRankK<double>(test);
  TestRankK<std::complex<float>>(test);
  TestRankK<std::complex<double>>(test);
  TestTriangular<float>(test);
  TestTriangular<double>(test);
  TestTriangular<std::complex<float>>(test);
  TestTriangular<std::complex<double>>(test);
  TestScalarEdges<float>(test);
  TestScalarEdges<double>(test);
  TestScalarEdges<std::complex<float>>(test);
  TestScalarEdges<std::complex<double>>(test);
  TestValidationAndEdges(test);
  return test.Finish();
}
