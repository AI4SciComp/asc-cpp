#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/layout.h"
#include "asc/dense/linalg.h"
#include "asc/dense/providers/cuda.h"
#include "asc/dense/view.h"
#include "test_support.h"

namespace {

using Shape1 = asc::Extents<asc::kDynamicExtent>;
using Shape2 = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

asc::Device CudaDevice() { return asc::Device{asc::Backend::kCuda, 0}; }

bool Wait(asc::Result<asc::CompletionEvent>& event,
          asc_dense_cuda_test::TestContext& test) {
  ASC_DENSE_CUDA_CHECK(test, event.ok());
  if (!event.ok()) {
    return false;
  }
  const asc::Status status = event->Wait();
  ASC_DENSE_CUDA_CHECK(test, status.ok());
  return status.ok();
}

template <typename Scalar>
void SetHost1D(asc::DenseArray<Scalar, Shape1>& array,
               std::span<const Scalar> values,
               asc_dense_cuda_test::TestContext& test) {
  auto view = array.view();
  ASC_DENSE_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  ASC_DENSE_CUDA_EQ(test, view->shape()[0],
                    static_cast<asc::extent_t>(values.size()));
  for (std::size_t index = 0; index < values.size(); ++index) {
    const std::array<asc::index_t, 1> coordinate = {
        static_cast<asc::index_t>(index)};
    auto element = view->At(coordinate);
    ASC_DENSE_CUDA_CHECK(test, element.ok());
    if (element.ok()) {
      **element = values[index];
    }
  }
}

template <typename Scalar>
void SetHost2D(asc::DenseArray<Scalar, Shape2>& array,
               std::span<const Scalar> column_major_values,
               asc_dense_cuda_test::TestContext& test) {
  auto view = array.view();
  ASC_DENSE_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  const std::size_t rows = static_cast<std::size_t>(view->shape()[0]);
  const std::size_t columns = static_cast<std::size_t>(view->shape()[1]);
  ASC_DENSE_CUDA_EQ(test, column_major_values.size(), rows * columns);
  for (std::size_t column = 0; column < columns; ++column) {
    for (std::size_t row = 0; row < rows; ++row) {
      const std::array<asc::index_t, 2> coordinate = {
          static_cast<asc::index_t>(row), static_cast<asc::index_t>(column)};
      auto element = view->At(coordinate);
      ASC_DENSE_CUDA_CHECK(test, element.ok());
      if (element.ok()) {
        **element = column_major_values[column * rows + row];
      }
    }
  }
}

template <typename Scalar, typename Layout>
asc::Result<asc::DenseArray<Scalar, Shape1>> Upload1D(
    std::span<const Scalar> values, Layout layout,
    asc::MemoryResource& device_resource,
    const asc::ExecutionContext& execution,
    asc_dense_cuda_test::TestContext& test) {
  const auto shape = Shape1::Create(static_cast<asc::extent_t>(values.size()));
  if (!shape.ok()) {
    return shape.status();
  }
  asc::HostMemoryResource host;
  auto source = asc::DenseArray<Scalar, Shape1>::Create(*shape, host, layout);
  if (!source.ok()) {
    return source.status();
  }
  SetHost1D(*source, values, test);
  return source->Clone(device_resource, execution);
}

template <typename Scalar, typename Layout>
asc::Result<asc::DenseArray<Scalar, Shape2>> Upload2D(
    asc::extent_t rows, asc::extent_t columns,
    std::span<const Scalar> column_major_values, Layout layout,
    asc::MemoryResource& device_resource,
    const asc::ExecutionContext& execution,
    asc_dense_cuda_test::TestContext& test) {
  const auto shape = Shape2::Create(rows, columns);
  if (!shape.ok()) {
    return shape.status();
  }
  asc::HostMemoryResource host;
  auto source = asc::DenseArray<Scalar, Shape2>::Create(*shape, host, layout);
  if (!source.ok()) {
    return source.status();
  }
  SetHost2D(*source, column_major_values, test);
  return source->Clone(device_resource, execution);
}

template <typename Scalar>
std::vector<Scalar> Download1D(const asc::DenseArray<Scalar, Shape1>& array,
                               const asc::ExecutionContext& execution,
                               asc_dense_cuda_test::TestContext& test) {
  asc::HostMemoryResource host;
  auto clone = array.Clone(host, execution);
  ASC_DENSE_CUDA_CHECK(test, clone.ok());
  if (!clone.ok()) {
    return {};
  }
  auto view = clone->view();
  ASC_DENSE_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return {};
  }
  std::vector<Scalar> values(static_cast<std::size_t>(view->shape()[0]));
  for (std::size_t index = 0; index < values.size(); ++index) {
    const std::array<asc::index_t, 1> coordinate = {
        static_cast<asc::index_t>(index)};
    auto element = view->At(coordinate);
    ASC_DENSE_CUDA_CHECK(test, element.ok());
    if (element.ok()) {
      values[index] = **element;
    }
  }
  return values;
}

template <typename Scalar>
std::vector<Scalar> Download2D(const asc::DenseArray<Scalar, Shape2>& array,
                               const asc::ExecutionContext& execution,
                               asc_dense_cuda_test::TestContext& test) {
  asc::HostMemoryResource host;
  auto clone = array.Clone(host, execution);
  ASC_DENSE_CUDA_CHECK(test, clone.ok());
  if (!clone.ok()) {
    return {};
  }
  auto view = clone->view();
  ASC_DENSE_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return {};
  }
  const std::size_t rows = static_cast<std::size_t>(view->shape()[0]);
  const std::size_t columns = static_cast<std::size_t>(view->shape()[1]);
  std::vector<Scalar> values(rows * columns);
  for (std::size_t column = 0; column < columns; ++column) {
    for (std::size_t row = 0; row < rows; ++row) {
      const std::array<asc::index_t, 2> coordinate = {
          static_cast<asc::index_t>(row), static_cast<asc::index_t>(column)};
      auto element = view->At(coordinate);
      ASC_DENSE_CUDA_CHECK(test, element.ok());
      if (element.ok()) {
        values[column * rows + row] = **element;
      }
    }
  }
  return values;
}

template <typename Scalar>
void CheckExact(std::span<const Scalar> actual,
                std::span<const Scalar> expected,
                asc_dense_cuda_test::TestContext& test) {
  ASC_DENSE_CUDA_EQ(test, actual.size(), expected.size());
  if (actual.size() != expected.size()) {
    return;
  }
  for (std::size_t index = 0; index < actual.size(); ++index) {
    ASC_DENSE_CUDA_EQ(test, actual[index], expected[index]);
  }
}

