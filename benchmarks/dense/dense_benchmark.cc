#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <span>
#include <string_view>

#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense.h"
#include "asc/expression/expression.h"

namespace {

using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using Clock = std::chrono::steady_clock;

const char* CompilerName() {
#if defined(__clang__)
  return "clang";
#elif defined(__GNUC__)
  return "gcc";
#elif defined(_MSC_VER)
  return "msvc";
#else
  return "unknown";
#endif
}

const char* BuildConfiguration() {
#if defined(NDEBUG)
  return "release-like";
#else
  return "debug-like";
#endif
}

template <typename Element>
asc::Status Initialize(asc::DenseView<Element, 2> view, Element scale) {
  for (asc::index_t column = 0; column < view.extents()[1]; ++column) {
    for (asc::index_t row = 0; row < view.extents()[0]; ++row) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto element = view.At(std::span<const asc::index_t, 2>(coordinate));
      if (!element.ok()) {
        return element.status();
      }
      **element = scale * static_cast<Element>(1 + (row + 3 * column) % 17);
    }
  }
  return asc::Status::Ok();
}

double Checksum(asc::DenseView<const double, 2> view) {
  double checksum = 0.0;
  for (asc::index_t column = 0; column < view.extents()[1]; ++column) {
    for (asc::index_t row = 0; row < view.extents()[0]; ++row) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto element = view.At(std::span<const asc::index_t, 2>(coordinate));
      if (!element.ok()) {
        return -1.0;
      }
      checksum += **element;
    }
  }
  return checksum;
}

bool VerifyEvaluation(asc::DenseView<const double, 2> view) {
  for (asc::index_t column = 0; column < view.extents()[1]; ++column) {
    for (asc::index_t row = 0; row < view.extents()[0]; ++row) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto element = view.At(std::span<const asc::index_t, 2>(coordinate));
      const double input =
          0.125 * static_cast<double>(1 + (row + 3 * column) % 17);
      const double expected = 2.0 * input - 1.0;
      if (!element.ok() || **element != expected) {
        return false;
      }
    }
  }
  return true;
}

bool VerifyGemm(asc::DenseView<const double, 2> view) {
  for (asc::index_t column = 0; column < view.extents()[1]; ++column) {
    for (asc::index_t row = 0; row < view.extents()[0]; ++row) {
      long double expected = 0.0L;
      for (asc::index_t inner = 0; inner < view.extents()[0]; ++inner) {
        const long double left =
            0.125L * static_cast<long double>(1 + (row + 3 * inner) % 17);
        const long double right =
            0.0625L * static_cast<long double>(1 + (inner + 3 * column) % 17);
        expected += left * right;
      }
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto element = view.At(std::span<const asc::index_t, 2>(coordinate));
      if (!element.ok() ||
          std::abs(static_cast<long double>(**element) - expected) > 1.0e-12L) {
        return false;
      }
    }
  }
  return true;
}

void PrintResult(std::string_view operation, std::size_t rows,
                 std::size_t columns, std::size_t iterations,
                 Clock::duration elapsed, double checksum,
                 std::size_t allocations) {
  const auto nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
  std::cout << "compiler=" << CompilerName()
            << " configuration=" << BuildConfiguration()
            << " operation=" << operation << " scalar=double shape=" << rows
            << 'x' << columns << " iterations=" << iterations
            << " total_ns=" << nanoseconds << " per_iteration_ns="
            << static_cast<double>(nanoseconds) /
                   static_cast<double>(iterations)
            << " checksum=" << checksum << " allocations=" << allocations
            << " oracle=independent\n";
}

}  // namespace

int main() {
  constexpr asc::extent_t kExtent = 32;
  constexpr std::size_t kEvaluationIterations = 64;
  constexpr std::size_t kGemmIterations = 4;

  asc::HostMemoryResource resource;
  auto extents = MatrixExtents::Create(kExtent, kExtent);
  if (!extents.ok()) {
    return 1;
  }
  auto input =
      asc::DenseArray<double, MatrixExtents>::Create(resource, *extents);
  auto right =
      asc::DenseArray<double, MatrixExtents>::Create(resource, *extents);
  auto output =
      asc::DenseArray<double, MatrixExtents>::Create(resource, *extents);
  if (!input.ok() || !right.ok() || !output.ok()) {
    return 2;
  }
  auto input_view = input->view();
  auto right_view = right->view();
  auto output_view = output->view();
  if (!input_view.ok() || !right_view.ok() || !output_view.ok()) {
    return 3;
  }
  if (!Initialize(*input_view, 0.125).ok() ||
      !Initialize(*right_view, 0.0625).ok()) {
    return 4;
  }

  asc::DenseView<const double, 2> const_input(*input_view);
  asc::DenseView<const double, 2> const_right(*right_view);
  auto multiplied = asc::MakeMultiply(2.0, const_input);
  if (!multiplied.ok()) {
    return 5;
  }
  auto expression = asc::MakeSubtract(*multiplied, 1.0);
  if (!expression.ok()) {
    return 6;
  }

  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  if (!asc::Evaluate(context, *expression, *output_view).ok()) {
    return 7;
  }
  const auto evaluation_start = Clock::now();
  std::size_t evaluation_allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    for (std::size_t iteration = 0; iteration < kEvaluationIterations;
         ++iteration) {
      if (!asc::Evaluate(context, *expression, *output_view).ok()) {
        return 8;
      }
    }
    evaluation_allocations = probe.count();
  }
  const auto evaluation_elapsed = Clock::now() - evaluation_start;
  const asc::DenseView<const double, 2> evaluation_output(*output_view);
  const double evaluation_checksum = Checksum(evaluation_output);
  if (!VerifyEvaluation(evaluation_output)) {
    return 11;
  }
  PrintResult("evaluate", kExtent, kExtent, kEvaluationIterations,
              evaluation_elapsed, evaluation_checksum, evaluation_allocations);

  if (!asc::Gemm(context, asc::DenseTranspose::kNone,
                 asc::DenseTranspose::kNone, 1.0, const_input, const_right, 0.0,
                 *output_view)
           .ok()) {
    return 9;
  }
  const auto gemm_start = Clock::now();
  std::size_t gemm_allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    for (std::size_t iteration = 0; iteration < kGemmIterations; ++iteration) {
      if (!asc::Gemm(context, asc::DenseTranspose::kNone,
                     asc::DenseTranspose::kNone, 1.0, const_input, const_right,
                     0.0, *output_view)
               .ok()) {
        return 10;
      }
    }
    gemm_allocations = probe.count();
  }
  const auto gemm_elapsed = Clock::now() - gemm_start;
  const asc::DenseView<const double, 2> gemm_output(*output_view);
  const double gemm_checksum = Checksum(gemm_output);
  if (!VerifyGemm(gemm_output)) {
    return 12;
  }
  PrintResult("gemm", kExtent, kExtent, kGemmIterations, gemm_elapsed,
              gemm_checksum, gemm_allocations);

  if (evaluation_allocations != 0 || gemm_allocations != 0 ||
      !std::isfinite(evaluation_checksum) || !std::isfinite(gemm_checksum)) {
    return 13;
  }
  return 0;
}
