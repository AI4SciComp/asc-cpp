#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse.h"
#include "asc/sparse/blas.h"

template <typename Element>
struct ConsumerVector {
  Element* data;
  asc::extent_t size;
};

namespace asc {

template <typename Element>
struct ExpressionAdapter<::ConsumerVector<Element>> {
  using value_type = std::remove_const_t<Element>;
  static constexpr rank_t rank = 1;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;

  static constexpr std::array<extent_t, 1> Shape(
      const ::ConsumerVector<Element>& vector) noexcept {
    return {vector.size};
  }

  static constexpr value_type Read(
      const ::ConsumerVector<Element>& vector,
      std::span<const index_t, 1> coordinate) noexcept {
    return vector.data[static_cast<std::size_t>(coordinate[0])];
  }

  static constexpr bool MayAlias(const ::ConsumerVector<Element>& vector,
                                 AliasToken token) noexcept {
    return vector.data == token.identity();
  }
};

template <typename Element>
struct ExpressionPlacementAdapter<::ConsumerVector<Element>> {
  static constexpr MemorySpace Space(
      const ::ConsumerVector<Element>&) noexcept {
    return MemorySpace::kHost;
  }

  static constexpr ExpressionAliasMetadata Alias(
      const ::ConsumerVector<Element>& vector) noexcept {
    return ExpressionAliasMetadata(vector.data, vector.data,
                                   static_cast<std::size_t>(vector.size) *
                                       sizeof(std::remove_const_t<Element>));
  }
};

template <typename Element>
  requires(!std::is_const_v<Element>)
struct WritableExpressionAdapter<::ConsumerVector<Element>> {
  using value_type = std::remove_const_t<Element>;

  static constexpr std::array<extent_t, 1> Shape(
      const ::ConsumerVector<Element>& vector) noexcept {
    return {vector.size};
  }

  static constexpr ExpressionAliasMetadata Alias(
      const ::ConsumerVector<Element>& vector) noexcept {
    return ExpressionPlacementAdapter<::ConsumerVector<Element>>::Alias(vector);
  }

  static constexpr bool IsUnique(const ::ConsumerVector<Element>&) noexcept {
    return true;
  }

  static constexpr void Write(::ConsumerVector<Element>& vector,
                              std::span<const index_t, 1> coordinate,
                              value_type value) noexcept {
    vector.data[static_cast<std::size_t>(coordinate[0])] = value;
  }
};

}  // namespace asc

int main() {
  constexpr std::array<asc::extent_t, 2> kShape{2, 3};
  constexpr std::array<asc::nnz_t, 3> kOffsets{0, 2, 3};
  constexpr std::array<asc::index_t, 3> kIndices{0, 2, 1};
  constexpr std::array<double, 3> kValues{2.0, -1.0, 4.0};
  asc::HostMemoryResource resource;
  auto matrix = asc::CsrArray<double>::Create(resource, kShape, kOffsets,
                                              kIndices, kValues);
  if (!matrix.ok()) {
    return 1;
  }
  auto matrix_view = matrix->view();
  if (!matrix_view.ok()) {
    return 2;
  }
  std::array<double, 3> input_values{3.0, 5.0, 7.0};
  std::array<double, 2> output_values{1.0, 2.0};
  const ConsumerVector<const double> input{input_values.data(), 3};
  ConsumerVector<double> output{output_values.data(), 2};
  const asc::Status status = asc::Spmv(asc::ExecutionContext::Serial(), 2.0,
                                       *matrix_view, input, -1.0, output);
  if (!status.ok()) {
    return 3;
  }
  if (output_values != std::array<double, 2>{-3.0, 38.0}) {
    return 4;
  }

  constexpr std::array<asc::index_t, 2> kSparseIndices{0, 2};
  constexpr std::array<double, 2> kSparseValues{2.0, -1.0};
  std::array<double, 3> dense_values{3.0, 4.0, 5.0};
  auto sparse = asc::SparseBlasIndexedVectorView<const double>::Create(
      kSparseIndices.data(), kSparseValues.data(), 2, 3,
      {kSparseIndices.data(), sizeof(kSparseIndices), asc::MemorySpace::kHost},
      {kSparseValues.data(), sizeof(kSparseValues), asc::MemorySpace::kHost});
  auto dense = asc::SparseBlasVectorView<double>::Create(
      dense_values.data(), 3, 1,
      {dense_values.data(), sizeof(dense_values), asc::MemorySpace::kHost});
  if (!sparse.ok() || !dense.ok() ||
      !asc::SparseAxpy(asc::ExecutionContext::Serial(), 2.0, *sparse, *dense)
           .ok()) {
    return 5;
  }
  return dense_values == std::array<double, 3>{7.0, 4.0, 3.0} ? 0 : 6;
}
