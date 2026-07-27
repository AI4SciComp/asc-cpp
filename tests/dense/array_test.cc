#include "asc/dense/array.h"

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "test_support.h"

namespace {

using DynamicMatrixExtents =
    asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using DynamicMatrix = asc::DenseArray<double, DynamicMatrixExtents>;
using StaticMatrixExtents = asc::Extents<2, 3>;
using StaticMatrix = asc::DenseArray<double, StaticMatrixExtents>;

static_assert(asc::DenseExtents<DynamicMatrixExtents>);
static_assert(std::is_move_constructible_v<DynamicMatrix>);
static_assert(std::is_move_assignable_v<DynamicMatrix>);
static_assert(!std::is_copy_constructible_v<DynamicMatrix>);
static_assert(!std::is_copy_assignable_v<DynamicMatrix>);
static_assert(std::same_as<decltype(std::declval<DynamicMatrix&>().view()),
                           asc::Result<asc::DenseView<double, 2>>>);
static_assert(
    std::same_as<decltype(std::declval<const DynamicMatrix&>().view()),
                 asc::Result<asc::DenseView<const double, 2>>>);

class DeviceMemoryResource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kDevice;
  }
  asc::Result<void*> Allocate(std::size_t, std::size_t) override {
    ++allocation_calls_;
    return asc::Status(asc::ErrorCode::kUnavailable,
                       "No device allocator in the dense test");
  }
  void Deallocate(void*, std::size_t, std::size_t) noexcept override {}
  [[nodiscard]] int allocation_calls() const noexcept {
    return allocation_calls_;
  }

 private:
  int allocation_calls_ = 0;
};

auto MakeExtents(asc::extent_t rows, asc::extent_t columns) {
  return DynamicMatrixExtents::Create(rows, columns);
}

void CheckCreationAndInitialization(asc_dense_test::TestContext& context) {
  asc_dense_test::CountingMemoryResource resource;
  const auto static_extents = StaticMatrixExtents::Create();
  ASC_DENSE_TEST_CHECK(context, static_extents.ok());
  {
    const auto array = StaticMatrix::Create(*static_extents, resource);
    ASC_DENSE_TEST_CHECK(context, array.ok());
    ASC_DENSE_TEST_EQ(context, array->size(), 6);
    ASC_DENSE_TEST_EQ(context, array->mapping().kind(),
                      asc::DenseLayoutKind::kLeft);
    ASC_DENSE_TEST_EQ(context, array->mapping().strides(),
                      (std::array<asc::stride_t, 2>{1, 2}));
    ASC_DENSE_TEST_EQ(context, array->resource(), &resource);

    const auto view = array->view();
    ASC_DENSE_TEST_CHECK(context, view.ok());
    for (asc::index_t column = 0; column < 3; ++column) {
      for (asc::index_t row = 0; row < 2; ++row) {
        const std::array<asc::index_t, 2> index = {row, column};
        const auto element = view->At(index);
        ASC_DENSE_TEST_CHECK(context, element.ok());
        ASC_DENSE_TEST_EQ(context, **element, 0.0);
      }
    }
  }
  ASC_DENSE_TEST_EQ(context, resource.successful_allocations(), 1U);
  ASC_DENSE_TEST_EQ(context, resource.deallocations(), 1U);
  ASC_DENSE_TEST_EQ(context, resource.allocated_bytes(), 6U * sizeof(double));
  ASC_DENSE_TEST_EQ(context, resource.deallocated_bytes(),
                    resource.allocated_bytes());

  DeviceMemoryResource device_resource;
  const auto rejected = StaticMatrix::Create(*static_extents, device_resource);
  ASC_DENSE_TEST_CHECK(context, !rejected.ok());
  ASC_DENSE_TEST_EQ(context, rejected.status().code(),
                    asc::ErrorCode::kUnsupported);
  ASC_DENSE_TEST_EQ(context, device_resource.allocation_calls(), 0);

  const auto empty_extents = MakeExtents(3, 0);
  ASC_DENSE_TEST_CHECK(context, empty_extents.ok());
  const auto empty = DynamicMatrix::Create(*empty_extents, resource);
  ASC_DENSE_TEST_CHECK(context, empty.ok());
  ASC_DENSE_TEST_EQ(context, empty->size(), 0);
  const auto empty_view = empty->view();
  ASC_DENSE_TEST_CHECK(context, empty_view.ok());
  ASC_DENSE_TEST_EQ(context, empty_view->data(), nullptr);
}

