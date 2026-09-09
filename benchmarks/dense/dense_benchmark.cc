#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "../../tests/allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/blas.h"
#include "asc/dense/evaluate.h"
#include "asc/dense/view.h"
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
                 std::size_t allocations,
                 std::size_t estimated_bytes_per_iteration = 0,
                 std::size_t samples = 1,
                 long double sample_variance_ns2 = 0.0L) {
  const auto nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
  std::cout << "compiler=" << CompilerName()
            << " configuration=" << BuildConfiguration()
            << " operation=" << operation << " scalar=double shape=" << rows
            << 'x' << columns << " iterations=" << iterations
            << " total_ns=" << nanoseconds << " per_iteration_ns="
            << static_cast<double>(nanoseconds) /
                   static_cast<double>(iterations)
            << " checksum=" << checksum << " allocations=" << allocations;
  std::cout << " samples=" << samples << " sample_variance_ns2="
            << static_cast<double>(sample_variance_ns2);
  if (estimated_bytes_per_iteration != 0 && nanoseconds > 0) {
    const long double seconds = static_cast<long double>(nanoseconds) / 1.0e9L;
    const long double bandwidth =
        static_cast<long double>(estimated_bytes_per_iteration) * iterations /
        seconds;
    std::cout << " estimated_bytes_per_iteration="
              << estimated_bytes_per_iteration
              << " estimated_bandwidth_bytes_per_second="
              << static_cast<double>(bandwidth);
  }
  std::cout << " oracle=independent\n";
}

constexpr asc::extent_t kExtent = 32;
constexpr asc::extent_t kLevel2Extent = 128;
constexpr asc::extent_t kVectorExtent = 1U << 16;
constexpr std::size_t kEvaluationIterations = 64;
constexpr std::size_t kGemmIterations = 4;
constexpr std::size_t kGemmSamples = 5;
constexpr std::size_t kLevel1Iterations = 64;
constexpr std::size_t kLevel2Iterations = 16;

struct Measurement {
  std::size_t allocations = 0;
  double checksum = 0.0;
};

struct Measurements {
  Measurement evaluation;
  Measurement gemm;
  Measurement level2;
  Measurement axpy;
  Measurement dot;
};

int RunEvaluation(asc::DenseView<const double, 2> const_input,
                  asc::DenseView<double, 2> output_view,
                  Measurement& measurement) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  auto multiplied = asc::MakeMultiply(2.0, const_input);
  if (!multiplied.ok()) {
    return 5;
  }
  auto expression = asc::MakeSubtract(*multiplied, 1.0);
  if (!expression.ok()) {
    return 6;
  }

  if (!asc::Evaluate(context, *expression, output_view).ok()) {
    return 7;
  }
  const auto evaluation_start = Clock::now();
  {
    asc_dense_test::AllocationProbe probe;
    for (std::size_t iteration = 0; iteration < kEvaluationIterations;
         ++iteration) {
      if (!asc::Evaluate(context, *expression, output_view).ok()) {
        return 8;
      }
    }
    measurement.allocations = probe.count();
  }
  const auto evaluation_elapsed = Clock::now() - evaluation_start;
  const asc::DenseView<const double, 2> evaluation_output(output_view);
  measurement.checksum = Checksum(evaluation_output);
  if (!VerifyEvaluation(evaluation_output)) {
    return 11;
  }
  PrintResult("evaluate", kExtent, kExtent, kEvaluationIterations,
              evaluation_elapsed, measurement.checksum,
              measurement.allocations);

  return 0;
}

