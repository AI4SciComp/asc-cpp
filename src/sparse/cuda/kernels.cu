#include <cuda_runtime_api.h>

#include <algorithm>
#include <cstdint>

#include "../../core/cuda/cuda_internal.h"
#include "kernels_internal.h"

namespace asc::internal_sparse_cuda {
namespace {

template <typename Element>
struct DeviceArithmetic {
  __device__ static Element Zero() { return Element{}; }
  __device__ static Element FromScalar(ScalarValue value) {
    return static_cast<Element>(value.real);
  }
  __device__ static Element Add(Element left, Element right) {
    return left + right;
  }
  __device__ static Element Subtract(Element left, Element right) {
    return left - right;
  }
  __device__ static Element Multiply(Element left, Element right) {
    return left * right;
  }
  __device__ static Element Divide(Element left, Element right) {
    return left / right;
  }
  __device__ static bool IsZero(Element value) { return value == Element{}; }
  __device__ static Element Conjugate(Element value) { return value; }
};

template <typename Real>
struct DeviceComplex {
  Real real;
  Real imaginary;
};

template <typename Real>
struct DeviceArithmetic<DeviceComplex<Real>> {
  using Element = DeviceComplex<Real>;

  __device__ static Element Zero() { return {Real{0}, Real{0}}; }
  __device__ static Element FromScalar(ScalarValue value) {
    return {static_cast<Real>(value.real), static_cast<Real>(value.imaginary)};
  }
  __device__ static Element Add(Element left, Element right) {
    return {left.real + right.real, left.imaginary + right.imaginary};
  }
  __device__ static Element Subtract(Element left, Element right) {
    return {left.real - right.real, left.imaginary - right.imaginary};
  }
  __device__ static Element Multiply(Element left, Element right) {
    return {left.real * right.real - left.imaginary * right.imaginary,
            left.real * right.imaginary + left.imaginary * right.real};
  }
  __device__ static Element Divide(Element left, Element right) {
    const Real absolute_real = right.real < Real{0} ? -right.real : right.real;
    const Real absolute_imaginary =
        right.imaginary < Real{0} ? -right.imaginary : right.imaginary;
    if (absolute_real >= absolute_imaginary) {
      const Real ratio = right.imaginary / right.real;
      const Real denominator = right.real + right.imaginary * ratio;
      return {(left.real + left.imaginary * ratio) / denominator,
              (left.imaginary - left.real * ratio) / denominator};
    }
    const Real ratio = right.real / right.imaginary;
    const Real denominator = right.imaginary + right.real * ratio;
    return {(left.real * ratio + left.imaginary) / denominator,
            (left.imaginary * ratio - left.real) / denominator};
  }
  __device__ static bool IsZero(Element value) {
    return value.real == Real{0} && value.imaginary == Real{0};
  }
  __device__ static Element Conjugate(Element value) {
    return {value.real, -value.imaginary};
  }
};

constexpr unsigned int kThreadsPerBlock = 256;
constexpr unsigned int kMaximumBlocks = 65535;

unsigned int BlockCount(std::uint64_t count) {
  const std::uint64_t requested =
      (count + kThreadsPerBlock - 1) / kThreadsPerBlock;
  return static_cast<unsigned int>(
      std::min<std::uint64_t>(requested, kMaximumBlocks));
}

template <typename Element>
__global__ void CsrSpmvKernel(Element alpha, const nnz_t* outer_offsets,
                              const index_t* inner_indices,
                              const Element* values, extent_t rows,
                              const Element* input, stride_t input_stride,
                              Element beta, Element* output,
                              stride_t output_stride) {
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t row = first; row < static_cast<std::uint64_t>(rows);
       row += step) {
    Element sum = Element{};
    const nnz_t begin = outer_offsets[row];
    const nnz_t end = outer_offsets[row + 1U];
    for (nnz_t position = begin; position < end; ++position) {
      sum += values[position] * input[inner_indices[position] * input_stride];
    }
    Element result = alpha * sum;
    if (beta != Element{}) {
      result += beta * output[row * output_stride];
    }
    output[row * output_stride] = result;
  }
}

template <typename Element>
__device__ Element ReadOperand(OperandDescriptor operand,
                               std::uint64_t position) {
  if (operand.kind == OperandKind::kScalar) {
    return static_cast<Element>(operand.scalar);
  }
  return static_cast<const Element*>(operand.view.values)[position];
}

template <typename Element>
__global__ void SparsePointwiseKernel(int operation, OperandDescriptor left,
                                      OperandDescriptor right,
                                      SparseDescriptor destination) {
  auto* output = static_cast<Element*>(const_cast<void*>(destination.values));
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t position = first;
       position < static_cast<std::uint64_t>(destination.nonzeros);
       position += step) {
    const Element left_value = ReadOperand<Element>(left, position);
    Element value = left_value;
    switch (static_cast<PointwiseOperation>(operation)) {
      case PointwiseOperation::kCopy:
        value = left_value;
        break;
      case PointwiseOperation::kNegate:
        value = -left_value;
        break;
      case PointwiseOperation::kAdd:
        value = left_value + ReadOperand<Element>(right, position);
        break;
      case PointwiseOperation::kSubtract:
        value = left_value - ReadOperand<Element>(right, position);
        break;
      case PointwiseOperation::kMultiply:
        value = left_value * ReadOperand<Element>(right, position);
        break;
    }
    output[position] = value;
  }
}

