#include <array>
#include <cmath>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/dense.h"

#if __has_include("asc/array.h")
#error "The asc-xde trial must not depend on the deleted asc/array.h facade"
#endif

#if __has_include("asc/linalg.h")
#error "The asc-xde trial must not depend on the deleted asc/linalg.h facade"
#endif

namespace {

using StateExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using State = asc::DenseArray<double, StateExtents>;

bool NearlyEqual(double left, double right) {
  return std::abs(left - right) <= 1.0e-12;
}

}  // namespace

int main() {
  auto extents = StateExtents::Create(2, 3);
  if (!extents.ok()) {
    return 1;
  }

  asc::HostMemoryResource resource;
  auto state = State::Create(resource, *extents);
  auto derivative = State::Create(resource, *extents);
  auto next = State::Create(resource, *extents);
  if (!state.ok() || !derivative.ok() || !next.ok()) {
    return 2;
  }

  auto state_view = state->view();
  auto derivative_view = derivative->view();
  auto next_view = next->view();
  if (!state_view.ok() || !derivative_view.ok() || !next_view.ok()) {
    return 3;
  }

  double initial = 1.0;
  for (asc::index_t column = 0; column < 3; ++column) {
    for (asc::index_t row = 0; row < 2; ++row) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto element =
          state_view->At(std::span<const asc::index_t, 2>(coordinate));
      if (!element.ok()) {
        return 4;
      }
      **element = initial++;
    }
  }

  constexpr double kDecay = 0.25;
  constexpr double kTimeStep = 0.2;
  const asc::DenseView<const double, 2> state_read(*state_view);
  auto derivative_expression = asc::MakeMultiply(-kDecay, state_read);
  if (!derivative_expression.ok()) {
    return 5;
  }
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  if (!asc::Evaluate(context, *derivative_expression, *derivative_view).ok()) {
    return 6;
  }

  const asc::DenseView<const double, 2> derivative_read(*derivative_view);
  auto increment = asc::MakeMultiply(kTimeStep, derivative_read);
  if (!increment.ok()) {
    return 7;
  }
  auto next_expression = asc::MakeAdd(state_read, *increment);
  if (!next_expression.ok() ||
      !asc::Evaluate(context, *next_expression, *next_view).ok()) {
    return 8;
  }

  initial = 1.0;
  for (asc::index_t column = 0; column < 3; ++column) {
    for (asc::index_t row = 0; row < 2; ++row) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto element =
          next_view->At(std::span<const asc::index_t, 2>(coordinate));
      if (!element.ok() || !NearlyEqual(**element, 0.95 * initial++)) {
        return 9;
      }
    }
  }

  auto checksum = asc::ReduceSum(context, *next_view);
  if (!checksum.ok() || !NearlyEqual(*checksum, 19.95)) {
    return 10;
  }
  return 0;
}
