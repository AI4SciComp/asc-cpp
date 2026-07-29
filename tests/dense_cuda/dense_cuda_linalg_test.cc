#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "../../src/core/cuda/runtime_test_internal.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/providers/cuda.h"
#include "asc/dense/view.h"
#include "counting_resource.h"
#include "device_test_helpers.h"
#include "test_support.h"

namespace {

using asc_dense_cuda_test::CopyAndWait;
using asc_dense_cuda_test::Data;
using asc_dense_cuda_test::MakeRightView;
using asc_dense_cuda_test::MakeView;

template <typename T>
void TestCopyScalAxpyRankOne(
    asc_dense_cuda_test::TestContext& test,
    asc_dense_cuda_test::CountingResource& pinned_resource,
    asc_dense_cuda_test::CountingResource& device_resource,
    asc::DenseCudaContext& context) {
  constexpr std::size_t kSpan = 9;
  constexpr std::size_t kBytes = kSpan * sizeof(T);
  auto host_source = asc::Buffer::Allocate(pinned_resource, kBytes, alignof(T));
  auto host_destination =
      asc::Buffer::Allocate(pinned_resource, kBytes, alignof(T));
  auto device_source =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(T));
  auto device_destination =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(T));
  ASC_DENSE_CUDA_CHECK(test, host_source.ok());
  ASC_DENSE_CUDA_CHECK(test, host_destination.ok());
  ASC_DENSE_CUDA_CHECK(test, device_source.ok());
  ASC_DENSE_CUDA_CHECK(test, device_destination.ok());
  if (!host_source.ok() || !host_destination.ok() || !device_source.ok() ||
      !device_destination.ok()) {
    return;
  }

  for (std::size_t index = 0; index < kSpan; ++index) {
    Data<T>(*host_source)[index] = static_cast<T>(-101);
    Data<T>(*host_destination)[index] = static_cast<T>(-303);
  }
  for (std::size_t logical = 0; logical < 5; ++logical) {
    Data<T>(*host_source)[logical * 2] =
        static_cast<T>(static_cast<std::int32_t>(logical) - 2);
    Data<T>(*host_destination)[logical * 2] =
        static_cast<T>(3 * static_cast<std::int32_t>(logical) + 1);
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *device_source, *host_source));
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), *device_destination,
                        *host_destination));

  const std::array<asc::extent_t, 1> extents{5};
  const std::array<asc::stride_t, 1> strides{2};
  auto source_mutable = MakeView(Data<T>(*device_source), extents, strides,
                                 asc::MemorySpace::kDevice);
  asc::DenseView<const T, 1> source = source_mutable;
  auto destination = MakeView(Data<T>(*device_destination), extents, strides,
                              asc::MemorySpace::kDevice);

  std::size_t allocation_checkpoint = device_resource.allocation_calls();
  auto copied = asc::CudaCopy(context, source, destination);
  ASC_DENSE_CUDA_CHECK(test, copied.ok());
  if (copied.ok()) {
    ASC_DENSE_CUDA_CHECK(test, copied->Wait().ok());
  }
  ASC_DENSE_CUDA_EQ(test, device_resource.allocation_calls(),
                    allocation_checkpoint);
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), *host_destination,
                        *device_destination));
  for (std::size_t index = 0; index < kSpan; ++index) {
    const T expected =
        index % 2 == 0 ? Data<T>(*host_source)[index] : static_cast<T>(-303);
    ASC_DENSE_CUDA_EQ(test, Data<T>(*host_destination)[index], expected);
  }

  auto scaled = asc::CudaScal(context, static_cast<T>(-2), destination);
  ASC_DENSE_CUDA_CHECK(test, scaled.ok());
  if (scaled.ok()) {
    ASC_DENSE_CUDA_CHECK(test, scaled->Wait().ok());
  }
  ASC_DENSE_CUDA_EQ(test, device_resource.allocation_calls(),
                    allocation_checkpoint);
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), *host_destination,
                        *device_destination));
  for (std::size_t logical = 0; logical < 5; ++logical) {
    ASC_DENSE_CUDA_EQ(test, Data<T>(*host_destination)[logical * 2],
                      static_cast<T>(-2) * Data<T>(*host_source)[logical * 2]);
  }

  auto axpy = asc::CudaAxpy(context, static_cast<T>(3), source, destination);
  ASC_DENSE_CUDA_CHECK(test, axpy.ok());
  if (axpy.ok()) {
    auto query = axpy->Query();
    ASC_DENSE_CUDA_CHECK(test, query.ok());
    ASC_DENSE_CUDA_CHECK(test, axpy->Wait().ok());
  }
  ASC_DENSE_CUDA_EQ(test, device_resource.allocation_calls(),
                    allocation_checkpoint);
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), *host_destination,
                        *device_destination));
  for (std::size_t logical = 0; logical < 5; ++logical) {
    ASC_DENSE_CUDA_EQ(test, Data<T>(*host_destination)[logical * 2],
                      Data<T>(*host_source)[logical * 2]);
  }

  auto self_copy = asc::CudaCopy(context, source, source_mutable);
  ASC_DENSE_CUDA_CHECK(test, self_copy.ok());
  if (self_copy.ok()) {
    ASC_DENSE_CUDA_CHECK(test, self_copy->Wait().ok());
  }
  auto self_axpy =
      asc::CudaAxpy(context, static_cast<T>(2), source, source_mutable);
  ASC_DENSE_CUDA_CHECK(test, self_axpy.ok());
  if (self_axpy.ok()) {
    ASC_DENSE_CUDA_CHECK(test, self_axpy->Wait().ok());
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *host_source, *device_source));
  for (std::size_t logical = 0; logical < 5; ++logical) {
    ASC_DENSE_CUDA_EQ(
        test, Data<T>(*host_source)[logical * 2],
        static_cast<T>(3) *
            static_cast<T>(static_cast<std::int32_t>(logical) - 2));
  }
}