template <typename Element>
__device__ Element& DenseVectorAt(VectorDescriptor vector, extent_t index) {
  auto* data = static_cast<Element*>(const_cast<void*>(vector.data));
  return data[index * vector.stride];
}

template <typename Element>
__device__ Element& DenseMatrixAt(MatrixDescriptor matrix, extent_t row,
                                  extent_t column) {
  auto* data = static_cast<Element*>(const_cast<void*>(matrix.data));
  const stride_t offset = matrix.layout == SparseBlasLayout::kColumnMajor
                              ? column * matrix.leading_dimension + row
                              : row * matrix.leading_dimension + column;
  return data[offset];
}

template <typename Element>
__device__ Element CsrOpValue(SparseDescriptor matrix,
                              SparseBlasTranspose transpose, extent_t row,
                              extent_t column) {
  const bool exchanged = transpose != SparseBlasTranspose::kNone;
  const extent_t source_row = exchanged ? column : row;
  const index_t source_column = exchanged ? row : column;
  const auto* offsets = static_cast<const nnz_t*>(matrix.structure_first);
  const auto* indices = static_cast<const index_t*>(matrix.structure_second);
  const auto* values = static_cast<const Element*>(matrix.values);
  Element result = DeviceArithmetic<Element>::Zero();
  for (nnz_t position = offsets[source_row]; position < offsets[source_row + 1];
       ++position) {
    if (indices[position] == source_column) {
      result = values[position];
      break;
    }
  }
  if (transpose == SparseBlasTranspose::kConjugateTranspose) {
    result = DeviceArithmetic<Element>::Conjugate(result);
  }
  return result;
}

template <typename Element>
__global__ void StandardLevel1Kernel(int operation,
                                     SparseBlasConjugation conjugation,
                                     ScalarValue alpha,
                                     IndexedVectorDescriptor sparse,
                                     VectorDescriptor dense,
                                     VectorDescriptor result) {
  using Arithmetic = DeviceArithmetic<Element>;
  const auto* indices = sparse.indices;
  auto* values = static_cast<Element*>(const_cast<void*>(sparse.values));
  if (static_cast<StandardOperation>(operation) == StandardOperation::kDot) {
    if (blockIdx.x != 0 || threadIdx.x != 0) {
      return;
    }
    Element sum = Arithmetic::Zero();
    for (nnz_t position = 0; position < sparse.nonzeros; ++position) {
      Element value = values[position];
      if (conjugation == SparseBlasConjugation::kConjugated) {
        value = Arithmetic::Conjugate(value);
      }
      sum = Arithmetic::Add(
          sum, Arithmetic::Multiply(
                   value, DenseVectorAt<Element>(dense, indices[position])));
    }
    DenseVectorAt<Element>(result, 0) = sum;
    return;
  }
  if (static_cast<StandardOperation>(operation) == StandardOperation::kAxpy &&
      Arithmetic::IsZero(Arithmetic::FromScalar(alpha))) {
    return;
  }
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t position = first;
       position < static_cast<std::uint64_t>(sparse.nonzeros);
       position += step) {
    Element& dense_value = DenseVectorAt<Element>(dense, indices[position]);
    switch (static_cast<StandardOperation>(operation)) {
      case StandardOperation::kAxpy:
        dense_value = Arithmetic::Add(
            dense_value, Arithmetic::Multiply(Arithmetic::FromScalar(alpha),
                                              values[position]));
        break;
      case StandardOperation::kGather:
        values[position] = dense_value;
        break;
      case StandardOperation::kGatherZero:
        values[position] = dense_value;
        dense_value = Arithmetic::Zero();
        break;
      case StandardOperation::kScatter:
        dense_value = values[position];
        break;
      case StandardOperation::kDot:
        break;
    }
  }
}

