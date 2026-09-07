#include <cuda_runtime_api.h>
#include <driver_types.h>

#include <cstdint>

#include "../../core/cuda/cuda_internal.h"
#include "asc/core/status.h"
#include "blas_level2_kernels_internal.h"

namespace asc::internal_dense_cuda {
namespace {

template <typename Real>
struct DeviceComplex {
  Real real;
  Real imaginary;
};

template <typename Real>
__device__ DeviceComplex<Real> Add(DeviceComplex<Real> left,
                                   DeviceComplex<Real> right) {
  return {left.real + right.real, left.imaginary + right.imaginary};
}

template <typename Real>
__device__ DeviceComplex<Real> Subtract(DeviceComplex<Real> left,
                                        DeviceComplex<Real> right) {
  return {left.real - right.real, left.imaginary - right.imaginary};
}

template <typename Real>
__device__ DeviceComplex<Real> Multiply(DeviceComplex<Real> left,
                                        DeviceComplex<Real> right) {
  return {left.real * right.real - left.imaginary * right.imaginary,
          left.real * right.imaginary + left.imaginary * right.real};
}

template <typename Real>
__device__ DeviceComplex<Real> Divide(DeviceComplex<Real> numerator,
                                      DeviceComplex<Real> denominator) {
  const Real scale = denominator.real * denominator.real +
                     denominator.imaginary * denominator.imaginary;
  return {(numerator.real * denominator.real +
           numerator.imaginary * denominator.imaginary) /
              scale,
          (numerator.imaginary * denominator.real -
           numerator.real * denominator.imaginary) /
              scale};
}

template <typename Real>
__device__ DeviceComplex<Real> Conjugate(DeviceComplex<Real> value) {
  return {value.real, -value.imaginary};
}

template <typename Real>
__device__ DeviceComplex<Real> Scalar(Level2ComplexScalar value) {
  return {static_cast<Real>(value.real), static_cast<Real>(value.imaginary)};
}

template <typename Real>
__device__ DeviceComplex<Real>& VectorAt(Level2VectorDescriptor vector,
                                         std::int64_t index) {
  auto* data = static_cast<DeviceComplex<Real>*>(vector.data);
  return data[index * vector.increment];
}

template <typename Element>
__global__ void ScaleRealKernel(Level2ComplexScalar beta_value,
                                Level2VectorDescriptor vector) {
  if (blockIdx.x != 0 || threadIdx.x != 0) {
    return;
  }
  auto* data = static_cast<Element*>(vector.data);
  const Element beta = static_cast<Element>(beta_value.real);
  for (std::int64_t index = 0; index < vector.size; ++index) {
    Element& value = data[index * vector.increment];
    value = beta == Element{0} ? Element{0} : beta * value;
  }
}

template <typename Real>
__global__ void ScaleComplexKernel(Level2ComplexScalar beta_value,
                                   Level2VectorDescriptor vector) {
  if (blockIdx.x != 0 || threadIdx.x != 0) {
    return;
  }
  const DeviceComplex<Real> beta = Scalar<Real>(beta_value);
  for (std::int64_t index = 0; index < vector.size; ++index) {
    DeviceComplex<Real>& value = VectorAt<Real>(vector, index);
    value = beta.real == 0 && beta.imaginary == 0 ? DeviceComplex<Real>{0, 0}
                                                  : Multiply(beta, value);
  }
}

__device__ std::int64_t PackedOffset(std::int64_t order, bool upper,
                                     std::int64_t row, std::int64_t column) {
  if (upper) {
    return row * order - row * (row - 1) / 2 + column - row;
  }
  return row * (row + 1) / 2 + column;
}

template <typename Real>
__device__ DeviceComplex<Real>& StoredAt(Level2MatrixDescriptor matrix,
                                         std::int64_t row,
                                         std::int64_t column) {
  auto* data = static_cast<DeviceComplex<Real>*>(matrix.data);
  std::int64_t offset = 0;
  if (matrix.storage == Level2StorageKind::kFull) {
    offset = row * matrix.leading_dimension + column;
  } else if (matrix.storage == Level2StorageKind::kGeneralBand) {
    offset =
        row * matrix.leading_dimension + matrix.lower_bandwidth + column - row;
  } else if (matrix.storage == Level2StorageKind::kTriangularBand) {
    offset = matrix.upper ? row * matrix.leading_dimension + column - row
                          : row * matrix.leading_dimension +
                                matrix.lower_bandwidth + column - row;
  } else {
    offset = PackedOffset(matrix.rows, matrix.upper, row, column);
  }
  return data[offset];
}

template <typename Real>
__device__ DeviceComplex<Real> GeneralAt(Level2MatrixDescriptor matrix,
                                         std::int64_t row,
                                         std::int64_t column) {
  if (matrix.storage == Level2StorageKind::kGeneralBand &&
      (column > row + matrix.upper_bandwidth ||
       row > column + matrix.lower_bandwidth)) {
    return {0, 0};
  }
  return StoredAt<Real>(matrix, row, column);
}

template <typename Real>
__device__ DeviceComplex<Real> HermitianAt(Level2MatrixDescriptor matrix,
                                           std::int64_t row,
                                           std::int64_t column) {
  if (matrix.storage == Level2StorageKind::kTriangularBand &&
      (row > column + matrix.lower_bandwidth ||
       column > row + matrix.lower_bandwidth)) {
    return {0, 0};
  }
  bool reflected = false;
  if ((matrix.upper && row > column) || (!matrix.upper && row < column)) {
    const std::int64_t temporary = row;
    row = column;
    column = temporary;
    reflected = true;
  }
  DeviceComplex<Real> value = StoredAt<Real>(matrix, row, column);
  if (row == column) {
    value.imaginary = 0;
  }
  return reflected ? Conjugate(value) : value;
}

template <typename Real>
__device__ DeviceComplex<Real> TriangularAt(Level2MatrixDescriptor matrix,
                                            std::int64_t row,
                                            std::int64_t column, bool unit) {
  if (row == column && unit) {
    return {1, 0};
  }
  if ((matrix.upper && row > column) || (!matrix.upper && row < column)) {
    return {0, 0};
  }
  if (matrix.storage == Level2StorageKind::kTriangularBand &&
      (row > column + matrix.lower_bandwidth ||
       column > row + matrix.lower_bandwidth)) {
    return {0, 0};
  }
  return StoredAt<Real>(matrix, row, column);
}

template <typename Real>
__global__ void ConjugateGemvKernel(Level2ComplexScalar alpha_value,
                                    Level2MatrixDescriptor matrix,
                                    Level2VectorDescriptor input,
                                    Level2ComplexScalar beta_value,
                                    Level2VectorDescriptor output) {
  if (blockIdx.x != 0 || threadIdx.x != 0) {
    return;
  }
  const DeviceComplex<Real> alpha = Scalar<Real>(alpha_value);
  const DeviceComplex<Real> beta = Scalar<Real>(beta_value);
  for (std::int64_t column = 0; column < matrix.columns; ++column) {
    DeviceComplex<Real> product{0, 0};
    if (alpha.real != 0 || alpha.imaginary != 0) {
      for (std::int64_t row = 0; row < matrix.rows; ++row) {
        product = Add(product,
                      Multiply(Conjugate(GeneralAt<Real>(matrix, row, column)),
                               VectorAt<Real>(input, row)));
      }
    }
    const DeviceComplex<Real> old = beta.real == 0 && beta.imaginary == 0
                                        ? DeviceComplex<Real>{0, 0}
                                        : VectorAt<Real>(output, column);
    VectorAt<Real>(output, column) =
        Add(Multiply(alpha, product), Multiply(beta, old));
  }
}

template <typename Real>
__global__ void HermitianMvKernel(Level2ComplexScalar alpha_value,
                                  Level2MatrixDescriptor matrix,
                                  Level2VectorDescriptor input,
                                  Level2ComplexScalar beta_value,
                                  Level2VectorDescriptor output) {
  if (blockIdx.x != 0 || threadIdx.x != 0) {
    return;
  }
  const DeviceComplex<Real> alpha = Scalar<Real>(alpha_value);
  const DeviceComplex<Real> beta = Scalar<Real>(beta_value);
  for (std::int64_t row = 0; row < matrix.rows; ++row) {
    DeviceComplex<Real> product{0, 0};
    if (alpha.real != 0 || alpha.imaginary != 0) {
      for (std::int64_t column = 0; column < matrix.rows; ++column) {
        product = Add(product, Multiply(HermitianAt<Real>(matrix, row, column),
                                        VectorAt<Real>(input, column)));
      }
    }
    const DeviceComplex<Real> old = beta.real == 0 && beta.imaginary == 0
                                        ? DeviceComplex<Real>{0, 0}
                                        : VectorAt<Real>(output, row);
    VectorAt<Real>(output, row) =
        Add(Multiply(alpha, product), Multiply(beta, old));
  }
}

template <typename Real>
__global__ void ConjugateTriangularKernel(Level2MatrixDescriptor matrix,
                                          Level2VectorDescriptor vector,
                                          bool unit, bool solve) {
  if (blockIdx.x != 0 || threadIdx.x != 0) {
    return;
  }
  const bool effective_upper = !matrix.upper;
  if (!solve) {
    for (std::int64_t iteration = 0; iteration < matrix.rows; ++iteration) {
      const std::int64_t row =
          effective_upper ? iteration : matrix.rows - 1 - iteration;
      DeviceComplex<Real> result{0, 0};
      const std::int64_t begin = effective_upper ? row : 0;
      const std::int64_t end = effective_upper ? matrix.rows : row + 1;
      for (std::int64_t column = begin; column < end; ++column) {
        result = Add(
            result,
            // Conjugate-transpose access intentionally reverses row/column.
            // NOLINTNEXTLINE(readability-suspicious-call-argument)
            Multiply(Conjugate(TriangularAt<Real>(matrix, column, row, unit)),
                     VectorAt<Real>(vector, column)));
      }
      VectorAt<Real>(vector, row) = result;
    }
    return;
  }
  for (std::int64_t iteration = 0; iteration < matrix.rows; ++iteration) {
    const std::int64_t row =
        effective_upper ? matrix.rows - 1 - iteration : iteration;
    DeviceComplex<Real> result = VectorAt<Real>(vector, row);
    const std::int64_t begin = effective_upper ? row + 1 : 0;
    const std::int64_t end = effective_upper ? matrix.rows : row;
    for (std::int64_t column = begin; column < end; ++column) {
      result = Subtract(
          result,
          // Conjugate-transpose access intentionally reverses row/column.
          // NOLINTNEXTLINE(readability-suspicious-call-argument)
          Multiply(Conjugate(TriangularAt<Real>(matrix, column, row, unit)),
                   VectorAt<Real>(vector, column)));
    }
    if (!unit) {
      result = Divide(result,
                      Conjugate(TriangularAt<Real>(matrix, row, row, false)));
    }
    VectorAt<Real>(vector, row) = result;
  }
}

template <typename Real>
__global__ void GercKernel(Level2ComplexScalar alpha_value,
                           Level2VectorDescriptor x, Level2VectorDescriptor y,
                           Level2MatrixDescriptor matrix) {
  if (blockIdx.x != 0 || threadIdx.x != 0) {
    return;
  }
  const DeviceComplex<Real> alpha = Scalar<Real>(alpha_value);
  for (std::int64_t row = 0; row < matrix.rows; ++row) {
    for (std::int64_t column = 0; column < matrix.columns; ++column) {
      DeviceComplex<Real>& destination = StoredAt<Real>(matrix, row, column);
      destination =
          Add(destination,
              Multiply(alpha, Multiply(VectorAt<Real>(x, row),
                                       Conjugate(VectorAt<Real>(y, column)))));
    }
  }
}

template <typename Real>
__global__ void HermitianUpdateKernel(Level2ComplexScalar alpha_value,
                                      Level2VectorDescriptor x,
                                      Level2VectorDescriptor y,
                                      Level2MatrixDescriptor matrix,
                                      bool rank_two) {
  if (blockIdx.x != 0 || threadIdx.x != 0) {
    return;
  }
  const DeviceComplex<Real> alpha = Scalar<Real>(alpha_value);
  for (std::int64_t row = 0; row < matrix.rows; ++row) {
    const std::int64_t begin = matrix.upper ? row : 0;
    const std::int64_t end = matrix.upper ? matrix.rows : row + 1;
    for (std::int64_t column = begin; column < end; ++column) {
      DeviceComplex<Real> update = Multiply(
          alpha, Multiply(VectorAt<Real>(x, row),
                          Conjugate(VectorAt<Real>(rank_two ? y : x, column))));
      if (rank_two) {
        update = Add(update,
                     Multiply(Conjugate(alpha),
                              Multiply(VectorAt<Real>(y, row),
                                       Conjugate(VectorAt<Real>(x, column)))));
      }
      DeviceComplex<Real>& destination = StoredAt<Real>(matrix, row, column);
      destination = Add(destination, update);
      if (row == column) {
        destination.imaginary = 0;
      }
    }
  }
}

template <typename Launch>
Status LaunchAndCheck(Launch launch) {
  launch();
  const cudaError_t error = cudaGetLastError();
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(
        error, ErrorCode::kProvider,
        "CUDA could not launch a Dense Level 2 project kernel");
  }
  return Status::Ok();
}

}  // namespace

