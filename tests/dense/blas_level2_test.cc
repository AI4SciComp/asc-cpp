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
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;

template <typename Element>
using Real = asc::DenseBlasRealType<Element>;

template <typename Element, std::size_t Extent>
asc::ConstMemoryView Storage(std::span<Element, Extent> storage,
                             asc::MemorySpace space = asc::MemorySpace::kHost) {
  return asc::ConstMemoryView(storage.data(), storage.size_bytes(), space);
}

template <typename Element>
asc::DenseBlasVectorView<Element> MakeVector(Element* first, asc::extent_t size,
                                             asc::stride_t increment,
                                             asc::ConstMemoryView storage) {
  auto result = asc::DenseBlasVectorView<Element>::Create(first, size,
                                                          increment, storage);
  if (!result.ok()) {
    std::abort();
  }
  return *result;
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
asc::DenseBlasBandMatrixView<Element> MakeBand(
    Element* data, asc::extent_t rows, asc::extent_t columns,
    asc::extent_t lower, asc::extent_t upper, asc::DenseBlasLayout layout,
    asc::stride_t leading_dimension, asc::ConstMemoryView storage) {
  auto result = asc::DenseBlasBandMatrixView<Element>::Create(
      data, rows, columns, lower, upper, layout, leading_dimension, storage);
  if (!result.ok()) {
    std::abort();
  }
  return *result;
}

template <typename Element>
asc::DenseBlasTriangularBandView<Element> MakeTriangularBand(
    Element* data, asc::extent_t order, asc::extent_t bandwidth,
    asc::DenseBlasLayout layout, asc::stride_t leading_dimension,
    asc::ConstMemoryView storage) {
  auto result = asc::DenseBlasTriangularBandView<Element>::Create(
      data, order, bandwidth, layout, leading_dimension, storage);
  if (!result.ok()) {
    std::abort();
  }
  return *result;
}

template <typename Element>
asc::DenseBlasPackedMatrixView<Element> MakePacked(
    Element* data, asc::extent_t order, asc::DenseBlasLayout layout,
    asc::ConstMemoryView storage) {
  auto result = asc::DenseBlasPackedMatrixView<Element>::Create(
      data, order, layout, storage);
  if (!result.ok()) {
    std::abort();
  }
  return *result;
}

template <typename Element>
Element& MatrixAt(asc::DenseBlasMatrixView<Element> matrix, asc::index_t row,
                  asc::index_t column) {
  const asc::stride_t offset =
      matrix.layout() == asc::DenseBlasLayout::kColumnMajor
          ? column * matrix.leading_dimension() + row
          : row * matrix.leading_dimension() + column;
  return matrix.data()[offset];
}

template <typename Element>
Element& BandAt(asc::DenseBlasBandMatrixView<Element> matrix, asc::index_t row,
                asc::index_t column) {
  const asc::stride_t offset =
      matrix.layout() == asc::DenseBlasLayout::kColumnMajor
          ? column * matrix.leading_dimension() + matrix.upper_bandwidth() +
                row - column
          : row * matrix.leading_dimension() + matrix.lower_bandwidth() +
                column - row;
  return matrix.data()[offset];
}

template <typename Element>
Element& TriangularBandAt(asc::DenseBlasTriangularBandView<Element> matrix,
                          asc::DenseBlasTriangle triangle, asc::index_t row,
                          asc::index_t column) {
  asc::stride_t offset = 0;
  if (matrix.layout() == asc::DenseBlasLayout::kColumnMajor) {
    offset = triangle == asc::DenseBlasTriangle::kUpper
                 ? column * matrix.leading_dimension() + matrix.bandwidth() +
                       row - column
                 : column * matrix.leading_dimension() + row - column;
  } else {
    offset = triangle == asc::DenseBlasTriangle::kUpper
                 ? row * matrix.leading_dimension() + column - row
                 : row * matrix.leading_dimension() + matrix.bandwidth() +
                       column - row;
  }
  return matrix.data()[offset];
}

asc::stride_t PackedOffset(asc::extent_t order, asc::DenseBlasLayout layout,
                           asc::DenseBlasTriangle triangle, asc::index_t row,
                           asc::index_t column) {
  if (layout == asc::DenseBlasLayout::kColumnMajor) {
    return triangle == asc::DenseBlasTriangle::kUpper
               ? column * (column + 1) / 2 + row
               : column * order - column * (column - 1) / 2 + row - column;
  }
  return triangle == asc::DenseBlasTriangle::kUpper
             ? row * order - row * (row - 1) / 2 + column - row
             : row * (row + 1) / 2 + column;
}

template <typename Element>
Element& PackedAt(asc::DenseBlasPackedMatrixView<Element> matrix,
                  asc::DenseBlasTriangle triangle, asc::index_t row,
                  asc::index_t column) {
  return matrix.data()[PackedOffset(matrix.order(), matrix.layout(), triangle,
                                    row, column)];
}

template <typename Element>
bool Near(Element actual, Element expected,
          Real<Element> scale = Real<Element>{1}) {
  const Real<Element> tolerance =
      Real<Element>{256} * std::numeric_limits<Real<Element>>::epsilon() *
      scale * (Real<Element>{1} + std::abs(expected));
  return std::abs(actual - expected) <= tolerance;
}

template <typename Element>
void CheckVector(TestContext& test, asc::DenseBlasVectorView<Element> actual,
                 std::span<const std::remove_const_t<Element>> expected) {
  ASC_DENSE_TEST_EQ(test, actual.size(),
                    static_cast<asc::extent_t>(expected.size()));
  for (asc::index_t index = 0; index < actual.size(); ++index) {
    ASC_DENSE_TEST_CHECK(test, Near(actual.data()[index * actual.increment()],
                                    expected[static_cast<std::size_t>(index)]));
  }
}

template <typename Element>
Element MaybeConjugate(Element value) {
  if constexpr (asc::DenseBlasComplex<Element>) {
    return std::conj(value);
  }
  return value;
}

template <typename Element>
void TestGeneralMatrixVector(TestContext& test) {
  const auto context = asc::ExecutionContext::Serial();
  for (asc::DenseBlasLayout layout :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    std::array<Element, 16> matrix_storage{};
    const asc::stride_t leading = 4;
    auto matrix = MakeMatrix(matrix_storage.data(), 2, 3, layout, leading,
                             Storage(std::span(matrix_storage)));
    constexpr std::array<Real<Element>, 6> kValues{1, 2, 3, 4, 5, 6};
    for (asc::index_t row = 0; row < 2; ++row) {
      for (asc::index_t column = 0; column < 3; ++column) {
        const Real<Element> value = kValues[row * 3 + column];
        if constexpr (asc::DenseBlasComplex<Element>) {
          MatrixAt(matrix, row, column) =
              Element{value, static_cast<Real<Element>>(row - column)};
        } else {
          MatrixAt(matrix, row, column) = value;
        }
      }
    }

    std::array<Element, 5> input_storage{};
    input_storage[4] = Element{1};
    input_storage[2] = Element{2};
    input_storage[0] = Element{-1};
    auto input = MakeVector<const Element>(input_storage.data() + 4, 3, -2,
                                           Storage(std::span(input_storage)));
    std::array<Element, 3> output_storage{Element{5}, Element{-99}, Element{7}};
    auto output = MakeVector(output_storage.data(), 2, 2,
                             Storage(std::span(output_storage)));
    ASC_DENSE_TEST_CHECK(
        test, asc::Gemv(context, asc::DenseBlasTranspose::kNone, Element{2},
                        asc::DenseBlasMatrixView<const Element>(matrix), input,
                        Element{-1}, output)
                  .ok());
    std::array<Element, 2> expected{};
    for (asc::index_t row = 0; row < 2; ++row) {
      Element product{0};
      for (asc::index_t column = 0; column < 3; ++column) {
        product += MatrixAt(matrix, row, column) *
                   input.data()[column * input.increment()];
      }
      expected[row] = Element{2} * product -
                      Element{static_cast<Real<Element>>(row == 0 ? 5 : 7)};
    }
    CheckVector(test, output, std::span<const Element>(expected));

    std::array<Element, 2> transpose_input_storage{Element{2}, Element{-1}};
    auto transpose_input =
        MakeVector<const Element>(transpose_input_storage.data(), 2, 1,
                                  Storage(std::span(transpose_input_storage)));
    std::array<Element, 3> transpose_output_storage{};
    auto transpose_output =
        MakeVector(transpose_output_storage.data(), 3, 1,
                   Storage(std::span(transpose_output_storage)));
    ASC_DENSE_TEST_CHECK(
        test,
        asc::Gemv(context, asc::DenseBlasTranspose::kConjugateTranspose,
                  Element{1}, asc::DenseBlasMatrixView<const Element>(matrix),
                  transpose_input, Element{0}, transpose_output)
            .ok());
    for (asc::index_t column = 0; column < 3; ++column) {
      Element expected_value{0};
      for (asc::index_t row = 0; row < 2; ++row) {
        expected_value += MaybeConjugate(MatrixAt(matrix, row, column)) *
                          transpose_input_storage[row];
      }
      ASC_DENSE_TEST_CHECK(
          test, Near(transpose_output_storage[column], expected_value));
    }
  }

  std::array<Element, 12> band_storage{};
  auto band = MakeBand(band_storage.data(), 3, 3, 1, 1,
                       asc::DenseBlasLayout::kColumnMajor, 4,
                       Storage(std::span(band_storage)));
  BandAt(band, 0, 0) = Element{2};
  BandAt(band, 0, 1) = Element{1};
  BandAt(band, 1, 0) = Element{-1};
  BandAt(band, 1, 1) = Element{3};
  BandAt(band, 1, 2) = Element{2};
  BandAt(band, 2, 1) = Element{4};
  BandAt(band, 2, 2) = Element{5};
  std::array<Element, 3> x_storage{Element{1}, Element{2}, Element{-1}};
  std::array<Element, 3> y_storage{};
  auto x = MakeVector<const Element>(x_storage.data(), 3, 1,
                                     Storage(std::span(x_storage)));
  auto y = MakeVector(y_storage.data(), 3, 1, Storage(std::span(y_storage)));
  ASC_DENSE_TEST_CHECK(
      test, asc::Gbmv(context, asc::DenseBlasTranspose::kNone, Element{1},
                      asc::DenseBlasBandMatrixView<const Element>(band), x,
                      Element{0}, y)
                .ok());
  constexpr std::array<Real<Element>, 3> kExpected{4, 3, 3};
  for (std::size_t index = 0; index < kExpected.size(); ++index) {
    ASC_DENSE_TEST_CHECK(test,
                         Near(y_storage[index], Element{kExpected[index]}));
  }
  y_storage.fill(Element{0});
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Gbmv(context, asc::DenseBlasTranspose::kConjugateTranspose,
                Element{1}, asc::DenseBlasBandMatrixView<const Element>(band),
                x, Element{0}, y)
          .ok());
  for (asc::index_t column = 0; column < 3; ++column) {
    Element expected{0};
    const asc::index_t row_begin = std::max<asc::index_t>(0, column - 1);
    const asc::index_t row_end = std::min<asc::index_t>(3, column + 2);
    for (asc::index_t row = row_begin; row < row_end; ++row) {
      expected += MaybeConjugate(BandAt(band, row, column)) * x_storage[row];
    }
    ASC_DENSE_TEST_CHECK(test, Near(y_storage[column], expected));
  }
}