template <typename T>
void TestRankTwoRightLayout(asc_dense_cuda_test::TestContext& test,
                            asc::MemoryResource& pinned_resource,
                            asc::MemoryResource& device_resource,
                            asc::DenseCudaContext& context) {
  constexpr std::size_t kElements = 6;
  constexpr std::size_t kBytes = kElements * sizeof(T);
  auto host_source = asc::Buffer::Allocate(pinned_resource, kBytes, alignof(T));
  auto host_destination =
      asc::Buffer::Allocate(pinned_resource, kBytes, alignof(T));
  auto device_source =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(T));
  auto device_destination =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(T));
  ASC_DENSE_CUDA_CHECK(test, host_source.ok());
  ASC_DENSE_CUDA_CHECK(test, host_destination.ok());
  ASC_DENSE_CUDA_CHECK(test, device_source.ok());
  ASC_DENSE_CUDA_CHECK(test, device_destination.ok());
  if (!host_source.ok() || !host_destination.ok() || !device_source.ok() ||
      !device_destination.ok()) {
    return;
  }
  for (std::size_t index = 0; index < kElements; ++index) {
    Data<T>(*host_source)[index] = static_cast<T>(index + 1);
    Data<T>(*host_destination)[index] = static_cast<T>(2);
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *device_source, *host_source));
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), *device_destination,
                        *host_destination));

  const std::array<asc::extent_t, 2> extents{2, 3};
  const std::array<asc::stride_t, 2> strides{3, 1};
  auto source_mutable = MakeView(Data<T>(*device_source), extents, strides,
                                 asc::MemorySpace::kDevice);
  asc::DenseView<const T, 2> source = source_mutable;
  auto destination = MakeView(Data<T>(*device_destination), extents, strides,
                              asc::MemorySpace::kDevice);
  auto copy = asc::CudaCopy(context, source, destination);
  ASC_DENSE_CUDA_CHECK(test, copy.ok());
  if (copy.ok()) {
    ASC_DENSE_CUDA_CHECK(test, copy->Wait().ok());
  }
  auto scal = asc::CudaScal(context, static_cast<T>(0.5), destination);
  ASC_DENSE_CUDA_CHECK(test, scal.ok());
  if (scal.ok()) {
    ASC_DENSE_CUDA_CHECK(test, scal->Wait().ok());
  }
  auto axpy = asc::CudaAxpy(context, static_cast<T>(2), source, destination);
  ASC_DENSE_CUDA_CHECK(test, axpy.ok());
  if (axpy.ok()) {
    ASC_DENSE_CUDA_CHECK(test, axpy->Wait().ok());
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), *host_destination,
                        *device_destination));
  for (std::size_t index = 0; index < kElements; ++index) {
    ASC_DENSE_CUDA_NEAR(test, Data<T>(*host_destination)[index],
                        static_cast<T>(2.5) * Data<T>(*host_source)[index],
                        static_cast<T>(1));
  }
}