Status LaunchLevel2ScaleVector(void* stream, Level2ElementKind element_kind,
                               Level2ComplexScalar beta,
                               Level2VectorDescriptor vector) {
  return LaunchAndCheck([&] {
    switch (element_kind) {
      case Level2ElementKind::kRealFloat:
        ScaleRealKernel<float>
            <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(beta, vector);
        break;
      case Level2ElementKind::kRealDouble:
        ScaleRealKernel<double>
            <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(beta, vector);
        break;
      case Level2ElementKind::kComplexFloat:
        ScaleComplexKernel<float>
            <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(beta, vector);
        break;
      case Level2ElementKind::kComplexDouble:
        ScaleComplexKernel<double>
            <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(beta, vector);
        break;
    }
  });
}

Status LaunchLevel2ConjugateGemv(void* stream, Level2ElementKind element_kind,
                                 Level2ComplexScalar alpha,
                                 Level2MatrixDescriptor matrix,
                                 Level2VectorDescriptor input,
                                 Level2ComplexScalar beta,
                                 Level2VectorDescriptor output) {
  return LaunchAndCheck([&] {
    if (element_kind == Level2ElementKind::kComplexFloat) {
      ConjugateGemvKernel<float>
          <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(alpha, matrix, input,
                                                           beta, output);
    } else {
      ConjugateGemvKernel<double>
          <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(alpha, matrix, input,
                                                           beta, output);
    }
  });
}