template <typename Scalar>
void CheckCopyScalAxpy(asc_dense_cuda_test::TestContext& test) {
  auto execution = asc::CreateCudaExecutionContext(CudaDevice());
  auto provider = asc::DenseCudaContext::Create(*execution);
  auto resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  ASC_DENSE_CUDA_CHECK(test, execution.ok());
  ASC_DENSE_CUDA_CHECK(test, provider.ok());
  ASC_DENSE_CUDA_CHECK(test, resource.ok());
  if (!execution.ok() || !provider.ok() || !resource.ok()) {
    return;
  }

  const std::array<Scalar, 4> source_values = {Scalar{1}, Scalar{-2}, Scalar{4},
                                               Scalar{-8}};
  const std::array<Scalar, 4> zeros = {};
  auto source = Upload1D<Scalar>(source_values, asc::LayoutLeft{}, **resource,
                                 *execution, test);
  auto destination =
      Upload1D<Scalar>(zeros, asc::LayoutLeft{}, **resource, *execution, test);
  auto source_view = source->view();
  auto destination_view = destination->view();
  const asc::DenseView<const Scalar, 1> const_source(*source_view);

  auto copied = asc::CudaCopy(*provider, const_source, *destination_view);
  Wait(copied, test);
  CheckExact<Scalar>(Download1D(*destination, *execution, test), source_values,
                     test);

  auto scaled = asc::CudaScal(*provider, Scalar{-2}, *destination_view);
  Wait(scaled, test);
  const std::array<Scalar, 4> scaled_values = {Scalar{-2}, Scalar{4},
                                               Scalar{-8}, Scalar{16}};
  CheckExact<Scalar>(Download1D(*destination, *execution, test), scaled_values,
                     test);

  auto axpy =
      asc::CudaAxpy(*provider, Scalar{0.5}, const_source, *destination_view);
  Wait(axpy, test);
  const std::array<Scalar, 4> axpy_values = {Scalar{-1.5}, Scalar{3},
                                             Scalar{-6}, Scalar{12}};
  CheckExact<Scalar>(Download1D(*destination, *execution, test), axpy_values,
                     test);

  const asc::DenseView<const Scalar, 1> const_destination(*destination_view);
  auto exact_self =
      asc::CudaCopy(*provider, const_destination, *destination_view);
  Wait(exact_self, test);
  auto exact_self_axpy =
      asc::CudaAxpy(*provider, Scalar{1}, const_destination, *destination_view);
  Wait(exact_self_axpy, test);
  const std::array<Scalar, 4> exact_self_axpy_values = {
      Scalar{-3}, Scalar{6}, Scalar{-12}, Scalar{24}};
  CheckExact<Scalar>(Download1D(*destination, *execution, test),
                     exact_self_axpy_values, test);

  const std::array<Scalar, 4> special_values = {
      Scalar{0}, -Scalar{0}, std::numeric_limits<Scalar>::infinity(),
      std::numeric_limits<Scalar>::quiet_NaN()};
  auto special = Upload1D<Scalar>(special_values, asc::LayoutLeft{}, **resource,
                                  *execution, test);
  auto special_destination =
      Upload1D<Scalar>(zeros, asc::LayoutLeft{}, **resource, *execution, test);
  const asc::DenseView<const Scalar, 1> special_source(*special->view());
  auto special_copy =
      asc::CudaCopy(*provider, special_source, *special_destination->view());
  Wait(special_copy, test);
  const auto copied_special =
      Download1D(*special_destination, *execution, test);
  ASC_DENSE_CUDA_EQ(test, std::signbit(copied_special[0]),
                    std::signbit(special_values[0]));
  ASC_DENSE_CUDA_EQ(test, std::signbit(copied_special[1]),
                    std::signbit(special_values[1]));
  ASC_DENSE_CUDA_EQ(test, copied_special[2], special_values[2]);
  ASC_DENSE_CUDA_CHECK(test, std::isnan(copied_special[3]));

  const std::array<Scalar, 4> matrix_values = {Scalar{1}, Scalar{2}, Scalar{3},
                                               Scalar{4}};
  auto matrix_source = Upload2D<Scalar>(2, 2, matrix_values, asc::LayoutRight{},
                                        **resource, *execution, test);
  auto matrix_destination = Upload2D<Scalar>(2, 2, zeros, asc::LayoutLeft{},
                                             **resource, *execution, test);
  const asc::DenseView<const Scalar, 2> const_matrix(*matrix_source->view());
  auto matrix_copy =
      asc::CudaCopy(*provider, const_matrix, *matrix_destination->view());
  Wait(matrix_copy, test);
  CheckExact<Scalar>(Download2D(*matrix_destination, *execution, test),
                     matrix_values, test);

  auto matrix_scale =
      asc::CudaScal(*provider, Scalar{-2}, *matrix_destination->view());
  Wait(matrix_scale, test);
  const std::array<Scalar, 4> scaled_matrix_values = {Scalar{-2}, Scalar{-4},
                                                      Scalar{-6}, Scalar{-8}};
  CheckExact<Scalar>(Download2D(*matrix_destination, *execution, test),
                     scaled_matrix_values, test);

  auto matrix_axpy = asc::CudaAxpy(*provider, Scalar{0.5}, const_matrix,
                                   *matrix_destination->view());
  Wait(matrix_axpy, test);
  const std::array<Scalar, 4> axpy_matrix_values = {Scalar{-1.5}, Scalar{-3},
                                                    Scalar{-4.5}, Scalar{-6}};
  CheckExact<Scalar>(Download2D(*matrix_destination, *execution, test),
                     axpy_matrix_values, test);
}

