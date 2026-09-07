#include "asc/sparse/blas.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>

#include "../allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/compressed.h"
#include "external_vector.h"
#include "test_support.h"

template <typename Element>
struct M4ByteBackedWritableVector {
  std::byte* data = nullptr;
  asc::extent_t size = 0;
  std::size_t write_count = 0;
};

namespace asc {

template <typename Element>
struct ExpressionAdapter<::M4ByteBackedWritableVector<Element>> {
  using value_type = Element;
  static constexpr rank_t rank = 1;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;
  static constexpr ExpressionOperation operation =
      ExpressionOperation::kExternal;

  static constexpr std::array<extent_t, 1> Shape(
      const ::M4ByteBackedWritableVector<Element>& vector) noexcept {
    return {vector.size};
  }

  static value_type Read(const ::M4ByteBackedWritableVector<Element>& vector,
                         std::span<const index_t, 1> coordinate) noexcept {
    value_type value{};
    std::memcpy(
        &value,
        vector.data + static_cast<std::size_t>(coordinate[0]) * sizeof(value),
        sizeof(value));
    return value;
  }

  static bool MayAlias(const ::M4ByteBackedWritableVector<Element>& vector,
                       AliasToken token) noexcept {
    if (vector.data == nullptr || vector.size <= 0 ||
        token.identity() == nullptr) {
      return false;
    }
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(vector.data);
    const std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(token.identity());
    const std::size_t bytes =
        static_cast<std::size_t>(vector.size) * sizeof(value_type);
    return bytes <= UINTPTR_MAX - begin && address >= begin &&
           address < begin + bytes;
  }
};

template <typename Element>
struct ExpressionPlacementAdapter<::M4ByteBackedWritableVector<Element>> {
  static constexpr MemorySpace Space(
      const ::M4ByteBackedWritableVector<Element>& vector) noexcept {
    static_cast<void>(vector);
    return MemorySpace::kHost;
  }

  static constexpr ExpressionAliasMetadata Alias(
      const ::M4ByteBackedWritableVector<Element>& vector) noexcept {
    return ExpressionAliasMetadata(
        vector.data, vector.data,
        static_cast<std::size_t>(vector.size) * sizeof(Element));
  }
};

template <typename Element>
struct WritableExpressionAdapter<::M4ByteBackedWritableVector<Element>> {
  using value_type = Element;

  static constexpr std::array<extent_t, 1> Shape(
      const ::M4ByteBackedWritableVector<Element>& vector) noexcept {
    return {vector.size};
  }

  static constexpr ExpressionAliasMetadata Alias(
      const ::M4ByteBackedWritableVector<Element>& vector) noexcept {
    return ExpressionPlacementAdapter<
        ::M4ByteBackedWritableVector<Element>>::Alias(vector);
  }

  static constexpr bool IsUnique(
      const ::M4ByteBackedWritableVector<Element>& vector) noexcept {
    static_cast<void>(vector);
    return true;
  }

  static void Write(::M4ByteBackedWritableVector<Element>& vector,
                    std::span<const index_t, 1> coordinate,
                    value_type value) noexcept {
    ++vector.write_count;
    std::memcpy(vector.data + static_cast<std::size_t>(coordinate[0]) *
                                  sizeof(value_type),
                &value, sizeof(value));
  }
};

}  // namespace asc

