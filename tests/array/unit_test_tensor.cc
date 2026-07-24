#include <asc/array/tensor.h>

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "test_resources.h"

namespace asc {
namespace {

using MatrixExtents = Extents<2, 3>;
using MatrixTensor = Tensor<int, MatrixExtents>;
using RightMatrixTensor =
    Tensor<int, MatrixExtents, LayoutRightMapping<MatrixExtents>>;

struct ThrowOnCopyAssignment {
  ThrowOnCopyAssignment() = default;

  ThrowOnCopyAssignment& operator=(const ThrowOnCopyAssignment& other) {
    if (throw_during_copy) {
      throw std::runtime_error("deterministic element copy failure");
    }
    value = other.value;
    return *this;
  }

  int value = 0;
  static inline bool throw_during_copy = false;
};

struct NonCopyAssignable {
  NonCopyAssignable() = default;
  NonCopyAssignable& operator=(const NonCopyAssignable&) = delete;
};

static_assert(!std::is_copy_constructible_v<MatrixTensor>);
static_assert(!std::is_copy_assignable_v<MatrixTensor>);
static_assert(std::is_nothrow_move_constructible_v<MatrixTensor>);
static_assert(std::is_nothrow_move_assignable_v<MatrixTensor>);
static_assert(!std::is_convertible_v<MatrixTensor&, int*>);
static_assert(!std::is_convertible_v<const MatrixTensor&, const int*>);
static_assert(std::is_same_v<
              decltype(std::declval<MatrixTensor&>().View()),
              Result<typename MatrixTensor::MutableView>>);
static_assert(std::is_same_v<
              decltype(std::declval<const MatrixTensor&>().View()),
              Result<typename MatrixTensor::ConstView>>);
static_assert(std::is_const_v<
              typename MatrixTensor::ConstView::ElementType>);
static_assert(!std::is_const_v<
              typename MatrixTensor::MutableView::ElementType>);

TEST(TensorTest, DefaultOwnerHasNoDescriptorOrAllocation) {
  MatrixTensor tensor;

  EXPECT_FALSE(tensor.HasDescriptor());
  EXPECT_TRUE(tensor.IsEmpty());
  EXPECT_EQ(tensor.GetSize(), 0);
  EXPECT_EQ(tensor.GetRequiredSpan(), 0);
  EXPECT_EQ(tensor.GetMemorySpace(), MemorySpace::kHost);
  ASSERT_NE(tensor.GetMemoryResource(), nullptr);
  Result<MatrixTensor::MutableView> view = tensor.View();
  ASSERT_FALSE(view.ok());
  EXPECT_EQ(view.status().code(), StatusCode::kFailedPrecondition);
}

TEST(TensorTest, ZeroSizeDescriptorIsDistinctFromDefaultOwner) {
  using EmptyExtents = Extents<2, dynamic_extent>;
  using EmptyTensor = Tensor<int, EmptyExtents>;
  Result<EmptyExtents> extents = EmptyExtents::Create(0);
  ASSERT_TRUE(extents.ok());
  auto resource = std::make_shared<test::ArrayCountingMemoryResource>();
  Result<EmptyTensor> result =
      EmptyTensor::Create(extents.value(), resource);

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().HasDescriptor());
  EXPECT_TRUE(result.value().IsEmpty());
  EXPECT_EQ(result.value().GetSize(), 0);
  EXPECT_EQ(result.value().GetRequiredSpan(), 0);
  EXPECT_EQ(result.value().GetExtent(0), 2);
  EXPECT_EQ(result.value().GetExtent(1), 0);
  EXPECT_EQ(resource->allocation_calls.load(), 0);
  Result<EmptyTensor::MutableView> view = result.value().View();
  ASSERT_TRUE(view.ok());
  EXPECT_EQ(view.value().Data(), nullptr);
  EXPECT_EQ(view.value().GetSize(), 0);
}

TEST(TensorTest, ClonePreservesZeroSizeDescriptorWithoutAllocating) {
  using EmptyExtents = Extents<2, dynamic_extent>;
  using EmptyTensor = Tensor<int, EmptyExtents>;
  Result<EmptyExtents> extents = EmptyExtents::Create(0);
  ASSERT_TRUE(extents.ok());
  Result<EmptyTensor> source = EmptyTensor::Create(extents.value());
  ASSERT_TRUE(source.ok());
  auto destination_resource =
      std::make_shared<test::ArrayCountingMemoryResource>();

  Result<EmptyTensor> clone = source.value().Clone(
      ExecutionContext::Serial(), destination_resource);

  ASSERT_TRUE(clone.ok());
  EXPECT_TRUE(clone.value().HasDescriptor());
  EXPECT_TRUE(clone.value().IsEmpty());
  EXPECT_EQ(clone.value().GetExtents(), extents.value());
  EXPECT_EQ(destination_resource->allocation_calls.load(), 0);
  ASSERT_TRUE(clone.value().View().ok());
  EXPECT_EQ(clone.value().View().value().Data(), nullptr);
}

TEST(TensorTest, CreatesOneHostAllocationAndMutableView) {
  auto resource = std::make_shared<test::ArrayCountingMemoryResource>();
  Result<MatrixTensor> result =
      MatrixTensor::Create(MatrixExtents(), resource);

  ASSERT_TRUE(result.ok());
  MatrixTensor& tensor = result.value();
  EXPECT_TRUE(tensor.HasDescriptor());
  EXPECT_FALSE(tensor.IsEmpty());
  EXPECT_EQ(tensor.GetSize(), 6);
  EXPECT_EQ(tensor.GetRequiredSpan(), 6);
  EXPECT_EQ(tensor.GetExtent(0), 2);
  EXPECT_EQ(tensor.GetExtent(1), 3);
  EXPECT_EQ(tensor.GetStride(0), 1);
  EXPECT_EQ(tensor.GetStride(1), 2);
  EXPECT_EQ(tensor.GetMemorySpace(), MemorySpace::kHost);
  EXPECT_EQ(tensor.GetMemoryResource(), resource);
  EXPECT_EQ(resource->allocation_calls.load(), 1);
  EXPECT_EQ(resource->allocated_bytes.load(), 6U * sizeof(int));

  Result<MatrixTensor::MutableView> view = tensor.View();
  ASSERT_TRUE(view.ok());
  view.value()(1, 2) = 41;
  EXPECT_EQ(view.value()(1, 2), 41);
}

TEST(TensorTest, ConstOwnerProducesOnlyConstElementView) {
  Result<MatrixTensor> result = MatrixTensor::Create(MatrixExtents());
  ASSERT_TRUE(result.ok());
  Result<MatrixTensor::MutableView> mutable_view = result.value().View();
  ASSERT_TRUE(mutable_view.ok());
  mutable_view.value()(1, 2) = 43;

  const MatrixTensor& tensor = result.value();
  Result<MatrixTensor::ConstView> const_view = tensor.View();
  ASSERT_TRUE(const_view.ok());
  EXPECT_EQ(const_view.value()(1, 2), 43);
}

TEST(TensorTest, RankZeroOwnerAllocatesOneScalar) {
  using ScalarTensor = Tensor<int, Extents<>>;
  auto resource = std::make_shared<test::ArrayCountingMemoryResource>();
  Result<ScalarTensor> result =
      ScalarTensor::Create(Extents<>(), resource);

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().HasDescriptor());
  EXPECT_EQ(result.value().GetSize(), 1);
  EXPECT_EQ(result.value().GetRequiredSpan(), 1);
  EXPECT_EQ(resource->allocation_calls.load(), 1);
  Result<ScalarTensor::MutableView> view = result.value().View();
  ASSERT_TRUE(view.ok());
  view.value()() = 47;
  EXPECT_EQ(view.value()(), 47);
}

