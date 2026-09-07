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
  }
  static_cast<void>(imaginary);
  return real;
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
asc::DenseBlasMatrixView<Element> MakeArrayMatrix(
    std::array<Element, 16>& buffer, asc::DenseBlasLayout layout) {
  auto result = asc::DenseBlasMatrixView<Element>::Create(
      buffer.data(), 2, 2, layout, 3,
      asc::ConstMemoryView(buffer.data(), sizeof(buffer),
                           asc::MemorySpace::kHost));
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
void Fill(asc::DenseBlasMatrixView<Element> matrix, Real<Element> seed) {
  for (asc::index_t row = 0; row < matrix.rows(); ++row) {
    for (asc::index_t column = 0; column < matrix.columns(); ++column) {
      At(matrix, row, column) =
          Value<Element>(seed + Real<Element>{2} * row + column,
                         Real<Element>{0.125} * (row - column));
    }
  }
}

template <typename Element>
bool Near(Element actual, Element expected) {
  const Real<Element> tolerance =
      Real<Element>{1536} * std::numeric_limits<Real<Element>>::epsilon() *
      (Real<Element>{1} + std::abs(expected));
  return std::abs(actual - expected) <= tolerance;
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
void CopyInputs(TestContext& test, asc::DenseCudaContext& context,
                BufferPair& left, BufferPair& right, BufferPair& output) {
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), left.device, left.host));
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), right.device, right.host));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         output.device, output.host));
}

template <typename Element>
void CheckOutput(TestContext& test, asc::DenseCudaContext& context,
                 BufferPair& output,
                 asc::DenseBlasMatrixView<const Element> expected,
                 asc::DenseBlasLayout layout) {
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         output.host, output.device));
  auto actual = MakeMatrix<Element>(output.host, 2, 2, layout, 3,
                                    asc::MemorySpace::kPinnedHost);
  for (asc::index_t row = 0; row < 2; ++row) {
    for (asc::index_t column = 0; column < 2; ++column) {
      ASC_DENSE_CUDA_CHECK(
          test, Near(At(actual, row, column), At(expected, row, column)));
    }
  }
}