template <typename T>
void TestGemv(asc_dense_cuda_test::TestContext& test,
              asc::MemoryResource& pinned_resource,
              asc::MemoryResource& device_resource,
              asc::DenseCudaContext& context, asc::MatrixOperation operation) {
  constexpr std::size_t kRows = 3;
  constexpr std::size_t kColumns = 2;
  constexpr std::size_t kLeadingDimension = 5;
  constexpr std::size_t kMatrixSpan =
      (kColumns - 1) * kLeadingDimension + kRows;
  const std::size_t input_size =
      operation == asc::MatrixOperation::kNone ? kColumns : kRows;
  const std::size_t output_size =
      operation == asc::MatrixOperation::kNone ? kRows : kColumns;
  const std::size_t input_span = (input_size - 1) * 2 + 1;
  const std::size_t output_span = (output_size - 1) * 2 + 1;

  auto host_matrix = asc::Buffer::Allocate(pinned_resource,
                                           kMatrixSpan * sizeof(T), alignof(T));
  auto host_input = asc::Buffer::Allocate(pinned_resource,
                                          input_span * sizeof(T), alignof(T));
  auto host_output = asc::Buffer::Allocate(pinned_resource,
                                           output_span * sizeof(T), alignof(T));
  auto device_matrix = asc::Buffer::Allocate(
      device_resource, kMatrixSpan * sizeof(T), alignof(T));
  auto device_input = asc::Buffer::Allocate(device_resource,
                                            input_span * sizeof(T), alignof(T));
  auto device_output = asc::Buffer::Allocate(
      device_resource, output_span * sizeof(T), alignof(T));
  ASC_DENSE_CUDA_CHECK(test, host_matrix.ok());
  ASC_DENSE_CUDA_CHECK(test, host_input.ok());
  ASC_DENSE_CUDA_CHECK(test, host_output.ok());
  ASC_DENSE_CUDA_CHECK(test, device_matrix.ok());
  ASC_DENSE_CUDA_CHECK(test, device_input.ok());
  ASC_DENSE_CUDA_CHECK(test, device_output.ok());
  if (!host_matrix.ok() || !host_input.ok() || !host_output.ok() ||
      !device_matrix.ok() || !device_input.ok() || !device_output.ok()) {
    return;
  }
  for (std::size_t index = 0; index < kMatrixSpan; ++index) {
    Data<T>(*host_matrix)[index] = static_cast<T>(-71);
  }
  for (std::size_t column = 0; column < kColumns; ++column) {
    for (std::size_t row = 0; row < kRows; ++row) {
      Data<T>(*host_matrix)[row + column * kLeadingDimension] =
          static_cast<T>(1 + row + 2 * column);
    }
  }
  for (std::size_t index = 0; index < input_span; ++index) {
    Data<T>(*host_input)[index] = static_cast<T>(-19);
  }
  for (std::size_t index = 0; index < input_size; ++index) {
    Data<T>(*host_input)[2 * index] =
        static_cast<T>(static_cast<std::int32_t>(index) - 1);
  }
  for (std::size_t index = 0; index < output_span; ++index) {
    Data<T>(*host_output)[index] = std::numeric_limits<T>::quiet_NaN();
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *device_matrix, *host_matrix));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *device_input, *host_input));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *device_output, *host_output));

  auto matrix_mutable = MakeView(
      Data<T>(*device_matrix), std::array<asc::extent_t, 2>{kRows, kColumns},
      std::array<asc::stride_t, 2>{1, kLeadingDimension},
      asc::MemorySpace::kDevice);
  asc::DenseView<const T, 2> matrix = matrix_mutable;
  auto input_mutable = MakeView(
      Data<T>(*device_input),
      std::array<asc::extent_t, 1>{static_cast<asc::extent_t>(input_size)},
      std::array<asc::stride_t, 1>{2}, asc::MemorySpace::kDevice);
  asc::DenseView<const T, 1> input = input_mutable;
  auto output = MakeView(
      Data<T>(*device_output),
      std::array<asc::extent_t, 1>{static_cast<asc::extent_t>(output_size)},
      std::array<asc::stride_t, 1>{2}, asc::MemorySpace::kDevice);

  auto event = asc::CudaGemv(context, operation, static_cast<T>(2), matrix,
                             input, static_cast<T>(0), output);
  ASC_DENSE_CUDA_CHECK(test, event.ok());
  if (event.ok()) {
    auto query = event->Query();
    ASC_DENSE_CUDA_CHECK(test, query.ok());
    ASC_DENSE_CUDA_CHECK(test, event->Wait().ok());
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *host_output, *device_output));
  for (std::size_t output_index = 0; output_index < output_size;
       ++output_index) {
    long double expected = 0;
    for (std::size_t inner = 0; inner < input_size; ++inner) {
      const std::size_t row =
          operation == asc::MatrixOperation::kNone ? output_index : inner;
      const std::size_t column =
          operation == asc::MatrixOperation::kNone ? inner : output_index;
      expected += static_cast<long double>(
                      Data<T>(*host_matrix)[row + column * kLeadingDimension]) *
                  static_cast<long double>(Data<T>(*host_input)[2 * inner]);
    }
    expected *= 2;
    ASC_DENSE_CUDA_NEAR(test, Data<T>(*host_output)[2 * output_index],
                        static_cast<T>(expected), static_cast<T>(input_size));
  }
}