template <typename Scalar>
void CheckGemv(asc_dense_cuda_test::TestContext& test) {
  auto execution = asc::CreateCudaExecutionContext(CudaDevice());
  auto provider = asc::DenseCudaContext::Create(*execution);
  auto resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  if (!execution.ok() || !provider.ok() || !resource.ok()) {
    ASC_DENSE_CUDA_CHECK(test, false);
    return;
  }

  const std::array<Scalar, 6> matrix_values = {Scalar{1}, Scalar{4}, Scalar{2},
                                               Scalar{5}, Scalar{3}, Scalar{6}};
  auto matrix = Upload2D<Scalar>(2, 3, matrix_values, asc::LayoutLeft{},
                                 **resource, *execution, test);
  const std::array<Scalar, 3> input_values = {Scalar{1}, Scalar{-1}, Scalar{2}};
  auto input = Upload1D<Scalar>(input_values, asc::LayoutLeft{}, **resource,
                                *execution, test);
  const std::array<Scalar, 2> output_values = {Scalar{10}, Scalar{20}};
  auto output = Upload1D<Scalar>(output_values, asc::LayoutLeft{}, **resource,
                                 *execution, test);
  const asc::DenseView<const Scalar, 2> matrix_view(*matrix->view());
  const asc::DenseView<const Scalar, 1> input_view(*input->view());
  auto event =
      asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{2},
                    matrix_view, input_view, Scalar{0.5}, *output->view());
  Wait(event, test);
  const auto actual = Download1D(*output, *execution, test);
  const std::array<long double, 2> expected = {15.0L, 32.0L};
  for (std::size_t row = 0; row < expected.size(); ++row) {
    ASC_DENSE_CUDA_NEAR(test, actual[row], expected[row], expected[row], 3);
  }

  const std::array<Scalar, 2> transpose_input_values = {Scalar{2}, Scalar{-1}};
  auto transpose_input = Upload1D<Scalar>(
      transpose_input_values, asc::LayoutLeft{}, **resource, *execution, test);
  const Scalar nan = std::numeric_limits<Scalar>::quiet_NaN();
  const std::array<Scalar, 3> nan_output = {nan, nan, nan};
  auto transpose_output = Upload1D<Scalar>(nan_output, asc::LayoutLeft{},
                                           **resource, *execution, test);
  const asc::DenseView<const Scalar, 1> transpose_input_view(
      *transpose_input->view());
  auto transpose_event = asc::CudaGemv(
      *provider, asc::MatrixOperation::kTranspose, Scalar{1}, matrix_view,
      transpose_input_view, Scalar{0}, *transpose_output->view());
  Wait(transpose_event, test);
  const std::array<Scalar, 3> transpose_expected = {Scalar{-2}, Scalar{-1},
                                                    Scalar{0}};
  CheckExact<Scalar>(Download1D(*transpose_output, *execution, test),
                     transpose_expected, test);

  auto row_major_matrix = Upload2D<Scalar>(
      2, 3, matrix_values, asc::LayoutRight{}, **resource, *execution, test);
  const asc::DenseView<const Scalar, 2> row_major_view(
      *row_major_matrix->view());
  const auto unsupported =
      asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                    row_major_view, input_view, Scalar{1}, *output->view());
  ASC_DENSE_CUDA_CHECK(test, !unsupported.ok());
  if (!unsupported.ok()) {
    ASC_DENSE_CUDA_EQ(test, unsupported.status().code(),
                      asc::ErrorCode::kUnsupported);
  }

  const auto invalid_operation = asc::CudaGemv(
      *provider, static_cast<asc::MatrixOperation>(255), Scalar{1}, matrix_view,
      input_view, Scalar{0}, *output->view());
  ASC_DENSE_CUDA_CHECK(test, !invalid_operation.ok());
  if (!invalid_operation.ok()) {
    ASC_DENSE_CUDA_EQ(test, invalid_operation.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }

  const std::array<asc::extent_t, 1> overlap_output_shape = {2};
  auto overlap_output_mapping = asc::DenseLayoutMapping<1>::Create(
      asc::LayoutLeft{}, overlap_output_shape);
  auto input_mutable_view = input->view();
  auto overlapping_output = asc::DenseView<Scalar, 1>::Create(
      input_mutable_view->data(), *overlap_output_mapping,
      asc::MemorySpace::kDevice);
  const auto output_overlap =
      asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                    matrix_view, input_view, Scalar{0}, *overlapping_output);
  ASC_DENSE_CUDA_CHECK(test, !output_overlap.ok());
  CheckExact<Scalar>(Download1D(*input, *execution, test), input_values, test);

  const auto zero_matrix_shape = Shape2::Create(2, 0);
  const auto zero_input_shape = Shape1::Create(0);
  auto zero_matrix = asc::DenseArray<Scalar, Shape2>::CreateUninitialized(
      *zero_matrix_shape, **resource, asc::LayoutLeft{});
  auto zero_input = asc::DenseArray<Scalar, Shape1>::CreateUninitialized(
      *zero_input_shape, **resource, asc::LayoutLeft{});
  const asc::DenseView<const Scalar, 2> zero_matrix_view(*zero_matrix->view());
  const asc::DenseView<const Scalar, 1> zero_input_view(*zero_input->view());
  const std::array<Scalar, 2> nan_inner_output = {nan, nan};
  auto zeroed_output = Upload1D<Scalar>(nan_inner_output, asc::LayoutLeft{},
                                        **resource, *execution, test);
  auto zero_inner = asc::CudaGemv(*provider, asc::MatrixOperation::kNone,
                                  Scalar{3}, zero_matrix_view, zero_input_view,
                                  Scalar{0}, *zeroed_output->view());
  Wait(zero_inner, test);
  const std::array<Scalar, 2> zeros = {Scalar{0}, Scalar{0}};
  CheckExact<Scalar>(Download1D(*zeroed_output, *execution, test), zeros, test);

  const std::array<Scalar, 2> scale_input = {Scalar{3}, Scalar{-4}};
  auto scaled_output = Upload1D<Scalar>(scale_input, asc::LayoutLeft{},
                                        **resource, *execution, test);
  auto scale_inner = asc::CudaGemv(*provider, asc::MatrixOperation::kNone,
                                   Scalar{3}, zero_matrix_view, zero_input_view,
                                   Scalar{2}, *scaled_output->view());
  Wait(scale_inner, test);
  const std::array<Scalar, 2> scaled_inner_output = {Scalar{6}, Scalar{-8}};
  CheckExact<Scalar>(Download1D(*scaled_output, *execution, test),
                     scaled_inner_output, test);
}

std::vector<long double> MatrixForOperation(
    std::span<const long double> logical, std::size_t logical_rows,
    std::size_t logical_columns, asc::MatrixOperation operation) {
  if (operation == asc::MatrixOperation::kNone) {
    return {logical.begin(), logical.end()};
  }
  std::vector<long double> stored(logical_rows * logical_columns);
  const std::size_t stored_rows = logical_columns;
  for (std::size_t column = 0; column < logical_rows; ++column) {
    for (std::size_t row = 0; row < logical_columns; ++row) {
      stored[column * stored_rows + row] = logical[row * logical_rows + column];
    }
  }
  return stored;
}

template <typename Scalar>
std::vector<Scalar> Convert(std::span<const long double> values) {
  std::vector<Scalar> converted(values.size());
  for (std::size_t index = 0; index < values.size(); ++index) {
    converted[index] = static_cast<Scalar>(values[index]);
  }
  return converted;
}