int RunGemm(asc::DenseBlasMatrixView<const double> level3_left,
            asc::DenseBlasMatrixView<const double> level3_right,
            asc::DenseBlasMatrixView<double> level3_output,
            asc::DenseView<double, 2> output_view, Measurement& measurement) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  if (!asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                 asc::DenseBlasTranspose::kNone, 1.0, level3_left, level3_right,
                 0.0, level3_output)
           .ok()) {
    return 9;
  }
  std::array<Clock::duration, kGemmSamples> gemm_samples{};
  {
    asc_dense_test::AllocationProbe probe;
    for (std::size_t sample = 0; sample < kGemmSamples; ++sample) {
      const auto sample_start = Clock::now();
      for (std::size_t iteration = 0; iteration < kGemmIterations;
           ++iteration) {
        if (!asc::Gemm(context, asc::DenseBlasTranspose::kNone,
                       asc::DenseBlasTranspose::kNone, 1.0, level3_left,
                       level3_right, 0.0, level3_output)
                 .ok()) {
          return 10;
        }
      }
      gemm_samples[sample] = Clock::now() - sample_start;
    }
    measurement.allocations = probe.count();
  }
  Clock::duration gemm_elapsed{};
  long double gemm_mean_ns = 0.0L;
  for (Clock::duration sample : gemm_samples) {
    gemm_elapsed += sample;
    gemm_mean_ns += static_cast<long double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(sample).count());
  }
  gemm_mean_ns /= static_cast<long double>(kGemmSamples);
  long double gemm_variance_ns2 = 0.0L;
  for (Clock::duration sample : gemm_samples) {
    const long double sample_ns = static_cast<long double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(sample).count());
    const long double difference = sample_ns - gemm_mean_ns;
    gemm_variance_ns2 += difference * difference;
  }
  gemm_variance_ns2 /= static_cast<long double>(kGemmSamples - 1);
  const asc::DenseView<const double, 2> gemm_output(output_view);
  measurement.checksum = Checksum(gemm_output);
  if (!VerifyGemm(gemm_output)) {
    return 12;
  }
  PrintResult("blas_level3_gemm", kExtent, kExtent,
              kGemmIterations * kGemmSamples, gemm_elapsed,
              measurement.checksum, measurement.allocations, 0, kGemmSamples,
              gemm_variance_ns2);

  return 0;
}

int RunMatrixViews(asc::DenseView<double, 2> input_view,
                   asc::DenseView<double, 2> right_view,
                   asc::DenseView<double, 2> output_view,
                   Measurements& measurements) {
  asc::DenseView<const double, 2> const_input(input_view);
  asc::DenseView<const double, 2> const_right(right_view);
  const std::size_t level3_bytes =
      static_cast<std::size_t>(kExtent * kExtent) * sizeof(double);
  auto level3_left = asc::DenseBlasMatrixView<const double>::Create(
      const_input.data(), kExtent, kExtent, asc::DenseBlasLayout::kColumnMajor,
      kExtent,
      asc::ConstMemoryView(const_input.data(), level3_bytes,
                           asc::MemorySpace::kHost));
  auto level3_right = asc::DenseBlasMatrixView<const double>::Create(
      const_right.data(), kExtent, kExtent, asc::DenseBlasLayout::kColumnMajor,
      kExtent,
      asc::ConstMemoryView(const_right.data(), level3_bytes,
                           asc::MemorySpace::kHost));
  auto level3_output = asc::DenseBlasMatrixView<double>::Create(
      output_view.data(), kExtent, kExtent, asc::DenseBlasLayout::kColumnMajor,
      kExtent,
      asc::ConstMemoryView(output_view.data(), level3_bytes,
                           asc::MemorySpace::kHost));
  if (!level3_left.ok() || !level3_right.ok() || !level3_output.ok()) {
    return 22;
  }
  const int evaluation_status =
      RunEvaluation(const_input, output_view, measurements.evaluation);
  if (evaluation_status != 0) {
    return evaluation_status;
  }
  return RunGemm(*level3_left, *level3_right, *level3_output, output_view,
                 measurements.gemm);
}

int RunMatrixBenchmarks(asc::HostMemoryResource& resource,
                        Measurements& measurements) {
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

  return RunMatrixViews(*input_view, *right_view, *output_view, measurements);
}

bool VerifyGemv(std::span<const double> level2_matrix,
                std::span<const double> level2_input,
                std::span<const double> level2_output,
                double& level2_checksum) {
  const std::size_t level2_extent = level2_input.size();
  level2_checksum = 0.0;
  for (std::size_t row = 0; row < level2_extent; ++row) {
    long double expected = 0.0L;
    for (std::size_t column = 0; column < level2_extent; ++column) {
      expected +=
          level2_matrix[row + column * level2_extent] * level2_input[column];
    }
    if (std::abs(static_cast<long double>(level2_output[row]) - expected) >
        1.0e-12L * std::max(1.0L, std::abs(expected))) {
      return false;
    }
    level2_checksum += level2_output[row];
  }
  return true;
}

