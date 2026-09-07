#include "asc/expression/expression.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

#include "../allocation_observation.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "test_support.h"

namespace allocation_probe {

bool enabled = false;
std::size_t count = 0;

#if !defined(ASC_TEST_SANITIZER_OWNS_GLOBAL_ALLOCATOR)
void Record() noexcept {
  if (enabled) {
    ++count;
  }
}
#endif

}  // namespace allocation_probe

#if !defined(ASC_TEST_SANITIZER_OWNS_GLOBAL_ALLOCATOR)
void* operator new(std::size_t size) {
  allocation_probe::Record();
  if (void* pointer = std::malloc(size)) {
    return pointer;
  }
  std::abort();
}

void* operator new[](std::size_t size) {
  allocation_probe::Record();
  if (void* pointer = std::malloc(size)) {
    return pointer;
  }
  std::abort();
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t size) noexcept {
  static_cast<void>(size);
  std::free(pointer);
}
void operator delete[](void* pointer, std::size_t size) noexcept {
  static_cast<void>(size);
  std::free(pointer);
}
#endif

namespace {

struct ExternalVector {
  double* values;
  asc::extent_t extent;
  const void* alias_identity;
  int* read_count;
  int* alias_query_count;
};

struct ExternalMatrix {
  double* values;
  std::array<asc::extent_t, 2> shape;
  const void* alias_identity;
};

class OwningVector {
 public:
  explicit OwningVector(std::array<double, 3> values) : values_(values) {}

  OwningVector(const OwningVector&) = delete;
  OwningVector& operator=(const OwningVector&) = delete;
  OwningVector(OwningVector&&) noexcept = default;
  OwningVector& operator=(OwningVector&&) noexcept = default;
  ~OwningVector() = default;

  [[nodiscard]] const std::array<double, 3>& values() const noexcept {
    return values_;
  }

 private:
  std::array<double, 3> values_;
};

struct MissingAdapter {};
struct IncompleteExpression {};

}  // namespace

namespace asc {

template <>
struct ExpressionAdapter<::ExternalVector> {
  using value_type = double;
  static constexpr rank_t rank = 1;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructureFiltering;

  static std::array<extent_t, 1> Shape(const ::ExternalVector& expression) {
    return {expression.extent};
  }

  static double Read(const ::ExternalVector& expression,
                     std::span<const index_t, 1> indices) {
    if (expression.read_count != nullptr) {
      ++*expression.read_count;
    }
    return expression.values[static_cast<std::size_t>(indices[0])];
  }

  static bool MayAlias(const ::ExternalVector& expression, AliasToken token) {
    if (expression.alias_query_count != nullptr) {
      ++*expression.alias_query_count;
    }
    return expression.alias_identity == token.identity();
  }
};

template <>
struct ExpressionAdapter<::ExternalMatrix> {
  using value_type = double;
  static constexpr rank_t rank = 2;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 2> Shape(const ::ExternalMatrix& expression) {
    return expression.shape;
  }

  static double Read(const ::ExternalMatrix& expression,
                     std::span<const index_t, 2> indices) {
    const auto offset =
        static_cast<std::size_t>(indices[0] * expression.shape[1] + indices[1]);
    return expression.values[offset];
  }

  static bool MayAlias(const ::ExternalMatrix& expression, AliasToken token) {
    return expression.alias_identity == token.identity();
  }
};

template <>
struct ExpressionAdapter<::OwningVector> {
  using value_type = double;
  static constexpr rank_t rank = 1;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(const ::OwningVector& expression) {
    static_cast<void>(expression);
    return {3};
  }

  static double Read(const ::OwningVector& expression,
                     std::span<const index_t, 1> indices) {
    return expression.values()[static_cast<std::size_t>(indices[0])];
  }

  static bool MayAlias(const ::OwningVector& expression, AliasToken token) {
    static_cast<void>(expression);
    static_cast<void>(token);
    return false;
  }
};

template <>
struct ExpressionAdapter<::IncompleteExpression> {
  using value_type = double;
  static constexpr rank_t rank = 1;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(
      const ::IncompleteExpression& expression) {
    static_cast<void>(expression);
    return {1};
  }

