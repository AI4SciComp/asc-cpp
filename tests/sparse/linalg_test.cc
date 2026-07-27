#include "asc/sparse/linalg.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>

#include "allocation_counter.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "test_support.h"

namespace {

template <typename Scalar>
using ExternalVector = external_sparse_test::Vector<Scalar>;

template <typename Scalar>
asc::Result<asc::CsrView<const Scalar>> MakeOracleMatrix(
    std::array<Scalar, 8>& values) {
  static constexpr std::array<asc::extent_t, 2> kShape = {4, 5};
  static constexpr std::array<asc::nnz_t, 5> kOffsets = {0, 2, 3, 6, 8};
  static constexpr std::array<asc::index_t, 8> kIndices = {0, 3, 1, 0,
                                                           2, 4, 1, 4};
  values = {static_cast<Scalar>(2),  static_cast<Scalar>(-1),
            static_cast<Scalar>(4),  static_cast<Scalar>(7),
            static_cast<Scalar>(3),  static_cast<Scalar>(5),
            static_cast<Scalar>(-2), static_cast<Scalar>(6)};
  return asc::CsrView<const Scalar>::Create(kShape, kOffsets, kIndices,
                                            std::span<const Scalar>(values),
                                            asc::MemorySpace::kHost);
}

template <typename Scalar>
void CheckNumericalOracle(asc_sparse_test::TestContext& context) {
  static_assert(asc::SparseLinearAlgebraScalar<Scalar>);
  std::array<Scalar, 8> matrix_values{};
  auto matrix = MakeOracleMatrix(matrix_values);
  ASC_SPARSE_TEST_CHECK(context, matrix.ok());

  std::array<Scalar, 5> input_values = {
      static_cast<Scalar>(1), static_cast<Scalar>(-2), static_cast<Scalar>(3),
      static_cast<Scalar>(4), static_cast<Scalar>(-1)};
  std::array<Scalar, 4> output_values = {
      std::numeric_limits<Scalar>::quiet_NaN(),
      std::numeric_limits<Scalar>::quiet_NaN(),
      std::numeric_limits<Scalar>::quiet_NaN(),
      std::numeric_limits<Scalar>::quiet_NaN()};
  std::size_t input_reads = 0;
  std::size_t output_reads = 0;
  std::size_t output_writes = 0;
  int input_identity = 0;
  int output_identity = 0;
  const ExternalVector<const Scalar> input = {
      .data = input_values.data(),
      .size = 5,
      .stride = 1,
      .space = asc::MemorySpace::kHost,
      .alias_identity = std::addressof(input_identity),
      .read_count = &input_reads,
  };
  ExternalVector<Scalar> output = {
      .data = output_values.data(),
      .size = 4,
      .stride = 1,
      .space = asc::MemorySpace::kHost,
      .alias_identity = std::addressof(output_identity),
      .read_count = &output_reads,
      .write_count = &output_writes,
      .poison_reads = true,
  };

  asc::Status status = asc::Status::Ok();
  std::size_t allocations = 1;
  {
    asc_sparse_test::AllocationCountScope allocation_scope;
    status = asc::Spmv(asc::ExecutionContext::Serial(), static_cast<Scalar>(1),
                       *matrix, input, static_cast<Scalar>(0), output);
    allocations = allocation_scope.count();
  }
  ASC_SPARSE_TEST_CHECK(context, status.ok());
  ASC_SPARSE_TEST_EQ(context, allocations, 0U);
  ASC_SPARSE_TEST_EQ(context, input_reads, 8U);
  ASC_SPARSE_TEST_EQ(context, output_reads, 0U);
  ASC_SPARSE_TEST_EQ(context, output_writes, 4U);
  ASC_SPARSE_TEST_RANGE_EQ(
      context, output_values,
      (std::array<Scalar, 4>{static_cast<Scalar>(-2), static_cast<Scalar>(-8),
                             static_cast<Scalar>(11),
                             static_cast<Scalar>(-2)}));

  output_values = {static_cast<Scalar>(10), static_cast<Scalar>(20),
                   static_cast<Scalar>(30), static_cast<Scalar>(40)};
  input_reads = 0;
  output_reads = 0;
  output_writes = 0;
  output.poison_reads = false;
  const asc::Status scaled =
      asc::Spmv(asc::ExecutionContext::Serial(), static_cast<Scalar>(2),
                *matrix, input, static_cast<Scalar>(-0.5), output);
  ASC_SPARSE_TEST_CHECK(context, scaled.ok());
  ASC_SPARSE_TEST_EQ(context, input_reads, 8U);
  ASC_SPARSE_TEST_EQ(context, output_reads, 4U);
  ASC_SPARSE_TEST_EQ(context, output_writes, 4U);
  ASC_SPARSE_TEST_RANGE_EQ(
      context, output_values,
      (std::array<Scalar, 4>{static_cast<Scalar>(-9), static_cast<Scalar>(-26),
                             static_cast<Scalar>(7),
                             static_cast<Scalar>(-24)}));
}

void CheckDenseAndExternalInteroperability(
    asc_sparse_test::TestContext& context) {
  std::array<double, 8> matrix_values{};
  auto matrix = MakeOracleMatrix(matrix_values);

  constexpr std::array<asc::extent_t, 1> kInputShape = {5};
  constexpr std::array<asc::extent_t, 1> kOutputShape = {4};
  constexpr std::array<asc::stride_t, 1> kInputStride = {2};
  constexpr std::array<asc::stride_t, 1> kOutputStride = {3};
  std::array<double, 9> input_storage = {1, 0, -2, 0, 3, 0, 4, 0, -1};
  std::array<double, 10> output_storage{};
  auto input_mapping = asc::DenseLayoutMapping<1>::Create(
      asc::LayoutStride{}, kInputShape, kInputStride);
  auto output_mapping = asc::DenseLayoutMapping<1>::Create(
      asc::LayoutStride{}, kOutputShape, kOutputStride);
  auto input = asc::DenseView<const double, 1>::Create(
      input_storage.data(), *input_mapping, asc::MemorySpace::kHost);
  auto output = asc::DenseView<double, 1>::Create(
      output_storage.data(), *output_mapping, asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, input.ok());
  ASC_SPARSE_TEST_CHECK(context, output.ok());
  ASC_SPARSE_TEST_CHECK(context, asc::Spmv(asc::ExecutionContext::Serial(), 1.0,
                                           *matrix, *input, 0.0, *output)
                                     .ok());
  ASC_SPARSE_TEST_EQ(context, output_storage[0], -2.0);
  ASC_SPARSE_TEST_EQ(context, output_storage[3], -8.0);
  ASC_SPARSE_TEST_EQ(context, output_storage[6], 11.0);
  ASC_SPARSE_TEST_EQ(context, output_storage[9], -2.0);

  // ExternalVector deletes unary operator&. Successful compilation and
  // execution prove that Spmv obtains object addresses with std::addressof.
  std::array<double, 5> external_input_values = {1, -2, 3, 4, -1};
  std::array<double, 4> external_output_values{};
  int input_identity = 0;
  int output_identity = 0;
  const ExternalVector<const double> external_input = {
      .data = external_input_values.data(),
      .size = 5,
      .alias_identity = std::addressof(input_identity),
  };
  ExternalVector<double> external_output = {
      .data = external_output_values.data(),
      .size = 4,
      .alias_identity = std::addressof(output_identity),
  };
  ASC_SPARSE_TEST_CHECK(context,
                        asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *matrix,
                                  external_input, 0.0, external_output)
                            .ok());
  ASC_SPARSE_TEST_RANGE_EQ(context, external_output_values,
                           (std::array<double, 4>{-2.0, -8.0, 11.0, -2.0}));
}