namespace {

using asc_sparse_test::TestContext;

constexpr std::array<asc::extent_t, 2> kShape{3, 4};
constexpr std::array<asc::nnz_t, 4> kOffsets{0, 2, 4, 5};
constexpr std::array<asc::index_t, 5> kIndices{1, 3, 0, 2, 1};

template <typename Result>
void CheckError(TestContext& test, const Result& result,
                asc::ErrorCode expected) {
  ASC_SPARSE_TEST_CHECK(test, !result.ok());
  if (!result.ok()) {
    ASC_SPARSE_TEST_EQ(test, result.status().code(), expected);
  }
}

void CheckError(TestContext& test, const asc::Status& status,
                asc::ErrorCode expected) {
  ASC_SPARSE_TEST_CHECK(test, !status.ok());
  if (!status.ok()) {
    ASC_SPARSE_TEST_EQ(test, status.code(), expected);
  }
}

template <typename Real>
asc::CsrView<const Real> MakeMatrix(
    std::array<Real, 5>& values,
    asc::MemorySpace space = asc::MemorySpace::kHost) {
  auto matrix = asc::CsrView<const Real>::Create(
      kOffsets.data(), kIndices.data(), values.data(), kShape, 5, space);
  if (!matrix.ok()) {
    std::abort();
  }
  return *matrix;
}

template <typename Real>
void CheckStridedOutput(TestContext& test, const std::array<Real, 6>& storage,
                        const std::array<Real, 3>& expected) {
  for (std::size_t index = 0; index < expected.size(); ++index) {
    ASC_SPARSE_TEST_NEAR(test, storage[index * 2], expected[index], Real{0},
                         asc_sparse_test::DefaultTolerance<Real>());
  }
}

template <typename Real>
void TestNumericalOracle(TestContext& test) {
  std::array<Real, 5> matrix_values{Real{2}, Real{-1}, Real{4}, Real{5},
                                    Real{3}};
  const auto matrix = MakeMatrix(matrix_values);
  std::array<Real, 4> input_values{Real{2}, Real{-1}, Real{3}, Real{4}};
  const M4ExternalVector<const Real> input{
      .data = input_values.data(),
      .size = 4,
      .stride = 1,
      .memory_space = asc::MemorySpace::kHost,
      .unique = true,
  };
  std::array<Real, 6> output_storage{Real{7},   Real{-90}, Real{11},
                                     Real{-91}, Real{13},  Real{-92}};
  M4ExternalVector<Real> output{
      .data = output_storage.data(),
      .size = 3,
      .stride = 2,
      .memory_space = asc::MemorySpace::kHost,
      .unique = true,
  };
  static_assert(asc::PlacedReadableExpression<decltype(input)>);
  static_assert(asc::WritableExpression<decltype(output)>);
  ASC_SPARSE_TEST_CHECK(test, asc::WritableExpressionIsUnique(output));
  const asc::Status status = asc::Spmv(asc::ExecutionContext::Serial(), Real{2},
                                       matrix, input, Real{-0.5}, output);
  ASC_SPARSE_TEST_CHECK(test, status.ok());
  constexpr std::array<Real, 3> kExpected{Real{-15.5}, Real{40.5}, Real{-12.5}};
  CheckStridedOutput(test, output_storage, kExpected);
  ASC_SPARSE_TEST_EQ(test, output_storage[1], Real{-90});
  ASC_SPARSE_TEST_EQ(test, output_storage[3], Real{-91});
  ASC_SPARSE_TEST_EQ(test, output_storage[5], Real{-92});

  output_storage[0] = std::numeric_limits<Real>::quiet_NaN();
  output_storage[2] = std::numeric_limits<Real>::quiet_NaN();
  output_storage[4] = std::numeric_limits<Real>::quiet_NaN();
  ASC_SPARSE_TEST_CHECK(test, asc::Spmv(asc::ExecutionContext::Serial(),
                                        Real{1}, matrix, input, Real{0}, output)
                                  .ok());
  constexpr std::array<Real, 3> kProduct{Real{-6}, Real{23}, Real{-3}};
  CheckStridedOutput(test, output_storage, kProduct);

  std::size_t allocations = 0;
  {
    asc_sparse_test::AllocationProbe probe;
    const asc::Status measured =
        asc::Spmv(asc::ExecutionContext::Serial(), Real{1}, matrix, input,
                  Real{0}, output);
    allocations = probe.count();
    ASC_SPARSE_TEST_CHECK(test, measured.ok());
  }
  ASC_SPARSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(allocations, 0));
}

void TestDeterministicRowOrder(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape{1, 3};
  constexpr std::array<asc::nnz_t, 2> kOffsets{0, 3};
  constexpr std::array<asc::index_t, 3> kIndices{0, 1, 2};
  std::array<float, 3> matrix_values{1.0e20F, 1.0F, -1.0e20F};
  auto matrix = asc::CsrView<const float>::Create(
      kOffsets.data(), kIndices.data(), matrix_values.data(), kShape, 3,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, matrix.ok());
  if (!matrix.ok()) {
    return;
  }
  std::array<float, 3> input_values{1.0F, 1.0F, 1.0F};
  std::array<float, 1> output_values{-1.0F};
  const M4ExternalVector<const float> input{.data = input_values.data(),
                                            .size = 3};
  M4ExternalVector<float> output{.data = output_values.data(), .size = 1};
  ASC_SPARSE_TEST_CHECK(test, asc::Spmv(asc::ExecutionContext::Serial(), 1.0F,
                                        *matrix, input, 0.0F, output)
                                  .ok());
  ASC_SPARSE_TEST_EQ(test, output_values[0], 0.0F);
}

