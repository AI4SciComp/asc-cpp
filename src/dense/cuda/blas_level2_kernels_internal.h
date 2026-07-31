#ifndef ASC_SRC_DENSE_CUDA_BLAS_LEVEL2_KERNELS_INTERNAL_H_
#define ASC_SRC_DENSE_CUDA_BLAS_LEVEL2_KERNELS_INTERNAL_H_

#include <cstdint>

#include "asc/core/status.h"

namespace asc::internal_dense_cuda {

enum class Level2ElementKind : std::uint8_t {
  kRealFloat,
  kRealDouble,
  kComplexFloat,
  kComplexDouble,
};

enum class Level2StorageKind : std::uint8_t {
  kFull,
  kGeneralBand,
  kTriangularBand,
  kPacked,
};

struct Level2ComplexScalar {
  double real = 0;
  double imaginary = 0;
};

struct Level2MatrixDescriptor {
  void* data = nullptr;
  std::int64_t rows = 0;
  std::int64_t columns = 0;
  std::int64_t leading_dimension = 0;
  std::int64_t lower_bandwidth = 0;
  std::int64_t upper_bandwidth = 0;
  Level2StorageKind storage = Level2StorageKind::kFull;
  bool upper = true;
};

struct Level2VectorDescriptor {
  void* data = nullptr;
  std::int64_t size = 0;
  std::int64_t increment = 1;
};

Status LaunchLevel2ScaleVector(void* stream, Level2ElementKind element_kind,
                               Level2ComplexScalar beta,
                               Level2VectorDescriptor vector);

Status LaunchLevel2ConjugateGemv(void* stream, Level2ElementKind element_kind,
                                 Level2ComplexScalar alpha,
                                 Level2MatrixDescriptor matrix,
                                 Level2VectorDescriptor input,
                                 Level2ComplexScalar beta,
                                 Level2VectorDescriptor output);

Status LaunchLevel2HermitianMv(void* stream, Level2ElementKind element_kind,
                               Level2ComplexScalar alpha,
                               Level2MatrixDescriptor matrix,
                               Level2VectorDescriptor input,
                               Level2ComplexScalar beta,
                               Level2VectorDescriptor output);

Status LaunchLevel2ConjugateTriangular(void* stream,
                                       Level2ElementKind element_kind,
                                       Level2MatrixDescriptor matrix,
                                       Level2VectorDescriptor vector, bool unit,
                                       bool solve);

Status LaunchLevel2Gerc(void* stream, Level2ElementKind element_kind,
                        Level2ComplexScalar alpha, Level2VectorDescriptor x,
                        Level2VectorDescriptor y,
                        Level2MatrixDescriptor matrix);

Status LaunchLevel2HermitianUpdate(void* stream, Level2ElementKind element_kind,
                                   Level2ComplexScalar alpha,
                                   Level2VectorDescriptor x,
                                   Level2VectorDescriptor y,
                                   Level2MatrixDescriptor matrix,
                                   bool rank_two);

}  // namespace asc::internal_dense_cuda

#endif  // ASC_SRC_DENSE_CUDA_BLAS_LEVEL2_KERNELS_INTERNAL_H_
