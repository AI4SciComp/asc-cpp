#include <complex>

#include "asc/core/execution.h"
#include "asc/dense/evaluate.h"
#include "asc/dense/view.h"

auto InvalidMaximum(asc::DenseView<const std::complex<double>, 1> input) {
  return asc::ReduceMax(asc::ExecutionContext::Serial(), input);
}
