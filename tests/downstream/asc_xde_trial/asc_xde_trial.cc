#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/evaluate.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"

#if __has_include("asc/array.h")
#error "The M8 downstream trial must not see deleted asc/array.h."
#endif
#if __has_include("asc/linalg.h")
#error "The M8 downstream trial must not see deleted asc/linalg.h."
#endif

#ifndef ASC_XDE_TRIAL_BASELINE
#error "ASC_XDE_TRIAL_BASELINE must record the skeletal asc-xde commit."
#endif

namespace {

template <std::size_t Rank>
bool Set(asc::DenseView<double, Rank> view,
         const std::array<asc::index_t, Rank>& index, double value) {
  auto element = view.At(index);
  if (!element.ok()) {
    return false;
  }
  **element = value;
  return true;
}

template <std::size_t Rank>
asc::Result<double*> Get(asc::DenseView<double, Rank> view,
                         const std::array<asc::index_t, Rank>& index) {
  return view.At(index);
}

bool Near(double actual, double expected) {
  return std::fabs(actual - expected) <= 1.0e-12;
}

bool RunOdeStep(asc::HostMemoryResource& resource,
                const asc::ExecutionContext& execution) {
  using Extents = asc::Extents<1>;
  using Array = asc::DenseArray<double, Extents>;
  const auto extents = Extents::Create();
  if (!extents.ok()) {
    return false;
  }
  auto state = Array::Create(*extents, resource);
  auto derivative = Array::Create(*extents, resource);
  auto next = Array::Create(*extents, resource);
  if (!state.ok() || !derivative.ok() || !next.ok()) {
    return false;
  }
  auto state_view = state->view();
  auto derivative_view = derivative->view();
  auto next_view = next->view();
  if (!state_view.ok() || !derivative_view.ok() || !next_view.ok()) {
    return false;
  }
  constexpr std::array<asc::index_t, 1> kIndex = {0};
  if (!Set(*state_view, kIndex, 1.0) || !Set(*derivative_view, kIndex, -2.0)) {
    return false;
  }

  auto increment = asc::MakeMultiply(*derivative_view, 0.1);
  if (!increment.ok()) {
    return false;
  }
  auto euler_step = asc::MakeAdd(*state_view, std::move(*increment));
  if (!euler_step.ok() ||
      !asc::Evaluate(execution, *next_view, *euler_step).ok()) {
    return false;
  }
  auto value = Get(*next_view, kIndex);
  return value.ok() && Near(**value, 0.8);
}

bool RunPdeStep(asc::HostMemoryResource& resource,
                const asc::ExecutionContext& execution) {
  using Extents = asc::Extents<5>;
  using Array = asc::DenseArray<double, Extents>;
  const auto extents = Extents::Create();
  if (!extents.ok()) {
    return false;
  }
  auto state = Array::Create(*extents, resource);
  auto laplacian = Array::Create(*extents, resource);
  auto next = Array::Create(*extents, resource);
  if (!state.ok() || !laplacian.ok() || !next.ok()) {
    return false;
  }
  auto state_view = state->view();
  auto laplacian_view = laplacian->view();
  auto next_view = next->view();
  if (!state_view.ok() || !laplacian_view.ok() || !next_view.ok()) {
    return false;
  }

  constexpr std::array<double, 5> kInitial = {0.0, 1.0, 2.0, 1.0, 0.0};
  for (asc::index_t index = 0; index < 5; ++index) {
    const std::array<asc::index_t, 1> coordinate = {index};
    if (!Set(*state_view, coordinate,
             kInitial[static_cast<std::size_t>(index)]) ||
        !Set(*laplacian_view, coordinate, 0.0)) {
      return false;
    }
  }
  for (asc::index_t index = 1; index < 4; ++index) {
    const std::array<asc::index_t, 1> left = {index - 1};
    const std::array<asc::index_t, 1> center = {index};
    const std::array<asc::index_t, 1> right = {index + 1};
    auto left_value = Get(*state_view, left);
    auto center_value = Get(*state_view, center);
    auto right_value = Get(*state_view, right);
    if (!left_value.ok() || !center_value.ok() || !right_value.ok()) {
      return false;
    }
    const double stencil = **left_value - 2.0 * **center_value + **right_value;
    if (!Set(*laplacian_view, center, stencil)) {
      return false;
    }
  }

  auto increment = asc::MakeMultiply(*laplacian_view, 0.25);
  if (!increment.ok()) {
    return false;
  }
  auto forward_euler = asc::MakeAdd(*state_view, std::move(*increment));
  if (!forward_euler.ok() ||
      !asc::Evaluate(execution, *next_view, *forward_euler).ok()) {
    return false;
  }

  constexpr std::array<double, 5> kExpected = {0.0, 1.0, 1.5, 1.0, 0.0};
  for (asc::index_t index = 0; index < 5; ++index) {
    const std::array<asc::index_t, 1> coordinate = {index};
    auto value = Get(*next_view, coordinate);
    if (!value.ok() ||
        !Near(**value, kExpected[static_cast<std::size_t>(index)])) {
      return false;
    }
  }
  const auto checksum = asc::ReduceSum(execution, *next_view);
  return checksum.ok() && Near(*checksum, 3.5);
}

}  // namespace

int main() {
  asc::HostMemoryResource resource;
  const asc::ExecutionContext execution = asc::ExecutionContext::Serial();
  if (!RunOdeStep(resource, execution) || !RunPdeStep(resource, execution)) {
    return 1;
  }
  std::cout << "M8_ASC_XDE_TRIAL baseline=" << ASC_XDE_TRIAL_BASELINE
            << " ode=pass pde=pass checksum=3.5\n";
  return 0;
}