template <typename T>
void TestGemm(asc_dense_cuda_test::TestContext& test,
              asc::MemoryResource& pinned_resource,
              asc::MemoryResource& device_resource,
              asc::DenseCudaContext& context,
              asc::MatrixOperation left_operation,
              asc::MatrixOperation right_operation) {
  constexpr std::size_t kM = 2;
  constexpr std::size_t kN = 3;
  constexpr std::size_t kK = 4;
  const std::size_t left_rows =
      left_operation == asc::MatrixOperation::kNone ? kM : kK;
  const std::size_t left_columns =
      left_operation == asc::MatrixOperation::kNone ? kK : kM;
  const std::size_t right_rows =
      right_operation == asc::MatrixOperation::kNone ? kK : kN;
  const std::size_t right_columns =
      right_operation == asc::MatrixOperation::kNone ? kN : kK;
  const std::size_t left_ld = left_rows + 2;
  const std::size_t right_ld = right_rows + 2;
  constexpr std::size_t kOutputLd = kM + 2;
  const std::size_t left_span = (left_columns - 1) * left_ld + left_rows;
  const std::size_t right_span = (right_columns - 1) * right_ld + right_rows;
  constexpr std::size_t kOutputSpan = (kN - 1) * kOutputLd + kM;

  auto host_left =
      asc::Buffer::Allocate(pinned_resource, left_span * sizeof(T), alignof(T));
  auto host_right = asc::Buffer::Allocate(pinned_resource,
                                          right_span * sizeof(T), alignof(T));
  auto host_output = asc::Buffer::Allocate(pinned_resource,
                                           kOutputSpan * sizeof(T), alignof(T));
  auto device_left =
      asc::Buffer::Allocate(device_resource, left_span * sizeof(T), alignof(T));
  auto device_right = asc::Buffer::Allocate(device_resource,
                                            right_span * sizeof(T), alignof(T));
  auto device_output = asc::Buffer::Allocate(
      device_resource, kOutputSpan * sizeof(T), alignof(T));
  ASC_DENSE_CUDA_CHECK(test, host_left.ok());
  ASC_DENSE_CUDA_CHECK(test, host_right.ok());
  ASC_DENSE_CUDA_CHECK(test, host_output.ok());
  ASC_DENSE_CUDA_CHECK(test, device_left.ok());
  ASC_DENSE_CUDA_CHECK(test, device_right.ok());
  ASC_DENSE_CUDA_CHECK(test, device_output.ok());
  if (!host_left.ok() || !host_right.ok() || !host_output.ok() ||
      !device_left.ok() || !device_right.ok() || !device_output.ok()) {
    return;
  }
  std::fill_n(Data<T>(*host_left), left_span, static_cast<T>(-41));
  std::fill_n(Data<T>(*host_right), right_span, static_cast<T>(-43));
  std::fill_n(Data<T>(*host_output), kOutputSpan,
              std::numeric_limits<T>::quiet_NaN());

  const auto left_value = [](std::size_t row, std::size_t column) {
    return static_cast<std::int32_t>(1 + row - 2 * column);
  };
  const auto right_value = [](std::size_t row, std::size_t column) {
    return static_cast<std::int32_t>(2 - row + 3 * column);
  };
  for (std::size_t row = 0; row < kM; ++row) {
    for (std::size_t inner = 0; inner < kK; ++inner) {
      const std::size_t physical_row =
          left_operation == asc::MatrixOperation::kNone ? row : inner;
      const std::size_t physical_column =
          left_operation == asc::MatrixOperation::kNone ? inner : row;
      Data<T>(*host_left)[physical_row + physical_column * left_ld] =
          static_cast<T>(left_value(row, inner));
    }
  }
  for (std::size_t inner = 0; inner < kK; ++inner) {
    for (std::size_t column = 0; column < kN; ++column) {
      const std::size_t physical_row =
          right_operation == asc::MatrixOperation::kNone ? inner : column;
      const std::size_t physical_column =
          right_operation == asc::MatrixOperation::kNone ? column : inner;
      Data<T>(*host_right)[physical_row + physical_column * right_ld] =
          static_cast<T>(right_value(inner, column));
    }
  }
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), *device_left, *host_left));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *device_right, *host_right));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *device_output, *host_output));

  auto left_mutable = MakeView(
      Data<T>(*device_left),
      std::array<asc::extent_t, 2>{static_cast<asc::extent_t>(left_rows),
                                   static_cast<asc::extent_t>(left_columns)},
      std::array<asc::stride_t, 2>{1, static_cast<asc::stride_t>(left_ld)},
      asc::MemorySpace::kDevice);
  auto right_mutable = MakeView(
      Data<T>(*device_right),
      std::array<asc::extent_t, 2>{static_cast<asc::extent_t>(right_rows),
                                   static_cast<asc::extent_t>(right_columns)},
      std::array<asc::stride_t, 2>{1, static_cast<asc::stride_t>(right_ld)},
      asc::MemorySpace::kDevice);
  asc::DenseView<const T, 2> left = left_mutable;
  asc::DenseView<const T, 2> right = right_mutable;
  auto output = MakeView(
      Data<T>(*device_output), std::array<asc::extent_t, 2>{kM, kN},
      std::array<asc::stride_t, 2>{1, kOutputLd}, asc::MemorySpace::kDevice);

  auto event = asc::CudaGemm(context, left_operation, right_operation,
                             static_cast<T>(1.5), left, right,
                             static_cast<T>(0), output);
  ASC_DENSE_CUDA_CHECK(test, event.ok());
  if (event.ok()) {
    ASC_DENSE_CUDA_CHECK(test, event->Wait().ok());
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *host_output, *device_output));
  for (std::size_t column = 0; column < kN; ++column) {
    for (std::size_t row = 0; row < kM; ++row) {
      long double expected = 0;
      for (std::size_t inner = 0; inner < kK; ++inner) {
        expected += static_cast<long double>(left_value(row, inner)) *
                    static_cast<long double>(right_value(inner, column));
      }
      expected *= 1.5L;
      ASC_DENSE_CUDA_NEAR(test, Data<T>(*host_output)[row + column * kOutputLd],
                          static_cast<T>(expected), static_cast<T>(kK));
    }
  }
}