int RunLevel2(Measurement& measurement) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const std::size_t level2_extent = static_cast<std::size_t>(kLevel2Extent);
  std::vector<double> level2_matrix(level2_extent * level2_extent);
  std::vector<double> level2_input(level2_extent);
  std::vector<double> level2_output(level2_extent);
  for (std::size_t column = 0; column < level2_extent; ++column) {
    level2_input[column] = 0.125 * static_cast<double>(1 + column % 11);
    for (std::size_t row = 0; row < level2_extent; ++row) {
      level2_matrix[row + column * level2_extent] =
          0.0625 * static_cast<double>(
                       1 + static_cast<std::int32_t>((row + column) % 7));
    }
  }
  auto level2_matrix_view = asc::DenseBlasMatrixView<const double>::Create(
      level2_matrix.data(), kLevel2Extent, kLevel2Extent,
      asc::DenseBlasLayout::kColumnMajor, kLevel2Extent,
      asc::ConstMemoryView(level2_matrix.data(),
                           level2_matrix.size() * sizeof(double),
                           asc::MemorySpace::kHost));
  auto level2_input_view = asc::DenseBlasVectorView<const double>::Create(
      level2_input.data(), kLevel2Extent, 1,
      asc::ConstMemoryView(level2_input.data(),
                           level2_input.size() * sizeof(double),
                           asc::MemorySpace::kHost));
  auto level2_output_view = asc::DenseBlasVectorView<double>::Create(
      level2_output.data(), kLevel2Extent, 1,
      asc::ConstMemoryView(level2_output.data(),
                           level2_output.size() * sizeof(double),
                           asc::MemorySpace::kHost));
  if (!level2_matrix_view.ok() || !level2_input_view.ok() ||
      !level2_output_view.ok()) {
    return 19;
  }
  const auto level2_start = Clock::now();
  {
    asc_dense_test::AllocationProbe probe;
    for (std::size_t iteration = 0; iteration < kLevel2Iterations;
         ++iteration) {
      if (!asc::Gemv(context, asc::DenseBlasTranspose::kNone, 1.0,
                     *level2_matrix_view, *level2_input_view, 0.0,
                     *level2_output_view)
               .ok()) {
        return 20;
      }
    }
    measurement.allocations = probe.count();
  }
  const auto level2_elapsed = Clock::now() - level2_start;
  if (!VerifyGemv(level2_matrix, level2_input, level2_output,
                  measurement.checksum)) {
    return 21;
  }
  PrintResult(
      "blas_level2_gemv", level2_extent, level2_extent, kLevel2Iterations,
      level2_elapsed, measurement.checksum, measurement.allocations,
      (level2_extent * level2_extent + 2 * level2_extent) * sizeof(double));

  return 0;
}

int RunAxpy(asc::DenseBlasVectorView<const double> left_vector,
            asc::DenseBlasVectorView<double> output_vector,
            std::span<const double> level1_output, Measurement& measurement) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const auto axpy_start = Clock::now();
  {
    asc_dense_test::AllocationProbe probe;
    for (std::size_t iteration = 0; iteration < kLevel1Iterations;
         ++iteration) {
      if (!asc::Axpy(context, 0.5, left_vector, output_vector).ok()) {
        return 15;
      }
    }
    measurement.allocations = probe.count();
  }
  const auto axpy_elapsed = Clock::now() - axpy_start;
  measurement.checksum = 0.0;
  const double axpy_scale =
      0.25 + 0.5 * 0.125 * static_cast<double>(kLevel1Iterations);
  for (asc::index_t index = 0; index < kVectorExtent; ++index) {
    const double expected = axpy_scale * static_cast<double>(1 + index % 17);
    const double actual = level1_output[static_cast<std::size_t>(index)];
    if (std::abs(actual - expected) >
        1.0e-12 * std::max(1.0, std::abs(expected))) {
      return 16;
    }
    measurement.checksum += actual;
  }
  PrintResult("blas_level1_axpy", static_cast<std::size_t>(kVectorExtent), 1,
              kLevel1Iterations, axpy_elapsed, measurement.checksum,
              measurement.allocations);

  return 0;
}

