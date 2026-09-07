#include <complex>

#include "asc/core/execution.h"
#include "asc/dense/evaluate.h"
#include "asc/dense/view.h"

auto InvalidMinimum(asc::DenseView<std::complex<float>, 1> input) {
  return asc::ReduceMin(asc::ExecutionContext::Serial(), input);
}