template <typename Element>
void FillStructured(asc::DenseBlasMatrixView<Element> full,
                    asc::DenseBlasTriangularBandView<Element> band,
                    asc::DenseBlasPackedMatrixView<Element> packed,
                    asc::DenseBlasTriangle triangle, bool hermitian) {
  for (asc::index_t row = 0; row < 3; ++row) {
    const asc::index_t begin = triangle == asc::DenseBlasTriangle::kUpper
                                   ? row
                                   : std::max<asc::index_t>(0, row - 1);
    const asc::index_t end = triangle == asc::DenseBlasTriangle::kUpper
                                 ? std::min<asc::index_t>(3, row + 2)
                                 : row + 1;
    for (asc::index_t column = begin; column < end; ++column) {
      const Real<Element> real = static_cast<Real<Element>>(
          row == column ? 3 + row : 1 + row + column);
      Element value{real};
      if constexpr (asc::DenseBlasComplex<Element>) {
        if (row != column) {
          value = Element{real, static_cast<Real<Element>>(row - column)};
        }
        if (hermitian && row == column) {
          value = Element{real, 9};
        }
      }
      MatrixAt(full, row, column) = value;
      TriangularBandAt(band, triangle, row, column) = value;
      PackedAt(packed, triangle, row, column) = value;
    }
  }
}

