#include <array>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense.h"

asc::Status AscM8DenseObservation(double* source, double* destination) {
  constexpr std::array<asc::extent_t, 2> kShape{4, 4};
  auto mapping =
      asc::DenseLayout<2>::Create(std::span<const asc::extent_t, 2>(kShape));
  if (!mapping.ok()) {
    return mapping.status();
  }
  auto input = asc::DenseView<const double, 2>::Create(source, *mapping,
                                                       asc::MemorySpace::kHost);
  auto output = asc::DenseView<double, 2>::Create(destination, *mapping,
                                                  asc::MemorySpace::kHost);
  if (!input.ok()) {
    return input.status();
  }
  if (!output.ok()) {
    return output.status();
  }
  auto expression = asc::MakeMultiply(2.0, *input);
  if (!expression.ok()) {
    return expression.status();
  }
  return asc::Evaluate(asc::ExecutionContext::Serial(), *expression, *output);
}