void CheckMoveAndClone(asc_dense_test::TestContext& context) {
  asc_dense_test::CountingMemoryResource source_resource;
  asc_dense_test::CountingMemoryResource clone_resource;
  const auto extents = MakeExtents(2, 3);
  ASC_DENSE_TEST_CHECK(context, extents.ok());

  auto created =
      DynamicMatrix::Create(*extents, source_resource, asc::LayoutRight{});
  ASC_DENSE_TEST_CHECK(context, created.ok());
  auto source = std::move(*created);
  const auto source_view = source.view();
  ASC_DENSE_TEST_CHECK(context, source_view.ok());
  for (asc::index_t row = 0; row < 2; ++row) {
    for (asc::index_t column = 0; column < 3; ++column) {
      const std::array<asc::index_t, 2> index = {row, column};
      const auto element = source_view->At(index);
      ASC_DENSE_TEST_CHECK(context, element.ok());
      **element = static_cast<double>(10 * row + column + 1);
    }
  }

  DynamicMatrix moved = std::move(source);
  ASC_DENSE_TEST_CHECK(context, !source.view().ok());
  ASC_DENSE_TEST_EQ(context, moved.mapping().kind(),
                    asc::DenseLayoutKind::kRight);
  const auto moved_view = moved.view();
  ASC_DENSE_TEST_CHECK(context, moved_view.ok());
  ASC_DENSE_TEST_EQ(context, moved_view->data(), source_view->data());

  auto clone = moved.Clone(clone_resource, asc::ExecutionContext::Serial());
  ASC_DENSE_TEST_CHECK(context, clone.ok());
  ASC_DENSE_TEST_EQ(context, clone->mapping().kind(),
                    asc::DenseLayoutKind::kRight);
  const auto clone_view = clone->view();
  ASC_DENSE_TEST_CHECK(context, clone_view.ok());
  ASC_DENSE_TEST_CHECK(context, clone_view->data() != moved_view->data());
  for (asc::index_t row = 0; row < 2; ++row) {
    for (asc::index_t column = 0; column < 3; ++column) {
      const std::array<asc::index_t, 2> index = {row, column};
      const auto source_element = moved_view->At(index);
      const auto clone_element = clone_view->At(index);
      ASC_DENSE_TEST_CHECK(context, source_element.ok());
      ASC_DENSE_TEST_CHECK(context, clone_element.ok());
      ASC_DENSE_TEST_EQ(context, **source_element, **clone_element);
    }
  }
  const std::array<asc::index_t, 2> first = {0, 0};
  const auto clone_first = clone_view->At(first);
  const auto moved_first = moved_view->At(first);
  ASC_DENSE_TEST_CHECK(context, clone_first.ok());
  ASC_DENSE_TEST_CHECK(context, moved_first.ok());
  **clone_first = -77.0;
  ASC_DENSE_TEST_CHECK(context, **moved_first != **clone_first);
}

void CheckResizeTransaction(asc_dense_test::TestContext& context) {
  asc_dense_test::CountingMemoryResource resource;
  const auto initial_extents = MakeExtents(2, 2);
  const auto replacement_extents = MakeExtents(3, 2);
  ASC_DENSE_TEST_CHECK(context, initial_extents.ok());
  ASC_DENSE_TEST_CHECK(context, replacement_extents.ok());
  auto created = DynamicMatrix::Create(*initial_extents, resource);
  ASC_DENSE_TEST_CHECK(context, created.ok());
  auto array = std::move(*created);

  const auto initial_view = array.view();
  ASC_DENSE_TEST_CHECK(context, initial_view.ok());
  const std::array<asc::index_t, 2> index = {1, 1};
  const auto initial_element = initial_view->At(index);
  ASC_DENSE_TEST_CHECK(context, initial_element.ok());
  **initial_element = 42.0;
  double* const initial_pointer = initial_view->data();
  const auto initial_mapping = array.mapping();
  const std::array<asc::extent_t, 2> initial_shape = {2, 2};

  resource.FailNextAllocation();
  const asc::Status failed = array.ResizeDiscard(*replacement_extents);
  ASC_DENSE_TEST_CHECK(context, !failed.ok());
  ASC_DENSE_TEST_EQ(context, failed.code(), asc::ErrorCode::kAllocation);
  ASC_DENSE_TEST_EQ(context, array.mapping(), initial_mapping);
  ASC_DENSE_TEST_EQ(context, array.extents().values()[0], initial_shape[0]);
  ASC_DENSE_TEST_EQ(context, array.extents().values()[1], initial_shape[1]);
  const auto unchanged_view = array.view();
  ASC_DENSE_TEST_CHECK(context, unchanged_view.ok());
  ASC_DENSE_TEST_EQ(context, unchanged_view->data(), initial_pointer);
  const auto unchanged_element = unchanged_view->At(index);
  ASC_DENSE_TEST_CHECK(context, unchanged_element.ok());
  ASC_DENSE_TEST_EQ(context, **unchanged_element, 42.0);

  const std::size_t deallocations_before = resource.deallocations();
  const asc::Status resized = array.ResizeDiscard(*replacement_extents);
  ASC_DENSE_TEST_CHECK(context, resized.ok());
  ASC_DENSE_TEST_EQ(context, array.extents().values()[0],
                    replacement_extents->values()[0]);
  ASC_DENSE_TEST_EQ(context, array.extents().values()[1],
                    replacement_extents->values()[1]);
  ASC_DENSE_TEST_EQ(context, array.size(), 6);
  ASC_DENSE_TEST_EQ(context, resource.deallocations(),
                    deallocations_before + 1);
  const auto resized_view = array.view();
  ASC_DENSE_TEST_CHECK(context, resized_view.ok());
  for (asc::index_t column = 0; column < 2; ++column) {
    for (asc::index_t row = 0; row < 3; ++row) {
      const std::array<asc::index_t, 2> resized_index = {row, column};
      const auto element = resized_view->At(resized_index);
      ASC_DENSE_TEST_CHECK(context, element.ok());
      ASC_DENSE_TEST_EQ(context, **element, 0.0);
    }
  }
}

}  // namespace

int main() {
  asc_dense_test::TestContext context;
  CheckCreationAndInitialization(context);
  CheckMoveAndClone(context);
  CheckResizeTransaction(context);
  return context.Finish();
}
