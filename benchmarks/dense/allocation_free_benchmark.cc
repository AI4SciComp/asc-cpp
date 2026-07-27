#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>

#include "allocation_counter.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/evaluate.h"
#include "asc/dense/linalg.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"

namespace {

constexpr asc::extent_t kRows = 32;
constexpr asc::extent_t kColumns = 32;
constexpr int kIterations = 100;

const char* CompilerName() {
#if defined(__clang__)
  return "Clang " __clang_version__;
#elif defined(__GNUC__)
  return "GCC " __VERSION__;
#elif defined(_MSC_VER)
  return "MSVC";
#else
  return "unknown";
#endif
}

const char* ConfigurationName() {
#if defined(NDEBUG)
  return "Release-like";
#else
  return "Debug-like";
#endif
}

}  // namespace

int main() {
  using Extents = asc::Extents<kRows, kColumns>;
  using Array = asc::DenseArray<double, Extents>;

  const auto extents = Extents::Create();
  if (!extents.ok()) {
    return 1;
  }
  asc::HostMemoryResource resource;
  auto left = Array::Create(*extents, resource);
  auto right = Array::Create(*extents, resource, asc::LayoutRight{});
  auto pointwise_output = Array::Create(*extents, resource);
  auto matrix_output = Array::Create(*extents, resource, asc::LayoutRight{});
  if (!left.ok() || !right.ok() || !pointwise_output.ok() ||
      !matrix_output.ok()) {
    return 2;
  }

  auto left_view = left->view();
  auto right_view = right->view();
  auto pointwise_view = pointwise_output->view();
  auto matrix_view = matrix_output->view();
  if (!left_view.ok() || !right_view.ok() || !pointwise_view.ok() ||
      !matrix_view.ok()) {
    return 3;
  }
  for (asc::index_t column = 0; column < kColumns; ++column) {
    for (asc::index_t row = 0; row < kRows; ++row) {
      const std::array<asc::index_t, 2> index = {row, column};
      auto left_element = left_view->At(index);
      auto right_element = right_view->At(index);
      if (!left_element.ok() || !right_element.ok()) {
        return 4;
      }
      **left_element = 0.5;
      **right_element = 0.25;
    }
  }

  auto expression = asc::MakeAdd(*left_view, *right_view);
  if (!expression.ok()) {
    return 5;
  }
  const asc::DenseView<const double, 2> const_left(*left_view);
  const asc::DenseView<const double, 2> const_right(*right_view);
  const asc::ExecutionContext execution = asc::ExecutionContext::Serial();

  asc::Status operation_status = asc::Status::Ok();
  std::size_t allocations = 0;
  const auto start = std::chrono::steady_clock::now();
  {
    asc_dense_test::AllocationCountScope allocation_scope;
    for (int iteration = 0; iteration < kIterations; ++iteration) {
      operation_status = asc::Evaluate(execution, *pointwise_view, *expression);
      if (!operation_status.ok()) {
        break;
      }
      operation_status = asc::Gemm(execution, asc::MatrixOperation::kNone,
                                   asc::MatrixOperation::kNone, 1.0, const_left,
                                   const_right, 0.0, *matrix_view);
      if (!operation_status.ok()) {
        break;
      }
    }
    allocations = allocation_scope.count();
  }
  const auto stop = std::chrono::steady_clock::now();
  if (!operation_status.ok()) {
    std::cerr << operation_status.ToString() << '\n';
    return 6;
  }

  const std::array<asc::index_t, 2> first = {0, 0};
  const auto pointwise_first = pointwise_view->At(first);
  const auto matrix_first = matrix_view->At(first);
  if (!pointwise_first.ok() || !matrix_first.ok()) {
    return 7;
  }
  const double checksum = **pointwise_first + **matrix_first;
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

  std::cout << "compiler=" << CompilerName() << '\n'
            << "configuration=" << ConfigurationName() << '\n'
            << "backend=serial-reference\n"
            << "shape=" << kRows << 'x' << kColumns << '\n'
            << "iterations=" << kIterations << '\n'
            << "operations=evaluate-add,gemm\n"
            << "allocations_in_operations=" << allocations << '\n'
            << "elapsed_us=" << elapsed.count() << '\n'
            << "checksum=" << checksum << '\n';
  return allocations == 0 && checksum == 4.75 ? 0 : 8;
}
