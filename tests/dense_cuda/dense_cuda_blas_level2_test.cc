#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/cuda.h"
#include "counting_resource.h"
#include "device_test_helpers.h"
#include "test_support.h"

namespace {

using asc_dense_cuda_test::CopyAndWait;
using asc_dense_cuda_test::Data;
using asc_dense_cuda_test::TestContext;

template <typename Element>
using Real = asc::DenseBlasRealType<Element>;

struct BufferPair {
  asc::Buffer host;
  asc::Buffer device;
};

template <typename Element>
asc::Result<BufferPair> AllocatePair(asc::MemoryResource& pinned,
                                     asc::MemoryResource& device,
                                     std::size_t count) {
  auto host =
      asc::Buffer::Allocate(pinned, count * sizeof(Element), alignof(Element));
  if (!host.ok()) {
    return host.status();
  }
  auto device_buffer =
      asc::Buffer::Allocate(device, count * sizeof(Element), alignof(Element));
  if (!device_buffer.ok()) {
    return device_buffer.status();
  }
  return BufferPair{std::move(*host), std::move(*device_buffer)};
}

asc::ConstMemoryView Storage(asc::Buffer& buffer, asc::MemorySpace space) {
  return {buffer.data(), buffer.size(), space};
}

template <typename Element>
Element Value(Real<Element> real, Real<Element> imaginary = {0}) {
  if constexpr (asc::DenseBlasComplex<Element>) {
    return Element{real, imaginary};
  } else {
    static_cast<void>(imaginary);
    return real;
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
asc::DenseBlasVectorView<Element> MakeVector(asc::Buffer& buffer,
                                             std::size_t logical_index,
                                             asc::extent_t size,
                                             asc::stride_t increment,
                                             asc::MemorySpace space) {
  auto result = asc::DenseBlasVectorView<Element>::Create(
      Data<Element>(buffer) + logical_index, size, increment,
      Storage(buffer, space));
  if (!result.ok()) {
    std::abort();
  }
  return *result;
}

template <typename Element>
asc::DenseBlasMatrixView<Element> MakeMatrix(asc::Buffer& buffer,
                                             asc::extent_t rows,
                                             asc::extent_t columns,
                                             asc::DenseBlasLayout layout,
                                             asc::stride_t leading_dimension,
                                             asc::MemorySpace space) {
  auto result = asc::DenseBlasMatrixView<Element>::Create(
      Data<Element>(buffer), rows, columns, layout, leading_dimension,
      Storage(buffer, space));
  if (!result.ok()) {
    std::abort();
  }
  return *result;
}

template <typename Element>
asc::DenseBlasBandMatrixView<Element> MakeBand(
    asc::Buffer& buffer, asc::extent_t rows, asc::extent_t columns,
    asc::extent_t lower, asc::extent_t upper, asc::DenseBlasLayout layout,
    asc::stride_t leading_dimension, asc::MemorySpace space) {
  auto result = asc::DenseBlasBandMatrixView<Element>::Create(
      Data<Element>(buffer), rows, columns, lower, upper, layout,
      leading_dimension, Storage(buffer, space));
  if (!result.ok()) {
    std::abort();
  }
  return *result;
}

template <typename Element>
asc::DenseBlasTriangularBandView<Element> MakeTriangularBand(
    asc::Buffer& buffer, asc::extent_t order, asc::extent_t bandwidth,
    asc::DenseBlasLayout layout, asc::stride_t leading_dimension,
    asc::MemorySpace space) {
  auto result = asc::DenseBlasTriangularBandView<Element>::Create(
      Data<Element>(buffer), order, bandwidth, layout, leading_dimension,
      Storage(buffer, space));
  if (!result.ok()) {
    std::abort();
  }
  return *result;
}

template <typename Element>
asc::DenseBlasPackedMatrixView<Element> MakePacked(asc::Buffer& buffer,
                                                   asc::extent_t order,
                                                   asc::DenseBlasLayout layout,
                                                   asc::MemorySpace space) {
  auto result = asc::DenseBlasPackedMatrixView<Element>::Create(
      Data<Element>(buffer), order, layout, Storage(buffer, space));
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

template <typename Operation>
bool Wait(TestContext& test, Operation operation) {
  ASC_DENSE_CUDA_CHECK(test, operation.ok());
  if (!operation.ok()) {
    std::cerr << operation.status().ToString() << '\n';
    return false;
  }
  auto query = operation->Query();
  ASC_DENSE_CUDA_CHECK(test, query.ok());
  const asc::Status status = operation->Wait();
  ASC_DENSE_CUDA_CHECK(test, status.ok());
  return status.ok();
}

template <typename Element>
void CheckNear(TestContext& test, Element actual, Element expected) {
  const Real<Element> scale = std::max(
      Real<Element>{1}, static_cast<Real<Element>>(std::abs(expected)));
  const Real<Element> tolerance =
      Real<Element>{512} * std::numeric_limits<Real<Element>>::epsilon() *
      scale;
  ASC_DENSE_CUDA_CHECK(test, std::abs(actual - expected) <= tolerance);
}

template <typename Element>
// General matrix layouts and transposes share fixtures and oracle data.
// NOLINTNEXTLINE(readability-function-size)
void TestGeneralMatrixVector(TestContext& test, asc::MemoryResource& pinned,
                             asc::MemoryResource& device,
                             asc::DenseCudaContext& context) {
  for (asc::DenseBlasLayout layout :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    auto matrix = AllocatePair<Element>(pinned, device, 12);
    auto input = AllocatePair<Element>(pinned, device, 5);
    auto output = AllocatePair<Element>(pinned, device, 3);
    ASC_DENSE_CUDA_CHECK(test, matrix.ok() && input.ok() && output.ok());
    if (!matrix.ok() || !input.ok() || !output.ok()) {
      return;
    }
    std::fill_n(Data<Element>(matrix->host), 12, Element{0});
    std::fill_n(Data<Element>(input->host), 5, Element{0});
    std::fill_n(Data<Element>(output->host), 3, Element{0});
    auto host_matrix = MakeMatrix<Element>(matrix->host, 2, 3, layout, 4,
                                           asc::MemorySpace::kPinnedHost);
    auto device_matrix = MakeMatrix<Element>(matrix->device, 2, 3, layout, 4,
                                             asc::MemorySpace::kDevice);
    for (asc::index_t row = 0; row < 2; ++row) {
      for (asc::index_t column = 0; column < 3; ++column) {
        MatrixAt(host_matrix, row, column) =
            Value<Element>(static_cast<Real<Element>>(1 + row * 3 + column),
                           static_cast<Real<Element>>(row - column));
      }
    }
    Data<Element>(input->host)[4] = Value<Element>(1);
    Data<Element>(input->host)[2] = Value<Element>(2);
    Data<Element>(input->host)[0] = Value<Element>(-1);
    ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                           matrix->device, matrix->host));
    ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                           input->device, input->host));
    ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                           output->device, output->host));
    auto x = MakeVector<const Element>(input->device, 4, 3, -2,
                                       asc::MemorySpace::kDevice);
    auto y =
        MakeVector<Element>(output->device, 0, 2, 1, asc::MemorySpace::kDevice);
    ASC_DENSE_CUDA_CHECK(
        test, Wait(test, asc::CudaGemv(context, asc::DenseBlasTranspose::kNone,
                                       Value<Element>(1),
                                       asc::DenseBlasMatrixView<const Element>(
                                           device_matrix),
                                       x, Value<Element>(0), y)));
    ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                           output->host, output->device));
    for (asc::index_t row = 0; row < 2; ++row) {
      Element expected{0};
      for (asc::index_t column = 0; column < 3; ++column) {
        expected += MatrixAt(host_matrix, row, column) *
                    Data<Element>(input->host)[4 - 2 * column];
      }
      CheckNear(test, Data<Element>(output->host)[row], expected);
    }

    {
      constexpr asc::DenseBlasTranspose kTranspose =
          asc::DenseBlasComplex<Element>
              ? asc::DenseBlasTranspose::kConjugateTranspose
              : asc::DenseBlasTranspose::kTranspose;
      std::fill_n(Data<Element>(output->host), 3, Element{0});
      Data<Element>(input->host)[0] = Value<Element>(2, -1);
      Data<Element>(input->host)[1] = Value<Element>(-1, 2);
      ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                             input->device, input->host));
      ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                             output->device, output->host));
      auto transpose_input = MakeVector<const Element>(
          input->device, 0, 2, 1, asc::MemorySpace::kDevice);
      auto transpose_output = MakeVector<Element>(output->device, 0, 3, 1,
                                                  asc::MemorySpace::kDevice);
      ASC_DENSE_CUDA_CHECK(
          test,
          Wait(test,
               asc::CudaGemv(
                   context, kTranspose, Value<Element>(1),
                   asc::DenseBlasMatrixView<const Element>(device_matrix),
                   transpose_input, Value<Element>(0), transpose_output)));
      ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                             output->host, output->device));
      for (asc::index_t column = 0; column < 3; ++column) {
        Element expected{0};
        for (asc::index_t row = 0; row < 2; ++row) {
          expected += MaybeConjugate(MatrixAt(host_matrix, row, column)) *
                      Data<Element>(input->host)[row];
        }
        CheckNear(test, Data<Element>(output->host)[column], expected);
      }
    }

    auto band = AllocatePair<Element>(pinned, device, 12);
    auto band_input = AllocatePair<Element>(pinned, device, 3);
    auto band_output = AllocatePair<Element>(pinned, device, 3);
    ASC_DENSE_CUDA_CHECK(test,
                         band.ok() && band_input.ok() && band_output.ok());
    if (!band.ok() || !band_input.ok() || !band_output.ok()) {
      return;
    }
    std::fill_n(Data<Element>(band->host), 12, Element{0});
    auto host_band = MakeBand<Element>(band->host, 3, 3, 1, 1, layout, 4,
                                       asc::MemorySpace::kPinnedHost);
    auto device_band = MakeBand<Element>(band->device, 3, 3, 1, 1, layout, 4,
                                         asc::MemorySpace::kDevice);
    BandAt(host_band, 0, 0) = Value<Element>(2);
    BandAt(host_band, 0, 1) = Value<Element>(1);
    BandAt(host_band, 1, 0) = Value<Element>(-1);
    BandAt(host_band, 1, 1) = Value<Element>(3);
    BandAt(host_band, 1, 2) = Value<Element>(2);
    BandAt(host_band, 2, 1) = Value<Element>(4);
    BandAt(host_band, 2, 2) = Value<Element>(5);
    Data<Element>(band_input->host)[0] = Value<Element>(1);
    Data<Element>(band_input->host)[1] = Value<Element>(2);
    Data<Element>(band_input->host)[2] = Value<Element>(-1);
    std::fill_n(Data<Element>(band_output->host), 3, Element{0});
    ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                           band->device, band->host));
    ASC_DENSE_CUDA_CHECK(
        test, CopyAndWait(context.execution_context(), band_input->device,
                          band_input->host));
    ASC_DENSE_CUDA_CHECK(
        test, CopyAndWait(context.execution_context(), band_output->device,
                          band_output->host));
    auto band_x = MakeVector<const Element>(band_input->device, 0, 3, 1,
                                            asc::MemorySpace::kDevice);
    auto band_y = MakeVector<Element>(band_output->device, 0, 3, 1,
                                      asc::MemorySpace::kDevice);
    ASC_DENSE_CUDA_CHECK(
        test,
        Wait(test,
             asc::CudaGbmv(
                 context, asc::DenseBlasTranspose::kNone, Value<Element>(1),
                 asc::DenseBlasBandMatrixView<const Element>(device_band),
                 band_x, Value<Element>(0), band_y)));
    ASC_DENSE_CUDA_CHECK(
        test, CopyAndWait(context.execution_context(), band_output->host,
                          band_output->device));
    constexpr int kExpected[3] = {4, 3, 3};
    for (std::size_t index = 0; index < 3; ++index) {
      CheckNear(test, Data<Element>(band_output->host)[index],
                Value<Element>(static_cast<Real<Element>>(kExpected[index])));
    }
    {
      constexpr asc::DenseBlasTranspose kTranspose =
          asc::DenseBlasComplex<Element>
              ? asc::DenseBlasTranspose::kConjugateTranspose
              : asc::DenseBlasTranspose::kTranspose;
      std::fill_n(Data<Element>(band_output->host), 3, Element{0});
      ASC_DENSE_CUDA_CHECK(
          test, CopyAndWait(context.execution_context(), band_output->device,
                            band_output->host));
      ASC_DENSE_CUDA_CHECK(
          test,
          Wait(test, asc::CudaGbmv(context, kTranspose, Value<Element>(1),
                                   asc::DenseBlasBandMatrixView<const Element>(
                                       device_band),
                                   band_x, Value<Element>(0), band_y)));
      ASC_DENSE_CUDA_CHECK(
          test, CopyAndWait(context.execution_context(), band_output->host,
                            band_output->device));
      for (asc::index_t column = 0; column < 3; ++column) {
        Element expected{0};
        const asc::index_t row_begin = std::max<asc::index_t>(0, column - 1);
        const asc::index_t row_end = std::min<asc::index_t>(3, column + 2);
        for (asc::index_t row = row_begin; row < row_end; ++row) {
          expected += MaybeConjugate(BandAt(host_band, row, column)) *
                      Data<Element>(band_input->host)[row];
        }
        CheckNear(test, Data<Element>(band_output->host)[column], expected);
      }
    }
  }
}

