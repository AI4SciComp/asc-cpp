#include "asc/random/providers/dense_cuda.h"

void FillConstDense(const asc::ExecutionContext& context,
                    asc::DenseView<const float, 1> destination) {
  const auto result =
      asc::CudaFillDenseUniform01(context, destination, 1, 2, 3);
  static_cast<void>(result);
}