template <typename Real>
void TestEmptyAndDegenerateMatrices(TestContext& test) {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();

  constexpr std::array<asc::extent_t, 2> kZeroRowsShape{0, 4};
  constexpr std::array<asc::nnz_t, 1> kZeroRowsOffsets{0};
  auto zero_rows = asc::CsrView<const Real>::Create(
      kZeroRowsOffsets.data(), nullptr, nullptr, kZeroRowsShape, 0,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, zero_rows.ok());
  std::array<Real, 4> input_values{};
  const M4ExternalVector<const Real> input{.data = input_values.data(),
                                           .size = 4};
  M4ExternalVector<Real> empty_output{.data = nullptr, .size = 0};
  if (zero_rows.ok()) {
    ASC_SPARSE_TEST_CHECK(test, asc::Spmv(context, Real{1}, *zero_rows, input,
                                          Real{0}, empty_output)
                                    .ok());
  }

  constexpr std::array<asc::extent_t, 2> kZeroColumnsShape{3, 0};
  constexpr std::array<asc::nnz_t, 4> kZeroColumnsOffsets{0, 0, 0, 0};
  auto zero_columns = asc::CsrView<const Real>::Create(
      kZeroColumnsOffsets.data(), nullptr, nullptr, kZeroColumnsShape, 0,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, zero_columns.ok());
  const M4ExternalVector<const Real> empty_input{.data = nullptr, .size = 0};
  std::array<Real, 3> output_values{Real{2}, Real{-3}, Real{4}};
  M4ExternalVector<Real> output{.data = output_values.data(), .size = 3};
  if (zero_columns.ok()) {
    ASC_SPARSE_TEST_CHECK(test, asc::Spmv(context, Real{7}, *zero_columns,
                                          empty_input, Real{2}, output)
                                    .ok());
    constexpr std::array<Real, 3> kExpected{Real{4}, Real{-6}, Real{8}};
    ASC_SPARSE_TEST_EQ(test, output_values, kExpected);
  }

  constexpr std::array<asc::extent_t, 2> kEmptyShape{3, 4};
  constexpr std::array<asc::nnz_t, 4> kEmptyOffsets{0, 0, 0, 0};
  auto empty =
      asc::CsrView<const Real>::Create(kEmptyOffsets.data(), nullptr, nullptr,
                                       kEmptyShape, 0, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, empty.ok());
  output_values = {std::numeric_limits<Real>::quiet_NaN(),
                   std::numeric_limits<Real>::quiet_NaN(),
                   std::numeric_limits<Real>::quiet_NaN()};
  if (empty.ok()) {
    ASC_SPARSE_TEST_CHECK(
        test, asc::Spmv(context, Real{1}, *empty, input, Real{0}, output).ok());
    constexpr std::array<Real, 3> kZeros{Real{0}, Real{0}, Real{0}};
    ASC_SPARSE_TEST_EQ(test, output_values, kZeros);
  }
}

