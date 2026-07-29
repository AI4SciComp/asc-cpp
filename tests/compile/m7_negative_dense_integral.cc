#include "asc/random/providers/dense_cuda.h"

void FillIntegralDense(const asc::ExecutionContext& context,
                       asc::DenseView<int, 1> destination) {
  const auto result =
      asc::CudaFillDenseUniform01(context, destination, 1, 2, 3);
  static_cast<void>(result);
}