  static bool MayAlias(const ::IncompleteExpression& expression,
                       AliasToken token) {
    static_cast<void>(expression);
    static_cast<void>(token);
    return false;
  }
};

}  // namespace asc

namespace {

template <typename T>
concept CanNegate =
    requires(T&& value) { asc::MakeNegate(std::forward<T>(value)); };

template <typename T, std::size_t Rank>
concept CanReadWithRank =
    requires(const T& expression, std::span<const asc::index_t, Rank> indices) {
      asc::ExpressionRead(expression, indices);
    };

static_assert(asc::ReadableExpression<ExternalVector>);
static_assert(asc::ReadableExpression<const ExternalVector&>);
static_assert(asc::ReadableExpression<OwningVector>);
static_assert(asc::ReadableExpression<int>);
static_assert(!asc::ReadableExpression<MissingAdapter>);
static_assert(!asc::ReadableExpression<IncompleteExpression>);
static_assert(!CanNegate<MissingAdapter>);
static_assert(CanReadWithRank<ExternalVector, 1>);
static_assert(!CanReadWithRank<ExternalVector, 0>);
static_assert(!CanReadWithRank<ExternalVector, 2>);

using LvalueNode = decltype(asc::MakeNegate(std::declval<ExternalVector&>()));
using LvalueStorage = std::remove_cvref_t<
    decltype(std::declval<const LvalueNode&>().operand_storage())>;
static_assert(std::is_same_v<LvalueStorage,
                             std::reference_wrapper<const ExternalVector>>);

using RvalueNode = decltype(asc::MakeNegate(std::declval<ExternalVector>()));
using RvalueStorage = std::remove_cvref_t<
    decltype(std::declval<const RvalueNode&>().operand_storage())>;
static_assert(std::is_same_v<RvalueStorage, ExternalVector>);

std::span<const asc::index_t, 1> Index(
    const std::array<asc::index_t, 1>& index) {
  return {index};
}

void CheckExternalProtocolAndOperations(
    asc_expression_test::TestContext& context) {
  double left_values[] = {1.0, 2.0, 3.0};
  double right_values[] = {4.0, 5.0, 6.0};
  int left_reads = 0;
  int right_reads = 0;
  int left_alias_queries = 0;
  int right_alias_queries = 0;
  int left_identity = 0;
  int right_identity = 0;
  ExternalVector left{left_values, 3, &left_identity, &left_reads,
                      &left_alias_queries};
  ExternalVector right{right_values, 3, &right_identity, &right_reads,
                       &right_alias_queries};
  constexpr std::array<asc::index_t, 1> kIndex = {1};

  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionShape(left),
                         (std::array<asc::extent_t, 1>{3}));
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionRead(left, Index(kIndex)),
                         2.0);
  ASC_EXPRESSION_TEST_EQ(context, left_reads, 1);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionOperationCategory(left),
                         asc::ExpressionOperation::kExternal);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionSparsityEffect(left),
                         asc::SparsityEffect::kStructureFiltering);

  auto negate = asc::MakeNegate(left);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionRead(negate, Index(kIndex)),
                         -2.0);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionOperationCategory(negate),
                         asc::ExpressionOperation::kNegate);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionSparsityEffect(negate),
                         asc::SparsityEffect::kStructurePreserving);

  auto add = asc::MakeAdd(left, right);
  auto subtract = asc::MakeSubtract(left, right);
  auto multiply = asc::MakeMultiply(left, right);
  ASC_EXPRESSION_TEST_CHECK(context, add.ok());
  ASC_EXPRESSION_TEST_CHECK(context, subtract.ok());
  ASC_EXPRESSION_TEST_CHECK(context, multiply.ok());
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionRead(*add, Index(kIndex)),
                         7.0);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionRead(*subtract, Index(kIndex)),
                         -3.0);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionRead(*multiply, Index(kIndex)),
                         10.0);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionOperationCategory(*add),
                         asc::ExpressionOperation::kAdd);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionOperationCategory(*subtract),
                         asc::ExpressionOperation::kSubtract);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionOperationCategory(*multiply),
                         asc::ExpressionOperation::kMultiply);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionSparsityEffect(*add),
                         asc::SparsityEffect::kStructureUnion);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionSparsityEffect(*subtract),
                         asc::SparsityEffect::kStructureUnion);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionSparsityEffect(*multiply),
                         asc::SparsityEffect::kStructureIntersection);

  const asc::AliasToken left_token(&left_identity);
  const asc::AliasToken right_token(&right_identity);
  const int other_identity = 0;
  const asc::AliasToken other_token(&other_identity);
  ASC_EXPRESSION_TEST_EQ(context, left_token.identity(),
                         static_cast<const void*>(&left_identity));
  ASC_EXPRESSION_TEST_CHECK(context, asc::MayAlias(*add, left_token));
  ASC_EXPRESSION_TEST_CHECK(context, asc::MayAlias(*add, right_token));
  ASC_EXPRESSION_TEST_CHECK(context, !asc::MayAlias(*add, other_token));
  ASC_EXPRESSION_TEST_CHECK(context, left_alias_queries > 0);
  ASC_EXPRESSION_TEST_CHECK(context, right_alias_queries > 0);
}

