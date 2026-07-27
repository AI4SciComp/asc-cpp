#include <array>
#include <cstddef>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense.h"

int main() {
  using Extents = asc::Extents<2, 2>;
  using Array = asc::DenseArray<double, Extents>;

  const auto extents = Extents::Create();
  if (!extents.ok()) {
    return 1;
  }
  asc::HostMemoryResource resource;
  auto source = Array::Create(*extents, resource);
  auto destination = Array::Create(*extents, resource, asc::LayoutRight{});
  if (!source.ok() || !destination.ok()) {
    return 2;
  }
  auto source_view = source->view();
  auto destination_view = destination->view();
  if (!source_view.ok() || !destination_view.ok()) {
    return 3;
  }

  constexpr std::array<double, 4> kValues = {1.0, 2.0, 3.0, 4.0};
  std::size_t logical = 0;
  for (asc::index_t column = 0; column < 2; ++column) {
    for (asc::index_t row = 0; row < 2; ++row) {
      const std::array<asc::index_t, 2> index = {row, column};
      auto element = source_view->At(index);
      if (!element.ok()) {
        return 4;
      }
      **element = kValues[logical++];
    }
  }

  auto expression = asc::MakeAdd(*source_view, 1.0);
  if (!expression.ok()) {
    return 5;
  }
  const asc::Status evaluated = asc::Evaluate(asc::ExecutionContext::Serial(),
                                              *destination_view, *expression);
  if (!evaluated.ok()) {
    return 6;
  }
  const asc::Status scaled =
      asc::Scal(asc::ExecutionContext::Serial(), 2.0, *destination_view);
  if (!scaled.ok()) {
    return 7;
  }
  const auto sum =
      asc::ReduceSum(asc::ExecutionContext::Serial(), *destination_view);
  return sum.ok() && *sum == 28.0 ? 0 : 8;
}