void CheckCoordinateOutputCoverage(asc_sparse_test::TestContext& context) {
  std::array<double, 8> matrix_values{};
  auto matrix = MakeOracleMatrix(matrix_values);
  std::array<double, 5> input_values = {1, -2, 3, 4, -1};
  int input_identity = 0;
  const ExternalVector<const double> input = {
      .data = input_values.data(),
      .size = 5,
      .alias_identity = std::addressof(input_identity),
  };

  constexpr std::array<asc::extent_t, 1> kOutputShape = {4};
  constexpr std::array<asc::index_t, 4> kFullCoordinates = {0, 1, 2, 3};
  std::array<double, 4> full_values = {9, 9, 9, 9};
  auto full = asc::CoordinateView<double, 1>::Create(
      kFullCoordinates.data(), full_values.data(), kOutputShape, 4,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, full.ok());
  ASC_SPARSE_TEST_CHECK(context, asc::Spmv(asc::ExecutionContext::Serial(), 1.0,
                                           *matrix, input, 0.0, *full)
                                     .ok());
  ASC_SPARSE_TEST_RANGE_EQ(context, full_values,
                           (std::array<double, 4>{-2, -8, 11, -2}));

  constexpr std::array<asc::index_t, 3> kMissingCoordinates = {0, 2, 3};
  std::array<double, 3> missing_values = {31, 32, 33};
  auto missing = asc::CoordinateView<double, 1>::Create(
      kMissingCoordinates.data(), missing_values.data(), kOutputShape, 3,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, missing.ok());
  std::size_t input_reads = 0;
  auto counted_input = input;
  counted_input.read_count = &input_reads;
  const auto before = missing_values;
  const asc::Status status = asc::Spmv(asc::ExecutionContext::Serial(), 1.0,
                                       *matrix, counted_input, 0.0, *missing);
  ASC_SPARSE_TEST_CHECK(context, !status.ok());
  ASC_SPARSE_TEST_EQ(context, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, input_reads, 0U);
  ASC_SPARSE_TEST_EQ(context, missing_values, before);
}

