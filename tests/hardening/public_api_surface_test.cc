#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/dense.h"

#if defined(ASC_M8_VERIFY_CUDA)
#include "asc/random/providers/cuda.h"
#endif

namespace {

using GridExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using Grid = asc::DenseArray<double, GridExtents>;

static_assert(std::is_move_constructible_v<asc::Buffer>);
static_assert(!std::is_copy_constructible_v<asc::Buffer>);
static_assert(std::is_move_constructible_v<asc::CompletionEvent>);
static_assert(!std::is_copy_constructible_v<asc::CompletionEvent>);
static_assert(std::is_move_constructible_v<Grid>);
static_assert(!std::is_copy_constructible_v<Grid>);
static_assert(std::is_convertible_v<asc::DenseView<double, 2>,
                                    asc::DenseView<const double, 2>>);

#if defined(ASC_M8_VERIFY_CUDA)
template <typename Destination>
concept AcceptsRawCudaDestination =
    requires(const asc::ExecutionContext& context, Destination destination) {
      {
        asc::CudaFillPhilox4x32(context, destination, std::uint64_t{4},
                                asc::RandomStream{1}, asc::RandomSubsequence{2},
                                asc::RandomOffset{3})
      } -> std::same_as<asc::Result<asc::CudaRandomWordGeneration>>;
    };

static_assert(AcceptsRawCudaDestination<asc::MutableMemoryView>);
static_assert(!AcceptsRawCudaDestination<std::uint32_t*>);
#endif

bool NearlyEqual(double left, double right) {
  return std::abs(left - right) <= 1.0e-12;
}

}  // namespace

int main() {
  auto extents = GridExtents::Create(2, 3);
  if (!extents.ok()) {
    return 1;
  }

  asc::HostMemoryResource resource;
  auto state = Grid::Create(resource, *extents);
  auto derivative = Grid::Create(resource, *extents);
  auto next = Grid::Create(resource, *extents);
  if (!state.ok() || !derivative.ok() || !next.ok()) {
    return 2;
  }

  auto state_view = state->view();
  auto derivative_view = derivative->view();
  auto next_view = next->view();
  if (!state_view.ok() || !derivative_view.ok() || !next_view.ok()) {
    return 3;
  }

  double value = 1.0;
  for (asc::index_t column = 0; column < 3; ++column) {
    for (asc::index_t row = 0; row < 2; ++row) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto element =
          state_view->At(std::span<const asc::index_t, 2>(coordinate));
      if (!element.ok()) {
        return 4;
      }
      **element = value++;
    }
  }

  const asc::DenseView<const double, 2> const_state(*state_view);
  auto derivative_expression = asc::MakeMultiply(-0.5, const_state);
  if (!derivative_expression.ok()) {
    return 5;
  }
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  if (!asc::Evaluate(context, *derivative_expression, *derivative_view).ok()) {
    return 6;
  }

  const asc::DenseView<const double, 2> const_derivative(*derivative_view);
  auto step = asc::MakeMultiply(0.1, const_derivative);
  if (!step.ok()) {
    return 7;
  }
  auto next_expression = asc::MakeAdd(const_state, *step);
  if (!next_expression.ok()) {
    return 8;
  }
  if (!asc::Evaluate(context, *next_expression, *next_view).ok()) {
    return 9;
  }

  value = 1.0;
  for (asc::index_t column = 0; column < 3; ++column) {
    for (asc::index_t row = 0; row < 2; ++row) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto element =
          next_view->At(std::span<const asc::index_t, 2>(coordinate));
      if (!element.ok() || !NearlyEqual(**element, 0.95 * value++)) {
        return 10;
      }
    }
  }

  auto checksum = asc::ReduceSum(context, *next_view);
  if (!checksum.ok() || !NearlyEqual(*checksum, 19.95)) {
    return 11;
  }

  std::cout << "sizeof(Status)=" << sizeof(asc::Status)
            << " alignof(Status)=" << alignof(asc::Status) << '\n'
            << "sizeof(MutableMemoryView)=" << sizeof(asc::MutableMemoryView)
            << " alignof(MutableMemoryView)=" << alignof(asc::MutableMemoryView)
            << '\n'
            << "sizeof(ExecutionContext)=" << sizeof(asc::ExecutionContext)
            << " alignof(ExecutionContext)=" << alignof(asc::ExecutionContext)
            << '\n'
            << "sizeof(CompletionEvent)=" << sizeof(asc::CompletionEvent)
            << " alignof(CompletionEvent)=" << alignof(asc::CompletionEvent)
            << '\n'
            << "sizeof(Grid)=" << sizeof(Grid)
            << " alignof(Grid)=" << alignof(Grid) << '\n'
            << "checksum=" << *checksum << '\n';
  return 0;
}