template <typename Element>
void TestStructuredMatrixVector(TestContext& test) {
  const auto context = asc::ExecutionContext::Serial();
  constexpr bool kHermitian = asc::DenseBlasComplex<Element>;
  std::array<Element, 12> full_storage{};
  std::array<Element, 6> band_storage{};
  std::array<Element, 6> packed_storage{};
  auto full =
      MakeMatrix(full_storage.data(), 3, 3, asc::DenseBlasLayout::kRowMajor, 4,
                 Storage(std::span(full_storage)));
  auto band = MakeTriangularBand(band_storage.data(), 3, 1,
                                 asc::DenseBlasLayout::kColumnMajor, 2,
                                 Storage(std::span(band_storage)));
  auto packed =
      MakePacked(packed_storage.data(), 3, asc::DenseBlasLayout::kRowMajor,
                 Storage(std::span(packed_storage)));
  std::array<Element, 5> x_storage{};
  x_storage[4] = Element{1};
  x_storage[2] = Element{-2};
  x_storage[0] = Element{3};
  auto x = MakeVector<const Element>(x_storage.data() + 4, 3, -2,
                                     Storage(std::span(x_storage)));

  auto run = [&](auto operation) {
    std::array<Element, 3> result_storage{Element{9}, Element{9}, Element{9}};
    auto result = MakeVector(result_storage.data(), 3, 1,
                             Storage(std::span(result_storage)));
    ASC_DENSE_TEST_CHECK(test, operation(result).ok());
    return result_storage;
  };

  for (asc::DenseBlasTriangle triangle :
       {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
    full_storage.fill(Element{0});
    band_storage.fill(Element{0});
    packed_storage.fill(Element{0});
    FillStructured(full, band, packed, triangle, kHermitian);
    std::array<Element, 3> full_result{};
    std::array<Element, 3> band_result{};
    std::array<Element, 3> packed_result{};
    if constexpr (asc::DenseBlasComplex<Element>) {
      full_result = run([&](auto output) {
        return asc::Hemv(context, triangle, Element{1},
                         asc::DenseBlasMatrixView<const Element>(full), x,
                         Element{0}, output);
      });
      band_result = run([&](auto output) {
        return asc::Hbmv(context, triangle, Element{1},
                         asc::DenseBlasTriangularBandView<const Element>(band),
                         x, Element{0}, output);
      });
      packed_result = run([&](auto output) {
        return asc::Hpmv(context, triangle, Element{1},
                         asc::DenseBlasPackedMatrixView<const Element>(packed),
                         x, Element{0}, output);
      });
    } else {
      full_result = run([&](auto output) {
        return asc::Symv(context, triangle, Element{1},
                         asc::DenseBlasMatrixView<const Element>(full), x,
                         Element{0}, output);
      });
      band_result = run([&](auto output) {
        return asc::Sbmv(context, triangle, Element{1},
                         asc::DenseBlasTriangularBandView<const Element>(band),
                         x, Element{0}, output);
      });
      packed_result = run([&](auto output) {
        return asc::Spmv(context, triangle, Element{1},
                         asc::DenseBlasPackedMatrixView<const Element>(packed),
                         x, Element{0}, output);
      });
    }
    for (std::size_t index = 0; index < 3; ++index) {
      ASC_DENSE_TEST_CHECK(test, Near(full_result[index], band_result[index]));
      ASC_DENSE_TEST_CHECK(test,
                           Near(full_result[index], packed_result[index]));
    }
  }
}

template <typename Element>
void FillUpperTriangular(asc::DenseBlasMatrixView<Element> full,
                         asc::DenseBlasTriangularBandView<Element> band,
                         asc::DenseBlasPackedMatrixView<Element> packed) {
  constexpr std::array<Real<Element>, 6> kValues{2, 1, 3, 3, -1, 4};
  std::size_t value = 0;
  for (asc::index_t row = 0; row < 3; ++row) {
    for (asc::index_t column = row; column < 3; ++column) {
      Element element{kValues[value++]};
      MatrixAt(full, row, column) = element;
      PackedAt(packed, asc::DenseBlasTriangle::kUpper, row, column) = element;
      if (column <= row + 1) {
        TriangularBandAt(band, asc::DenseBlasTriangle::kUpper, row, column) =
            element;
      }
    }
  }
}

template <typename Element>
void FillLowerTriangular(asc::DenseBlasMatrixView<Element> full,
                         asc::DenseBlasTriangularBandView<Element> band,
                         asc::DenseBlasPackedMatrixView<Element> packed) {
  constexpr std::array<Real<Element>, 6> kValues{9, 1, 8, 3, -1, 7};
  std::size_t value = 0;
  for (asc::index_t row = 0; row < 3; ++row) {
    for (asc::index_t column = 0; column <= row; ++column) {
      Element element{kValues[value++]};
      if (row == column) {
        const Real<Element> nan =
            std::numeric_limits<Real<Element>>::quiet_NaN();
        if constexpr (asc::DenseBlasComplex<Element>) {
          element = Element{nan, nan};
        } else {
          element = nan;
        }
      }
      MatrixAt(full, row, column) = element;
      PackedAt(packed, asc::DenseBlasTriangle::kLower, row, column) = element;
      if (row <= column + 1) {
        TriangularBandAt(band, asc::DenseBlasTriangle::kLower, row, column) =
            element;
      }
    }
  }
}

template <typename Element>
void TestTriangular(TestContext& test) {
  const auto context = asc::ExecutionContext::Serial();
  std::array<Element, 12> full_storage{};
  std::array<Element, 6> band_storage{};
  std::array<Element, 6> packed_storage{};
  auto full =
      MakeMatrix(full_storage.data(), 3, 3, asc::DenseBlasLayout::kColumnMajor,
                 4, Storage(std::span(full_storage)));
  auto band = MakeTriangularBand(band_storage.data(), 3, 1,
                                 asc::DenseBlasLayout::kRowMajor, 2,
                                 Storage(std::span(band_storage)));
  auto packed =
      MakePacked(packed_storage.data(), 3, asc::DenseBlasLayout::kRowMajor,
                 Storage(std::span(packed_storage)));
  FillUpperTriangular(full, band, packed);

  auto round_trip = [&](auto multiply, auto solve) {
    std::array<Element, 5> vector_storage{};
    vector_storage[4] = Element{1};
    vector_storage[2] = Element{-2};
    vector_storage[0] = Element{3};
    auto vector = MakeVector(vector_storage.data() + 4, 3, -2,
                             Storage(std::span(vector_storage)));
    ASC_DENSE_TEST_CHECK(test, multiply(vector).ok());
    ASC_DENSE_TEST_CHECK(test, solve(vector).ok());
    constexpr std::array<Real<Element>, 3> kExpected{1, -2, 3};
    for (std::size_t index = 0; index < kExpected.size(); ++index) {
      ASC_DENSE_TEST_CHECK(test,
                           Near(vector.data()[static_cast<asc::index_t>(index) *
                                              vector.increment()],
                                Element{kExpected[index]}));
    }
  };

  round_trip(
      [&](auto vector) {
        return asc::Trmv(context, asc::DenseBlasTriangle::kUpper,
                         asc::DenseBlasTranspose::kNone,
                         asc::DenseBlasDiagonal::kNonUnit,
                         asc::DenseBlasMatrixView<const Element>(full), vector);
      },
      [&](auto vector) {
        return asc::Trsv(context, asc::DenseBlasTriangle::kUpper,
                         asc::DenseBlasTranspose::kNone,
                         asc::DenseBlasDiagonal::kNonUnit,
                         asc::DenseBlasMatrixView<const Element>(full), vector);
      });
  round_trip(
      [&](auto vector) {
        return asc::Tbmv(context, asc::DenseBlasTriangle::kUpper,
                         asc::DenseBlasTranspose::kTranspose,
                         asc::DenseBlasDiagonal::kNonUnit,
                         asc::DenseBlasTriangularBandView<const Element>(band),
                         vector);
      },
      [&](auto vector) {
        return asc::Tbsv(context, asc::DenseBlasTriangle::kUpper,
                         asc::DenseBlasTranspose::kTranspose,
                         asc::DenseBlasDiagonal::kNonUnit,
                         asc::DenseBlasTriangularBandView<const Element>(band),
                         vector);
      });
  round_trip(
      [&](auto vector) {
        return asc::Tpmv(context, asc::DenseBlasTriangle::kUpper,
                         asc::DenseBlasTranspose::kConjugateTranspose,
                         asc::DenseBlasDiagonal::kNonUnit,
                         asc::DenseBlasPackedMatrixView<const Element>(packed),
                         vector);
      },
      [&](auto vector) {
        return asc::Tpsv(context, asc::DenseBlasTriangle::kUpper,
                         asc::DenseBlasTranspose::kConjugateTranspose,
                         asc::DenseBlasDiagonal::kNonUnit,
                         asc::DenseBlasPackedMatrixView<const Element>(packed),
                         vector);
      });

  FillLowerTriangular(full, band, packed);
  round_trip(
      [&](auto vector) {
        return asc::Trmv(context, asc::DenseBlasTriangle::kLower,
                         asc::DenseBlasTranspose::kTranspose,
                         asc::DenseBlasDiagonal::kUnit,
                         asc::DenseBlasMatrixView<const Element>(full), vector);
      },
      [&](auto vector) {
        return asc::Trsv(context, asc::DenseBlasTriangle::kLower,
                         asc::DenseBlasTranspose::kTranspose,
                         asc::DenseBlasDiagonal::kUnit,
                         asc::DenseBlasMatrixView<const Element>(full), vector);
      });
  round_trip(
      [&](auto vector) {
        return asc::Tbmv(
            context, asc::DenseBlasTriangle::kLower,
            asc::DenseBlasTranspose::kNone, asc::DenseBlasDiagonal::kUnit,
            asc::DenseBlasTriangularBandView<const Element>(band), vector);
      },
      [&](auto vector) {
        return asc::Tbsv(
            context, asc::DenseBlasTriangle::kLower,
            asc::DenseBlasTranspose::kNone, asc::DenseBlasDiagonal::kUnit,
            asc::DenseBlasTriangularBandView<const Element>(band), vector);
      });
  round_trip(
      [&](auto vector) {
        return asc::Tpmv(context, asc::DenseBlasTriangle::kLower,
                         asc::DenseBlasTranspose::kConjugateTranspose,
                         asc::DenseBlasDiagonal::kUnit,
                         asc::DenseBlasPackedMatrixView<const Element>(packed),
                         vector);
      },
      [&](auto vector) {
        return asc::Tpsv(context, asc::DenseBlasTriangle::kLower,
                         asc::DenseBlasTranspose::kConjugateTranspose,
                         asc::DenseBlasDiagonal::kUnit,
                         asc::DenseBlasPackedMatrixView<const Element>(packed),
                         vector);
      });
}

template <typename Element>
void TestRankUpdates(TestContext& test) {
  const auto context = asc::ExecutionContext::Serial();
  std::array<Element, 2> x_storage{Element{1}, Element{2}};
  std::array<Element, 2> y_storage{Element{3}, Element{-1}};
  auto x = MakeVector<const Element>(x_storage.data(), 2, 1,
                                     Storage(std::span(x_storage)));
  auto y = MakeVector<const Element>(y_storage.data(), 2, 1,
                                     Storage(std::span(y_storage)));
  std::array<Element, 6> matrix_storage{};
  auto matrix =
      MakeMatrix(matrix_storage.data(), 2, 2, asc::DenseBlasLayout::kRowMajor,
                 3, Storage(std::span(matrix_storage)));
  std::array<Element, 3> packed_storage{};
  auto packed =
      MakePacked(packed_storage.data(), 2, asc::DenseBlasLayout::kColumnMajor,
                 Storage(std::span(packed_storage)));
  auto reset_matrix = [&]() { matrix_storage.fill(Element{0}); };
  auto reset_packed = [&]() { packed_storage.fill(Element{0}); };
  if constexpr (asc::DenseBlasComplex<Element>) {
    reset_matrix();
    ASC_DENSE_TEST_CHECK(test,
                         asc::Geru(context, Element{1}, x, y, matrix).ok());
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 0), Element{3}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 1), Element{-1}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 0), Element{6}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 1), Element{-2}));
    reset_matrix();
    ASC_DENSE_TEST_CHECK(test,
                         asc::Gerc(context, Element{1}, x, y, matrix).ok());
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 0), Element{3}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 1), Element{-1}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 0), Element{6}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 1), Element{-2}));
    reset_matrix();
    ASC_DENSE_TEST_CHECK(test, asc::Her(context, asc::DenseBlasTriangle::kUpper,
                                        Real<Element>{2}, x, matrix)
                                   .ok());
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 0), Element{2}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 1), Element{4}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 1), Element{8}));
    reset_packed();
    ASC_DENSE_TEST_CHECK(test, asc::Hpr(context, asc::DenseBlasTriangle::kUpper,
                                        Real<Element>{2}, x, packed)
                                   .ok());
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kUpper, 0, 0),
                   Element{2}));
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kUpper, 0, 1),
                   Element{4}));
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kUpper, 1, 1),
                   Element{8}));
    reset_matrix();
    ASC_DENSE_TEST_CHECK(
        test, asc::Her2(context, asc::DenseBlasTriangle::kLower, Element{1, 1},
                        x, y, matrix)
                  .ok());
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 0), Element{6}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 0), Element{5, 7}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 1), Element{-4}));
    reset_packed();
    ASC_DENSE_TEST_CHECK(
        test, asc::Hpr2(context, asc::DenseBlasTriangle::kLower, Element{1, 1},
                        x, y, packed)
                  .ok());
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kLower, 0, 0),
                   Element{6}));
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kLower, 1, 0),
                   Element{5, 7}));
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kLower, 1, 1),
                   Element{-4}));
  } else {
    reset_matrix();
    ASC_DENSE_TEST_CHECK(test,
                         asc::Ger(context, Element{1}, x, y, matrix).ok());
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 0), Element{3}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 1), Element{-1}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 0), Element{6}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 1), Element{-2}));
    reset_matrix();
    ASC_DENSE_TEST_CHECK(test, asc::Syr(context, asc::DenseBlasTriangle::kUpper,
                                        Element{2}, x, matrix)
                                   .ok());
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 0), Element{2}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 1), Element{4}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 1), Element{8}));
    reset_packed();
    ASC_DENSE_TEST_CHECK(test, asc::Spr(context, asc::DenseBlasTriangle::kUpper,
                                        Element{2}, x, packed)
                                   .ok());
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kUpper, 0, 0),
                   Element{2}));
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kUpper, 0, 1),
                   Element{4}));
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kUpper, 1, 1),
                   Element{8}));
    reset_matrix();
    ASC_DENSE_TEST_CHECK(
        test, asc::Syr2(context, asc::DenseBlasTriangle::kLower, Element{1}, x,
                        y, matrix)
                  .ok());
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 0, 0), Element{6}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 0), Element{5}));
    ASC_DENSE_TEST_CHECK(test, Near(MatrixAt(matrix, 1, 1), Element{-4}));
    reset_packed();
    ASC_DENSE_TEST_CHECK(
        test, asc::Spr2(context, asc::DenseBlasTriangle::kLower, Element{1}, x,
                        y, packed)
                  .ok());
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kLower, 0, 0),
                   Element{6}));
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kLower, 1, 0),
                   Element{5}));
    ASC_DENSE_TEST_CHECK(
        test, Near(PackedAt(packed, asc::DenseBlasTriangle::kLower, 1, 1),
                   Element{-4}));
  }
}