template <typename Element>
void FillStructured(asc::DenseBlasMatrixView<Element> full,
                    asc::DenseBlasTriangularBandView<Element> band,
                    asc::DenseBlasPackedMatrixView<Element> packed,
                    asc::DenseBlasTriangle triangle) {
  MatrixAt(full, 0, 0) = Value<Element>(2, 7);
  MatrixAt(full, 1, 1) = Value<Element>(3, -8);
  TriangularBandAt(band, triangle, 0, 0) = Value<Element>(2, 7);
  TriangularBandAt(band, triangle, 1, 1) = Value<Element>(3, -8);
  PackedAt(packed, triangle, 0, 0) = Value<Element>(2, 7);
  PackedAt(packed, triangle, 1, 1) = Value<Element>(3, -8);
  if (triangle == asc::DenseBlasTriangle::kUpper) {
    MatrixAt(full, 0, 1) = Value<Element>(1, 1);
    TriangularBandAt(band, triangle, 0, 1) = Value<Element>(1, 1);
    PackedAt(packed, triangle, 0, 1) = Value<Element>(1, 1);
  } else {
    MatrixAt(full, 1, 0) = Value<Element>(1, -1);
    TriangularBandAt(band, triangle, 1, 0) = Value<Element>(1, -1);
    PackedAt(packed, triangle, 1, 0) = Value<Element>(1, -1);
  }
}

