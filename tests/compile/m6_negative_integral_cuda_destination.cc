#include "asc/dense/providers/cuda.h"

void EvaluateIntoIntegral(asc::DenseCudaContext& context,
                          asc::DenseView<int, 1> destination) {
  const auto event = asc::CudaEvaluate(context, 1, destination);
  static_cast<void>(event);
}