template <typename Element>
__global__ void StandardSpmvKernel(SparseBlasTranspose transpose,
                                   ScalarValue alpha, SparseDescriptor matrix,
                                   VectorDescriptor input,
                                   VectorDescriptor output) {
  using Arithmetic = DeviceArithmetic<Element>;
  if (Arithmetic::IsZero(Arithmetic::FromScalar(alpha))) {
    return;
  }
  const extent_t output_extent =
      transpose == SparseBlasTranspose::kNone ? matrix.rows : matrix.columns;
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t output_index = first;
       output_index < static_cast<std::uint64_t>(output_extent);
       output_index += step) {
    Element sum = Arithmetic::Zero();
    if (transpose == SparseBlasTranspose::kNone) {
      const auto* offsets = static_cast<const nnz_t*>(matrix.structure_first);
      const auto* indices =
          static_cast<const index_t*>(matrix.structure_second);
      const auto* values = static_cast<const Element*>(matrix.values);
      for (nnz_t position = offsets[output_index];
           position < offsets[output_index + 1]; ++position) {
        sum = Arithmetic::Add(
            sum, Arithmetic::Multiply(
                     values[position],
                     DenseVectorAt<Element>(input, indices[position])));
      }
    } else {
      for (extent_t input_index = 0; input_index < matrix.rows; ++input_index) {
        sum = Arithmetic::Add(
            sum,
            Arithmetic::Multiply(CsrOpValue<Element>(matrix, transpose,
                                                     output_index, input_index),
                                 DenseVectorAt<Element>(input, input_index)));
      }
    }
    Element& destination = DenseVectorAt<Element>(output, output_index);
    destination = Arithmetic::Add(
        destination, Arithmetic::Multiply(Arithmetic::FromScalar(alpha), sum));
  }
}

template <typename Element>
__global__ void StandardSpmmKernel(SparseBlasTranspose transpose,
                                   ScalarValue alpha, SparseDescriptor matrix,
                                   MatrixDescriptor input,
                                   MatrixDescriptor output) {
  using Arithmetic = DeviceArithmetic<Element>;
  if (Arithmetic::IsZero(Arithmetic::FromScalar(alpha))) {
    return;
  }
  const std::uint64_t count = static_cast<std::uint64_t>(output.rows) *
                              static_cast<std::uint64_t>(output.columns);
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t linear = first; linear < count; linear += step) {
    const extent_t row = linear / static_cast<std::uint64_t>(output.columns);
    const extent_t rhs = linear % static_cast<std::uint64_t>(output.columns);
    Element sum = Arithmetic::Zero();
    if (transpose == SparseBlasTranspose::kNone) {
      const auto* offsets = static_cast<const nnz_t*>(matrix.structure_first);
      const auto* indices =
          static_cast<const index_t*>(matrix.structure_second);
      const auto* values = static_cast<const Element*>(matrix.values);
      for (nnz_t position = offsets[row]; position < offsets[row + 1];
           ++position) {
        sum = Arithmetic::Add(
            sum, Arithmetic::Multiply(
                     values[position],
                     DenseMatrixAt<Element>(input, indices[position], rhs)));
      }
    } else {
      for (extent_t inner = 0; inner < input.rows; ++inner) {
        sum = Arithmetic::Add(
            sum, Arithmetic::Multiply(
                     CsrOpValue<Element>(matrix, transpose, row, inner),
                     DenseMatrixAt<Element>(input, inner, rhs)));
      }
    }
    Element& destination = DenseMatrixAt<Element>(output, row, rhs);
    destination = Arithmetic::Add(
        destination, Arithmetic::Multiply(Arithmetic::FromScalar(alpha), sum));
  }
}