template <typename Element>
// Structured matrix forms share buffers and layout-specific oracle checks.
// NOLINTNEXTLINE(readability-function-size)
void TestStructuredMatrixVector(TestContext& test, asc::MemoryResource& pinned,
                                asc::MemoryResource& device,
                                asc::DenseCudaContext& context) {
  for (asc::DenseBlasLayout layout :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    auto full = AllocatePair<Element>(pinned, device, 6);
    auto band = AllocatePair<Element>(pinned, device, 4);
    auto packed = AllocatePair<Element>(pinned, device, 3);
    auto input = AllocatePair<Element>(pinned, device, 2);
    auto output = AllocatePair<Element>(pinned, device, 2);
    ASC_DENSE_CUDA_CHECK(test, full.ok() && band.ok() && packed.ok() &&
                                   input.ok() && output.ok());
    if (!full.ok() || !band.ok() || !packed.ok() || !input.ok() ||
        !output.ok()) {
      return;
    }
    std::fill_n(Data<Element>(full->host), 6, Element{0});
    std::fill_n(Data<Element>(band->host), 4, Element{0});
    std::fill_n(Data<Element>(packed->host), 3, Element{0});
    auto host_full = MakeMatrix<Element>(full->host, 2, 2, layout, 3,
                                         asc::MemorySpace::kPinnedHost);
    auto device_full = MakeMatrix<Element>(full->device, 2, 2, layout, 3,
                                           asc::MemorySpace::kDevice);
    auto host_band = MakeTriangularBand<Element>(band->host, 2, 1, layout, 2,
                                                 asc::MemorySpace::kPinnedHost);
    auto device_band = MakeTriangularBand<Element>(
        band->device, 2, 1, layout, 2, asc::MemorySpace::kDevice);
    auto host_packed = MakePacked<Element>(packed->host, 2, layout,
                                           asc::MemorySpace::kPinnedHost);
    auto device_packed = MakePacked<Element>(packed->device, 2, layout,
                                             asc::MemorySpace::kDevice);
    Data<Element>(input->host)[0] = Value<Element>(1);
    Data<Element>(input->host)[1] = Value<Element>(2);
    ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                           input->device, input->host));
    auto x = MakeVector<const Element>(input->device, 0, 2, 1,
                                       asc::MemorySpace::kDevice);
    auto run = [&](auto operation) {
      std::fill_n(Data<Element>(output->host), 2, Value<Element>(-99, -99));
      ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                             output->device, output->host));
      auto y = MakeVector<Element>(output->device, 0, 2, 1,
                                   asc::MemorySpace::kDevice);
      ASC_DENSE_CUDA_CHECK(test, Wait(test, operation(y)));
      ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                             output->host, output->device));
      CheckNear(test, Data<Element>(output->host)[0], Value<Element>(4, 2));
      CheckNear(test, Data<Element>(output->host)[1], Value<Element>(7, -1));
    };
    for (asc::DenseBlasTriangle triangle :
         {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
      std::fill_n(Data<Element>(full->host), 6, Element{0});
      std::fill_n(Data<Element>(band->host), 4, Element{0});
      std::fill_n(Data<Element>(packed->host), 3, Element{0});
      FillStructured(host_full, host_band, host_packed, triangle);
      ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                             full->device, full->host));
      ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                             band->device, band->host));
      ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                             packed->device, packed->host));
      if constexpr (asc::DenseBlasComplex<Element>) {
        run([&](auto y) {
          return asc::CudaHemv(
              context, triangle, Value<Element>(1),
              asc::DenseBlasMatrixView<const Element>(device_full), x,
              Value<Element>(0), y);
        });
        run([&](auto y) {
          return asc::CudaHbmv(
              context, triangle, Value<Element>(1),
              asc::DenseBlasTriangularBandView<const Element>(device_band), x,
              Value<Element>(0), y);
        });
        run([&](auto y) {
          return asc::CudaHpmv(
              context, triangle, Value<Element>(1),
              asc::DenseBlasPackedMatrixView<const Element>(device_packed), x,
              Value<Element>(0), y);
        });
      } else {
        run([&](auto y) {
          return asc::CudaSymv(
              context, triangle, Value<Element>(1),
              asc::DenseBlasMatrixView<const Element>(device_full), x,
              Value<Element>(0), y);
        });
        run([&](auto y) {
          return asc::CudaSbmv(
              context, triangle, Value<Element>(1),
              asc::DenseBlasTriangularBandView<const Element>(device_band), x,
              Value<Element>(0), y);
        });
        run([&](auto y) {
          return asc::CudaSpmv(
              context, triangle, Value<Element>(1),
              asc::DenseBlasPackedMatrixView<const Element>(device_packed), x,
              Value<Element>(0), y);
        });
      }
    }
  }
}

