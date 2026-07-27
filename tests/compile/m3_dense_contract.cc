#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense.h"
#include "asc/expression/expression.h"

namespace {

using Extents = asc::Extents<2, asc::kDynamicExtent>;
using Owner = asc::DenseArray<double, Extents>;

struct StructuralExtentsImpostor {
  [[maybe_unused]] static constexpr std::size_t kRank = 2;

  [[nodiscard]] std::span<const asc::extent_t, 2> values() const {
    return values_;
  }
  [[nodiscard]] asc::extent_t logical_size() const { return 4; }

  std::array<asc::extent_t, 2> values_ = {2, 2};
};

static_assert(asc::DenseExtents<Extents>);
static_assert(!asc::DenseExtents<StructuralExtentsImpostor>);
static_assert(asc::DenseElement<int>);
static_assert(asc::DenseElement<const double>);
static_assert(!asc::DenseElement<bool>);
static_assert(!asc::DenseElement<const bool>);
static_assert(!asc::DenseElement<volatile double>);
static_assert(asc::DenseLinearAlgebraScalar<float>);
static_assert(asc::DenseLinearAlgebraScalar<double>);
static_assert(!asc::DenseLinearAlgebraScalar<const double>);
static_assert(!asc::DenseLinearAlgebraScalar<int>);
static_assert(asc::DenseLayoutMapping<0>::rank() == 0);
static_assert(asc::DenseLayoutMapping<2>::rank() == 2);
static_assert(std::is_move_constructible_v<Owner>);
static_assert(std::is_move_assignable_v<Owner>);
static_assert(!std::is_copy_constructible_v<Owner>);
static_assert(!std::is_copy_assignable_v<Owner>);
static_assert(std::is_trivially_copyable_v<asc::DenseView<double, 2>>);
static_assert(std::is_trivially_copyable_v<asc::DenseView<const double, 2>>);
static_assert(std::is_constructible_v<asc::DenseView<const double, 2>,
                                      asc::DenseView<double, 2>>);
static_assert(!std::is_constructible_v<asc::DenseView<double, 2>,
                                       asc::DenseView<const double, 2>>);
static_assert(asc::ReadableExpression<asc::DenseView<double, 2>>);
static_assert(asc::kExpressionRank<asc::DenseView<double, 2>> == 2);
static_assert(
    std::same_as<asc::ExpressionValue<asc::DenseView<double, 2>>, double>);

static_assert(requires(const asc::ExecutionContext& execution,
                       asc::DenseView<const double, 1> const_vector,
                       asc::DenseView<double, 1> vector,
                       asc::DenseView<const double, 2> const_matrix,
                       asc::DenseView<double, 2> matrix) {
  { asc::Copy(execution, const_vector, vector) } -> std::same_as<asc::Status>;
  { asc::Copy(execution, vector, vector) } -> std::same_as<asc::Status>;
  { asc::Scal(execution, 2.0, vector) } -> std::same_as<asc::Status>;
  {
    asc::Axpy(execution, 2.0, const_vector, vector)
  } -> std::same_as<asc::Status>;
  { asc::Axpy(execution, 2.0, vector, vector) } -> std::same_as<asc::Status>;
  {
    asc::Dot(execution, const_vector, const_vector)
  } -> std::same_as<asc::Result<double>>;
  { asc::Dot(execution, vector, vector) } -> std::same_as<asc::Result<double>>;
  { asc::Nrm2(execution, const_vector) } -> std::same_as<asc::Result<double>>;
  { asc::Nrm2(execution, vector) } -> std::same_as<asc::Result<double>>;
  {
    asc::Gemv(execution, asc::MatrixOperation::kNone, 1.0, const_matrix,
              const_vector, 0.0, vector)
  } -> std::same_as<asc::Status>;
  {
    asc::Gemv(execution, asc::MatrixOperation::kNone, 1.0, matrix, vector, 0.0,
              vector)
  } -> std::same_as<asc::Status>;
  {
    asc::Gemm(execution, asc::MatrixOperation::kNone,
              asc::MatrixOperation::kTranspose, 1.0, const_matrix, const_matrix,
              0.0, matrix)
  } -> std::same_as<asc::Status>;
  {
    asc::Gemm(execution, asc::MatrixOperation::kNone,
              asc::MatrixOperation::kTranspose, 1.0, matrix, matrix, 0.0,
              matrix)
  } -> std::same_as<asc::Status>;
  {
    asc::Evaluate(execution, matrix, const_matrix)
  } -> std::same_as<asc::Status>;
  {
    asc::ReduceSum(execution, const_matrix)
  } -> std::same_as<asc::Result<double>>;
  {
    asc::ReduceMin(execution, const_matrix)
  } -> std::same_as<asc::Result<double>>;
  {
    asc::ReduceMax(execution, const_matrix)
  } -> std::same_as<asc::Result<double>>;
});

}  // namespace

int main() {
  constexpr std::array<asc::extent_t, 2> kShape = {2, 3};
  const auto mapping =
      asc::DenseLayoutMapping<2>::Create(asc::LayoutLeft{}, kShape);
  return mapping.ok() && mapping->is_unique() && mapping->is_exhaustive() ? 0
                                                                          : 1;
}