void CheckAscViewOverlapTransactions(asc_sparse_test::TestContext& context) {
  constexpr std::array<asc::extent_t, 1> kInputShape = {5};
  constexpr std::array<asc::extent_t, 1> kOutputShape = {4};
  constexpr std::array<asc::index_t, 5> kInputCoordinates = {0, 1, 2, 3, 4};
  constexpr std::array<asc::index_t, 4> kOutputCoordinates = {0, 1, 2, 3};
  std::array<double, 8> matrix_values{};
  auto matrix = MakeOracleMatrix(matrix_values);

  std::array<double, 6> shared_vectors = {1, -2, 3, 4, -1, 99};
  auto input = asc::CoordinateView<const double, 1>::Create(
      kInputCoordinates.data(), shared_vectors.data(), kInputShape, 5,
      asc::MemorySpace::kHost);
  auto output = asc::CoordinateView<double, 1>::Create(
      kOutputCoordinates.data(), shared_vectors.data() + 1, kOutputShape, 4,
      asc::MemorySpace::kHost);
  ASC_SPARSE_TEST_CHECK(context, input.ok());
  ASC_SPARSE_TEST_CHECK(context, output.ok());
  const auto vector_before = shared_vectors;
  const asc::Status vector_overlap = asc::Spmv(
      asc::ExecutionContext::Serial(), 1.0, *matrix, *input, 0.0, *output);
  ASC_SPARSE_TEST_CHECK(context, !vector_overlap.ok());
  ASC_SPARSE_TEST_EQ(context, vector_overlap.code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, shared_vectors, vector_before);

  std::array<double, 12> matrix_output_storage = {2,  -1, 4,  7,  3,  5,
                                                  -2, 6,  71, 72, 73, 74};
  constexpr std::array<asc::extent_t, 2> kMatrixShape = {4, 5};
  constexpr std::array<asc::nnz_t, 5> kOffsets = {0, 2, 3, 6, 8};
  constexpr std::array<asc::index_t, 8> kIndices = {0, 3, 1, 0, 2, 4, 1, 4};
  auto overlapping_matrix = asc::CsrView<const double>::Create(
      kMatrixShape, kOffsets, kIndices,
      std::span<const double>(matrix_output_storage.data(), 8),
      asc::MemorySpace::kHost);
  auto overlapping_output = asc::CoordinateView<double, 1>::Create(
      kOutputCoordinates.data(), matrix_output_storage.data() + 6, kOutputShape,
      4, asc::MemorySpace::kHost);
  std::array<double, 5> independent_input_values = {1, -2, 3, 4, -1};
  int independent_input_identity = 0;
  const ExternalVector<const double> independent_input = {
      .data = independent_input_values.data(),
      .size = 5,
      .alias_identity = std::addressof(independent_input_identity),
  };
  ASC_SPARSE_TEST_CHECK(context, overlapping_matrix.ok());
  ASC_SPARSE_TEST_CHECK(context, overlapping_output.ok());
  const auto matrix_before = matrix_output_storage;
  const asc::Status matrix_overlap =
      asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *overlapping_matrix,
                independent_input, 0.0, *overlapping_output);
  ASC_SPARSE_TEST_CHECK(context, !matrix_overlap.ok());
  ASC_SPARSE_TEST_EQ(context, matrix_overlap.code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, matrix_output_storage, matrix_before);
}