Status LaunchLevel2HermitianMv(void* stream, Level2ElementKind element_kind,
                               Level2ComplexScalar alpha,
                               Level2MatrixDescriptor matrix,
                               Level2VectorDescriptor input,
                               Level2ComplexScalar beta,
                               Level2VectorDescriptor output) {
  return LaunchAndCheck([&] {
    if (element_kind == Level2ElementKind::kComplexFloat) {
      HermitianMvKernel<float><<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(
          alpha, matrix, input, beta, output);
    } else {
      HermitianMvKernel<double><<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(
          alpha, matrix, input, beta, output);
    }
  });
}

Status LaunchLevel2ConjugateTriangular(void* stream,
                                       Level2ElementKind element_kind,
                                       Level2MatrixDescriptor matrix,
                                       Level2VectorDescriptor vector, bool unit,
                                       bool solve) {
  return LaunchAndCheck([&] {
    if (element_kind == Level2ElementKind::kComplexFloat) {
      ConjugateTriangularKernel<float>
          <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(matrix, vector, unit,
                                                           solve);
    } else {
      ConjugateTriangularKernel<double>
          <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(matrix, vector, unit,
                                                           solve);
    }
  });
}

Status LaunchLevel2Gerc(void* stream, Level2ElementKind element_kind,
                        Level2ComplexScalar alpha, Level2VectorDescriptor x,
                        Level2VectorDescriptor y,
                        Level2MatrixDescriptor matrix) {
  return LaunchAndCheck([&] {
    if (element_kind == Level2ElementKind::kComplexFloat) {
      GercKernel<float>
          <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(alpha, x, y, matrix);
    } else {
      GercKernel<double>
          <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(alpha, x, y, matrix);
    }
  });
}

Status LaunchLevel2HermitianUpdate(void* stream, Level2ElementKind element_kind,
                                   Level2ComplexScalar alpha,
                                   Level2VectorDescriptor x,
                                   Level2VectorDescriptor y,
                                   Level2MatrixDescriptor matrix,
                                   bool rank_two) {
  return LaunchAndCheck([&] {
    if (element_kind == Level2ElementKind::kComplexFloat) {
      HermitianUpdateKernel<float>
          <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(alpha, x, y, matrix,
                                                           rank_two);
    } else {
      HermitianUpdateKernel<double>
          <<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(alpha, x, y, matrix,
                                                           rank_two);
    }
  });
}

}  // namespace asc::internal_dense_cuda