#if defined(ASC_CPP_CUDA_RUNTIME_TEST_HOOKS)
void TestPostEnqueueEventFailures(asc_dense_cuda_test::TestContext& test,
                                  asc::MemoryResource& pinned_resource,
                                  asc::MemoryResource& device_resource,
                                  asc::DenseCudaContext& context) {
  constexpr std::size_t kElements = 4;
  constexpr std::size_t kBytes = kElements * sizeof(double);
  auto host_left =
      asc::Buffer::Allocate(pinned_resource, kBytes, alignof(double));
  auto host_right =
      asc::Buffer::Allocate(pinned_resource, kBytes, alignof(double));
  auto host_output =
      asc::Buffer::Allocate(pinned_resource, kBytes, alignof(double));
  auto device_left =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(double));
  auto device_right =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(double));
  auto device_output =
      asc::Buffer::Allocate(device_resource, kBytes, alignof(double));
  ASC_DENSE_CUDA_CHECK(test, host_left.ok());
  ASC_DENSE_CUDA_CHECK(test, host_right.ok());
  ASC_DENSE_CUDA_CHECK(test, host_output.ok());
  ASC_DENSE_CUDA_CHECK(test, device_left.ok());
  ASC_DENSE_CUDA_CHECK(test, device_right.ok());
  ASC_DENSE_CUDA_CHECK(test, device_output.ok());
  if (!host_left.ok() || !host_right.ok() || !host_output.ok() ||
      !device_left.ok() || !device_right.ok() || !device_output.ok()) {
    return;
  }

  constexpr std::array<double, kElements> kIdentity{1.0, 0.0, 0.0, 1.0};
  constexpr std::array<double, kElements> kRight{2.0, 4.0, 3.0, 5.0};
  std::copy(kIdentity.begin(), kIdentity.end(), Data<double>(*host_left));
  std::copy(kRight.begin(), kRight.end(), Data<double>(*host_right));
  std::fill_n(Data<double>(*host_output), kElements, -1.0);
  ASC_DENSE_CUDA_CHECK(
      test, CopyAndWait(context.execution_context(), *device_left, *host_left));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *device_right, *host_right));
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *device_output, *host_output));

  constexpr std::array<asc::extent_t, 2> kShape{2, 2};
  constexpr std::array<asc::stride_t, 2> kStrides{1, 2};
  auto left_mutable = MakeView(Data<double>(*device_left), kShape, kStrides,
                               asc::MemorySpace::kDevice);
  auto right_mutable = MakeView(Data<double>(*device_right), kShape, kStrides,
                                asc::MemorySpace::kDevice);
  auto output = MakeView(Data<double>(*device_output), kShape, kStrides,
                         asc::MemorySpace::kDevice);
  const asc::DenseView<const double, 2> left = left_mutable;
  const asc::DenseView<const double, 2> right = right_mutable;

  asc::internal_core_cuda::ResetCudaRuntimeTestState();
  asc::internal_core_cuda::SetCudaRuntimeFault(
      asc::internal_core_cuda::CudaRuntimeFault::kEventRecord);
  auto gemm_failure =
      asc::CudaGemm(context, asc::MatrixOperation::kNone,
                    asc::MatrixOperation::kNone, 1.0, left, right, 0.0, output);
  ASC_DENSE_CUDA_CHECK(test, !gemm_failure.ok());
  if (!gemm_failure.ok()) {
    ASC_DENSE_CUDA_EQ(test, gemm_failure.status().code(),
                      asc::ErrorCode::kProvider);
    ASC_DENSE_CUDA_CHECK(test, gemm_failure.status().native_code() != 0);
  }
  ASC_DENSE_CUDA_EQ(test, asc::internal_core_cuda::CudaRuntimeDrainCount(),
                    std::size_t{1});
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *host_output, *device_output));
  for (std::size_t index = 0; index < kElements; ++index) {
    ASC_DENSE_CUDA_EQ(test, Data<double>(*host_output)[index], kRight[index]);
  }

  asc::internal_core_cuda::ResetCudaRuntimeTestState();
  asc::internal_core_cuda::SetCudaRuntimeFault(
      asc::internal_core_cuda::CudaRuntimeFault::kEventCreate);
  auto kernel_failure = asc::CudaScal(context, 2.0, output);
  ASC_DENSE_CUDA_CHECK(test, !kernel_failure.ok());
  if (!kernel_failure.ok()) {
    ASC_DENSE_CUDA_EQ(test, kernel_failure.status().code(),
                      asc::ErrorCode::kProvider);
    ASC_DENSE_CUDA_CHECK(test, kernel_failure.status().native_code() != 0);
  }
  ASC_DENSE_CUDA_EQ(test, asc::internal_core_cuda::CudaRuntimeDrainCount(),
                    std::size_t{1});
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *host_output, *device_output));
  for (std::size_t index = 0; index < kElements; ++index) {
    ASC_DENSE_CUDA_EQ(test, Data<double>(*host_output)[index],
                      2.0 * kRight[index]);
  }
  asc::internal_core_cuda::ResetCudaRuntimeTestState();
}
#endif