TEST(TensorTest, MoveConstructionTransfersExactlyOneOwnership) {
  auto resource = std::make_shared<test::ArrayCountingMemoryResource>();
  {
    Result<MatrixTensor> result =
        MatrixTensor::Create(MatrixExtents(), resource);
    ASSERT_TRUE(result.ok());
    MatrixTensor source = std::move(result).value();
    Result<MatrixTensor::MutableView> source_view = source.View();
    ASSERT_TRUE(source_view.ok());
    int* original_data = source_view.value().Data();

    MatrixTensor destination(std::move(source));
    EXPECT_FALSE(source.HasDescriptor());
    EXPECT_TRUE(source.IsEmpty());
    ASSERT_FALSE(source.View().ok());
    ASSERT_TRUE(destination.View().ok());
    EXPECT_EQ(destination.View().value().Data(), original_data);
    EXPECT_EQ(resource->deallocation_calls.load(), 0);
  }
  EXPECT_EQ(resource->allocation_calls.load(), 1);
  EXPECT_EQ(resource->deallocation_calls.load(), 1);
  EXPECT_EQ(resource->allocated_bytes.load(),
            resource->deallocated_bytes.load());
}

TEST(TensorTest, MoveAssignmentReleasesPreviousAllocation) {
  auto resource = std::make_shared<test::ArrayCountingMemoryResource>();
  {
    Result<MatrixTensor> first =
        MatrixTensor::Create(MatrixExtents(), resource);
    Result<MatrixTensor> second =
        MatrixTensor::Create(MatrixExtents(), resource);
    ASSERT_TRUE(first.ok());
    ASSERT_TRUE(second.ok());
    MatrixTensor destination = std::move(first).value();
    MatrixTensor source = std::move(second).value();

    destination = std::move(source);
    EXPECT_TRUE(destination.HasDescriptor());
    EXPECT_FALSE(source.HasDescriptor());
    EXPECT_EQ(resource->deallocation_calls.load(), 1);

    destination = std::move(destination);
    EXPECT_TRUE(destination.HasDescriptor());
  }
  EXPECT_EQ(resource->allocation_calls.load(), 2);
  EXPECT_EQ(resource->deallocation_calls.load(), 2);
}