void CheckDenseSpanOverlapTransactions(asc_sparse_test::TestContext& context) {
  constexpr std::array<asc::extent_t, 1> kInputShape = {5};
  constexpr std::array<asc::extent_t, 1> kOutputShape = {4};
  auto input_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kInputShape);
  auto output_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kOutputShape);
  std::array<double, 8> matrix_values{};
  auto matrix = MakeOracleMatrix(matrix_values);
  ASC_SPARSE_TEST_CHECK(context, input_mapping.ok());
  ASC_SPARSE_TEST_CHECK(context, output_mapping.ok());
  ASC_SPARSE_TEST_CHECK(context, matrix.ok());

  std::array<double, 6> output_inside_input = {1, -2, 3, 4, -1, 99};
  auto first_input = asc::DenseView<const double, 1>::Create(
      output_inside_input.data(), *input_mapping, asc::MemorySpace::kHost);
  auto first_output = asc::DenseView<double, 1>::Create(
      output_inside_input.data() + 1, *output_mapping, asc::MemorySpace::kHost);
  const auto first_before = output_inside_input;
  const asc::Status first_status =
      asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *matrix, *first_input,
                0.0, *first_output);
  ASC_SPARSE_TEST_CHECK(context, !first_status.ok());
  ASC_SPARSE_TEST_EQ(context, first_status.code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, output_inside_input, first_before);

  std::array<double, 6> output_before_input = {99, 1, -2, 3, 4, -1};
  auto second_input = asc::DenseView<const double, 1>::Create(
      output_before_input.data() + 1, *input_mapping, asc::MemorySpace::kHost);
  auto second_output = asc::DenseView<double, 1>::Create(
      output_before_input.data(), *output_mapping, asc::MemorySpace::kHost);
  const auto second_before = output_before_input;
  const asc::Status second_status =
      asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *matrix, *second_input,
                0.0, *second_output);
  ASC_SPARSE_TEST_CHECK(context, !second_status.ok());
  ASC_SPARSE_TEST_EQ(context, second_status.code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, output_before_input, second_before);

  std::array<double, 5> independent_input_values = {1, -2, 3, 4, -1};
  int independent_input_identity = 0;
  const ExternalVector<const double> independent_input = {
      .data = independent_input_values.data(),
      .size = 5,
      .alias_identity = std::addressof(independent_input_identity),
  };
  constexpr std::array<asc::extent_t, 2> kMatrixShape = {4, 5};
  constexpr std::array<asc::nnz_t, 5> kOffsets = {0, 2, 3, 6, 8};
  constexpr std::array<asc::index_t, 8> kIndices = {0, 3, 1, 0, 2, 4, 1, 4};

  std::array<double, 12> output_inside_matrix = {2,  -1, 4,  7,  3,  5,
                                                 -2, 6,  71, 72, 73, 74};
  auto first_matrix = asc::CsrView<const double>::Create(
      kMatrixShape, kOffsets, kIndices,
      std::span<const double>(output_inside_matrix.data(), 8),
      asc::MemorySpace::kHost);
  auto first_dense_output = asc::DenseView<double, 1>::Create(
      output_inside_matrix.data() + 6, *output_mapping,
      asc::MemorySpace::kHost);
  const auto first_matrix_before = output_inside_matrix;
  const asc::Status first_matrix_status =
      asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *first_matrix,
                independent_input, 0.0, *first_dense_output);
  ASC_SPARSE_TEST_CHECK(context, !first_matrix_status.ok());
  ASC_SPARSE_TEST_EQ(context, first_matrix_status.code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, output_inside_matrix, first_matrix_before);

  std::array<double, 10> output_before_matrix = {91, 92, 2, -1, 4,
                                                 7,  3,  5, -2, 6};
  auto second_matrix = asc::CsrView<const double>::Create(
      kMatrixShape, kOffsets, kIndices,
      std::span<const double>(output_before_matrix.data() + 2, 8),
      asc::MemorySpace::kHost);
  auto second_dense_output = asc::DenseView<double, 1>::Create(
      output_before_matrix.data(), *output_mapping, asc::MemorySpace::kHost);
  const auto second_matrix_before = output_before_matrix;
  const asc::Status second_matrix_status =
      asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *second_matrix,
                independent_input, 0.0, *second_dense_output);
  ASC_SPARSE_TEST_CHECK(context, !second_matrix_status.ok());
  ASC_SPARSE_TEST_EQ(context, second_matrix_status.code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, output_before_matrix, second_matrix_before);
}

