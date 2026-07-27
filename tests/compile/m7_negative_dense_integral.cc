#include "asc/random/providers/dense_cuda.h"

void FillIntegralDense(const asc::ExecutionContext& context,
                       asc::DenseView<int, 1> destination) {
  static_cast<void>(asc::CudaFillDenseUniform01(
      context, destination, asc::RandomStream{1}, asc::RandomSubsequence{2},
      asc::RandomOffset{3}));
}
