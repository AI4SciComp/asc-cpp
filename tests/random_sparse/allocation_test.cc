#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../dense/allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "asc/random/generator.h"
#include "asc/random/sparse.h"
#include "test_support.h"

int main() {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  using Generation = asc::SparseUniform01Generation<float, Shape>;

  asc_random_sparse_test::TestContext test;
  auto shape = Shape::Create(8, 8);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, shape.ok());
  if (!shape.ok()) {
    return test.Finish();
  }
  asc_random_sparse_test::TrackingMemoryResource resource;
  asc::Result<Generation> generated =
      asc::Status(asc::ErrorCode::kInternal, "not generated");
  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    generated = asc::GenerateSparseUniform01<float>(
        asc::ExecutionContext::Serial(), *shape, 16, resource, 1, 2, 3, 4, 5,
        6);
    allocations = probe.count();
  }
  ASC_RANDOM_SPARSE_TEST_CHECK(test, generated.ok());
  ASC_RANDOM_SPARSE_TEST_EQ(test, resource.allocation_attempts(),
                            std::size_t{2});
  ASC_RANDOM_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(
                allocations, asc_test::ProcessVisibleResourceAllocationCount(
                                 resource.allocation_attempts())));

  auto generated_view = generated->array.view();
  ASC_RANDOM_SPARSE_TEST_CHECK(test, generated_view.ok());
  auto uniform = asc::UniformRealDistribution<float>::Create(0.0F, 1.0F);
  asc::UniformGenerator<asc::Pcg32, float> pseudo(asc::Pcg32(11, 13), *uniform);
  asc::Result<asc::RandomOffset> filled =
      asc::Status(asc::ErrorCode::kInternal, "not filled");
  asc::Status pseudo_status;
  std::size_t value_fill_allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    filled = asc::FillSparseUniform01(asc::ExecutionContext::Serial(),
                                      *generated_view, 17, 19, 23);
    pseudo_status = asc::FillSparsePseudo(asc::ExecutionContext::Serial(),
                                          *generated_view, pseudo);
    value_fill_allocations = probe.count();
  }
  ASC_RANDOM_SPARSE_TEST_CHECK(test, filled.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, pseudo_status.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(value_fill_allocations, 0));

  std::array<asc::SparseRandomStructureCandidate, 64> workspace{};
  std::array<std::uint64_t, 16> ordinals{};
  asc::Result<asc::RandomOffset> structure =
      asc::Status(asc::ErrorCode::kInternal, "not generated");
  std::size_t structure_allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    structure =
        asc::GenerateSparseStructure(asc::ExecutionContext::Serial(), *shape,
                                     16, 7, 8, 9, workspace, ordinals);
    structure_allocations = probe.count();
  }
  ASC_RANDOM_SPARSE_TEST_CHECK(test, structure.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(structure_allocations, 0));
  return test.Finish();
}