void TestValidation(asc_dense_cuda_test::TestContext& test,
                    asc::MemoryResource& device_resource,
                    asc::DenseCudaContext& context) {
  auto storage = asc::Buffer::Allocate(device_resource, 32 * sizeof(float),
                                       alignof(float));
  auto other = asc::Buffer::Allocate(device_resource, 32 * sizeof(float),
                                     alignof(float));
  ASC_DENSE_CUDA_CHECK(test, storage.ok());
  ASC_DENSE_CUDA_CHECK(test, other.ok());
  if (!storage.ok() || !other.ok()) {
    return;
  }

  auto right_layout =
      MakeView(Data<float>(*storage), std::array<asc::extent_t, 2>{2, 2},
               std::array<asc::stride_t, 2>{2, 1}, asc::MemorySpace::kDevice);
  auto actual_right_layout =
      MakeRightView(Data<float>(*storage), std::array<asc::extent_t, 2>{2, 2},
                    asc::MemorySpace::kDevice);
  auto column_major =
      MakeView(Data<float>(*other), std::array<asc::extent_t, 2>{2, 2},
               std::array<asc::stride_t, 2>{1, 2}, asc::MemorySpace::kDevice);
  asc::DenseView<const float, 2> right_const = right_layout;
  asc::DenseView<const float, 2> column_const = column_major;
  auto unsupported_layout = asc::CudaGemm(
      context, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone, 1.0F,
      right_const, column_const, 0.0F, column_major);
  ASC_DENSE_CUDA_CHECK(test, !unsupported_layout.ok());
  if (!unsupported_layout.ok()) {
    ASC_DENSE_CUDA_EQ(test, unsupported_layout.status().code(),
                      asc::ErrorCode::kUnsupported);
  }
  asc::DenseView<const float, 2> actual_right_const = actual_right_layout;
  auto unsupported_actual_right = asc::CudaGemm(
      context, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone, 1.0F,
      actual_right_const, column_const, 0.0F, column_major);
  ASC_DENSE_CUDA_CHECK(test, !unsupported_actual_right.ok());
  if (!unsupported_actual_right.ok()) {
    ASC_DENSE_CUDA_EQ(test, unsupported_actual_right.status().code(),
                      asc::ErrorCode::kUnsupported);
  }

  auto degenerate_right_left =
      MakeRightView(Data<float>(*storage), std::array<asc::extent_t, 2>{1, 1},
                    asc::MemorySpace::kDevice);
  auto degenerate_right_right = MakeRightView(
      Data<float>(*storage) + 8, std::array<asc::extent_t, 2>{1, 1},
      asc::MemorySpace::kDevice);
  auto degenerate_right_output = MakeRightView(
      Data<float>(*storage) + 16, std::array<asc::extent_t, 2>{1, 1},
      asc::MemorySpace::kDevice);
  asc::DenseView<const float, 2> degenerate_right_left_const =
      degenerate_right_left;
  asc::DenseView<const float, 2> degenerate_right_right_const =
      degenerate_right_right;
  auto unsupported_degenerate_right = asc::CudaGemm(
      context, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone, 1.0F,
      degenerate_right_left_const, degenerate_right_right_const, 0.0F,
      degenerate_right_output);
  ASC_DENSE_CUDA_CHECK(test, !unsupported_degenerate_right.ok());
  if (!unsupported_degenerate_right.ok()) {
    ASC_DENSE_CUDA_EQ(test, unsupported_degenerate_right.status().code(),
                      asc::ErrorCode::kUnsupported);
  }

  auto alias_failure = asc::CudaGemm(
      context, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone, 1.0F,
      column_const, column_const, 0.0F, column_major);
  ASC_DENSE_CUDA_CHECK(test, !alias_failure.ok());
  if (!alias_failure.ok()) {
    ASC_DENSE_CUDA_EQ(test, alias_failure.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }

  auto partial_source =
      MakeView(Data<float>(*storage), std::array<asc::extent_t, 1>{4},
               std::array<asc::stride_t, 1>{1}, asc::MemorySpace::kDevice);
  auto partial_destination =
      MakeView(Data<float>(*storage) + 1, std::array<asc::extent_t, 1>{4},
               std::array<asc::stride_t, 1>{1}, asc::MemorySpace::kDevice);
  asc::DenseView<const float, 1> partial_source_const = partial_source;
  auto partial_copy =
      asc::CudaCopy(context, partial_source_const, partial_destination);
  ASC_DENSE_CUDA_CHECK(test, !partial_copy.ok());
  auto partial_axpy =
      asc::CudaAxpy(context, 1.0F, partial_source_const, partial_destination);
  ASC_DENSE_CUDA_CHECK(test, !partial_axpy.ok());

  auto serial_dense =
      asc::DenseCudaContext::Create(asc::ExecutionContext::Serial());
  ASC_DENSE_CUDA_CHECK(test, !serial_dense.ok());
  if (!serial_dense.ok()) {
    ASC_DENSE_CUDA_EQ(test, serial_dense.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
}

void TestProviderWidthValidation(
    asc_dense_cuda_test::TestContext& test,
    asc_dense_cuda_test::CountingResource& device_resource,
    asc::DenseCudaContext& context) {
  auto storage = asc::Buffer::Allocate(device_resource, 32 * sizeof(float),
                                       alignof(float));
  ASC_DENSE_CUDA_CHECK(test, storage.ok());
  if (!storage.ok()) {
    return;
  }
  const asc::extent_t beyond_provider =
      static_cast<asc::extent_t>(std::numeric_limits<int>::max()) + 1;
  const asc::stride_t beyond_provider_stride =
      static_cast<asc::stride_t>(std::numeric_limits<int>::max()) + 1;
  const std::size_t allocation_checkpoint = device_resource.allocation_calls();

  auto wide_matrix =
      MakeView(static_cast<const float*>(nullptr),
               std::array<asc::extent_t, 2>{0, beyond_provider},
               std::array<asc::stride_t, 2>{1, 1}, asc::MemorySpace::kDevice);
  auto wide_input =
      MakeView(static_cast<const float*>(storage->data()),
               std::array<asc::extent_t, 1>{beyond_provider},
               std::array<asc::stride_t, 1>{1}, asc::MemorySpace::kDevice);
  auto empty_output =
      MakeView(static_cast<float*>(nullptr), std::array<asc::extent_t, 1>{0},
               std::array<asc::stride_t, 1>{1}, asc::MemorySpace::kDevice);
  auto extent_overflow =
      asc::CudaGemv(context, asc::MatrixOperation::kNone, 1.0F, wide_matrix,
                    wide_input, 0.0F, empty_output);
  ASC_DENSE_CUDA_CHECK(test, !extent_overflow.ok());
  if (!extent_overflow.ok()) {
    ASC_DENSE_CUDA_EQ(test, extent_overflow.status().code(),
                      asc::ErrorCode::kOverflow);
  }

  auto wide_leading_dimension_matrix = MakeView(
      static_cast<const float*>(nullptr), std::array<asc::extent_t, 2>{0, 1},
      std::array<asc::stride_t, 2>{1, beyond_provider_stride},
      asc::MemorySpace::kDevice);
  auto unit_input =
      MakeView(static_cast<const float*>(storage->data()),
               std::array<asc::extent_t, 1>{1}, std::array<asc::stride_t, 1>{1},
               asc::MemorySpace::kDevice);
  auto leading_dimension_overflow = asc::CudaGemv(
      context, asc::MatrixOperation::kNone, 1.0F, wide_leading_dimension_matrix,
      unit_input, 0.0F, empty_output);
  ASC_DENSE_CUDA_CHECK(test, !leading_dimension_overflow.ok());
  if (!leading_dimension_overflow.ok()) {
    ASC_DENSE_CUDA_EQ(test, leading_dimension_overflow.status().code(),
                      asc::ErrorCode::kOverflow);
  }

  auto unit_matrix =
      MakeView(static_cast<const float*>(storage->data()),
               std::array<asc::extent_t, 2>{1, 1},
               std::array<asc::stride_t, 2>{1, 1}, asc::MemorySpace::kDevice);
  auto wide_increment_input =
      MakeView(static_cast<const float*>(Data<float>(*storage) + 8),
               std::array<asc::extent_t, 1>{1},
               std::array<asc::stride_t, 1>{beyond_provider_stride},
               asc::MemorySpace::kDevice);
  auto unit_output =
      MakeView(Data<float>(*storage) + 16, std::array<asc::extent_t, 1>{1},
               std::array<asc::stride_t, 1>{1}, asc::MemorySpace::kDevice);
  auto input_increment_overflow =
      asc::CudaGemv(context, asc::MatrixOperation::kNone, 1.0F, unit_matrix,
                    wide_increment_input, 0.0F, unit_output);
  ASC_DENSE_CUDA_CHECK(test, !input_increment_overflow.ok());
  if (!input_increment_overflow.ok()) {
    ASC_DENSE_CUDA_EQ(test, input_increment_overflow.status().code(),
                      asc::ErrorCode::kOverflow);
  }

  auto wide_increment_output =
      MakeView(Data<float>(*storage) + 16, std::array<asc::extent_t, 1>{1},
               std::array<asc::stride_t, 1>{beyond_provider_stride},
               asc::MemorySpace::kDevice);
  auto output_increment_overflow =
      asc::CudaGemv(context, asc::MatrixOperation::kNone, 1.0F, unit_matrix,
                    unit_input, 0.0F, wide_increment_output);
  ASC_DENSE_CUDA_CHECK(test, !output_increment_overflow.ok());
  if (!output_increment_overflow.ok()) {
    ASC_DENSE_CUDA_EQ(test, output_increment_overflow.status().code(),
                      asc::ErrorCode::kOverflow);
  }

  auto wide_left =
      MakeView(static_cast<const float*>(nullptr),
               std::array<asc::extent_t, 2>{0, beyond_provider},
               std::array<asc::stride_t, 2>{1, 1}, asc::MemorySpace::kDevice);
  auto wide_right =
      MakeView(static_cast<const float*>(nullptr),
               std::array<asc::extent_t, 2>{beyond_provider, 0},
               std::array<asc::stride_t, 2>{1, 1}, asc::MemorySpace::kDevice);
  auto empty_matrix_output =
      MakeView(static_cast<float*>(nullptr), std::array<asc::extent_t, 2>{0, 0},
               std::array<asc::stride_t, 2>{1, 1}, asc::MemorySpace::kDevice);
  auto gemm_extent_overflow = asc::CudaGemm(
      context, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone, 1.0F,
      wide_left, wide_right, 0.0F, empty_matrix_output);
  ASC_DENSE_CUDA_CHECK(test, !gemm_extent_overflow.ok());
  if (!gemm_extent_overflow.ok()) {
    ASC_DENSE_CUDA_EQ(test, gemm_extent_overflow.status().code(),
                      asc::ErrorCode::kOverflow);
  }

  ASC_DENSE_CUDA_EQ(test, device_resource.allocation_calls(),
                    allocation_checkpoint);
}

void TestDegenerateDimensions(asc_dense_cuda_test::TestContext& test,
                              asc::MemoryResource& pinned_resource,
                              asc::MemoryResource& device_resource,
                              asc::DenseCudaContext& context) {
  constexpr std::size_t kGemvOutputSize = 3;
  auto host_vector = asc::Buffer::Allocate(
      pinned_resource, kGemvOutputSize * sizeof(double), alignof(double));
  auto device_vector = asc::Buffer::Allocate(
      device_resource, kGemvOutputSize * sizeof(double), alignof(double));
  ASC_DENSE_CUDA_CHECK(test, host_vector.ok());
  ASC_DENSE_CUDA_CHECK(test, device_vector.ok());
  if (!host_vector.ok() || !device_vector.ok()) {
    return;
  }
  for (std::size_t index = 0; index < kGemvOutputSize; ++index) {
    Data<double>(*host_vector)[index] = static_cast<double>(index + 1);
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *device_vector, *host_vector));
  auto empty_matrix = MakeView(
      static_cast<const double*>(nullptr), std::array<asc::extent_t, 2>{3, 0},
      std::array<asc::stride_t, 2>{1, 3}, asc::MemorySpace::kDevice);
  auto empty_input = MakeView(
      static_cast<const double*>(nullptr), std::array<asc::extent_t, 1>{0},
      std::array<asc::stride_t, 1>{1}, asc::MemorySpace::kDevice);
  auto vector_output =
      MakeView(Data<double>(*device_vector), std::array<asc::extent_t, 1>{3},
               std::array<asc::stride_t, 1>{1}, asc::MemorySpace::kDevice);
  auto gemv = asc::CudaGemv(context, asc::MatrixOperation::kNone, 5.0,
                            empty_matrix, empty_input, 2.0, vector_output);
  ASC_DENSE_CUDA_CHECK(test, gemv.ok());
  if (gemv.ok()) {
    ASC_DENSE_CUDA_CHECK(test, gemv->Wait().ok());
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *host_vector, *device_vector));
  for (std::size_t index = 0; index < kGemvOutputSize; ++index) {
    ASC_DENSE_CUDA_EQ(test, Data<double>(*host_vector)[index],
                      static_cast<double>(2 * (index + 1)));
  }

  constexpr std::size_t kGemmOutputSize = 6;
  auto host_matrix = asc::Buffer::Allocate(
      pinned_resource, kGemmOutputSize * sizeof(float), alignof(float));
  auto device_matrix = asc::Buffer::Allocate(
      device_resource, kGemmOutputSize * sizeof(float), alignof(float));
  ASC_DENSE_CUDA_CHECK(test, host_matrix.ok());
  ASC_DENSE_CUDA_CHECK(test, device_matrix.ok());
  if (!host_matrix.ok() || !device_matrix.ok()) {
    return;
  }
  std::fill_n(Data<float>(*host_matrix), kGemmOutputSize,
              std::numeric_limits<float>::quiet_NaN());
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *device_matrix, *host_matrix));
  auto empty_left = MakeView(
      static_cast<const float*>(nullptr), std::array<asc::extent_t, 2>{2, 0},
      std::array<asc::stride_t, 2>{1, 2}, asc::MemorySpace::kDevice);
  auto empty_right = MakeView(
      static_cast<const float*>(nullptr), std::array<asc::extent_t, 2>{0, 3},
      std::array<asc::stride_t, 2>{1, 1}, asc::MemorySpace::kDevice);
  auto matrix_output =
      MakeView(Data<float>(*device_matrix), std::array<asc::extent_t, 2>{2, 3},
               std::array<asc::stride_t, 2>{1, 2}, asc::MemorySpace::kDevice);
  auto gemm = asc::CudaGemm(context, asc::MatrixOperation::kNone,
                            asc::MatrixOperation::kNone, 3.0F, empty_left,
                            empty_right, 0.0F, matrix_output);
  ASC_DENSE_CUDA_CHECK(test, gemm.ok());
  if (gemm.ok()) {
    ASC_DENSE_CUDA_CHECK(test, gemm->Wait().ok());
  }
  ASC_DENSE_CUDA_CHECK(test, CopyAndWait(context.execution_context(),
                                         *host_matrix, *device_matrix));
  for (std::size_t index = 0; index < kGemmOutputSize; ++index) {
    ASC_DENSE_CUDA_EQ(test, Data<float>(*host_matrix)[index], 0.0F);
  }
}

}  // namespace