template <typename Real>
// All rejected operations verify the same destination rollback invariant.
// NOLINTNEXTLINE(readability-function-size)
void TestTransactionalFailures(TestContext& test) {
  std::array<Real, 5> matrix_values{Real{2}, Real{-1}, Real{4}, Real{5},
                                    Real{3}};
  const auto matrix = MakeMatrix(matrix_values);
  std::array<Real, 8> shared{Real{2}, Real{-1}, Real{3},  Real{4},
                             Real{7}, Real{11}, Real{13}, Real{17}};
  const M4ExternalVector<const Real> input{.data = shared.data(), .size = 4};
  M4ExternalVector<Real> output{.data = shared.data() + 4, .size = 3};
  const std::array<Real, 3> before{shared[4], shared[5], shared[6]};
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();

  const M4ExternalVector<const Real> short_input{.data = shared.data(),
                                                 .size = 3};
  CheckError(test,
             asc::Spmv(context, Real{1}, matrix, short_input, Real{0}, output),
             asc::ErrorCode::kShape);
  ASC_SPARSE_TEST_EQ(
      test, (std::array<Real, 3>{shared[4], shared[5], shared[6]}), before);

  M4ExternalVector<Real> short_output{.data = shared.data() + 4, .size = 2};
  CheckError(test,
             asc::Spmv(context, Real{1}, matrix, input, Real{0}, short_output),
             asc::ErrorCode::kShape);
  ASC_SPARSE_TEST_EQ(
      test, (std::array<Real, 3>{shared[4], shared[5], shared[6]}), before);

  M4ExternalVector<Real> nonunique{
      .data = shared.data() + 4, .size = 3, .stride = 0, .unique = false};
  CheckError(test,
             asc::Spmv(context, Real{1}, matrix, input, Real{0}, nonunique),
             asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(
      test, (std::array<Real, 3>{shared[4], shared[5], shared[6]}), before);

  // Exact and partial input/output overlap are both forbidden.
  M4ExternalVector<Real> exact_overlap{.data = shared.data(), .size = 3};
  const M4ExternalVector<const Real> exact_input{.data = shared.data(),
                                                 .size = 4};
  CheckError(
      test,
      asc::Spmv(context, Real{1}, matrix, exact_input, Real{0}, exact_overlap),
      asc::ErrorCode::kInvalidArgument);

  M4ExternalVector<Real> partial_output{.data = shared.data() + 1, .size = 3};
  CheckError(
      test,
      asc::Spmv(context, Real{1}, matrix, exact_input, Real{0}, partial_output),
      asc::ErrorCode::kInvalidArgument);

  M4ExternalVector<Real> matrix_overlap{.data = matrix_values.data(),
                                        .size = 3};
  const auto matrix_before = matrix_values;
  CheckError(
      test, asc::Spmv(context, Real{1}, matrix, input, Real{0}, matrix_overlap),
      asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(test, matrix_values, matrix_before);

  const M4ExternalVector<const Real> device_input{
      .data = shared.data(),
      .size = 4,
      .stride = 1,
      .memory_space = asc::MemorySpace::kDevice,
      .unique = true,
  };
  CheckError(test,
             asc::Spmv(context, Real{1}, matrix, device_input, Real{0}, output),
             asc::ErrorCode::kMemoryAccess);
  ASC_SPARSE_TEST_EQ(
      test, (std::array<Real, 3>{shared[4], shared[5], shared[6]}), before);

  M4ExternalVector<Real> device_output{
      .data = shared.data() + 4,
      .size = 3,
      .stride = 1,
      .memory_space = asc::MemorySpace::kDevice,
      .unique = true,
  };
  CheckError(test,
             asc::Spmv(context, Real{1}, matrix, input, Real{0}, device_output),
             asc::ErrorCode::kMemoryAccess);
  ASC_SPARSE_TEST_EQ(
      test, (std::array<Real, 3>{shared[4], shared[5], shared[6]}), before);

  auto device_matrix = asc::CsrView<const Real>::Create(
      kOffsets.data(), kIndices.data(), matrix_values.data(), kShape, 5,
      asc::MemorySpace::kDevice);
  ASC_SPARSE_TEST_CHECK(test, device_matrix.ok());
  if (device_matrix.ok()) {
    CheckError(
        test,
        asc::Spmv(context, Real{1}, *device_matrix, input, Real{0}, output),
        asc::ErrorCode::kMemoryAccess);
  }
  ASC_SPARSE_TEST_EQ(
      test, (std::array<Real, 3>{shared[4], shared[5], shared[6]}), before);
}

template <typename Real>
void TestStructuralOutputOverlapRejected(TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kOneByOneShape{1, 1};
  std::array<asc::nnz_t, 2> offsets{0, 1};
  std::array<asc::index_t, 1> indices{0};
  std::array<Real, 1> values{Real{2}};
  auto matrix = asc::CsrView<const Real>::Create(offsets.data(), indices.data(),
                                                 values.data(), kOneByOneShape,
                                                 1, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(test, matrix.ok());
  if (!matrix.ok()) {
    return;
  }
  const std::array<Real, 1> input_values{Real{3}};
  const M4ExternalVector<const Real> input{.data = input_values.data(),
                                           .size = 1};
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();

  // The output's writable byte span partially overlaps both outer-offset
  // objects. The byte adapter makes an accidental pre-validation write legal,
  // while the one-row matrix ensures no structure is read after that write.
  const auto offsets_before = offsets;
  M4ByteBackedWritableVector<Real> offsets_output{
      .data =
          reinterpret_cast<std::byte*>(offsets.data()) + sizeof(asc::nnz_t) / 2,
      .size = 1,
  };
  const asc::Status offsets_status =
      asc::Spmv(context, Real{1}, *matrix, input, Real{0}, offsets_output);
  const std::size_t offsets_writes = offsets_output.write_count;
  offsets = offsets_before;
  CheckError(test, offsets_status, asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(test, offsets_writes, std::size_t{0});
  ASC_SPARSE_TEST_EQ(test, offsets, offsets_before);

  const auto indices_before = indices;
  M4ByteBackedWritableVector<Real> indices_output{
      .data = reinterpret_cast<std::byte*>(indices.data()),
      .size = 1,
  };
  const asc::Status indices_status =
      asc::Spmv(context, Real{1}, *matrix, input, Real{0}, indices_output);
  const std::size_t indices_writes = indices_output.write_count;
  indices = indices_before;
  CheckError(test, indices_status, asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(test, indices_writes, std::size_t{0});
  ASC_SPARSE_TEST_EQ(test, indices, indices_before);
}

}  // namespace

int main() {
  TestContext test;
  TestNumericalOracle<float>(test);
  TestNumericalOracle<double>(test);
  TestDeterministicRowOrder(test);
  TestEmptyAndDegenerateMatrices<float>(test);
  TestEmptyAndDegenerateMatrices<double>(test);
  TestTransactionalFailures<float>(test);
  TestTransactionalFailures<double>(test);
  TestStructuralOutputOverlapRejected<float>(test);
  TestStructuralOutputOverlapRejected<double>(test);
  return test.Finish();
}
