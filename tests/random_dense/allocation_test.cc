#include <array>
#include <span>

#include "../dense/allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "test_support.h"

int main() {
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
  return test.Finish();
}
