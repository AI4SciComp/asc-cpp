#include "asc/random/dense.h"

void MustNotCompile(const asc::ExecutionContext& context,
                    asc::DenseView<int, 1> destination) {
  (void)asc::FillDenseUniform01(context, destination, 1, 2, 3);
}