template <typename Scalar>
void CheckGemm(asc_dense_cuda_test::TestContext& test) {
  auto execution = asc::CreateCudaExecutionContext(CudaDevice());
  auto provider = asc::DenseCudaContext::Create(*execution);
  auto resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  if (!execution.ok() || !provider.ok() || !resource.ok()) {
    ASC_DENSE_CUDA_CHECK(test, false);
    return;
  }

  constexpr std::size_t kM = 2;
  constexpr std::size_t kN = 2;
  constexpr std::size_t kK = 3;
  const std::array<long double, kM * kK> left_logical = {1.0L, 4.0L, 2.0L,
                                                         5.0L, 3.0L, 6.0L};
  const std::array<long double, kK * kN> right_logical = {7.0L, 9.0L,  11.0L,
                                                          8.0L, 10.0L, 12.0L};
  std::array<long double, kM * kN> expected{};
  for (std::size_t column = 0; column < kN; ++column) {
    for (std::size_t row = 0; row < kM; ++row) {
      long double sum = 0.0L;
      for (std::size_t inner = 0; inner < kK; ++inner) {
        sum +=
            left_logical[inner * kM + row] * right_logical[column * kK + inner];
      }
      expected[column * kM + row] = sum;
    }
  }

  for (const asc::MatrixOperation left_operation :
       {asc::MatrixOperation::kNone, asc::MatrixOperation::kTranspose}) {
    for (const asc::MatrixOperation right_operation :
         {asc::MatrixOperation::kNone, asc::MatrixOperation::kTranspose}) {
      const auto left_stored =
          MatrixForOperation(left_logical, kM, kK, left_operation);
      const auto right_stored =
          MatrixForOperation(right_logical, kK, kN, right_operation);
      const auto left_values = Convert<Scalar>(left_stored);
      const auto right_values = Convert<Scalar>(right_stored);
      const asc::extent_t left_rows =
          left_operation == asc::MatrixOperation::kNone ? kM : kK;
      const asc::extent_t left_columns =
          left_operation == asc::MatrixOperation::kNone ? kK : kM;
      const asc::extent_t right_rows =
          right_operation == asc::MatrixOperation::kNone ? kK : kN;
      const asc::extent_t right_columns =
          right_operation == asc::MatrixOperation::kNone ? kN : kK;
      auto left =
          Upload2D<Scalar>(left_rows, left_columns, left_values,
                           asc::LayoutLeft{}, **resource, *execution, test);
      auto right =
          Upload2D<Scalar>(right_rows, right_columns, right_values,
                           asc::LayoutLeft{}, **resource, *execution, test);
      const Scalar nan = std::numeric_limits<Scalar>::quiet_NaN();
      const std::array<Scalar, kM * kN> output_values = {nan, nan, nan, nan};
      auto output = Upload2D<Scalar>(kM, kN, output_values, asc::LayoutLeft{},
                                     **resource, *execution, test);
      const asc::DenseView<const Scalar, 2> left_view(*left->view());
      const asc::DenseView<const Scalar, 2> right_view(*right->view());
      auto event =
          asc::CudaGemm(*provider, left_operation, right_operation, Scalar{1},
                        left_view, right_view, Scalar{0}, *output->view());
      Wait(event, test);
      const auto actual = Download2D(*output, *execution, test);
      for (std::size_t index = 0; index < expected.size(); ++index) {
        ASC_DENSE_CUDA_NEAR(test, actual[index], expected[index],
                            expected[index], kK);
      }
    }
  }

  constexpr std::array<asc::extent_t, 2> kPaddedLeftShape = {2, 3};
  constexpr std::array<asc::stride_t, 2> kPaddedLeftStrides = {1, 4};
  constexpr std::array<asc::extent_t, 2> kPaddedRightShape = {3, 2};
  constexpr std::array<asc::stride_t, 2> kPaddedRightStrides = {1, 5};
  constexpr std::array<asc::extent_t, 2> kPaddedOutputShape = {2, 2};
  constexpr std::array<asc::stride_t, 2> kPaddedOutputStrides = {1, 4};
  auto padded_left_mapping = asc::DenseLayoutMapping<2>::Create(
      asc::LayoutStride{}, kPaddedLeftShape, kPaddedLeftStrides);
  auto padded_right_mapping = asc::DenseLayoutMapping<2>::Create(
      asc::LayoutStride{}, kPaddedRightShape, kPaddedRightStrides);
  auto padded_output_mapping = asc::DenseLayoutMapping<2>::Create(
      asc::LayoutStride{}, kPaddedOutputShape, kPaddedOutputStrides);
  std::array<Scalar, 10> padded_left_host{};
  padded_left_host.fill(Scalar{-91});
  padded_left_host[0] = Scalar{1};
  padded_left_host[1] = Scalar{4};
  padded_left_host[4] = Scalar{2};
  padded_left_host[5] = Scalar{5};
  padded_left_host[8] = Scalar{3};
  padded_left_host[9] = Scalar{6};
  std::array<Scalar, 8> padded_right_host{};
  padded_right_host.fill(Scalar{-92});
  padded_right_host[0] = Scalar{7};
  padded_right_host[1] = Scalar{9};
  padded_right_host[2] = Scalar{11};
  padded_right_host[5] = Scalar{8};
  padded_right_host[6] = Scalar{10};
  padded_right_host[7] = Scalar{12};
  std::array<Scalar, 6> padded_output_host = {
      Scalar{2}, Scalar{4}, Scalar{-93}, Scalar{-93}, Scalar{6}, Scalar{8}};
  auto padded_left_buffer = asc::Buffer::Allocate(
      **resource, sizeof(padded_left_host), alignof(Scalar));
  auto padded_right_buffer = asc::Buffer::Allocate(
      **resource, sizeof(padded_right_host), alignof(Scalar));
  auto padded_output_buffer = asc::Buffer::Allocate(
      **resource, sizeof(padded_output_host), alignof(Scalar));
  auto padded_left_upload = asc::CopyBytes(
      *execution, *padded_left_buffer->mutable_view(),
      asc::ConstMemoryView(padded_left_host.data(), sizeof(padded_left_host),
                           asc::MemorySpace::kHost));
  auto padded_right_upload = asc::CopyBytes(
      *execution, *padded_right_buffer->mutable_view(),
      asc::ConstMemoryView(padded_right_host.data(), sizeof(padded_right_host),
                           asc::MemorySpace::kHost));
  auto padded_output_upload =
      asc::CopyBytes(*execution, *padded_output_buffer->mutable_view(),
                     asc::ConstMemoryView(padded_output_host.data(),
                                          sizeof(padded_output_host),
                                          asc::MemorySpace::kHost));
  Wait(padded_left_upload, test);
  Wait(padded_right_upload, test);
  Wait(padded_output_upload, test);
  auto padded_left = asc::DenseView<const Scalar, 2>::Create(
      static_cast<const Scalar*>(padded_left_buffer->data()),
      *padded_left_mapping, asc::MemorySpace::kDevice);
  auto padded_right = asc::DenseView<const Scalar, 2>::Create(
      static_cast<const Scalar*>(padded_right_buffer->data()),
      *padded_right_mapping, asc::MemorySpace::kDevice);
  auto padded_output = asc::DenseView<Scalar, 2>::Create(
      static_cast<Scalar*>(padded_output_buffer->data()),
      *padded_output_mapping, asc::MemorySpace::kDevice);
  auto padded_event = asc::CudaGemm(
      *provider, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone,
      Scalar{1}, *padded_left, *padded_right, Scalar{0.5}, *padded_output);
  Wait(padded_event, test);
  std::array<Scalar, 6> padded_result{};
  auto padded_download = asc::CopyBytes(
      *execution,
      asc::MutableMemoryView(padded_result.data(), sizeof(padded_result),
                             asc::MemorySpace::kHost),
      *padded_output_buffer->const_view());
  Wait(padded_download, test);
  ASC_DENSE_CUDA_NEAR(test, padded_result[0], Scalar{59}, Scalar{59}, kK);
  ASC_DENSE_CUDA_NEAR(test, padded_result[1], Scalar{141}, Scalar{141}, kK);
  ASC_DENSE_CUDA_EQ(test, padded_result[2], Scalar{-93});
  ASC_DENSE_CUDA_EQ(test, padded_result[3], Scalar{-93});
  ASC_DENSE_CUDA_NEAR(test, padded_result[4], Scalar{67}, Scalar{67}, kK);
  ASC_DENSE_CUDA_NEAR(test, padded_result[5], Scalar{158}, Scalar{158}, kK);

  auto overlapping_output = asc::DenseView<Scalar, 2>::Create(
      static_cast<Scalar*>(padded_left_buffer->data()), *padded_output_mapping,
      asc::MemorySpace::kDevice);
  const auto output_overlap = asc::CudaGemm(
      *provider, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone,
      Scalar{1}, *padded_left, *padded_right, Scalar{0}, *overlapping_output);
  ASC_DENSE_CUDA_CHECK(test, !output_overlap.ok());
  std::array<Scalar, 10> padded_left_after_overlap{};
  auto padded_left_download =
      asc::CopyBytes(*execution,
                     asc::MutableMemoryView(padded_left_after_overlap.data(),
                                            sizeof(padded_left_after_overlap),
                                            asc::MemorySpace::kHost),
                     *padded_left_buffer->const_view());
  Wait(padded_left_download, test);
  CheckExact<Scalar>(padded_left_after_overlap, padded_left_host, test);

  const auto left_values = Convert<Scalar>(left_logical);
  const auto right_values = Convert<Scalar>(right_logical);
  auto row_major_left = Upload2D<Scalar>(
      kM, kK, left_values, asc::LayoutRight{}, **resource, *execution, test);
  auto column_major_right = Upload2D<Scalar>(
      kK, kN, right_values, asc::LayoutLeft{}, **resource, *execution, test);
  const std::array<Scalar, kM * kN> row_major_output_values = {};
  auto row_major_output =
      Upload2D<Scalar>(kM, kN, row_major_output_values, asc::LayoutLeft{},
                       **resource, *execution, test);
  const asc::DenseView<const Scalar, 2> row_major_left_view(
      *row_major_left->view());
  const asc::DenseView<const Scalar, 2> column_major_right_view(
      *column_major_right->view());
  const auto row_major = asc::CudaGemm(
      *provider, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone,
      Scalar{1}, row_major_left_view, column_major_right_view, Scalar{0},
      *row_major_output->view());
  ASC_DENSE_CUDA_CHECK(test, !row_major.ok());
  if (!row_major.ok()) {
    ASC_DENSE_CUDA_EQ(test, row_major.status().code(),
                      asc::ErrorCode::kUnsupported);
  }

  const auto zero_left_shape = Shape2::Create(2, 0);
  const auto zero_right_shape = Shape2::Create(0, 2);
  auto zero_left = asc::DenseArray<Scalar, Shape2>::CreateUninitialized(
      *zero_left_shape, **resource, asc::LayoutLeft{});
  auto zero_right = asc::DenseArray<Scalar, Shape2>::CreateUninitialized(
      *zero_right_shape, **resource, asc::LayoutLeft{});
  const asc::DenseView<const Scalar, 2> zero_left_view(*zero_left->view());
  const asc::DenseView<const Scalar, 2> zero_right_view(*zero_right->view());
  const Scalar nan = std::numeric_limits<Scalar>::quiet_NaN();
  const std::array<Scalar, 4> nan_inner_output = {nan, nan, nan, nan};
  auto zeroed_inner_output = Upload2D<Scalar>(
      2, 2, nan_inner_output, asc::LayoutLeft{}, **resource, *execution, test);
  auto zero_inner =
      asc::CudaGemm(*provider, asc::MatrixOperation::kNone,
                    asc::MatrixOperation::kNone, Scalar{3}, zero_left_view,
                    zero_right_view, Scalar{0}, *zeroed_inner_output->view());
  Wait(zero_inner, test);
  const std::array<Scalar, 4> zeros = {};
  CheckExact<Scalar>(Download2D(*zeroed_inner_output, *execution, test), zeros,
                     test);

  const std::array<Scalar, 4> scale_input = {Scalar{1}, Scalar{-2}, Scalar{3},
                                             Scalar{-4}};
  auto scaled_inner_output = Upload2D<Scalar>(
      2, 2, scale_input, asc::LayoutLeft{}, **resource, *execution, test);
  auto scale_inner =
      asc::CudaGemm(*provider, asc::MatrixOperation::kNone,
                    asc::MatrixOperation::kNone, Scalar{3}, zero_left_view,
                    zero_right_view, Scalar{2}, *scaled_inner_output->view());
  Wait(scale_inner, test);
  const std::array<Scalar, 4> scaled_expected = {Scalar{2}, Scalar{-4},
                                                 Scalar{6}, Scalar{-8}};
  CheckExact<Scalar>(Download2D(*scaled_inner_output, *execution, test),
                     scaled_expected, test);

  const auto empty_shape = Shape2::Create(0, 3);
  auto empty = asc::DenseArray<Scalar, Shape2>::CreateUninitialized(
      *empty_shape, **resource, asc::LayoutLeft{});
  const auto right_shape = Shape2::Create(3, 2);
  auto right = asc::DenseArray<Scalar, Shape2>::CreateUninitialized(
      *right_shape, **resource, asc::LayoutLeft{});
  const auto output_shape = Shape2::Create(0, 2);
  auto output = asc::DenseArray<Scalar, Shape2>::CreateUninitialized(
      *output_shape, **resource, asc::LayoutLeft{});
  const asc::DenseView<const Scalar, 2> empty_view(*empty->view());
  const asc::DenseView<const Scalar, 2> right_view(*right->view());
  auto empty_event = asc::CudaGemm(
      *provider, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone,
      Scalar{1}, empty_view, right_view, Scalar{0}, *output->view());
  Wait(empty_event, test);
}