void CheckAliasTokenContract(asc_sparse_test::TestContext& context) {
  std::array<double, 6> storage{};
  auto span =
      asc::AliasToken::FromAddressSpan(storage.data() + 1, 2 * sizeof(double));
  auto same_span =
      asc::AliasToken::FromAddressSpan(storage.data() + 1, 2 * sizeof(double));
  auto disjoint_span =
      asc::AliasToken::FromAddressSpan(storage.data() + 4, sizeof(double));
  auto zero_span = asc::AliasToken::FromAddressSpan(storage.data() + 1, 0);
  ASC_SPARSE_TEST_CHECK(context, span.ok());
  ASC_SPARSE_TEST_CHECK(context, same_span.ok());
  ASC_SPARSE_TEST_CHECK(context, disjoint_span.ok());
  ASC_SPARSE_TEST_CHECK(context, zero_span.ok());
  const asc::AliasToken inside =
      asc::AliasToken::FromIdentity(storage.data() + 2);
  const asc::AliasToken outside =
      asc::AliasToken::FromIdentity(storage.data() + 5);
  ASC_SPARSE_TEST_CHECK(context, asc::AliasTokensMayOverlap(*span, inside));
  ASC_SPARSE_TEST_CHECK(context, asc::AliasTokensMayOverlap(inside, *span));
  ASC_SPARSE_TEST_CHECK(context, asc::AliasTokensMayOverlap(*span, *same_span));
  ASC_SPARSE_TEST_CHECK(context, !asc::AliasTokensMayOverlap(*span, outside));
  ASC_SPARSE_TEST_CHECK(context,
                        !asc::AliasTokensMayOverlap(*span, *disjoint_span));
  ASC_SPARSE_TEST_CHECK(context,
                        !asc::AliasTokensMayOverlap(*zero_span, inside));

  constexpr std::array<asc::extent_t, 1> kParentShape = {6};
  auto parent_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kParentShape);
  auto parent = asc::DenseView<double, 1>::Create(
      storage.data(), *parent_mapping, asc::MemorySpace::kHost);
  auto first = parent->Subview(std::array<asc::index_t, 1>{0},
                               std::array<asc::extent_t, 1>{2});
  auto last = parent->Subview(std::array<asc::index_t, 1>{4},
                              std::array<asc::extent_t, 1>{2});
  ASC_SPARSE_TEST_CHECK(context, parent.ok());
  ASC_SPARSE_TEST_CHECK(context, first.ok());
  ASC_SPARSE_TEST_CHECK(context, last.ok());
  ASC_SPARSE_TEST_CHECK(
      context,
      asc::AliasTokensMayOverlap(first->alias_token(), last->alias_token()));
  ASC_SPARSE_TEST_CHECK(
      context,
      asc::MayAlias(*first, asc::AliasToken::FromIdentity(storage.data() + 5)));
}