template <typename Real>
void TestAlphaBetaEdges(TestContext& test) {
  const auto context = asc::ExecutionContext::Serial();
  std::array<Real, 1> matrix_storage{Real{1}};
  std::array<Real, 1> input_storage{Real{1}};
  std::array<Real, 1> output_storage{};
  auto matrix = MakeMatrix(matrix_storage.data(), 1, 1,
                           asc::DenseBlasLayout::kColumnMajor, 1,
                           Storage(std::span(matrix_storage)));
  auto input = MakeVector<const Real>(input_storage.data(), 1, 1,
                                      Storage(std::span(input_storage)));
  auto output = MakeVector(output_storage.data(), 1, 1,
                           Storage(std::span(output_storage)));
  const std::array<Real, 4> edges{
      std::numeric_limits<Real>::denorm_min(),
      std::numeric_limits<Real>::max() / Real{4},
      std::numeric_limits<Real>::infinity(),
      std::numeric_limits<Real>::quiet_NaN(),
  };
  auto check = [&](Real actual, Real expected) {
    if (std::isnan(expected)) {
      ASC_DENSE_TEST_CHECK(test, std::isnan(actual));
    } else if (std::isinf(expected)) {
      ASC_DENSE_TEST_CHECK(test, std::isinf(actual));
      ASC_DENSE_TEST_CHECK(test,
                           std::signbit(actual) == std::signbit(expected));
    } else if (std::fpclassify(expected) == FP_SUBNORMAL) {
      ASC_DENSE_TEST_EQ(test, actual, expected);
    } else {
      ASC_DENSE_TEST_CHECK(test, Near(actual, expected));
    }
  };
  for (Real alpha : edges) {
    output_storage[0] = Real{0};
    ASC_DENSE_TEST_CHECK(
        test, asc::Gemv(context, asc::DenseBlasTranspose::kNone, alpha,
                        asc::DenseBlasMatrixView<const Real>(matrix), input,
                        Real{0}, output)
                  .ok());
    check(output_storage[0], alpha);
  }
  for (Real beta : edges) {
    output_storage[0] = Real{1};
    ASC_DENSE_TEST_CHECK(
        test, asc::Gemv(context, asc::DenseBlasTranspose::kNone, Real{0},
                        asc::DenseBlasMatrixView<const Real>(matrix), input,
                        beta, output)
                  .ok());
    check(output_storage[0], beta);
  }
}