template <typename Element>
void FillTriangular(asc::DenseBlasMatrixView<Element> full,
                    asc::DenseBlasTriangularBandView<Element> band,
                    asc::DenseBlasPackedMatrixView<Element> packed) {
  constexpr int kValues[6] = {2, 1, 3, 3, -1, 4};
  std::size_t value = 0;
  for (asc::index_t row = 0; row < 3; ++row) {
    for (asc::index_t column = row; column < 3; ++column) {
      const Element element = Value<Element>(
          static_cast<Real<Element>>(kValues[value]),
          row == column ? Real<Element>{0}
                        : static_cast<Real<Element>>(column - row));
      ++value;
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
  constexpr int kValues[6] = {9, 1, 8, 3, -1, 7};
  std::size_t value = 0;
  for (asc::index_t row = 0; row < 3; ++row) {
    for (asc::index_t column = 0; column <= row; ++column) {
      const Real<Element> diagonal =
          std::numeric_limits<Real<Element>>::quiet_NaN();
      const Element element = Value<Element>(
          row == column ? diagonal : static_cast<Real<Element>>(kValues[value]),
          row == column ? diagonal : static_cast<Real<Element>>(row - column));
      ++value;
      MatrixAt(full, row, column) = element;
      PackedAt(packed, asc::DenseBlasTriangle::kLower, row, column) = element;
      if (row <= column + 1) {
        TriangularBandAt(band, asc::DenseBlasTriangle::kLower, row, column) =
            element;
      }
    }
  }
}

template <typename Element, typename Multiply, typename Solve>
void CheckTriangularRoundTrip(TestContext& test, BufferPair& vector,
                              asc::DenseCudaContext& context, Multiply multiply,
                              Solve solve) {
  std::fill_n(Data<Element>(vector.host), 5, Element{0});
  Data<Element>(vector.host)[4] = Value<Element>(1);
  Data<Element>(vector.host)[2] = Value<Element>(-2);
  Data<Element>(vector.host)[0] = Value<Element>(3);
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         vector.device, vector.host));
  auto value =
      MakeVector<Element>(vector.device, 4, 3, -2, asc::MemorySpace::kDevice);
  ASC_DENSE_CUDA_CHECK(test, Wait(test, multiply(value)));
  ASC_DENSE_CUDA_CHECK(test, Wait(test, solve(value)));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         vector.host, vector.device));
  CheckNear(test, Data<Element>(vector.host)[4], Value<Element>(1));
  CheckNear(test, Data<Element>(vector.host)[2], Value<Element>(-2));
  CheckNear(test, Data<Element>(vector.host)[0], Value<Element>(3));
}

template <typename Element>
// Multiply and solve variants share one triangular fixture and oracle.
// NOLINTNEXTLINE(readability-function-size)
void TestTriangular(TestContext& test, asc::MemoryResource& pinned,
                    asc::MemoryResource& device,
                    asc::DenseCudaContext& context) {
  auto full = AllocatePair<Element>(pinned, device, 12);
  auto band = AllocatePair<Element>(pinned, device, 6);
  auto packed = AllocatePair<Element>(pinned, device, 6);
  auto vector = AllocatePair<Element>(pinned, device, 5);
  ASC_DENSE_CUDA_CHECK(test,
                       full.ok() && band.ok() && packed.ok() && vector.ok());
  if (!full.ok() || !band.ok() || !packed.ok() || !vector.ok()) {
    return;
  }
  std::fill_n(Data<Element>(full->host), 12, Element{0});
  std::fill_n(Data<Element>(band->host), 6, Element{0});
  std::fill_n(Data<Element>(packed->host), 6, Element{0});
  auto host_full =
      MakeMatrix<Element>(full->host, 3, 3, asc::DenseBlasLayout::kColumnMajor,
                          4, asc::MemorySpace::kPinnedHost);
  auto device_full = MakeMatrix<Element>(full->device, 3, 3,
                                         asc::DenseBlasLayout::kColumnMajor, 4,
                                         asc::MemorySpace::kDevice);
  auto host_band = MakeTriangularBand<Element>(
      band->host, 3, 1, asc::DenseBlasLayout::kRowMajor, 2,
      asc::MemorySpace::kPinnedHost);
  auto device_band = MakeTriangularBand<Element>(
      band->device, 3, 1, asc::DenseBlasLayout::kRowMajor, 2,
      asc::MemorySpace::kDevice);
  auto host_packed =
      MakePacked<Element>(packed->host, 3, asc::DenseBlasLayout::kRowMajor,
                          asc::MemorySpace::kPinnedHost);
  auto device_packed =
      MakePacked<Element>(packed->device, 3, asc::DenseBlasLayout::kRowMajor,
                          asc::MemorySpace::kDevice);
  FillTriangular(host_full, host_band, host_packed);
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), full->device, full->host));
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), band->device, band->host));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         packed->device, packed->host));

  CheckTriangularRoundTrip<Element>(
      test, *vector, context,
      [&](auto value) {
        return asc::CudaTrmv(
            context, asc::DenseBlasTriangle::kUpper,
            asc::DenseBlasTranspose::kNone, asc::DenseBlasDiagonal::kNonUnit,
            asc::DenseBlasMatrixView<const Element>(device_full), value);
      },
      [&](auto value) {
        return asc::CudaTrsv(
            context, asc::DenseBlasTriangle::kUpper,
            asc::DenseBlasTranspose::kNone, asc::DenseBlasDiagonal::kNonUnit,
            asc::DenseBlasMatrixView<const Element>(device_full), value);
      });
  CheckTriangularRoundTrip<Element>(
      test, *vector, context,
      [&](auto value) {
        return asc::CudaTbmv(
            context, asc::DenseBlasTriangle::kUpper,
            asc::DenseBlasTranspose::kTranspose,
            asc::DenseBlasDiagonal::kNonUnit,
            asc::DenseBlasTriangularBandView<const Element>(device_band),
            value);
      },
      [&](auto value) {
        return asc::CudaTbsv(
            context, asc::DenseBlasTriangle::kUpper,
            asc::DenseBlasTranspose::kTranspose,
            asc::DenseBlasDiagonal::kNonUnit,
            asc::DenseBlasTriangularBandView<const Element>(device_band),
            value);
      });
  CheckTriangularRoundTrip<Element>(
      test, *vector, context,
      [&](auto value) {
        return asc::CudaTpmv(
            context, asc::DenseBlasTriangle::kUpper,
            asc::DenseBlasTranspose::kConjugateTranspose,
            asc::DenseBlasDiagonal::kNonUnit,
            asc::DenseBlasPackedMatrixView<const Element>(device_packed),
            value);
      },
      [&](auto value) {
        return asc::CudaTpsv(
            context, asc::DenseBlasTriangle::kUpper,
            asc::DenseBlasTranspose::kConjugateTranspose,
            asc::DenseBlasDiagonal::kNonUnit,
            asc::DenseBlasPackedMatrixView<const Element>(device_packed),
            value);
      });
  if constexpr (asc::DenseBlasComplex<Element>) {
    std::fill_n(Data<Element>(full->host), 12, Element{0});
    std::fill_n(Data<Element>(band->host), 6, Element{0});
    auto host_full_row =
        MakeMatrix<Element>(full->host, 3, 3, asc::DenseBlasLayout::kRowMajor,
                            4, asc::MemorySpace::kPinnedHost);
    auto device_full_row =
        MakeMatrix<Element>(full->device, 3, 3, asc::DenseBlasLayout::kRowMajor,
                            4, asc::MemorySpace::kDevice);
    FillTriangular(host_full_row, host_band, host_packed);
    ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                           full->device, full->host));
    ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                           band->device, band->host));
    CheckTriangularRoundTrip<Element>(
        test, *vector, context,
        [&](auto value) {
          return asc::CudaTrmv(
              context, asc::DenseBlasTriangle::kUpper,
              asc::DenseBlasTranspose::kConjugateTranspose,
              asc::DenseBlasDiagonal::kNonUnit,
              asc::DenseBlasMatrixView<const Element>(device_full_row), value);
        },
        [&](auto value) {
          return asc::CudaTrsv(
              context, asc::DenseBlasTriangle::kUpper,
              asc::DenseBlasTranspose::kConjugateTranspose,
              asc::DenseBlasDiagonal::kNonUnit,
              asc::DenseBlasMatrixView<const Element>(device_full_row), value);
        });
    CheckTriangularRoundTrip<Element>(
        test, *vector, context,
        [&](auto value) {
          return asc::CudaTbmv(
              context, asc::DenseBlasTriangle::kUpper,
              asc::DenseBlasTranspose::kConjugateTranspose,
              asc::DenseBlasDiagonal::kNonUnit,
              asc::DenseBlasTriangularBandView<const Element>(device_band),
              value);
        },
        [&](auto value) {
          return asc::CudaTbsv(
              context, asc::DenseBlasTriangle::kUpper,
              asc::DenseBlasTranspose::kConjugateTranspose,
              asc::DenseBlasDiagonal::kNonUnit,
              asc::DenseBlasTriangularBandView<const Element>(device_band),
              value);
        });
  }

  std::fill_n(Data<Element>(full->host), 12, Element{0});
  std::fill_n(Data<Element>(band->host), 6, Element{0});
  std::fill_n(Data<Element>(packed->host), 6, Element{0});
  FillLowerTriangular(host_full, host_band, host_packed);
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), full->device, full->host));
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), band->device, band->host));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         packed->device, packed->host));
  CheckTriangularRoundTrip<Element>(
      test, *vector, context,
      [&](auto value) {
        return asc::CudaTrmv(
            context, asc::DenseBlasTriangle::kLower,
            asc::DenseBlasTranspose::kTranspose, asc::DenseBlasDiagonal::kUnit,
            asc::DenseBlasMatrixView<const Element>(device_full), value);
      },
      [&](auto value) {
        return asc::CudaTrsv(
            context, asc::DenseBlasTriangle::kLower,
            asc::DenseBlasTranspose::kTranspose, asc::DenseBlasDiagonal::kUnit,
            asc::DenseBlasMatrixView<const Element>(device_full), value);
      });
  CheckTriangularRoundTrip<Element>(
      test, *vector, context,
      [&](auto value) {
        return asc::CudaTbmv(
            context, asc::DenseBlasTriangle::kLower,
            asc::DenseBlasTranspose::kNone, asc::DenseBlasDiagonal::kUnit,
            asc::DenseBlasTriangularBandView<const Element>(device_band),
            value);
      },
      [&](auto value) {
        return asc::CudaTbsv(
            context, asc::DenseBlasTriangle::kLower,
            asc::DenseBlasTranspose::kNone, asc::DenseBlasDiagonal::kUnit,
            asc::DenseBlasTriangularBandView<const Element>(device_band),
            value);
      });
  CheckTriangularRoundTrip<Element>(
      test, *vector, context,
      [&](auto value) {
        return asc::CudaTpmv(
            context, asc::DenseBlasTriangle::kLower,
            asc::DenseBlasTranspose::kConjugateTranspose,
            asc::DenseBlasDiagonal::kUnit,
            asc::DenseBlasPackedMatrixView<const Element>(device_packed),
            value);
      },
      [&](auto value) {
        return asc::CudaTpsv(
            context, asc::DenseBlasTriangle::kLower,
            asc::DenseBlasTranspose::kConjugateTranspose,
            asc::DenseBlasDiagonal::kUnit,
            asc::DenseBlasPackedMatrixView<const Element>(device_packed),
            value);
      });
}