void CheckDegenerateAndIeee(asc_sparse_test::TestContext& context) {
  int first_identity = 0;
  int second_identity = 0;

  constexpr std::array<asc::extent_t, 2> kZeroRowsShape = {0, 3};
  constexpr std::array<asc::nnz_t, 1> kZeroRowsOffsets = {0};
  auto zero_rows = asc::CsrView<const double>::Create(
      kZeroRowsShape, kZeroRowsOffsets, std::span<const asc::index_t>(),
      std::span<const double>(), asc::MemorySpace::kHost);
  std::array<double, 3> three_values = {1, 2, 3};
  const ExternalVector<const double> length_three = {
      .data = three_values.data(),
      .size = 3,
      .alias_identity = std::addressof(first_identity),
  };
  ExternalVector<double> length_zero = {
      .data = nullptr,
      .size = 0,
      .alias_identity = std::addressof(second_identity),
  };
  ASC_SPARSE_TEST_CHECK(context, zero_rows.ok());
  ASC_SPARSE_TEST_CHECK(
      context, asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *zero_rows,
                         length_three, 0.0, length_zero)
                   .ok());

  constexpr std::array<asc::extent_t, 2> kZeroColumnsShape = {3, 0};
  constexpr std::array<asc::nnz_t, 4> kZeroColumnsOffsets = {0, 0, 0, 0};
  auto zero_columns = asc::CsrView<const double>::Create(
      kZeroColumnsShape, kZeroColumnsOffsets, std::span<const asc::index_t>(),
      std::span<const double>(), asc::MemorySpace::kHost);
  const ExternalVector<const double> empty_input = {
      .data = nullptr,
      .size = 0,
      .alias_identity = std::addressof(first_identity),
  };
  std::array<double, 3> degenerate_output_values = {9, 8, 7};
  ExternalVector<double> degenerate_output = {
      .data = degenerate_output_values.data(),
      .size = 3,
      .alias_identity = std::addressof(second_identity),
      .poison_reads = true,
  };
  ASC_SPARSE_TEST_CHECK(context, zero_columns.ok());
  ASC_SPARSE_TEST_CHECK(
      context, asc::Spmv(asc::ExecutionContext::Serial(), 2.0, *zero_columns,
                         empty_input, 0.0, degenerate_output)
                   .ok());
  ASC_SPARSE_TEST_RANGE_EQ(context, degenerate_output_values,
                           (std::array<double, 3>{0, 0, 0}));

  constexpr std::array<asc::extent_t, 2> kTwoShape = {2, 2};
  constexpr std::array<asc::nnz_t, 3> kEmptyOffsets = {0, 0, 0};
  auto empty_matrix = asc::CsrView<const double>::Create(
      kTwoShape, kEmptyOffsets, std::span<const asc::index_t>(),
      std::span<const double>(), asc::MemorySpace::kHost);
  std::array<double, 2> two_input_values = {1, 1};
  std::array<double, 2> beta_output_values = {3, -4};
  const ExternalVector<const double> two_input = {
      .data = two_input_values.data(),
      .size = 2,
      .alias_identity = std::addressof(first_identity),
  };
  ExternalVector<double> beta_output = {
      .data = beta_output_values.data(),
      .size = 2,
      .alias_identity = std::addressof(second_identity),
  };
  ASC_SPARSE_TEST_CHECK(
      context, asc::Spmv(asc::ExecutionContext::Serial(), 5.0, *empty_matrix,
                         two_input, 2.0, beta_output)
                   .ok());
  ASC_SPARSE_TEST_RANGE_EQ(context, beta_output_values,
                           (std::array<double, 2>{6, -8}));

  constexpr std::array<asc::nnz_t, 3> kDiagonalOffsets = {0, 1, 2};
  constexpr std::array<asc::index_t, 2> kDiagonalIndices = {0, 1};
  std::array<double, 2> special_values = {
      std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::quiet_NaN()};
  auto special_matrix = asc::CsrView<const double>::Create(
      kTwoShape, kDiagonalOffsets, kDiagonalIndices,
      std::span<const double>(special_values), asc::MemorySpace::kHost);
  std::array<double, 2> special_output_values{};
  ExternalVector<double> special_output = {
      .data = special_output_values.data(),
      .size = 2,
      .alias_identity = std::addressof(second_identity),
  };
  ASC_SPARSE_TEST_CHECK(
      context, asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *special_matrix,
                         two_input, 0.0, special_output)
                   .ok());
  ASC_SPARSE_TEST_CHECK(context, std::isinf(special_output_values[0]));
  ASC_SPARSE_TEST_CHECK(context, std::isnan(special_output_values[1]));
}