void CheckShapeAndScalarRules(asc_expression_test::TestContext& context) {
  double three_values[] = {1.0, 2.0, 3.0};
  double two_values[] = {4.0, 5.0};
  double matrix_values[] = {1.0, 2.0, 3.0, 4.0};
  ExternalVector three{three_values, 3, nullptr, nullptr, nullptr};
  ExternalVector two{two_values, 2, nullptr, nullptr, nullptr};
  ExternalMatrix matrix{matrix_values, {2, 2}, nullptr};
  constexpr std::array<asc::index_t, 1> kIndex = {2};

  ASC_EXPRESSION_TEST_EQ(context, asc::MakeAdd(three, two).status().code(),
                         asc::ErrorCode::kShape);
  ASC_EXPRESSION_TEST_EQ(context,
                         asc::MakeMultiply(three, matrix).status().code(),
                         asc::ErrorCode::kShape);

  auto scalar_right = asc::MakeAdd(three, 2.0);
  auto scalar_left = asc::MakeSubtract(10.0, three);
  auto scalar_multiply = asc::MakeMultiply(three, 3.0);
  ASC_EXPRESSION_TEST_CHECK(context, scalar_right.ok());
  ASC_EXPRESSION_TEST_CHECK(context, scalar_left.ok());
  ASC_EXPRESSION_TEST_CHECK(context, scalar_multiply.ok());
  static_assert(asc::kExpressionRank<decltype(*scalar_right)> == 1);
  static_assert(
      std::is_same_v<asc::ExpressionValue<decltype(*scalar_right)>, double>);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionShape(*scalar_right),
                         (std::array<asc::extent_t, 1>{3}));
  ASC_EXPRESSION_TEST_EQ(
      context, asc::ExpressionRead(*scalar_right, Index(kIndex)), 5.0);
  ASC_EXPRESSION_TEST_EQ(context,
                         asc::ExpressionRead(*scalar_left, Index(kIndex)), 7.0);
  ASC_EXPRESSION_TEST_EQ(
      context, asc::ExpressionRead(*scalar_multiply, Index(kIndex)), 9.0);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionSparsityEffect(*scalar_right),
                         asc::SparsityEffect::kValueDependent);
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionSparsityEffect(*scalar_left),
                         asc::SparsityEffect::kValueDependent);
  ASC_EXPRESSION_TEST_EQ(context,
                         asc::ExpressionSparsityEffect(*scalar_multiply),
                         asc::SparsityEffect::kValueDependent);

  auto scalars = asc::MakeAdd(2, 3.5);
  ASC_EXPRESSION_TEST_CHECK(context, scalars.ok());
  static_assert(asc::kExpressionRank<decltype(*scalars)> == 0);
  ASC_EXPRESSION_TEST_CHECK(context, asc::ExpressionShape(*scalars).empty());
  ASC_EXPRESSION_TEST_EQ(
      context,
      asc::ExpressionRead(*scalars, std::span<const asc::index_t, 0>()), 5.5);
  ASC_EXPRESSION_TEST_CHECK(context,
                            !asc::MayAlias(*scalars, asc::AliasToken(&three)));
}