template <typename Element>
// Rank-update variants share one fixture and allocation assertions.
// NOLINTNEXTLINE(readability-function-size)
void TestRankUpdates(TestContext& test, asc::MemoryResource& pinned,
                     asc::MemoryResource& device,
                     asc::DenseCudaContext& context) {
  auto x_pair = AllocatePair<Element>(pinned, device, 2);
  auto y_pair = AllocatePair<Element>(pinned, device, 2);
  auto matrix_pair = AllocatePair<Element>(pinned, device, 6);
  auto packed_pair = AllocatePair<Element>(pinned, device, 3);
  ASC_DENSE_CUDA_CHECK(
      test, x_pair.ok() && y_pair.ok() && matrix_pair.ok() && packed_pair.ok());
  if (!x_pair.ok() || !y_pair.ok() || !matrix_pair.ok() || !packed_pair.ok()) {
    return;
  }
  Data<Element>(x_pair->host)[0] = Value<Element>(1, 1);
  Data<Element>(x_pair->host)[1] = Value<Element>(2, -1);
  Data<Element>(y_pair->host)[0] = Value<Element>(3, -1);
  Data<Element>(y_pair->host)[1] = Value<Element>(-1, 2);
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         x_pair->device, x_pair->host));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         y_pair->device, y_pair->host));
  auto x = MakeVector<const Element>(x_pair->device, 0, 2, 1,
                                     asc::MemorySpace::kDevice);
  auto y = MakeVector<const Element>(y_pair->device, 0, 2, 1,
                                     asc::MemorySpace::kDevice);
  auto reset_matrix = [&](asc::DenseBlasLayout layout) {
    std::fill_n(Data<Element>(matrix_pair->host), 6, Element{0});
    ASC_DENSE_CUDA_CHECK(
        test, CopyAndWait(context.execution_context(), matrix_pair->device,
                          matrix_pair->host));
    return MakeMatrix<Element>(matrix_pair->device, 2, 2, layout, 3,
                               asc::MemorySpace::kDevice);
  };
  auto reset_packed = [&](asc::DenseBlasLayout layout) {
    std::fill_n(Data<Element>(packed_pair->host), 3, Element{0});
    ASC_DENSE_CUDA_CHECK(
        test, CopyAndWait(context.execution_context(), packed_pair->device,
                          packed_pair->host));
    return MakePacked<Element>(packed_pair->device, 2, layout,
                               asc::MemorySpace::kDevice);
  };
  auto check_matrix = [&](asc::DenseBlasLayout layout, Element expected_00,
                          Element expected_01, Element expected_10,
                          Element expected_11) {
    ASC_DENSE_CUDA_CHECK(
        test, CopyAndWait(context.execution_context(), matrix_pair->host,
                          matrix_pair->device));
    auto host_matrix = MakeMatrix<Element>(matrix_pair->host, 2, 2, layout, 3,
                                           asc::MemorySpace::kPinnedHost);
    CheckNear(test, MatrixAt(host_matrix, 0, 0), expected_00);
    CheckNear(test, MatrixAt(host_matrix, 0, 1), expected_01);
    CheckNear(test, MatrixAt(host_matrix, 1, 0), expected_10);
    CheckNear(test, MatrixAt(host_matrix, 1, 1), expected_11);
  };
  auto check_packed = [&](asc::DenseBlasLayout layout,
                          asc::DenseBlasTriangle triangle, Element expected_00,
                          Element expected_off_diagonal, Element expected_11) {
    ASC_DENSE_CUDA_CHECK(
        test, CopyAndWait(context.execution_context(), packed_pair->host,
                          packed_pair->device));
    auto host_packed = MakePacked<Element>(packed_pair->host, 2, layout,
                                           asc::MemorySpace::kPinnedHost);
    CheckNear(test, PackedAt(host_packed, triangle, 0, 0), expected_00);
    if (triangle == asc::DenseBlasTriangle::kUpper) {
      CheckNear(test, PackedAt(host_packed, triangle, 0, 1),
                expected_off_diagonal);
    } else {
      CheckNear(test, PackedAt(host_packed, triangle, 1, 0),
                expected_off_diagonal);
    }
    CheckNear(test, PackedAt(host_packed, triangle, 1, 1), expected_11);
  };

  if constexpr (asc::DenseBlasComplex<Element>) {
    auto matrix = reset_matrix(asc::DenseBlasLayout::kColumnMajor);
    ASC_DENSE_CUDA_CHECK(
        test,
        Wait(test, asc::CudaGeru(context, Value<Element>(1), x, y, matrix)));
    check_matrix(asc::DenseBlasLayout::kColumnMajor, Value<Element>(4, 2),
                 Value<Element>(-3, 1), Value<Element>(5, -5),
                 Value<Element>(0, 5));
    matrix = reset_matrix(asc::DenseBlasLayout::kRowMajor);
    ASC_DENSE_CUDA_CHECK(
        test,
        Wait(test, asc::CudaGerc(context, Value<Element>(1), x, y, matrix)));
    check_matrix(asc::DenseBlasLayout::kRowMajor, Value<Element>(2, 4),
                 Value<Element>(1, -3), Value<Element>(7, -1),
                 Value<Element>(-4, -3));
    matrix = reset_matrix(asc::DenseBlasLayout::kRowMajor);
    ASC_DENSE_CUDA_CHECK(
        test, Wait(test, asc::CudaHer(context, asc::DenseBlasTriangle::kUpper,
                                      Real<Element>{2}, x, matrix)));
    check_matrix(asc::DenseBlasLayout::kRowMajor, Value<Element>(4),
                 Value<Element>(2, 6), Value<Element>(0), Value<Element>(10));
    auto packed = reset_packed(asc::DenseBlasLayout::kRowMajor);
    ASC_DENSE_CUDA_CHECK(
        test, Wait(test, asc::CudaHpr(context, asc::DenseBlasTriangle::kUpper,
                                      Real<Element>{2}, x, packed)));
    check_packed(asc::DenseBlasLayout::kRowMajor,
                 asc::DenseBlasTriangle::kUpper, Value<Element>(4),
                 Value<Element>(2, 6), Value<Element>(10));
    matrix = reset_matrix(asc::DenseBlasLayout::kRowMajor);
    ASC_DENSE_CUDA_CHECK(
        test, Wait(test, asc::CudaHer2(context, asc::DenseBlasTriangle::kLower,
                                       Value<Element>(1, 1), x, y, matrix)));
    check_matrix(asc::DenseBlasLayout::kRowMajor, Value<Element>(-4),
                 Value<Element>(0), Value<Element>(12, 8), Value<Element>(-2));
    packed = reset_packed(asc::DenseBlasLayout::kRowMajor);
    ASC_DENSE_CUDA_CHECK(
        test, Wait(test, asc::CudaHpr2(context, asc::DenseBlasTriangle::kLower,
                                       Value<Element>(1, 1), x, y, packed)));
    check_packed(asc::DenseBlasLayout::kRowMajor,
                 asc::DenseBlasTriangle::kLower, Value<Element>(-4),
                 Value<Element>(12, 8), Value<Element>(-2));
  } else {
    auto matrix = reset_matrix(asc::DenseBlasLayout::kRowMajor);
    ASC_DENSE_CUDA_CHECK(
        test,
        Wait(test, asc::CudaGer(context, Value<Element>(1), x, y, matrix)));
    check_matrix(asc::DenseBlasLayout::kRowMajor, Value<Element>(3),
                 Value<Element>(-1), Value<Element>(6), Value<Element>(-2));
    matrix = reset_matrix(asc::DenseBlasLayout::kColumnMajor);
    ASC_DENSE_CUDA_CHECK(
        test, Wait(test, asc::CudaSyr(context, asc::DenseBlasTriangle::kUpper,
                                      Value<Element>(2), x, matrix)));
    check_matrix(asc::DenseBlasLayout::kColumnMajor, Value<Element>(2),
                 Value<Element>(4), Value<Element>(0), Value<Element>(8));
    auto packed = reset_packed(asc::DenseBlasLayout::kRowMajor);
    ASC_DENSE_CUDA_CHECK(
        test, Wait(test, asc::CudaSpr(context, asc::DenseBlasTriangle::kUpper,
                                      Value<Element>(2), x, packed)));
    check_packed(asc::DenseBlasLayout::kRowMajor,
                 asc::DenseBlasTriangle::kUpper, Value<Element>(2),
                 Value<Element>(4), Value<Element>(8));
    matrix = reset_matrix(asc::DenseBlasLayout::kRowMajor);
    ASC_DENSE_CUDA_CHECK(
        test, Wait(test, asc::CudaSyr2(context, asc::DenseBlasTriangle::kLower,
                                       Value<Element>(1), x, y, matrix)));
    check_matrix(asc::DenseBlasLayout::kRowMajor, Value<Element>(6),
                 Value<Element>(0), Value<Element>(5), Value<Element>(-4));
    packed = reset_packed(asc::DenseBlasLayout::kColumnMajor);
    ASC_DENSE_CUDA_CHECK(
        test, Wait(test, asc::CudaSpr2(context, asc::DenseBlasTriangle::kLower,
                                       Value<Element>(1), x, y, packed)));
    check_packed(asc::DenseBlasLayout::kColumnMajor,
                 asc::DenseBlasTriangle::kLower, Value<Element>(6),
                 Value<Element>(5), Value<Element>(-4));
  }
}