template <typename Scalar>
void CheckPaddedGemvAndFailures(asc_dense_cuda_test::TestContext& test) {
  auto execution = asc::CreateCudaExecutionContext(CudaDevice());
  auto provider = asc::DenseCudaContext::Create(*execution);
  auto resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  if (!execution.ok() || !provider.ok() || !resource.ok()) {
    ASC_DENSE_CUDA_CHECK(test, false);
    return;
  }

  constexpr std::array<asc::extent_t, 2> kMatrixShape = {2, 3};
  constexpr std::array<asc::stride_t, 2> kMatrixStrides = {1, 4};
  constexpr std::array<asc::extent_t, 1> kInputShape = {3};
  constexpr std::array<asc::stride_t, 1> kInputStride = {2};
  constexpr std::array<asc::extent_t, 1> kOutputShape = {2};
  constexpr std::array<asc::stride_t, 1> kOutputStride = {3};
  auto matrix_mapping = asc::DenseLayoutMapping<2>::Create(
      asc::LayoutStride{}, kMatrixShape, kMatrixStrides);
  auto input_mapping = asc::DenseLayoutMapping<1>::Create(
      asc::LayoutStride{}, kInputShape, kInputStride);
  auto output_mapping = asc::DenseLayoutMapping<1>::Create(
      asc::LayoutStride{}, kOutputShape, kOutputStride);
  std::array<Scalar, 10> matrix_host{};
  matrix_host.fill(Scalar{-91});
  matrix_host[0] = Scalar{1};
  matrix_host[1] = Scalar{4};
  matrix_host[4] = Scalar{2};
  matrix_host[5] = Scalar{5};
  matrix_host[8] = Scalar{3};
  matrix_host[9] = Scalar{6};
  std::array<Scalar, 5> input_host = {Scalar{1}, Scalar{-92}, Scalar{-1},
                                      Scalar{-92}, Scalar{2}};
  std::array<Scalar, 4> output_host = {Scalar{10}, Scalar{-93}, Scalar{-93},
                                       Scalar{20}};

  auto matrix_buffer =
      asc::Buffer::Allocate(**resource, sizeof(matrix_host), alignof(Scalar));
  auto input_buffer =
      asc::Buffer::Allocate(**resource, sizeof(input_host), alignof(Scalar));
  auto output_buffer =
      asc::Buffer::Allocate(**resource, sizeof(output_host), alignof(Scalar));
  auto matrix_upload = asc::CopyBytes(
      *execution, *matrix_buffer->mutable_view(),
      asc::ConstMemoryView(matrix_host.data(), sizeof(matrix_host),
                           asc::MemorySpace::kHost));
  auto input_upload =
      asc::CopyBytes(*execution, *input_buffer->mutable_view(),
                     asc::ConstMemoryView(input_host.data(), sizeof(input_host),
                                          asc::MemorySpace::kHost));
  auto output_upload = asc::CopyBytes(
      *execution, *output_buffer->mutable_view(),
      asc::ConstMemoryView(output_host.data(), sizeof(output_host),
                           asc::MemorySpace::kHost));
  Wait(matrix_upload, test);
  Wait(input_upload, test);
  Wait(output_upload, test);
  auto matrix = asc::DenseView<const Scalar, 2>::Create(
      static_cast<const Scalar*>(matrix_buffer->data()), *matrix_mapping,
      asc::MemorySpace::kDevice);
  auto input = asc::DenseView<const Scalar, 1>::Create(
      static_cast<const Scalar*>(input_buffer->data()), *input_mapping,
      asc::MemorySpace::kDevice);
  auto output = asc::DenseView<Scalar, 1>::Create(
      static_cast<Scalar*>(output_buffer->data()), *output_mapping,
      asc::MemorySpace::kDevice);
  auto event = asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{2},
                             *matrix, *input, Scalar{0.5}, *output);
  Wait(event, test);
  std::array<Scalar, 4> padded_result{};
  auto result_copy = asc::CopyBytes(
      *execution,
      asc::MutableMemoryView(padded_result.data(), sizeof(padded_result),
                             asc::MemorySpace::kHost),
      *output_buffer->const_view());
  Wait(result_copy, test);
  ASC_DENSE_CUDA_NEAR(test, padded_result[0], Scalar{15}, Scalar{15}, 3);
  ASC_DENSE_CUDA_EQ(test, padded_result[1], Scalar{-93});
  ASC_DENSE_CUDA_EQ(test, padded_result[2], Scalar{-93});
  ASC_DENSE_CUDA_NEAR(test, padded_result[3], Scalar{32}, Scalar{32}, 3);

  constexpr std::array<asc::extent_t, 1> kOverlapShape = {4};
  const auto overlap_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kOverlapShape);
  std::array<Scalar, 5> overlap_host = {Scalar{1}, Scalar{2}, Scalar{3},
                                        Scalar{4}, Scalar{5}};
  auto overlap_buffer =
      asc::Buffer::Allocate(**resource, sizeof(overlap_host), alignof(Scalar));
  auto overlap_upload = asc::CopyBytes(
      *execution, *overlap_buffer->mutable_view(),
      asc::ConstMemoryView(overlap_host.data(), sizeof(overlap_host),
                           asc::MemorySpace::kHost));
  Wait(overlap_upload, test);
  auto overlap_source = asc::DenseView<const Scalar, 1>::Create(
      static_cast<const Scalar*>(overlap_buffer->data()), *overlap_mapping,
      asc::MemorySpace::kDevice);
  auto overlap_destination = asc::DenseView<Scalar, 1>::Create(
      static_cast<Scalar*>(overlap_buffer->data()) + 1, *overlap_mapping,
      asc::MemorySpace::kDevice);
  const auto overlap_copy =
      asc::CudaCopy(*provider, *overlap_source, *overlap_destination);
  const auto overlap_axpy = asc::CudaAxpy(*provider, Scalar{1}, *overlap_source,
                                          *overlap_destination);
  ASC_DENSE_CUDA_CHECK(test, !overlap_copy.ok());
  ASC_DENSE_CUDA_CHECK(test, !overlap_axpy.ok());
  std::array<Scalar, 5> overlap_result{};
  auto overlap_download = asc::CopyBytes(
      *execution,
      asc::MutableMemoryView(overlap_result.data(), sizeof(overlap_result),
                             asc::MemorySpace::kHost),
      *overlap_buffer->const_view());
  Wait(overlap_download, test);
  CheckExact<Scalar>(overlap_result, overlap_host, test);

  constexpr asc::extent_t kAboveInt =
      static_cast<asc::extent_t>(std::numeric_limits<int>::max()) + 1;
  const std::array<asc::extent_t, 2> huge_matrix_shape = {kAboveInt, 0};
  const std::array<asc::extent_t, 1> empty_vector_shape = {0};
  const std::array<asc::extent_t, 1> huge_output_shape = {kAboveInt};
  auto huge_matrix_mapping =
      asc::DenseLayoutMapping<2>::Create(asc::LayoutLeft{}, huge_matrix_shape);
  auto empty_vector_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, empty_vector_shape);
  auto huge_output_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, huge_output_shape);
  auto* fake_output = reinterpret_cast<Scalar*>(std::uintptr_t{0x1000000000});
  auto huge_matrix = asc::DenseView<const Scalar, 2>::Create(
      nullptr, *huge_matrix_mapping, asc::MemorySpace::kDevice);
  auto empty_vector = asc::DenseView<const Scalar, 1>::Create(
      nullptr, *empty_vector_mapping, asc::MemorySpace::kDevice);
  auto huge_output = asc::DenseView<Scalar, 1>::Create(
      fake_output, *huge_output_mapping, asc::MemorySpace::kDevice);
  const auto huge_gemv =
      asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                    *huge_matrix, *empty_vector, Scalar{0}, *huge_output);
  ASC_DENSE_CUDA_CHECK(test, !huge_gemv.ok());
  if (!huge_gemv.ok()) {
    ASC_DENSE_CUDA_EQ(test, huge_gemv.status().code(),
                      asc::ErrorCode::kOverflow);
  }

  const std::array<asc::extent_t, 2> huge_left_shape = {kAboveInt, 0};
  const std::array<asc::extent_t, 2> empty_right_shape = {0, 1};
  const std::array<asc::extent_t, 2> huge_gemm_output_shape = {kAboveInt, 1};
  auto huge_left_mapping =
      asc::DenseLayoutMapping<2>::Create(asc::LayoutLeft{}, huge_left_shape);
  auto empty_right_mapping =
      asc::DenseLayoutMapping<2>::Create(asc::LayoutLeft{}, empty_right_shape);
  auto huge_gemm_output_mapping = asc::DenseLayoutMapping<2>::Create(
      asc::LayoutLeft{}, huge_gemm_output_shape);
  auto huge_left = asc::DenseView<const Scalar, 2>::Create(
      nullptr, *huge_left_mapping, asc::MemorySpace::kDevice);
  auto empty_right = asc::DenseView<const Scalar, 2>::Create(
      nullptr, *empty_right_mapping, asc::MemorySpace::kDevice);
  auto huge_gemm_output = asc::DenseView<Scalar, 2>::Create(
      fake_output, *huge_gemm_output_mapping, asc::MemorySpace::kDevice);
  const auto huge_gemm = asc::CudaGemm(
      *provider, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone,
      Scalar{1}, *huge_left, *empty_right, Scalar{0}, *huge_gemm_output);
  ASC_DENSE_CUDA_CHECK(test, !huge_gemm.ok());
  if (!huge_gemm.ok()) {
    ASC_DENSE_CUDA_EQ(test, huge_gemm.status().code(),
                      asc::ErrorCode::kOverflow);
  }

  const std::array<asc::extent_t, 2> small_matrix_shape = {2, 2};
  const std::array<asc::extent_t, 1> small_vector_shape = {2};
  const std::array<asc::stride_t, 2> huge_matrix_strides = {1, kAboveInt};
  const std::array<asc::stride_t, 1> huge_vector_stride = {kAboveInt};
  auto huge_stride_matrix_mapping = asc::DenseLayoutMapping<2>::Create(
      asc::LayoutStride{}, small_matrix_shape, huge_matrix_strides);
  auto huge_stride_vector_mapping = asc::DenseLayoutMapping<1>::Create(
      asc::LayoutStride{}, small_vector_shape, huge_vector_stride);
  auto* fake_matrix =
      reinterpret_cast<const Scalar*>(std::uintptr_t{0x10000000000});
  auto* fake_input =
      reinterpret_cast<const Scalar*>(std::uintptr_t{0x20000000000});
  auto huge_stride_matrix = asc::DenseView<const Scalar, 2>::Create(
      fake_matrix, *huge_stride_matrix_mapping, asc::MemorySpace::kDevice);
  auto huge_stride_input = asc::DenseView<const Scalar, 1>::Create(
      fake_input, *huge_stride_vector_mapping, asc::MemorySpace::kDevice);
  const auto huge_stride_gemv =
      asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                    *huge_stride_matrix, *input, Scalar{0}, *output);
  ASC_DENSE_CUDA_CHECK(test, !huge_stride_gemv.ok());
  if (!huge_stride_gemv.ok()) {
    ASC_DENSE_CUDA_EQ(test, huge_stride_gemv.status().code(),
                      asc::ErrorCode::kOverflow);
  }
  const auto huge_increment_gemv =
      asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1}, *matrix,
                    *huge_stride_input, Scalar{0}, *output);
  ASC_DENSE_CUDA_CHECK(test, !huge_increment_gemv.ok());
  if (!huge_increment_gemv.ok()) {
    ASC_DENSE_CUDA_EQ(test, huge_increment_gemv.status().code(),
                      asc::ErrorCode::kOverflow);
  }

  const std::array<asc::extent_t, 1> host_shape = {2};
  auto host_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, host_shape);
  std::array<Scalar, 2> host_storage = {Scalar{1}, Scalar{2}};
  auto host_view = asc::DenseView<Scalar, 1>::Create(
      host_storage.data(), *host_mapping, asc::MemorySpace::kHost);
  const auto wrong_placement = asc::CudaScal(*provider, Scalar{2}, *host_view);
  ASC_DENSE_CUDA_CHECK(test, !wrong_placement.ok());
  if (!wrong_placement.ok()) {
    ASC_DENSE_CUDA_EQ(test, wrong_placement.status().code(),
                      asc::ErrorCode::kMemoryAccess);
  }
  const std::array<Scalar, 2> unchanged_host = {Scalar{1}, Scalar{2}};
  CheckExact<Scalar>(host_storage, unchanged_host, test);

  asc::DenseCudaContext moved_provider(std::move(*provider));
  const auto moved_from = asc::CudaScal(*provider, Scalar{2}, *output);
  ASC_DENSE_CUDA_CHECK(test, !moved_from.ok());
  if (!moved_from.ok()) {
    ASC_DENSE_CUDA_EQ(test, moved_from.status().code(),
                      asc::ErrorCode::kInvalidState);
  }
  auto moved_to = asc::CudaScal(moved_provider, Scalar{1}, *output);
  Wait(moved_to, test);
}