TEST(TensorTest, CloneAllocatesOneIndependentDestination) {
  auto source_resource =
      std::make_shared<test::ArrayCountingMemoryResource>();
  auto destination_resource =
      std::make_shared<test::ArrayCountingMemoryResource>();
  Result<MatrixTensor> source_result =
      MatrixTensor::Create(MatrixExtents(), source_resource);
  ASSERT_TRUE(source_result.ok());
  MatrixTensor& source = source_result.value();
  Result<MatrixTensor::MutableView> source_view = source.View();
  ASSERT_TRUE(source_view.ok());
  for (index_t column = 0; column < 3; ++column) {
    for (index_t row = 0; row < 2; ++row) {
      source_view.value()(row, column) =
          static_cast<int>(10 * row + column);
    }
  }

  Result<MatrixTensor> clone_result = source.Clone(
      ExecutionContext::Serial(), destination_resource);

  ASSERT_TRUE(clone_result.ok());
  EXPECT_EQ(source_resource->allocation_calls.load(), 1);
  EXPECT_EQ(destination_resource->allocation_calls.load(), 1);
  ASSERT_TRUE(source.View().ok());
  ASSERT_TRUE(clone_result.value().View().ok());
  EXPECT_NE(source.View().value().Data(),
            clone_result.value().View().value().Data());
  for (index_t column = 0; column < 3; ++column) {
    for (index_t row = 0; row < 2; ++row) {
      EXPECT_EQ(clone_result.value().View().value()(row, column),
                source.View().value()(row, column));
    }
  }

  clone_result.value().View().value()(1, 2) = 101;
  EXPECT_NE(source.View().value()(1, 2), 101);
}

TEST(TensorTest, ClonePreservesRightMajorMappingOrder) {
  Result<RightMatrixTensor> source =
      RightMatrixTensor::Create(MatrixExtents());
  ASSERT_TRUE(source.ok());
  ASSERT_TRUE(source.value().View().ok());
  for (index_t row = 0; row < 2; ++row) {
    for (index_t column = 0; column < 3; ++column) {
      source.value().View().value()(row, column) =
          static_cast<int>(100 * row + column);
    }
  }

  Result<RightMatrixTensor> clone = source.value().Clone(
      ExecutionContext::Serial(), MemorySpace::kHost);

  ASSERT_TRUE(clone.ok());
  EXPECT_EQ(clone.value().GetStride(0), 3);
  EXPECT_EQ(clone.value().GetStride(1), 1);
  for (index_t row = 0; row < 2; ++row) {
    for (index_t column = 0; column < 3; ++column) {
      EXPECT_EQ(clone.value().View().value()(row, column),
                static_cast<int>(100 * row + column));
    }
  }
}

TEST(TensorTest, ContextFactoryUsesRequestedHostSpace) {
  const ExecutionContext context = ExecutionContext::Serial();
  Result<MatrixTensor> result = MatrixTensor::Create(
      MatrixExtents(), context, MemorySpace::kHost);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().GetMemorySpace(), MemorySpace::kHost);
  EXPECT_EQ(result.value().GetMemoryResource(), GetHostMemoryResource());
}