template <typename Real>
void TestAlphaBetaEdges(TestContext& test, asc::MemoryResource& pinned,
                        asc::MemoryResource& device,
                        asc::DenseCudaContext& context) {
  auto matrix_pair = AllocatePair<Real>(pinned, device, 1);
  auto input_pair = AllocatePair<Real>(pinned, device, 1);
  auto output_pair = AllocatePair<Real>(pinned, device, 1);
  ASC_DENSE_CUDA_CHECK(test,
                       matrix_pair.ok() && input_pair.ok() && output_pair.ok());
  if (!matrix_pair.ok() || !input_pair.ok() || !output_pair.ok()) {
    return;
  }
  Data<Real>(matrix_pair->host)[0] = Real{1};
  Data<Real>(input_pair->host)[0] = Real{1};
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), matrix_pair->device,
                        matrix_pair->host));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         input_pair->device, input_pair->host));
  auto matrix = MakeMatrix<Real>(matrix_pair->device, 1, 1,
                                 asc::DenseBlasLayout::kColumnMajor, 1,
                                 asc::MemorySpace::kDevice);
  auto input = MakeVector<const Real>(input_pair->device, 0, 1, 1,
                                      asc::MemorySpace::kDevice);
  auto output =
      MakeVector<Real>(output_pair->device, 0, 1, 1, asc::MemorySpace::kDevice);
  const std::array<Real, 4> edges{
      std::numeric_limits<Real>::denorm_min(),
      std::numeric_limits<Real>::max() / Real{4},
      std::numeric_limits<Real>::infinity(),
      std::numeric_limits<Real>::quiet_NaN(),
  };
  auto check = [&](Real actual, Real expected) {
    if (std::isnan(expected)) {
      ASC_DENSE_CUDA_CHECK(test, std::isnan(actual));
    } else if (std::isinf(expected)) {
      ASC_DENSE_CUDA_CHECK(test, std::isinf(actual));
      ASC_DENSE_CUDA_CHECK(test,
                           std::signbit(actual) == std::signbit(expected));
    } else if (std::fpclassify(expected) == FP_SUBNORMAL) {
      ASC_DENSE_CUDA_EQ(test, actual, expected);
    } else {
      CheckNear(test, actual, expected);
    }
  };
  auto run = [&](Real alpha, Real beta, Real initial, Real expected) {
    Data<Real>(output_pair->host)[0] = initial;
    ASC_DENSE_CUDA_CHECK(
        test, CopyAndWait(context.execution_context(), output_pair->device,
                          output_pair->host));
    ASC_DENSE_CUDA_CHECK(
        test,
        Wait(test, asc::CudaGemv(context, asc::DenseBlasTranspose::kNone, alpha,
                                 asc::DenseBlasMatrixView<const Real>(matrix),
                                 input, beta, output)));
    ASC_DENSE_CUDA_CHECK(
        test, CopyAndWait(context.execution_context(), output_pair->host,
                          output_pair->device));
    check(Data<Real>(output_pair->host)[0], expected);
  };
  for (Real alpha : edges) {
    run(alpha, Real{0}, Real{0}, alpha);
  }
  for (Real beta : edges) {
    run(Real{0}, beta, Real{1}, beta);
  }
}