int main() {
  if (asc_dense_cuda_test::ForceNoCudaDevice()) {
    return asc_dense_cuda_test::kSkipReturnCode;
  }
  asc_dense_cuda_test::TestContext test;
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
  auto dense_context = asc::DenseCudaContext::Create(*execution);
  ASC_DENSE_CUDA_CHECK(test, dense_context.ok());
  if (!dense_context.ok()) {
    return test.Finish();
  }
  asc_dense_cuda_test::CountingResource pinned(**pinned_upstream);
  asc_dense_cuda_test::CountingResource device(**device_upstream);

  TestCopyScalAxpyRankOne<float>(test, pinned, device, *dense_context);
  TestCopyScalAxpyRankOne<double>(test, pinned, device, *dense_context);
  TestRankTwoRightLayout<float>(test, pinned, device, *dense_context);
  TestRankTwoRightLayout<double>(test, pinned, device, *dense_context);
  TestGemv<float>(test, pinned, device, *dense_context,
                  asc::MatrixOperation::kNone);
  TestGemv<double>(test, pinned, device, *dense_context,
                   asc::MatrixOperation::kTranspose);
  TestGemm<float>(test, pinned, device, *dense_context,
                  asc::MatrixOperation::kNone, asc::MatrixOperation::kNone);
  TestGemm<double>(test, pinned, device, *dense_context,
                   asc::MatrixOperation::kNone,
                   asc::MatrixOperation::kTranspose);
  TestGemm<double>(test, pinned, device, *dense_context,
                   asc::MatrixOperation::kTranspose,
                   asc::MatrixOperation::kNone);
  TestGemm<float>(test, pinned, device, *dense_context,
                  asc::MatrixOperation::kTranspose,
                  asc::MatrixOperation::kTranspose);
  TestDegenerateDimensions(test, pinned, device, *dense_context);
#if defined(ASC_CPP_CUDA_RUNTIME_TEST_HOOKS)
  TestPostEnqueueEventFailures(test, pinned, device, *dense_context);
#endif
  TestValidation(test, device, *dense_context);
  TestProviderWidthValidation(test, device, *dense_context);

  ASC_DENSE_CUDA_EQ(test, pinned.live_allocations(), std::size_t{0});
  ASC_DENSE_CUDA_EQ(test, device.live_allocations(), std::size_t{0});
  ASC_DENSE_CUDA_EQ(test, pinned.allocated_bytes(), pinned.deallocated_bytes());
  ASC_DENSE_CUDA_EQ(test, device.allocated_bytes(), device.deallocated_bytes());
  return test.Finish();
}