void CheckValidationTransactions(asc_sparse_test::TestContext& context) {
  std::array<double, 8> matrix_values{};
  auto matrix = MakeOracleMatrix(matrix_values);
  std::array<double, 4> short_input_values = {1, 2, 3, 4};
  std::array<double, 4> output_values = {21, 22, 23, 24};
  int input_identity = 0;
  int output_identity = 0;
  const ExternalVector<const double> short_input = {
      .data = short_input_values.data(),
      .size = 4,
      .alias_identity = std::addressof(input_identity),
  };
  ExternalVector<double> output = {
      .data = output_values.data(),
      .size = 4,
      .alias_identity = std::addressof(output_identity),
  };
  const auto before = output_values;
  const asc::Status shape_status = asc::Spmv(
      asc::ExecutionContext::Serial(), 1.0, *matrix, short_input, 0.0, output);
  ASC_SPARSE_TEST_CHECK(context, !shape_status.ok());
  ASC_SPARSE_TEST_EQ(context, shape_status.code(), asc::ErrorCode::kShape);
  ASC_SPARSE_TEST_EQ(context, output_values, before);

  std::array<double, 5> input_values = {1, -2, 3, 4, -1};
  ExternalVector<const double> device_input = {
      .data = input_values.data(),
      .size = 5,
      .space = asc::MemorySpace::kDevice,
      .alias_identity = std::addressof(input_identity),
  };
  const asc::Status device_status = asc::Spmv(
      asc::ExecutionContext::Serial(), 1.0, *matrix, device_input, 0.0, output);
  ASC_SPARSE_TEST_CHECK(context, !device_status.ok());
  ASC_SPARSE_TEST_EQ(context, device_status.code(),
                     asc::ErrorCode::kMemoryAccess);
  ASC_SPARSE_TEST_EQ(context, output_values, before);

  const ExternalVector<const double> aliased_input = {
      .data = input_values.data(),
      .size = 5,
      .alias_identity = std::addressof(output_identity),
  };
  const asc::Status alias_status =
      asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *matrix, aliased_input,
                0.0, output);
  ASC_SPARSE_TEST_CHECK(context, !alias_status.ok());
  ASC_SPARSE_TEST_EQ(context, alias_status.code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_SPARSE_TEST_EQ(context, output_values, before);
}

}  // namespace

int main() {
  asc_sparse_test::TestContext context;
  CheckNumericalOracle<float>(context);
  CheckNumericalOracle<double>(context);
  CheckDenseAndExternalInteroperability(context);
  CheckCoordinateOutputCoverage(context);
  CheckAscViewOverlapTransactions(context);
  CheckDenseSpanOverlapTransactions(context);
  CheckAliasTokenContract(context);
  CheckDegenerateAndIeee(context);
  CheckValidationTransactions(context);
  return context.Finish();
}