// Validation cases share a known-good descriptor and allocation checkpoints.
// NOLINTNEXTLINE(readability-function-size)
void TestValidationAndNoAllocation(
    TestContext& test, asc_dense_cuda_test::CountingResource& pinned,
    asc_dense_cuda_test::CountingResource& device,
    asc::DenseCudaContext& context) {
  auto matrix_pair = AllocatePair<float>(pinned, device, 4);
  auto vector_pair = AllocatePair<float>(pinned, device, 4);
  ASC_DENSE_CUDA_CHECK(test, matrix_pair.ok() && vector_pair.ok());
  if (!matrix_pair.ok() || !vector_pair.ok()) {
    return;
  }
  std::fill_n(Data<float>(matrix_pair->host), 4, 0.0F);
  std::fill_n(Data<float>(vector_pair->host), 4, 0.0F);
  Data<float>(matrix_pair->host)[0] = 1.0F;
  Data<float>(matrix_pair->host)[3] = 1.0F;
  Data<float>(vector_pair->host)[0] = 1.0F;
  Data<float>(vector_pair->host)[1] = 2.0F;
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), matrix_pair->device,
                        matrix_pair->host));
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), vector_pair->device,
                        vector_pair->host));
  auto matrix = MakeMatrix<float>(matrix_pair->device, 2, 2,
                                  asc::DenseBlasLayout::kColumnMajor, 2,
                                  asc::MemorySpace::kDevice);
  auto input = MakeVector<const float>(vector_pair->device, 0, 2, 1,
                                       asc::MemorySpace::kDevice);
  auto overlapping = MakeVector<float>(vector_pair->device, 0, 2, 1,
                                       asc::MemorySpace::kDevice);
  auto short_input = MakeVector<const float>(vector_pair->device, 0, 1, 1,
                                             asc::MemorySpace::kDevice);
  auto device_output = MakeVector<float>(vector_pair->device, 2, 2, 1,
                                         asc::MemorySpace::kDevice);
  auto bad_shape = asc::CudaGemv(context, asc::DenseBlasTranspose::kNone, 1.0F,
                                 asc::DenseBlasMatrixView<const float>(matrix),
                                 short_input, 0.0F, device_output);
  ASC_DENSE_CUDA_CHECK(test, !bad_shape.ok());
  if (!bad_shape.ok()) {
    ASC_DENSE_CUDA_EQ(test, bad_shape.status().code(), asc::ErrorCode::kShape);
  }
  auto bad_diagonal = asc::CudaTrmv(
      context, asc::DenseBlasTriangle::kUpper, asc::DenseBlasTranspose::kNone,
      static_cast<asc::DenseBlasDiagonal>(255),
      asc::DenseBlasMatrixView<const float>(matrix), device_output);
  ASC_DENSE_CUDA_CHECK(test, !bad_diagonal.ok());
  if (!bad_diagonal.ok()) {
    ASC_DENSE_CUDA_EQ(test, bad_diagonal.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  auto bad_triangle =
      asc::CudaSymv(context, static_cast<asc::DenseBlasTriangle>(255), 1.0F,
                    asc::DenseBlasMatrixView<const float>(matrix), input, 0.0F,
                    device_output);
  ASC_DENSE_CUDA_CHECK(test, !bad_triangle.ok());
  if (!bad_triangle.ok()) {
    ASC_DENSE_CUDA_EQ(test, bad_triangle.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  auto invalid = asc::CudaGemv(
      context, static_cast<asc::DenseBlasTranspose>(255), 1.0F,
      asc::DenseBlasMatrixView<const float>(matrix), input, 0.0F, overlapping);
  ASC_DENSE_CUDA_CHECK(test, !invalid.ok());
  if (!invalid.ok()) {
    ASC_DENSE_CUDA_EQ(test, invalid.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  auto overlap = asc::CudaGemv(context, asc::DenseBlasTranspose::kNone, 1.0F,
                               asc::DenseBlasMatrixView<const float>(matrix),
                               input, 0.0F, overlapping);
  ASC_DENSE_CUDA_CHECK(test, !overlap.ok());
  if (!overlap.ok()) {
    ASC_DENSE_CUDA_EQ(test, overlap.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }

  auto host_matrix = MakeMatrix<float>(matrix_pair->host, 2, 2,
                                       asc::DenseBlasLayout::kColumnMajor, 2,
                                       asc::MemorySpace::kPinnedHost);
  auto wrong_backend =
      asc::CudaGemv(context, asc::DenseBlasTranspose::kNone, 1.0F,
                    asc::DenseBlasMatrixView<const float>(host_matrix), input,
                    0.0F, device_output);
  ASC_DENSE_CUDA_CHECK(test, !wrong_backend.ok());
  if (!wrong_backend.ok()) {
    ASC_DENSE_CUDA_EQ(test, wrong_backend.status().code(),
                      asc::ErrorCode::kMemoryAccess);
  }

  const float nan = std::numeric_limits<float>::quiet_NaN();
  std::fill_n(Data<float>(matrix_pair->host), 4, nan);
  Data<float>(vector_pair->host)[0] = nan;
  Data<float>(vector_pair->host)[1] = nan;
  Data<float>(vector_pair->host)[2] = nan;
  Data<float>(vector_pair->host)[3] = nan;
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), matrix_pair->device,
                        matrix_pair->host));
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), vector_pair->device,
                        vector_pair->host));
  const std::size_t allocation_checkpoint = device.allocation_calls();
  ASC_DENSE_CUDA_CHECK(
      test,
      Wait(test, asc::CudaGemv(context, asc::DenseBlasTranspose::kNone, 0.0F,
                               asc::DenseBlasMatrixView<const float>(matrix),
                               input, 0.0F, device_output)));
  ASC_DENSE_CUDA_EQ(test, device.allocation_calls(), allocation_checkpoint);
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), vector_pair->host,
                        vector_pair->device));
  ASC_DENSE_CUDA_EQ(test, Data<float>(vector_pair->host)[2], 0.0F);
  ASC_DENSE_CUDA_EQ(test, Data<float>(vector_pair->host)[3], 0.0F);

  auto empty_matrix = asc::DenseBlasMatrixView<const float>::Create(
      nullptr, 0, 2, asc::DenseBlasLayout::kColumnMajor, 1,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
  auto empty_output = asc::DenseBlasVectorView<float>::Create(
      nullptr, 0, 1,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
  ASC_DENSE_CUDA_CHECK(test, empty_matrix.ok() && empty_output.ok());
  if (empty_matrix.ok() && empty_output.ok()) {
    ASC_DENSE_CUDA_CHECK(
        test,
        Wait(test, asc::CudaGemv(context, asc::DenseBlasTranspose::kNone, 1.0F,
                                 *empty_matrix, input, 0.0F, *empty_output)));
  }
  auto empty_band = asc::DenseBlasBandMatrixView<const float>::Create(
      nullptr, 0, 2, 0, 1, asc::DenseBlasLayout::kColumnMajor, 2,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
  auto empty_input = asc::DenseBlasVectorView<const float>::Create(
      nullptr, 0, 1,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
  ASC_DENSE_CUDA_CHECK(test, empty_band.ok() && empty_input.ok());
  if (empty_band.ok() && empty_input.ok()) {
    Data<float>(vector_pair->host)[2] = 3.0F;
    Data<float>(vector_pair->host)[3] = 4.0F;
    ASC_DENSE_CUDA_CHECK(
        test, CopyAndWait(context.execution_context(), vector_pair->device,
                          vector_pair->host));
    ASC_DENSE_CUDA_CHECK(
        test, Wait(test, asc::CudaGbmv(
                             context, asc::DenseBlasTranspose::kTranspose, 1.0F,
                             *empty_band, *empty_input, 2.0F, device_output)));
    ASC_DENSE_CUDA_CHECK(
        test, CopyAndWait(context.execution_context(), vector_pair->host,
                          vector_pair->device));
    ASC_DENSE_CUDA_EQ(test, Data<float>(vector_pair->host)[2], 6.0F);
    ASC_DENSE_CUDA_EQ(test, Data<float>(vector_pair->host)[3], 8.0F);
  }
}

}  // namespace

int main() {
  if (asc_dense_cuda_test::ForceNoCudaDevice()) {
    return asc_dense_cuda_test::kSkipReturnCode;
  }
  TestContext test;
  auto count = asc::CudaDeviceCount();
  ASC_DENSE_CUDA_CHECK(test, count.ok());
  if (!count.ok()) {
    return test.Finish();
  }
  if (*count == 0) {
    return asc_dense_cuda_test::kSkipReturnCode;
  }
  auto pinned_upstream =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  auto device_upstream =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  auto execution = asc::CreateCudaExecutionContext(0);
  ASC_DENSE_CUDA_CHECK(test, pinned_upstream.ok());
  ASC_DENSE_CUDA_CHECK(test, device_upstream.ok());
  ASC_DENSE_CUDA_CHECK(test, execution.ok());
  if (!pinned_upstream.ok() || !device_upstream.ok() || !execution.ok()) {
    return test.Finish();
  }
  auto context = asc::DenseCudaContext::Create(*execution);
  ASC_DENSE_CUDA_CHECK(test, context.ok());
  if (!context.ok()) {
    return test.Finish();
  }
  asc_dense_cuda_test::CountingResource pinned(**pinned_upstream);
  asc_dense_cuda_test::CountingResource device(**device_upstream);

  TestGeneralMatrixVector<float>(test, pinned, device, *context);
  TestGeneralMatrixVector<double>(test, pinned, device, *context);
  TestGeneralMatrixVector<std::complex<float>>(test, pinned, device, *context);
  TestGeneralMatrixVector<std::complex<double>>(test, pinned, device, *context);
  TestStructuredMatrixVector<float>(test, pinned, device, *context);
  TestStructuredMatrixVector<double>(test, pinned, device, *context);
  TestStructuredMatrixVector<std::complex<float>>(test, pinned, device,
                                                  *context);
  TestStructuredMatrixVector<std::complex<double>>(test, pinned, device,
                                                   *context);
  TestTriangular<float>(test, pinned, device, *context);
  TestTriangular<double>(test, pinned, device, *context);
  TestTriangular<std::complex<float>>(test, pinned, device, *context);
  TestTriangular<std::complex<double>>(test, pinned, device, *context);
  TestRankUpdates<float>(test, pinned, device, *context);
  TestRankUpdates<double>(test, pinned, device, *context);
  TestRankUpdates<std::complex<float>>(test, pinned, device, *context);
  TestRankUpdates<std::complex<double>>(test, pinned, device, *context);
  TestAlphaBetaEdges<float>(test, pinned, device, *context);
  TestAlphaBetaEdges<double>(test, pinned, device, *context);
  TestValidationAndNoAllocation(test, pinned, device, *context);

  ASC_DENSE_CUDA_EQ(test, pinned.live_allocations(), std::size_t{0});
  ASC_DENSE_CUDA_EQ(test, device.live_allocations(), std::size_t{0});
  return test.Finish();
}