template <typename Scalar>
void CheckPlacementAndUniquenessFailures(
    asc_dense_cuda_test::TestContext& test) {
  auto execution = asc::CreateCudaExecutionContext(CudaDevice());
  auto provider = asc::DenseCudaContext::Create(*execution);
  auto resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  if (!execution.ok() || !provider.ok() || !resource.ok()) {
    ASC_DENSE_CUDA_CHECK(test, false);
    return;
  }

  std::array<Scalar, 64> initial{};
  for (std::size_t index = 0; index < initial.size(); ++index) {
    initial[index] = static_cast<Scalar>(index + 1);
  }
  auto buffer =
      asc::Buffer::Allocate(**resource, sizeof(initial), alignof(Scalar));
  auto upload =
      asc::CopyBytes(*execution, *buffer->mutable_view(),
                     asc::ConstMemoryView(initial.data(), sizeof(initial),
                                          asc::MemorySpace::kHost));
  Wait(upload, test);
  auto* base = static_cast<Scalar*>(buffer->data());

  constexpr std::array<asc::extent_t, 1> kVectorShape = {2};
  constexpr std::array<asc::extent_t, 2> kMatrixShape = {2, 2};
  constexpr std::array<asc::stride_t, 1> kNonUniqueVectorStride = {0};
  constexpr std::array<asc::stride_t, 2> kNonUniqueMatrixStrides = {1, 1};
  auto vector_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kVectorShape);
  auto matrix_mapping =
      asc::DenseLayoutMapping<2>::Create(asc::LayoutLeft{}, kMatrixShape);
  auto non_unique_vector_mapping = asc::DenseLayoutMapping<1>::Create(
      asc::LayoutStride{}, kVectorShape, kNonUniqueVectorStride);
  auto non_unique_matrix_mapping = asc::DenseLayoutMapping<2>::Create(
      asc::LayoutStride{}, kMatrixShape, kNonUniqueMatrixStrides);
  ASC_DENSE_CUDA_CHECK(test, !non_unique_vector_mapping->is_unique());
  ASC_DENSE_CUDA_CHECK(test, !non_unique_matrix_mapping->is_unique());

  auto source = asc::DenseView<const Scalar, 1>::Create(
      base, *vector_mapping, asc::MemorySpace::kDevice);
  auto destination = asc::DenseView<Scalar, 1>::Create(
      base + 4, *vector_mapping, asc::MemorySpace::kDevice);
  auto matrix = asc::DenseView<const Scalar, 2>::Create(
      base + 8, *matrix_mapping, asc::MemorySpace::kDevice);
  auto right = asc::DenseView<const Scalar, 2>::Create(
      base + 16, *matrix_mapping, asc::MemorySpace::kDevice);
  auto gemv_output = asc::DenseView<Scalar, 1>::Create(
      base + 24, *vector_mapping, asc::MemorySpace::kDevice);
  auto gemm_output = asc::DenseView<Scalar, 2>::Create(
      base + 28, *matrix_mapping, asc::MemorySpace::kDevice);
  auto non_unique_vector = asc::DenseView<const Scalar, 1>::Create(
      base + 36, *non_unique_vector_mapping, asc::MemorySpace::kDevice);
  auto non_unique_matrix = asc::DenseView<const Scalar, 2>::Create(
      base + 40, *non_unique_matrix_mapping, asc::MemorySpace::kDevice);

  const auto CheckRejected = [&](const auto& result) {
    ASC_DENSE_CUDA_CHECK(test, !result.ok());
  };
  CheckRejected(asc::CudaCopy(*provider, *non_unique_vector, *destination));
  CheckRejected(
      asc::CudaAxpy(*provider, Scalar{1}, *non_unique_vector, *destination));
  CheckRejected(asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                              *non_unique_matrix, *source, Scalar{0},
                              *gemv_output));
  CheckRejected(asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                              *matrix, *non_unique_vector, Scalar{0},
                              *gemv_output));
  CheckRejected(asc::CudaGemm(
      *provider, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone,
      Scalar{1}, *non_unique_matrix, *right, Scalar{0}, *gemm_output));
  CheckRejected(asc::CudaGemm(*provider, asc::MatrixOperation::kNone,
                              asc::MatrixOperation::kNone, Scalar{1}, *matrix,
                              *non_unique_matrix, Scalar{0}, *gemm_output));

  auto host_source = asc::DenseView<const Scalar, 1>::Create(
      source->data(), *vector_mapping, asc::MemorySpace::kHost);
  auto managed_source = asc::DenseView<const Scalar, 1>::Create(
      source->data(), *vector_mapping, asc::MemorySpace::kManaged);
  auto host_destination = asc::DenseView<Scalar, 1>::Create(
      destination->data(), *vector_mapping, asc::MemorySpace::kHost);
  auto managed_destination = asc::DenseView<Scalar, 1>::Create(
      destination->data(), *vector_mapping, asc::MemorySpace::kManaged);
  CheckRejected(asc::CudaCopy(*provider, *host_source, *destination));
  CheckRejected(asc::CudaCopy(*provider, *managed_source, *destination));
  CheckRejected(asc::CudaCopy(*provider, *source, *host_destination));
  CheckRejected(asc::CudaCopy(*provider, *source, *managed_destination));
  CheckRejected(asc::CudaScal(*provider, Scalar{2}, *host_destination));
  CheckRejected(asc::CudaScal(*provider, Scalar{2}, *managed_destination));
  CheckRejected(
      asc::CudaAxpy(*provider, Scalar{1}, *host_source, *destination));
  CheckRejected(
      asc::CudaAxpy(*provider, Scalar{1}, *managed_source, *destination));
  CheckRejected(
      asc::CudaAxpy(*provider, Scalar{1}, *source, *host_destination));
  CheckRejected(
      asc::CudaAxpy(*provider, Scalar{1}, *source, *managed_destination));

  auto host_matrix = asc::DenseView<const Scalar, 2>::Create(
      matrix->data(), *matrix_mapping, asc::MemorySpace::kHost);
  auto managed_matrix = asc::DenseView<const Scalar, 2>::Create(
      matrix->data(), *matrix_mapping, asc::MemorySpace::kManaged);
  auto host_right = asc::DenseView<const Scalar, 2>::Create(
      right->data(), *matrix_mapping, asc::MemorySpace::kHost);
  auto managed_right = asc::DenseView<const Scalar, 2>::Create(
      right->data(), *matrix_mapping, asc::MemorySpace::kManaged);
  auto host_gemv_output = asc::DenseView<Scalar, 1>::Create(
      gemv_output->data(), *vector_mapping, asc::MemorySpace::kHost);
  auto managed_gemv_output = asc::DenseView<Scalar, 1>::Create(
      gemv_output->data(), *vector_mapping, asc::MemorySpace::kManaged);
  auto host_gemm_output = asc::DenseView<Scalar, 2>::Create(
      gemm_output->data(), *matrix_mapping, asc::MemorySpace::kHost);
  auto managed_gemm_output = asc::DenseView<Scalar, 2>::Create(
      gemm_output->data(), *matrix_mapping, asc::MemorySpace::kManaged);

  CheckRejected(asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                              *host_matrix, *source, Scalar{0}, *gemv_output));
  CheckRejected(asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                              *managed_matrix, *source, Scalar{0},
                              *gemv_output));
  CheckRejected(asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                              *matrix, *host_source, Scalar{0}, *gemv_output));
  CheckRejected(asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                              *matrix, *managed_source, Scalar{0},
                              *gemv_output));
  CheckRejected(asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                              *matrix, *source, Scalar{0}, *host_gemv_output));
  CheckRejected(asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                              *matrix, *source, Scalar{0},
                              *managed_gemv_output));

  CheckRejected(asc::CudaGemm(*provider, asc::MatrixOperation::kNone,
                              asc::MatrixOperation::kNone, Scalar{1},
                              *host_matrix, *right, Scalar{0}, *gemm_output));
  CheckRejected(asc::CudaGemm(
      *provider, asc::MatrixOperation::kNone, asc::MatrixOperation::kNone,
      Scalar{1}, *managed_matrix, *right, Scalar{0}, *gemm_output));
  CheckRejected(asc::CudaGemm(*provider, asc::MatrixOperation::kNone,
                              asc::MatrixOperation::kNone, Scalar{1}, *matrix,
                              *host_right, Scalar{0}, *gemm_output));
  CheckRejected(asc::CudaGemm(*provider, asc::MatrixOperation::kNone,
                              asc::MatrixOperation::kNone, Scalar{1}, *matrix,
                              *managed_right, Scalar{0}, *gemm_output));
  CheckRejected(asc::CudaGemm(*provider, asc::MatrixOperation::kNone,
                              asc::MatrixOperation::kNone, Scalar{1}, *matrix,
                              *right, Scalar{0}, *host_gemm_output));
  CheckRejected(asc::CudaGemm(*provider, asc::MatrixOperation::kNone,
                              asc::MatrixOperation::kNone, Scalar{1}, *matrix,
                              *right, Scalar{0}, *managed_gemm_output));

  std::array<Scalar, 64> result{};
  auto download =
      asc::CopyBytes(*execution,
                     asc::MutableMemoryView(result.data(), sizeof(result),
                                            asc::MemorySpace::kHost),
                     *buffer->const_view());
  Wait(download, test);
  CheckExact<Scalar>(result, initial, test);
}

}  // namespace

int main() {
  asc_dense_cuda_test::TestContext test;
  CheckCopyScalAxpy<float>(test);
  CheckCopyScalAxpy<double>(test);
  CheckGemv<float>(test);
  CheckGemv<double>(test);
  CheckGemm<float>(test);
  CheckGemm<double>(test);
  CheckPaddedGemvAndFailures<float>(test);
  CheckPaddedGemvAndFailures<double>(test);
  CheckPlacementAndUniquenessFailures<float>(test);
  CheckPlacementAndUniquenessFailures<double>(test);
  return test.Finish();
}
