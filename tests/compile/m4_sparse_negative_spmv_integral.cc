#include "asc/core/execution.h"
#include "asc/dense/view.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/linalg.h"

void IntegralSpmv(asc::CsrView<const int> matrix,
                  asc::DenseView<const int, 1> input,
                  asc::DenseView<int, 1> output) {
  static_cast<void>(
      asc::Spmv(asc::ExecutionContext::Serial(), 1, matrix, input, 0, output));
}

int main() { return 0; }
