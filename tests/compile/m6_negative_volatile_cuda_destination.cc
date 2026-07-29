#include "asc/dense/providers/cuda.h"

void EvaluateIntoVolatile(asc::DenseCudaContext& context,
                          asc::DenseView<volatile float, 1> destination) {
  const auto event = asc::CudaEvaluate(context, 1.0F, destination);
  static_cast<void>(event);
}