template <typename Element>
// Layout and transpose rows share buffers and the same independent oracle.
// NOLINTNEXTLINE(readability-function-size)
void TestLevel3Rows(TestContext& test, asc::MemoryResource& pinned,
                    asc_dense_cuda_test::CountingResource& device,
                    asc::DenseCudaContext& context) {
  auto left_pair = AllocatePair<Element>(pinned, device, 16);
  auto right_pair = AllocatePair<Element>(pinned, device, 16);
  auto output_pair = AllocatePair<Element>(pinned, device, 16);
  ASC_DENSE_CUDA_CHECK(test,
                       left_pair.ok() && right_pair.ok() && output_pair.ok());
  if (!left_pair.ok() || !right_pair.ok() || !output_pair.ok()) {
    return;
  }
  for (asc::DenseBlasLayout layout :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    auto host_left = MakeMatrix<Element>(left_pair->host, 2, 2, layout, 3,
                                         asc::MemorySpace::kPinnedHost);
    auto host_right = MakeMatrix<Element>(right_pair->host, 2, 2, layout, 3,
                                          asc::MemorySpace::kPinnedHost);
    auto host_output = MakeMatrix<Element>(output_pair->host, 2, 2, layout, 3,
                                           asc::MemorySpace::kPinnedHost);
    auto device_left = MakeMatrix<Element>(left_pair->device, 2, 2, layout, 3,
                                           asc::MemorySpace::kDevice);
    auto device_right = MakeMatrix<Element>(right_pair->device, 2, 2, layout, 3,
                                            asc::MemorySpace::kDevice);
    auto device_output = MakeMatrix<Element>(output_pair->device, 2, 2, layout,
                                             3, asc::MemorySpace::kDevice);
    std::array<Element, 16> expected_storage{};
    auto expected = MakeArrayMatrix(expected_storage, layout);

    auto reset = [&] {
      std::fill_n(Data<Element>(left_pair->host), 16, Element{});
      std::fill_n(Data<Element>(right_pair->host), 16, Element{});
      std::fill_n(Data<Element>(output_pair->host), 16, Element{});
      expected_storage.fill(Element{});
      Fill(host_left, Real<Element>{2});
      Fill(host_right, Real<Element>{-1});
      Fill(host_output, Real<Element>{0.5});
      Fill(expected, Real<Element>{0.5});
      CopyInputs<Element>(test, context, *left_pair, *right_pair, *output_pair);
    };
    auto check = [&] {
      CheckOutput(test, context, *output_pair,
                  asc::DenseBlasMatrixView<const Element>(expected), layout);
    };
    const auto none = asc::DenseBlasTranspose::kNone;
    const bool row_major = layout == asc::DenseBlasLayout::kRowMajor;
    const auto triangle = row_major ? asc::DenseBlasTriangle::kLower
                                    : asc::DenseBlasTriangle::kUpper;
    const auto gemm_left_operation =
        row_major ? asc::DenseBlasTranspose::kConjugateTranspose : none;
    const auto gemm_right_operation =
        row_major ? asc::DenseBlasTranspose::kTranspose : none;
    const auto structured_side =
        row_major ? asc::DenseBlasSide::kLeft : asc::DenseBlasSide::kRight;
    const auto symmetric_operation =
        row_major ? asc::DenseBlasTranspose::kTranspose : none;
    const auto hermitian_operation =
        row_major ? asc::DenseBlasTranspose::kConjugateTranspose : none;
    const Element alpha =
        Value<Element>(Real<Element>{0.75}, Real<Element>{0.125});
    const Element beta = Value<Element>(Real<Element>{-0.25});
    const std::size_t allocation_checkpoint = device.allocation_calls();

    reset();
    ASC_DENSE_CUDA_CHECK(
        test, asc::Gemm(asc::ExecutionContext::Serial(), gemm_left_operation,
                        gemm_right_operation, alpha,
                        asc::DenseBlasMatrixView<const Element>(host_left),
                        asc::DenseBlasMatrixView<const Element>(host_right),
                        beta, expected)
                  .ok());
    Wait(test, asc::CudaGemm(
                   context, gemm_left_operation, gemm_right_operation, alpha,
                   asc::DenseBlasMatrixView<const Element>(device_left),
                   asc::DenseBlasMatrixView<const Element>(device_right), beta,
                   device_output));
    check();

    const Element nan =
        Value<Element>(std::numeric_limits<Real<Element>>::quiet_NaN());
    std::fill_n(Data<Element>(left_pair->host), 16, nan);
    std::fill_n(Data<Element>(right_pair->host), 16, nan);
    std::fill_n(Data<Element>(output_pair->host), 16, nan);
    expected_storage.fill(Element{});
    CopyInputs<Element>(test, context, *left_pair, *right_pair, *output_pair);
    Wait(test,
         asc::CudaGemm(context, gemm_left_operation, gemm_right_operation,
                       Element{0},
                       asc::DenseBlasMatrixView<const Element>(device_left),
                       asc::DenseBlasMatrixView<const Element>(device_right),
                       Element{0}, device_output));
    check();

    reset();
    ASC_DENSE_CUDA_CHECK(
        test,
        asc::Symm(asc::ExecutionContext::Serial(), structured_side, triangle,
                  alpha, asc::DenseBlasMatrixView<const Element>(host_left),
                  asc::DenseBlasMatrixView<const Element>(host_right), beta,
                  expected)
            .ok());
    Wait(test,
         asc::CudaSymm(context, structured_side, triangle, alpha,
                       asc::DenseBlasMatrixView<const Element>(device_left),
                       asc::DenseBlasMatrixView<const Element>(device_right),
                       beta, device_output));
    check();

    if constexpr (asc::DenseBlasComplex<Element>) {
      reset();
      ASC_DENSE_CUDA_CHECK(
          test,
          asc::Hemm(asc::ExecutionContext::Serial(), structured_side, triangle,
                    alpha, asc::DenseBlasMatrixView<const Element>(host_left),
                    asc::DenseBlasMatrixView<const Element>(host_right), beta,
                    expected)
              .ok());
      Wait(test,
           asc::CudaHemm(context, structured_side, triangle, alpha,
                         asc::DenseBlasMatrixView<const Element>(device_left),
                         asc::DenseBlasMatrixView<const Element>(device_right),
                         beta, device_output));
      check();
    }

    reset();
    ASC_DENSE_CUDA_CHECK(
        test, asc::Syrk(asc::ExecutionContext::Serial(), triangle,
                        symmetric_operation, alpha,
                        asc::DenseBlasMatrixView<const Element>(host_left),
                        beta, expected)
                  .ok());
    Wait(test,
         asc::CudaSyrk(context, triangle, symmetric_operation, alpha,
                       asc::DenseBlasMatrixView<const Element>(device_left),
                       beta, device_output));
    check();

    reset();
    ASC_DENSE_CUDA_CHECK(
        test, asc::Syr2k(asc::ExecutionContext::Serial(), triangle,
                         symmetric_operation, alpha,
                         asc::DenseBlasMatrixView<const Element>(host_left),
                         asc::DenseBlasMatrixView<const Element>(host_right),
                         beta, expected)
                  .ok());
    Wait(test,
         asc::CudaSyr2k(context, triangle, symmetric_operation, alpha,
                        asc::DenseBlasMatrixView<const Element>(device_left),
                        asc::DenseBlasMatrixView<const Element>(device_right),
                        beta, device_output));
    check();

    if constexpr (asc::DenseBlasComplex<Element>) {
      reset();
      ASC_DENSE_CUDA_CHECK(
          test, asc::Herk(asc::ExecutionContext::Serial(), triangle,
                          hermitian_operation, Real<Element>{0.75},
                          asc::DenseBlasMatrixView<const Element>(host_left),
                          Real<Element>{-0.25}, expected)
                    .ok());
      Wait(test,
           asc::CudaHerk(context, triangle, hermitian_operation,
                         Real<Element>{0.75},
                         asc::DenseBlasMatrixView<const Element>(device_left),
                         Real<Element>{-0.25}, device_output));
      check();

      reset();
      ASC_DENSE_CUDA_CHECK(
          test, asc::Her2k(asc::ExecutionContext::Serial(), triangle,
                           hermitian_operation, alpha,
                           asc::DenseBlasMatrixView<const Element>(host_left),
                           asc::DenseBlasMatrixView<const Element>(host_right),
                           Real<Element>{-0.25}, expected)
                    .ok());
      Wait(test,
           asc::CudaHer2k(context, triangle, hermitian_operation, alpha,
                          asc::DenseBlasMatrixView<const Element>(device_left),
                          asc::DenseBlasMatrixView<const Element>(device_right),
                          Real<Element>{-0.25}, device_output));
      check();
    }

    reset();
    ASC_DENSE_CUDA_CHECK(
        test,
        asc::Trmm(
            asc::ExecutionContext::Serial(),
            row_major ? asc::DenseBlasSide::kRight : asc::DenseBlasSide::kLeft,
            triangle, gemm_left_operation,
            row_major ? asc::DenseBlasDiagonal::kUnit
                      : asc::DenseBlasDiagonal::kNonUnit,
            alpha, asc::DenseBlasMatrixView<const Element>(host_left), expected)
            .ok());
    Wait(test,
         asc::CudaTrmm(
             context,
             row_major ? asc::DenseBlasSide::kRight : asc::DenseBlasSide::kLeft,
             triangle, gemm_left_operation,
             row_major ? asc::DenseBlasDiagonal::kUnit
                       : asc::DenseBlasDiagonal::kNonUnit,
             alpha, asc::DenseBlasMatrixView<const Element>(device_left),
             device_output));
    check();

    reset();
    ASC_DENSE_CUDA_CHECK(
        test,
        asc::Trsm(
            asc::ExecutionContext::Serial(),
            row_major ? asc::DenseBlasSide::kLeft : asc::DenseBlasSide::kRight,
            triangle,
            row_major ? asc::DenseBlasTranspose::kTranspose
                      : asc::DenseBlasTranspose::kConjugateTranspose,
            row_major ? asc::DenseBlasDiagonal::kNonUnit
                      : asc::DenseBlasDiagonal::kUnit,
            alpha, asc::DenseBlasMatrixView<const Element>(host_left), expected)
            .ok());
    Wait(test,
         asc::CudaTrsm(
             context,
             row_major ? asc::DenseBlasSide::kLeft : asc::DenseBlasSide::kRight,
             triangle,
             row_major ? asc::DenseBlasTranspose::kTranspose
                       : asc::DenseBlasTranspose::kConjugateTranspose,
             row_major ? asc::DenseBlasDiagonal::kNonUnit
                       : asc::DenseBlasDiagonal::kUnit,
             alpha, asc::DenseBlasMatrixView<const Element>(device_left),
             device_output));
    check();
    ASC_DENSE_CUDA_EQ(test, device.allocation_calls(), allocation_checkpoint);
  }
}