void TestInvalidEmptyAndEdges(TestContext& test) {
  std::array<float, 4> storage{};
  auto too_small = asc::DenseBlasMatrixView<float>::Create(
      storage.data(), 2, 2, asc::DenseBlasLayout::kColumnMajor, 1,
      Storage(std::span(storage)));
  ASC_DENSE_TEST_CHECK(test, !too_small.ok());
  auto bad_band = asc::DenseBlasBandMatrixView<float>::Create(
      storage.data(), 2, 2, 2, 0, asc::DenseBlasLayout::kColumnMajor, 3,
      Storage(std::span(storage)));
  ASC_DENSE_TEST_CHECK(test, !bad_band.ok());
  auto bad_empty_band = asc::DenseBlasBandMatrixView<float>::Create(
      nullptr, 0, 2, 1, 0, asc::DenseBlasLayout::kColumnMajor, 2,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost));
  ASC_DENSE_TEST_CHECK(test, !bad_empty_band.ok());
  auto overflow_matrix = asc::DenseBlasMatrixView<float>::Create(
      storage.data(), std::numeric_limits<asc::extent_t>::max(),
      std::numeric_limits<asc::extent_t>::max(),
      asc::DenseBlasLayout::kColumnMajor,
      std::numeric_limits<asc::stride_t>::max(), Storage(std::span(storage)));
  ASC_DENSE_TEST_CHECK(test, !overflow_matrix.ok());
  auto overflow_packed = asc::DenseBlasPackedMatrixView<float>::Create(
      storage.data(), std::numeric_limits<asc::extent_t>::max(),
      asc::DenseBlasLayout::kColumnMajor, Storage(std::span(storage)));
  ASC_DENSE_TEST_CHECK(test, !overflow_packed.ok());

  const auto context = asc::ExecutionContext::Serial();
  std::array<float, 4> matrix_storage{1, 0, 0, 1};
  auto matrix = MakeMatrix(matrix_storage.data(), 2, 2,
                           asc::DenseBlasLayout::kColumnMajor, 2,
                           Storage(std::span(matrix_storage)));
  std::array<float, 2> vector_storage{1, 2};
  auto input = MakeVector<const float>(vector_storage.data(), 2, 1,
                                       Storage(std::span(vector_storage)));
  auto output = MakeVector(vector_storage.data(), 2, 1,
                           Storage(std::span(vector_storage)));
  std::array<float, 1> short_input_storage{1};
  auto short_input =
      MakeVector<const float>(short_input_storage.data(), 1, 1,
                              Storage(std::span(short_input_storage)));
  const auto before = vector_storage;
  auto bad_shape = asc::Gemv(context, asc::DenseBlasTranspose::kNone, 1.0F,
                             asc::DenseBlasMatrixView<const float>(matrix),
                             short_input, 0.0F, output);
  ASC_DENSE_TEST_CHECK(test, !bad_shape.ok());
  ASC_DENSE_TEST_EQ(test, vector_storage, before);
  auto bad_diagonal = asc::Trmv(
      context, asc::DenseBlasTriangle::kUpper, asc::DenseBlasTranspose::kNone,
      static_cast<asc::DenseBlasDiagonal>(255),
      asc::DenseBlasMatrixView<const float>(matrix), output);
  ASC_DENSE_TEST_CHECK(test, !bad_diagonal.ok());
  ASC_DENSE_TEST_EQ(test, vector_storage, before);
  auto bad_triangle = asc::Symv(
      context, static_cast<asc::DenseBlasTriangle>(255), 1.0F,
      asc::DenseBlasMatrixView<const float>(matrix), input, 0.0F, output);
  ASC_DENSE_TEST_CHECK(test, !bad_triangle.ok());
  ASC_DENSE_TEST_EQ(test, vector_storage, before);
  auto invalid = asc::Gemv(context, static_cast<asc::DenseBlasTranspose>(255),
                           1.0F, asc::DenseBlasMatrixView<const float>(matrix),
                           input, 0.0F, output);
  ASC_DENSE_TEST_CHECK(test, !invalid.ok());
  ASC_DENSE_TEST_EQ(test, vector_storage, before);
  auto overlap = asc::Gemv(context, asc::DenseBlasTranspose::kNone, 1.0F,
                           asc::DenseBlasMatrixView<const float>(matrix), input,
                           0.0F, output);
  ASC_DENSE_TEST_CHECK(test, !overlap.ok());
  ASC_DENSE_TEST_EQ(test, vector_storage, before);

  const float nan = std::numeric_limits<float>::quiet_NaN();
  matrix_storage.fill(nan);
  std::array<float, 2> nan_input{nan, nan};
  std::array<float, 2> nan_output{nan, nan};
  input = MakeVector<const float>(nan_input.data(), 2, 1,
                                  Storage(std::span(nan_input)));
  output = MakeVector(nan_output.data(), 2, 1, Storage(std::span(nan_output)));
  ASC_DENSE_TEST_CHECK(
      test, asc::Gemv(context, asc::DenseBlasTranspose::kNone, 0.0F,
                      asc::DenseBlasMatrixView<const float>(matrix), input,
                      0.0F, output)
                .ok());
  ASC_DENSE_TEST_CHECK(test,
                       nan_output[0] == 0.0F && !std::signbit(nan_output[0]));
  ASC_DENSE_TEST_CHECK(test,
                       nan_output[1] == 0.0F && !std::signbit(nan_output[1]));

  std::array<float, 1> empty_backing{};
  auto empty_matrix = MakeMatrix<float>(
      nullptr, 0, 2, asc::DenseBlasLayout::kColumnMajor, 1,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost));
  auto empty_output = MakeVector<float>(
      nullptr, 0, 1, asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost));
  std::array<float, 2> empty_input_storage{1, 2};
  auto empty_matrix_input =
      MakeVector<const float>(empty_input_storage.data(), 2, 1,
                              Storage(std::span(empty_input_storage)));
  ASC_DENSE_TEST_CHECK(
      test, asc::Gemv(context, asc::DenseBlasTranspose::kNone, 1.0F,
                      asc::DenseBlasMatrixView<const float>(empty_matrix),
                      empty_matrix_input, 0.0F, empty_output)
                .ok());
  auto empty_band = asc::DenseBlasBandMatrixView<const float>::Create(
      nullptr, 0, 2, 0, 1, asc::DenseBlasLayout::kColumnMajor, 2,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost));
  auto empty_input = asc::DenseBlasVectorView<const float>::Create(
      nullptr, 0, 1, asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost));
  std::array<float, 2> scaled_output_storage{3, 4};
  auto scaled_output = MakeVector(scaled_output_storage.data(), 2, 1,
                                  Storage(std::span(scaled_output_storage)));
  ASC_DENSE_TEST_CHECK(test, empty_band.ok() && empty_input.ok());
  if (empty_band.ok() && empty_input.ok()) {
    ASC_DENSE_TEST_CHECK(
        test, asc::Gbmv(context, asc::DenseBlasTranspose::kTranspose, 1.0F,
                        *empty_band, *empty_input, 2.0F, scaled_output)
                  .ok());
    ASC_DENSE_TEST_EQ(test, scaled_output_storage,
                      (std::array<float, 2>{6, 8}));
  }
  static_cast<void>(empty_backing);

  auto device_matrix = MakeMatrix(
      matrix_storage.data(), 2, 2, asc::DenseBlasLayout::kColumnMajor, 2,
      Storage(std::span(matrix_storage), asc::MemorySpace::kDevice));
  auto device_status =
      asc::Gemv(context, asc::DenseBlasTranspose::kNone, 1.0F,
                asc::DenseBlasMatrixView<const float>(device_matrix), input,
                0.0F, output);
  ASC_DENSE_TEST_CHECK(test, !device_status.ok());
  if (!device_status.ok()) {
    ASC_DENSE_TEST_EQ(test, device_status.code(),
                      asc::ErrorCode::kMemoryAccess);
  }

  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    auto status = asc::Gemv(context, asc::DenseBlasTranspose::kNone, 0.0F,
                            asc::DenseBlasMatrixView<const float>(matrix),
                            input, 0.0F, output);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    allocations = probe.count();
  }
  ASC_DENSE_TEST_EQ(test, allocations, std::size_t{0});
}

}  // namespace

int main() {
  TestContext test;
  TestGeneralMatrixVector<float>(test);
  TestGeneralMatrixVector<double>(test);
  TestGeneralMatrixVector<std::complex<float>>(test);
  TestGeneralMatrixVector<std::complex<double>>(test);
  TestStructuredMatrixVector<float>(test);
  TestStructuredMatrixVector<double>(test);
  TestStructuredMatrixVector<std::complex<float>>(test);
  TestStructuredMatrixVector<std::complex<double>>(test);
  TestTriangular<float>(test);
  TestTriangular<double>(test);
  TestTriangular<std::complex<float>>(test);
  TestTriangular<std::complex<double>>(test);
  TestRankUpdates<float>(test);
  TestRankUpdates<double>(test);
  TestRankUpdates<std::complex<float>>(test);
  TestRankUpdates<std::complex<double>>(test);
  TestAlphaBetaEdges<float>(test);
  TestAlphaBetaEdges<double>(test);
  TestInvalidEmptyAndEdges(test);
  return test.Finish();
}
