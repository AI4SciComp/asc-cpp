#include "asc/random/providers/dense_cuda.h"

void FillConstDense(const asc::ExecutionContext& context,
                    asc::DenseView<const float, 1> destination) {
  static_cast<void>(asc::CudaFillDenseUniform01(
      context, destination, asc::RandomStream{1}, asc::RandomSubsequence{2},
      asc::RandomOffset{3}));
}