// Edge and validation cases share a known-good provider baseline.
// NOLINTNEXTLINE(readability-function-size)
void TestValidationAndEdges(TestContext& test, asc::MemoryResource& pinned,
                            asc_dense_cuda_test::CountingResource& device,
                            asc::DenseCudaContext& context) {
  auto left_pair = AllocatePair<float>(pinned, device, 16);
  auto right_pair = AllocatePair<float>(pinned, device, 16);
  auto output_pair = AllocatePair<float>(pinned, device, 16);
  ASC_DENSE_CUDA_CHECK(test,
                       left_pair.ok() && right_pair.ok() && output_pair.ok());
  if (!left_pair.ok() || !right_pair.ok() || !output_pair.ok()) {
    return;
  }
  auto left = MakeMatrix<const float>(left_pair->device, 2, 2,
                                      asc::DenseBlasLayout::kColumnMajor, 3,
                                      asc::MemorySpace::kDevice);
  auto right = MakeMatrix<const float>(right_pair->device, 2, 2,
                                       asc::DenseBlasLayout::kColumnMajor, 3,
                                       asc::MemorySpace::kDevice);
  auto output = MakeMatrix<float>(output_pair->device, 2, 2,
                                  asc::DenseBlasLayout::kColumnMajor, 3,
                                  asc::MemorySpace::kDevice);
  auto row_right = MakeMatrix<const float>(right_pair->device, 2, 2,
                                           asc::DenseBlasLayout::kRowMajor, 3,
                                           asc::MemorySpace::kDevice);
  auto bad_layout = asc::CudaGemm(context, asc::DenseBlasTranspose::kNone,
                                  asc::DenseBlasTranspose::kNone, 1.0F, left,
                                  row_right, 0.0F, output);
  ASC_DENSE_CUDA_CHECK(test, !bad_layout.ok());
  if (!bad_layout.ok()) {
    ASC_DENSE_CUDA_EQ(test, bad_layout.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  auto invalid = asc::CudaTrmm(
      context, static_cast<asc::DenseBlasSide>(255),
      asc::DenseBlasTriangle::kUpper, asc::DenseBlasTranspose::kNone,
      asc::DenseBlasDiagonal::kNonUnit, 1.0F, left, output);
  ASC_DENSE_CUDA_CHECK(test, !invalid.ok());
  if (!invalid.ok()) {
    ASC_DENSE_CUDA_EQ(test, invalid.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  auto overlap =
      asc::CudaGemm(context, asc::DenseBlasTranspose::kNone,
                    asc::DenseBlasTranspose::kNone, 1.0F, left, right, 0.0F,
                    MakeMatrix<float>(left_pair->device, 2, 2,
                                      asc::DenseBlasLayout::kColumnMajor, 3,
                                      asc::MemorySpace::kDevice));
  ASC_DENSE_CUDA_CHECK(test, !overlap.ok());
  if (!overlap.ok()) {
    ASC_DENSE_CUDA_EQ(test, overlap.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  auto host_left = MakeMatrix<const float>(left_pair->host, 2, 2,
                                           asc::DenseBlasLayout::kColumnMajor,
                                           3, asc::MemorySpace::kPinnedHost);
  auto wrong_backend = asc::CudaGemm(context, asc::DenseBlasTranspose::kNone,
                                     asc::DenseBlasTranspose::kNone, 1.0F,
                                     host_left, right, 0.0F, output);
  ASC_DENSE_CUDA_CHECK(test, !wrong_backend.ok());
  if (!wrong_backend.ok()) {
    ASC_DENSE_CUDA_EQ(test, wrong_backend.status().code(),
                      asc::ErrorCode::kMemoryAccess);
  }

  const float nan = std::numeric_limits<float>::quiet_NaN();
  std::fill_n(Data<float>(left_pair->host), 16, nan);
  std::fill_n(Data<float>(right_pair->host), 16, nan);
  std::fill_n(Data<float>(output_pair->host), 16, nan);
  CopyInputs<float>(test, context, *left_pair, *right_pair, *output_pair);
  const std::size_t allocation_checkpoint = device.allocation_calls();
  ASC_DENSE_CUDA_CHECK(
      test, Wait(test, asc::CudaGemm(context, asc::DenseBlasTranspose::kNone,
                                     asc::DenseBlasTranspose::kNone, 0.0F, left,
                                     right, 0.0F, output)));
  ASC_DENSE_CUDA_EQ(test, device.allocation_calls(), allocation_checkpoint);
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), output_pair->host,
                        output_pair->device));
  auto host_output = MakeMatrix<float>(output_pair->host, 2, 2,
                                       asc::DenseBlasLayout::kColumnMajor, 3,
                                       asc::MemorySpace::kPinnedHost);
  for (asc::index_t row = 0; row < 2; ++row) {
    for (asc::index_t column = 0; column < 2; ++column) {
      ASC_DENSE_CUDA_EQ(test, At(host_output, row, column), 0.0F);
    }
  }

  auto empty = asc::DenseBlasMatrixView<const float>::Create(
      nullptr, 0, 2, asc::DenseBlasLayout::kColumnMajor, 1,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
  auto empty_output = asc::DenseBlasMatrixView<float>::Create(
      nullptr, 0, 2, asc::DenseBlasLayout::kColumnMajor, 1,
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kDevice));
  ASC_DENSE_CUDA_CHECK(test, empty.ok() && empty_output.ok());
  if (empty.ok() && empty_output.ok()) {
    ASC_DENSE_CUDA_CHECK(
        test, Wait(test, asc::CudaGemm(context, asc::DenseBlasTranspose::kNone,
                                       asc::DenseBlasTranspose::kNone, 1.0F,
                                       *empty, right, 0.0F, *empty_output)));
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
  TestLevel3Rows<float>(test, pinned, device, *context);
  TestLevel3Rows<double>(test, pinned, device, *context);
  TestLevel3Rows<std::complex<float>>(test, pinned, device, *context);
  TestLevel3Rows<std::complex<double>>(test, pinned, device, *context);
  TestValidationAndEdges(test, pinned, device, *context);
  ASC_DENSE_CUDA_EQ(test, pinned.live_allocations(), std::size_t{0});
  ASC_DENSE_CUDA_EQ(test, device.live_allocations(), std::size_t{0});
  return test.Finish();
}
