#include <cstddef>

#include "../dense/allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/random/engine.h"
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
      test, asc_test::ProcessAllocationCountMatches(allocations, 2));
  return test.Finish();
}
