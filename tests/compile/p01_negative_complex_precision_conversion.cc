#include <complex>

#include "asc/core/execution.h"
#include "asc/dense/evaluate.h"
#include "asc/dense/view.h"

auto InvalidConversion(asc::DenseView<const std::complex<float>, 1> input,
                       asc::DenseView<std::complex<double>, 1> output) {
  return asc::Evaluate(asc::ExecutionContext::Serial(), input, output);
}
