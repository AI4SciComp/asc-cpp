#include <array>
#include <thread>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/result.h"
#include "asc/random/sparse.h"
#include "test_support.h"

namespace {

using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using Generation = asc::SparseUniform01Generation<float, Shape>;

}  // namespace

int main() {
  asc_random_sparse_test::TestContext test;
  auto extents = Shape::Create(16, 16);
  ASC_RANDOM_SPARSE_TEST_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return test.Finish();
  }
  asc_random_sparse_test::TrackingMemoryResource left_resource;
  asc_random_sparse_test::TrackingMemoryResource right_resource;
  asc::Result<Generation> left =
      asc::Status(asc::ErrorCode::kInternal, "not generated");
  asc::Result<Generation> right =
      asc::Status(asc::ErrorCode::kInternal, "not generated");
  std::thread first([&] {
    left = asc::GenerateSparseUniform01<float>(asc::ExecutionContext::Serial(),
                                               *extents, 32, left_resource, 1,
                                               2, 3, 4, 5, 6);
  });
  std::thread second([&] {
    right = asc::GenerateSparseUniform01<float>(asc::ExecutionContext::Serial(),
                                                *extents, 32, right_resource, 1,
                                                2, 3, 4, 5, 6);
  });
  first.join();
  second.join();
  ASC_RANDOM_SPARSE_TEST_CHECK(test, left.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, right.ok());
  if (!left.ok() || !right.ok()) {
    return test.Finish();
  }
  auto left_view = left->array.view();
  auto right_view = right->array.view();
  ASC_RANDOM_SPARSE_TEST_CHECK(test, left_view.ok());
  ASC_RANDOM_SPARSE_TEST_CHECK(test, right_view.ok());
  if (!left_view.ok() || !right_view.ok()) {
    return test.Finish();
  }
  for (asc::nnz_t position = 0; position < 32; ++position) {
    auto left_coordinate = left_view->Coordinate(position);
    auto right_coordinate = right_view->Coordinate(position);
    auto left_value = left_view->AtStored(position);
    auto right_value = right_view->AtStored(position);
    ASC_RANDOM_SPARSE_TEST_CHECK(test, left_coordinate.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(test, right_coordinate.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(test, left_value.ok());
    ASC_RANDOM_SPARSE_TEST_CHECK(test, right_value.ok());
    if (left_coordinate.ok() && right_coordinate.ok()) {
      const std::array<asc::index_t, 2> left_pair{(*left_coordinate)[0],
                                                  (*left_coordinate)[1]};
      const std::array<asc::index_t, 2> right_pair{(*right_coordinate)[0],
                                                   (*right_coordinate)[1]};
      ASC_RANDOM_SPARSE_TEST_EQ(test, left_pair, right_pair);
    }
    if (left_value.ok() && right_value.ok()) {
      ASC_RANDOM_SPARSE_TEST_EQ(test, **left_value, **right_value);
    }
  }
  return test.Finish();
}
