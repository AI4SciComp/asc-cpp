#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "asc/random/generator.h"
#include "test_support.h"

int main() {  // NOLINT(readability-function-size)
  asc_random_dense_test::TestContext test;
  constexpr std::array<asc::extent_t, 2> kExtents{16, 16};
  auto mapping = asc::DenseLayout<2>::Create(
      std::span<const asc::extent_t, 2>(kExtents), asc::LayoutLeft{});
  ASC_RANDOM_DENSE_TEST_CHECK(test, mapping.ok());
  if (!mapping.ok()) {
    return test.Finish();
  }
  std::array<double, 256> storage{};
  auto view = asc::DenseView<double, 2>::Create(storage.data(), *mapping,
                                                asc::MemorySpace::kHost);
  ASC_RANDOM_DENSE_TEST_CHECK(test, view.ok());
  if (!view.ok()) {
    return test.Finish();
  }
  asc::Result<asc::RandomOffset> generated =
      asc::Status(asc::ErrorCode::kInternal, "not generated");
  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    generated = asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *view,
                                        1, 2, 3);
    allocations = probe.count();
  }
  ASC_RANDOM_DENSE_TEST_CHECK(test, generated.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(allocations, 0));

  constexpr std::array<asc::extent_t, 1> kDimensionExtents{3};
  constexpr std::array<asc::extent_t, 2> kMatrixExtents{3, 3};
  constexpr std::array<asc::extent_t, 2> kSampleExtents{16, 3};
  std::array<double, 3> mean_data{1.0, -2.0, 0.5};
  std::array<double, 9> covariance_data{1.0,  0.3,  -0.2, 0.3, 1.25,
                                        0.25, -0.2, 0.25, 0.75};
  std::array<double, 9> factor_data{};
  std::array<double, 48> sample_data{};
  std::array<double, 3> sample_workspace_data{};
  std::array<std::uint32_t, 48> permutation_workspace_data{};
  auto mean_mapping = asc::DenseLayout<1>::Create(kDimensionExtents);
  auto matrix_mapping = asc::DenseLayout<2>::Create(kMatrixExtents);
  auto sample_mapping = asc::DenseLayout<2>::Create(kSampleExtents);
  auto mean_mutable = asc::DenseView<double, 1>::Create(
      mean_data.data(), *mean_mapping, asc::MemorySpace::kHost);
  auto covariance_mutable = asc::DenseView<double, 2>::Create(
      covariance_data.data(), *matrix_mapping, asc::MemorySpace::kHost);
  auto factor = asc::DenseView<double, 2>::Create(
      factor_data.data(), *matrix_mapping, asc::MemorySpace::kHost);
  auto samples = asc::DenseView<double, 2>::Create(
      sample_data.data(), *sample_mapping, asc::MemorySpace::kHost);
  auto sample_workspace = asc::DenseView<double, 1>::Create(
      sample_workspace_data.data(), *mean_mapping, asc::MemorySpace::kHost);
  auto permutation_workspace = asc::DenseView<std::uint32_t, 2>::Create(
      permutation_workspace_data.data(), *sample_mapping,
      asc::MemorySpace::kHost);
  asc::DenseView<const double, 1> mean = *mean_mutable;
  asc::DenseView<const double, 2> covariance = *covariance_mutable;
  auto normal_distribution = asc::NormalDistribution<double>::Create(0.0, 1.0);
  asc::NormalGenerator<asc::Pcg32, double> normal(asc::Pcg32(5, 7),
                                                  *normal_distribution);
  std::size_t advanced_allocations = 0;
  asc::Status prepare_status;
  asc::Status normal_status;
  asc::Status sphere_status;
  asc::Status pseudo_status;
  asc::Status latin_status;
  asc::Status sobol_status;
  {
    asc_dense_test::AllocationProbe probe;
    prepare_status = asc::PrepareDenseMultivariateNormal(
        asc::ExecutionContext::Serial(), mean, covariance, *factor);
    asc::DenseView<const double, 2> const_factor = *factor;
    normal_status = asc::FillDenseMultivariateNormal(
        asc::ExecutionContext::Serial(), *samples, mean, const_factor, normal,
        *sample_workspace);
    asc::Pcg32 sphere_engine(11, 13);
    sphere_status =
        asc::FillDenseUnitSphere(asc::ExecutionContext::Serial(), *samples,
                                 sphere_engine, *sample_workspace);
    pseudo_status =
        asc::FillDensePseudo(asc::ExecutionContext::Serial(), *samples, normal);
    asc::Pcg32 latin_engine(17, 19);
    latin_status = asc::FillDenseLatinHypercubeJittered(
        asc::ExecutionContext::Serial(), *samples, latin_engine,
        *permutation_workspace);
    sobol_status = asc::FillDenseSobol(asc::ExecutionContext::Serial(),
                                       *samples, 0, *sample_workspace);
    advanced_allocations = probe.count();
  }
  ASC_RANDOM_DENSE_TEST_CHECK(test, prepare_status.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, normal_status.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, sphere_status.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, pseudo_status.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, latin_status.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(test, sobol_status.ok());
  ASC_RANDOM_DENSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(advanced_allocations, 0));
  return test.Finish();
}
