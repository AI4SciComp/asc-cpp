#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse.h"

namespace m4_contract {

struct Vector {
  double* data = nullptr;
  std::array<asc::extent_t, 1> shape = {0};
  const void* identity = nullptr;
};

}  // namespace m4_contract

namespace asc {

template <>
struct ExpressionAdapter<m4_contract::Vector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(
      const m4_contract::Vector& vector) noexcept {
    return vector.shape;
  }
  static double Read(const m4_contract::Vector& vector,
                     std::span<const index_t, 1> coordinate) noexcept {
    return vector.data[static_cast<std::size_t>(coordinate[0])];
  }
  static bool MayAlias(const m4_contract::Vector& vector,
                       AliasToken alias) noexcept {
    return AliasToken::FromIdentity(vector.identity) == alias;
  }
};

template <>
struct ExpressionPlacementAdapter<m4_contract::Vector> {
  static MemorySpace Space(const m4_contract::Vector&) noexcept {
    return MemorySpace::kHost;
  }
};

template <>
struct WritableExpressionAdapter<m4_contract::Vector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;

  static std::array<extent_t, 1> Shape(
      const m4_contract::Vector& vector) noexcept {
    return vector.shape;
  }
  static AliasToken Alias(const m4_contract::Vector& vector) noexcept {
    return AliasToken::FromIdentity(vector.identity);
  }
  static void Write(m4_contract::Vector& vector,
                    std::span<const index_t, 1> coordinate,
                    double value) noexcept {
    vector.data[static_cast<std::size_t>(coordinate[0])] = value;
  }
};

}  // namespace asc

static_assert(asc::SparseElement<int>);
static_assert(asc::SparseElement<float>);
static_assert(asc::SparseElement<const double>);
static_assert(!asc::SparseElement<bool>);
static_assert(!asc::SparseElement<volatile double>);
static_assert(asc::SparseExtents<asc::Extents<>>);
static_assert(asc::SparseExtents<asc::Extents<asc::kDynamicExtent, 3>>);
static_assert(!asc::SparseExtents<const asc::Extents<1>>);
static_assert(asc::ReadableExpression<asc::CoordinateView<const double, 2>>);
static_assert(asc::WritableExpression<asc::CoordinateView<double, 2>>);
static_assert(!asc::WritableExpression<const asc::CoordinateView<double, 2>>);
static_assert(!asc::WritableExpression<asc::CoordinateView<const double, 2>>);
static_assert(asc::ReadableExpression<asc::CsrView<const double>>);
static_assert(asc::WritableExpression<asc::CscView<double>>);
static_assert(!asc::WritableExpression<const m4_contract::Vector>);
static_assert(asc::PlacedReadableExpression<m4_contract::Vector>);
static_assert(asc::PlacedReadableExpression<const m4_contract::Vector>);
static_assert(asc::WritableExpression<m4_contract::Vector>);
static_assert(!asc::ReadableExpression<volatile m4_contract::Vector>);
static_assert(!asc::ReadableExpression<const volatile m4_contract::Vector>);
static_assert(!asc::PlacedReadableExpression<volatile m4_contract::Vector>);
static_assert(
    !asc::PlacedReadableExpression<const volatile m4_contract::Vector>);
static_assert(!asc::WritableExpression<volatile m4_contract::Vector>);
static_assert(!asc::WritableExpression<const volatile m4_contract::Vector>);
static_assert(!std::copy_constructible<
              asc::CoordinateBuilder<double, asc::Extents<2, 3>>>);
static_assert(!std::copy_constructible<asc::CsrArray<double>>);
static_assert(std::move_constructible<asc::CscArray<double>>);
static_assert(std::is_trivially_copyable_v<asc::CoordinateView<double, 2>>);
static_assert(std::is_trivially_copyable_v<asc::CsrView<const double>>);
static_assert(asc::SparseLinearAlgebraScalar<float>);
static_assert(asc::SparseLinearAlgebraScalar<double>);
static_assert(!asc::SparseLinearAlgebraScalar<int>);

int main() {
  constexpr std::array<asc::extent_t, 2> kShape = {1, 1};
  constexpr std::array<asc::nnz_t, 2> kOffsets = {0, 1};
  constexpr std::array<asc::index_t, 1> kIndices = {0};
  std::array<double, 1> values = {2.0};
  const auto view = asc::CsrView<double>::Create(
      kShape, kOffsets, kIndices, values, asc::MemorySpace::kHost);
  if (!view.ok()) {
    return 1;
  }
  return asc::kExpressionRank<decltype(*view)> == 2 &&
                 asc::kExpressionSparsityEffect<decltype(*view)> ==
                     asc::SparsityEffect::kStructurePreserving
             ? 0
             : 1;
}