template <typename Element>
__global__ void StandardTriangularSolveKernel(
    SparseBlasTranspose transpose, ScalarValue alpha, SparseDescriptor matrix,
    SparseBlasTriangle triangle, SparseBlasDiagonal diagonal,
    MatrixDescriptor right_hand_sides) {
  using Arithmetic = DeviceArithmetic<Element>;
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  const bool transposed = transpose != SparseBlasTranspose::kNone;
  const bool lower = transposed ? triangle == SparseBlasTriangle::kUpper
                                : triangle == SparseBlasTriangle::kLower;
  const Element coefficient = Arithmetic::FromScalar(alpha);
  for (std::uint64_t rhs = first;
       rhs < static_cast<std::uint64_t>(right_hand_sides.columns);
       rhs += step) {
    if (Arithmetic::IsZero(coefficient)) {
      for (extent_t row = 0; row < matrix.rows; ++row) {
        DenseMatrixAt<Element>(right_hand_sides, row, rhs) = Arithmetic::Zero();
      }
      continue;
    }
    for (extent_t solve_step = 0; solve_step < matrix.rows; ++solve_step) {
      const extent_t row = lower ? solve_step : matrix.rows - 1 - solve_step;
      Element value = Arithmetic::Multiply(
          coefficient, DenseMatrixAt<Element>(right_hand_sides, row, rhs));
      for (extent_t column = 0; column < matrix.columns; ++column) {
        if ((lower && column >= row) || (!lower && column <= row)) {
          continue;
        }
        value = Arithmetic::Subtract(
            value, Arithmetic::Multiply(
                       CsrOpValue<Element>(matrix, transpose, row, column),
                       DenseMatrixAt<Element>(right_hand_sides, column, rhs)));
      }
      if (diagonal == SparseBlasDiagonal::kNonUnit) {
        value = Arithmetic::Divide(
            value, CsrOpValue<Element>(matrix, transpose, row, row));
      }
      DenseMatrixAt<Element>(right_hand_sides, row, rhs) = value;
    }
  }
}

Status LaunchStatus(const char* message) {
  const cudaError_t error = cudaGetLastError();
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(error, ErrorCode::kProvider, message);
  }
  return Status::Ok();
}

template <typename Element>
Status LaunchSpmvTyped(void* stream, double alpha, SparseDescriptor matrix,
                       VectorDescriptor input, double beta,
                       VectorDescriptor output) {
  if (matrix.extents[0] == 0) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  CsrSpmvKernel<Element>
      <<<BlockCount(static_cast<std::uint64_t>(matrix.extents[0])),
         kThreadsPerBlock, 0, static_cast<cudaStream_t>(stream)>>>(
          static_cast<Element>(alpha),
          static_cast<const nnz_t*>(matrix.structure_first),
          static_cast<const index_t*>(matrix.structure_second),
          static_cast<const Element*>(matrix.values), matrix.extents[0],
          static_cast<const Element*>(input.data), input.stride,
          static_cast<Element>(beta),
          static_cast<Element*>(const_cast<void*>(output.data)), output.stride);
  return LaunchStatus("CUDA could not launch the strided CSR SpMV kernel");
}

template <typename Element>
Status LaunchPointwiseTyped(void* stream, PointwiseOperation operation,
                            OperandDescriptor left, OperandDescriptor right,
                            SparseDescriptor destination) {
  if (destination.nonzeros == 0) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  SparsePointwiseKernel<Element>
      <<<BlockCount(static_cast<std::uint64_t>(destination.nonzeros)),
         kThreadsPerBlock, 0, static_cast<cudaStream_t>(stream)>>>(
          static_cast<int>(operation), left, right, destination);
  return LaunchStatus("CUDA could not launch the sparse pointwise kernel");
}

}  // namespace

Status LaunchStridedCsrSpmv(void* stream, double alpha, SparseDescriptor matrix,
                            VectorDescriptor input, double beta,
                            VectorDescriptor output) {
  if (matrix.element_kind == ElementKind::kFloat) {
    return LaunchSpmvTyped<float>(stream, alpha, matrix, input, beta, output);
  }
  return LaunchSpmvTyped<double>(stream, alpha, matrix, input, beta, output);
}

Status LaunchSparsePointwise(void* stream, PointwiseOperation operation,
                             OperandDescriptor left, OperandDescriptor right,
                             SparseDescriptor destination) {
  if (destination.element_kind == ElementKind::kFloat) {
    return LaunchPointwiseTyped<float>(stream, operation, left, right,
                                       destination);
  }
  return LaunchPointwiseTyped<double>(stream, operation, left, right,
                                      destination);
}