int RunDot(asc::DenseBlasVectorView<const double> left_vector,
           asc::DenseBlasVectorView<double> output_vector,
           asc::DenseBlasVectorView<double> result_vector,
           std::span<const double> level1_left,
           std::span<const double> level1_output,
           const std::array<double, 1>& level1_result,
           Measurement& measurement) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const auto dot_start = Clock::now();
  {
    asc_dense_test::AllocationProbe probe;
    for (std::size_t iteration = 0; iteration < kLevel1Iterations;
         ++iteration) {
      if (!asc::Dot(context, left_vector,
                    asc::DenseBlasVectorView<const double>(output_vector),
                    result_vector)
               .ok()) {
        return 17;
      }
    }
    measurement.allocations = probe.count();
  }
  const auto dot_elapsed = Clock::now() - dot_start;
  long double expected_dot = 0.0L;
  for (asc::index_t index = 0; index < kVectorExtent; ++index) {
    expected_dot +=
        static_cast<long double>(level1_left[static_cast<std::size_t>(index)]) *
        static_cast<long double>(
            level1_output[static_cast<std::size_t>(index)]);
  }
  if (std::abs(static_cast<long double>(level1_result[0]) - expected_dot) >
      1.0e-12L * std::max(1.0L, std::abs(expected_dot))) {
    return 18;
  }
  PrintResult("blas_level1_dot", static_cast<std::size_t>(kVectorExtent), 1,
              kLevel1Iterations, dot_elapsed, level1_result[0],
              measurement.allocations);

  measurement.checksum = level1_result[0];
  return 0;
}

int RunLevel1(Measurement& axpy, Measurement& dot) {
  std::vector<double> level1_left(static_cast<std::size_t>(kVectorExtent));
  std::vector<double> level1_output(static_cast<std::size_t>(kVectorExtent));
  for (asc::index_t index = 0; index < kVectorExtent; ++index) {
    const double value = static_cast<double>(1 + index % 17);
    level1_left[static_cast<std::size_t>(index)] = 0.125 * value;
    level1_output[static_cast<std::size_t>(index)] = 0.25 * value;
  }
  std::array<double, 1> level1_result{};
  auto left_vector = asc::DenseBlasVectorView<const double>::Create(
      level1_left.data(), kVectorExtent, 1,
      asc::ConstMemoryView(level1_left.data(),
                           level1_left.size() * sizeof(double),
                           asc::MemorySpace::kHost));
  auto output_vector = asc::DenseBlasVectorView<double>::Create(
      level1_output.data(), kVectorExtent, 1,
      asc::ConstMemoryView(level1_output.data(),
                           level1_output.size() * sizeof(double),
                           asc::MemorySpace::kHost));
  auto result_vector = asc::DenseBlasVectorView<double>::Create(
      level1_result.data(), 1, 1,
      asc::ConstMemoryView(level1_result.data(), sizeof(level1_result),
                           asc::MemorySpace::kHost));
  if (!left_vector.ok() || !output_vector.ok() || !result_vector.ok()) {
    return 14;
  }

  const int axpy_status =
      RunAxpy(*left_vector, *output_vector, level1_output, axpy);
  if (axpy_status != 0) {
    return axpy_status;
  }
  return RunDot(*left_vector, *output_vector, *result_vector, level1_left,
                level1_output, level1_result, dot);
}

}  // namespace

int main() {
  asc::HostMemoryResource resource;
  Measurements measurements;
  const int matrix_status = RunMatrixBenchmarks(resource, measurements);
  if (matrix_status != 0) {
    return matrix_status;
  }
  const int level2_status = RunLevel2(measurements.level2);
  if (level2_status != 0) {
    return level2_status;
  }
  const int level1_status = RunLevel1(measurements.axpy, measurements.dot);
  if (level1_status != 0) {
    return level1_status;
  }
  if (!asc_test::ProcessAllocationCountMatches(
          measurements.evaluation.allocations, 0) ||
      !asc_test::ProcessAllocationCountMatches(measurements.gemm.allocations,
                                               0) ||
      !asc_test::ProcessAllocationCountMatches(measurements.level2.allocations,
                                               0) ||
      !asc_test::ProcessAllocationCountMatches(measurements.axpy.allocations,
                                               0) ||
      !asc_test::ProcessAllocationCountMatches(measurements.dot.allocations,
                                               0) ||
      !std::isfinite(measurements.evaluation.checksum) ||
      !std::isfinite(measurements.gemm.checksum) ||
      !std::isfinite(measurements.level2.checksum) ||
      !std::isfinite(measurements.axpy.checksum) ||
      !std::isfinite(measurements.dot.checksum)) {
    return 13;
  }
  return 0;
}