TEST(TensorTest, RejectsNullAndNonHostResourcesBeforeAllocation) {
  Result<MatrixTensor> null_resource =
      MatrixTensor::Create(MatrixExtents(), nullptr);
  auto device_resource =
      std::make_shared<test::ArrayCountingMemoryResource>(
          GetHostMemoryResource(), MemorySpace::kDevice);
  Result<MatrixTensor> device =
      MatrixTensor::Create(MatrixExtents(), device_resource);

  ASSERT_FALSE(null_resource.ok());
  ASSERT_FALSE(device.ok());
  EXPECT_EQ(null_resource.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(device.status().code(), StatusCode::kUnsupported);
  EXPECT_EQ(device_resource->allocation_calls.load(), 0);
}

TEST(TensorTest, RejectsUnsupportedContextSpaceBeforePointerAccess) {
  const ExecutionContext context = ExecutionContext::Serial();
  Result<MatrixTensor> created = MatrixTensor::Create(
      MatrixExtents(), context, MemorySpace::kDevice);
  ASSERT_FALSE(created.ok());
  EXPECT_EQ(created.status().code(), StatusCode::kUnsupported);

  Result<MatrixTensor> source = MatrixTensor::Create(MatrixExtents());
  ASSERT_TRUE(source.ok());
  Result<MatrixTensor> clone =
      source.value().Clone(context, MemorySpace::kDevice);
  ASSERT_FALSE(clone.ok());
  EXPECT_EQ(clone.status().code(), StatusCode::kUnsupported);
  EXPECT_TRUE(source.value().HasDescriptor());
  EXPECT_TRUE(source.value().View().ok());
}

TEST(TensorTest, MappingOverflowFailsBeforeResourceAllocation) {
  using LargeExtents = DynamicTensorExtents<2>;
  using LargeTensor = Tensor<int, LargeExtents>;
  Result<LargeExtents> extents = LargeExtents::Create(
      std::numeric_limits<extent_t>::max(), 2);
  ASSERT_TRUE(extents.ok());
  auto resource = std::make_shared<test::ArrayCountingMemoryResource>();

  Result<LargeTensor> result =
      LargeTensor::Create(extents.value(), resource);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kOverflow);
  EXPECT_EQ(resource->allocation_calls.load(), 0);
}

TEST(TensorTest, PropagatesAllocationAndCloneFailureTransactionally) {
  auto failing_resource =
      std::make_shared<test::ArrayFailingMemoryResource>();
  Result<MatrixTensor> creation =
      MatrixTensor::Create(MatrixExtents(), failing_resource);
  ASSERT_FALSE(creation.ok());
  EXPECT_EQ(creation.status().code(), StatusCode::kAllocationFailed);
  EXPECT_EQ(failing_resource->allocation_calls.load(), 1);
  EXPECT_EQ(failing_resource->deallocation_calls.load(), 0);

  Result<MatrixTensor> source = MatrixTensor::Create(MatrixExtents());
  ASSERT_TRUE(source.ok());
  ASSERT_TRUE(source.value().View().ok());
  source.value().View().value()(1, 2) = 53;
  Result<MatrixTensor> clone = source.value().Clone(
      ExecutionContext::Serial(), failing_resource);
  ASSERT_FALSE(clone.ok());
  EXPECT_EQ(clone.status().code(), StatusCode::kAllocationFailed);
  EXPECT_EQ(source.value().View().value()(1, 2), 53);
}

TEST(TensorTest, CloneRollsBackWhenElementCopyThrows) {
  using ThrowingTensor = Tensor<ThrowOnCopyAssignment, Extents<2>>;
  auto destination_resource =
      std::make_shared<test::ArrayCountingMemoryResource>();
  Result<ThrowingTensor> source =
      ThrowingTensor::Create(Extents<2>());
  ASSERT_TRUE(source.ok());
  ASSERT_TRUE(source.value().View().ok());
  source.value().View().value()(0).value = 59;
  source.value().View().value()(1).value = 61;
  ThrowOnCopyAssignment::throw_during_copy = true;

  Result<ThrowingTensor> clone = source.value().Clone(
      ExecutionContext::Serial(), destination_resource);
  ThrowOnCopyAssignment::throw_during_copy = false;

  ASSERT_FALSE(clone.ok());
  EXPECT_EQ(clone.status().code(), StatusCode::kInternal);
  EXPECT_EQ(destination_resource->allocation_calls.load(), 1);
  EXPECT_EQ(destination_resource->deallocation_calls.load(), 1);
  EXPECT_EQ(source.value().View().value()(0).value, 59);
  EXPECT_EQ(source.value().View().value()(1).value, 61);
}

TEST(TensorTest, CloneRejectsNonCopyAssignableElementsBeforeAllocation) {
  using NonCopyTensor = Tensor<NonCopyAssignable, Extents<2>>;
  Result<NonCopyTensor> source =
      NonCopyTensor::Create(Extents<2>());
  ASSERT_TRUE(source.ok());
  auto destination_resource =
      std::make_shared<test::ArrayCountingMemoryResource>();

  Result<NonCopyTensor> clone = source.value().Clone(
      ExecutionContext::Serial(), destination_resource);

  ASSERT_FALSE(clone.ok());
  EXPECT_EQ(clone.status().code(), StatusCode::kUnsupported);
  EXPECT_EQ(destination_resource->allocation_calls.load(), 0);
  EXPECT_TRUE(source.value().HasDescriptor());
}

}  // namespace
}  // namespace asc