void CheckCaptureAndLifetime(asc_expression_test::TestContext& context) {
  double first_values[] = {1.0, 2.0, 3.0};
  double second_values[] = {7.0, 8.0, 9.0};
  ExternalVector lvalue{first_values, 3, nullptr, nullptr, nullptr};
  constexpr std::array<asc::index_t, 1> kIndex = {0};

  double scalar = 3.0;
  auto scalar_node = asc::MakeNegate(scalar);
  scalar = 9.0;
  ASC_EXPRESSION_TEST_EQ(context, scalar, 9.0);
  ASC_EXPRESSION_TEST_EQ(
      context,
      asc::ExpressionRead(scalar_node, std::span<const asc::index_t, 0>()),
      -3.0);

  auto lvalue_node = asc::MakeNegate(lvalue);
  lvalue.values = second_values;
  ASC_EXPRESSION_TEST_EQ(context,
                         asc::ExpressionRead(lvalue_node, Index(kIndex)), -7.0);

  auto copied_view_node = asc::MakeNegate(
      ExternalVector{first_values, 3, nullptr, nullptr, nullptr});
  first_values[0] = 11.0;
  ASC_EXPRESSION_TEST_EQ(
      context, asc::ExpressionRead(copied_view_node, Index(kIndex)), -11.0);

  auto owning_result =
      asc::MakeAdd(OwningVector(std::array<double, 3>{2.0, 4.0, 6.0}), 3.0);
  ASC_EXPRESSION_TEST_CHECK(context, owning_result.ok());
  auto owning_node = std::move(*owning_result);
  ASC_EXPRESSION_TEST_EQ(context,
                         asc::ExpressionRead(owning_node, Index(kIndex)), 5.0);

  auto nested_result = asc::MakeMultiply(
      asc::MakeNegate(OwningVector(std::array<double, 3>{1.0, 2.0, 3.0})), 2.0);
  ASC_EXPRESSION_TEST_CHECK(context, nested_result.ok());
  auto nested_node = std::move(*nested_result);
  auto moved_node = std::move(nested_node);
  ASC_EXPRESSION_TEST_EQ(context,
                         asc::ExpressionRead(moved_node, Index(kIndex)), -2.0);
}

void CheckConstructionHasNoEffects(asc_expression_test::TestContext& context) {
  double left_values[] = {1.0, 2.0, 3.0};
  double right_values[] = {4.0, 5.0, 6.0};
  const std::array<double, 3> destination = {9.0, 8.0, 7.0};
  const std::array<double, 3> expected_destination = destination;
  int left_reads = 0;
  int right_reads = 0;
  int alias_queries = 0;
  ExternalVector left{left_values, 3, left_values, &left_reads, &alias_queries};
  ExternalVector right{right_values, 3, right_values, &right_reads,
                       &alias_queries};

  allocation_probe::count = 0;
  allocation_probe::enabled = true;
  auto expression = asc::MakeAdd(left, right);
  allocation_probe::enabled = false;

  ASC_EXPRESSION_TEST_CHECK(context, expression.ok());
  ASC_EXPRESSION_TEST_CHECK(context, asc_test::ProcessAllocationCountMatches(
                                         allocation_probe::count, 0));
  ASC_EXPRESSION_TEST_EQ(context, left_reads, 0);
  ASC_EXPRESSION_TEST_EQ(context, right_reads, 0);
  ASC_EXPRESSION_TEST_EQ(context, alias_queries, 0);
  ASC_EXPRESSION_TEST_EQ(context, destination, expected_destination);
}

}  // namespace

int main() {
  asc_expression_test::TestContext context;
  CheckExternalProtocolAndOperations(context);
  CheckShapeAndScalarRules(context);
  CheckCaptureAndLifetime(context);
  CheckConstructionHasNoEffects(context);
  return context.Finish();
}