template <typename Element>
Status LaunchStandardLevel1Typed(void* stream, StandardOperation operation,
                                 SparseBlasConjugation conjugation,
                                 ScalarValue alpha,
                                 IndexedVectorDescriptor sparse,
                                 VectorDescriptor dense,
                                 VectorDescriptor result) {
  if (operation != StandardOperation::kDot && sparse.nonzeros == 0) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  const unsigned int blocks =
      operation == StandardOperation::kDot ? 1U : BlockCount(sparse.nonzeros);
  StandardLevel1Kernel<Element>
      <<<blocks, kThreadsPerBlock, 0, static_cast<cudaStream_t>(stream)>>>(
          static_cast<int>(operation), conjugation, alpha, sparse, dense,
          result);
  return LaunchStatus("CUDA could not launch a Sparse BLAS level-one kernel");
}

template <typename Function>
Status DispatchStandard(ElementKind kind, Function function) {
  switch (kind) {
    case ElementKind::kFloat:
      return function.template operator()<float>();
    case ElementKind::kDouble:
      return function.template operator()<double>();
    case ElementKind::kComplexFloat:
      static_assert(sizeof(std::complex<float>) ==
                    sizeof(DeviceComplex<float>));
      static_assert(alignof(std::complex<float>) ==
                    alignof(DeviceComplex<float>));
      return function.template operator()<DeviceComplex<float>>();
    case ElementKind::kComplexDouble:
      static_assert(sizeof(std::complex<double>) ==
                    sizeof(DeviceComplex<double>));
      static_assert(alignof(std::complex<double>) ==
                    alignof(DeviceComplex<double>));
      return function.template operator()<DeviceComplex<double>>();
  }
  return Status(ErrorCode::kInvalidArgument,
                "CUDA Sparse BLAS scalar kind is invalid");
}

Status LaunchStandardLevel1(void* stream, StandardOperation operation,
                            SparseBlasConjugation conjugation,
                            ScalarValue alpha, IndexedVectorDescriptor sparse,
                            VectorDescriptor dense, VectorDescriptor result) {
  return DispatchStandard(sparse.element_kind, [&]<typename Element>() {
    return LaunchStandardLevel1Typed<Element>(stream, operation, conjugation,
                                              alpha, sparse, dense, result);
  });
}

Status LaunchStandardSpmv(void* stream, SparseBlasTranspose transpose,
                          ScalarValue alpha, SparseDescriptor matrix,
                          VectorDescriptor input, VectorDescriptor output) {
  if (output.extent == 0) {
    return Status::Ok();
  }
  return DispatchStandard(matrix.element_kind, [&]<typename Element>() {
    static_cast<void>(cudaGetLastError());
    StandardSpmvKernel<Element><<<BlockCount(output.extent), kThreadsPerBlock,
                                  0, static_cast<cudaStream_t>(stream)>>>(
        transpose, alpha, matrix, input, output);
    return LaunchStatus("CUDA could not launch the Sparse BLAS SpMV kernel");
  });
}

Status LaunchStandardSpmm(void* stream, SparseBlasTranspose transpose,
                          ScalarValue alpha, SparseDescriptor matrix,
                          MatrixDescriptor input, MatrixDescriptor output) {
  const std::uint64_t count = static_cast<std::uint64_t>(output.rows) *
                              static_cast<std::uint64_t>(output.columns);
  if (count == 0) {
    return Status::Ok();
  }
  return DispatchStandard(matrix.element_kind, [&]<typename Element>() {
    static_cast<void>(cudaGetLastError());
    StandardSpmmKernel<Element><<<BlockCount(count), kThreadsPerBlock, 0,
                                  static_cast<cudaStream_t>(stream)>>>(
        transpose, alpha, matrix, input, output);
    return LaunchStatus("CUDA could not launch the Sparse BLAS SpMM kernel");
  });
}

Status LaunchStandardTriangularSolve(void* stream,
                                     SparseBlasTranspose transpose,
                                     ScalarValue alpha, SparseDescriptor matrix,
                                     SparseBlasTriangle triangle,
                                     SparseBlasDiagonal diagonal,
                                     MatrixDescriptor right_hand_sides) {
  if (right_hand_sides.columns == 0 || right_hand_sides.rows == 0) {
    return Status::Ok();
  }
  return DispatchStandard(matrix.element_kind, [&]<typename Element>() {
    static_cast<void>(cudaGetLastError());
    StandardTriangularSolveKernel<Element>
        <<<BlockCount(right_hand_sides.columns), kThreadsPerBlock, 0,
           static_cast<cudaStream_t>(stream)>>>(
            transpose, alpha, matrix, triangle, diagonal, right_hand_sides);
    return LaunchStatus(
        "CUDA could not launch the Sparse BLAS triangular-solve kernel");
  });
}

}  // namespace asc::internal_sparse_cuda
