#include <array>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/linalg.h"
#include "m4_multi_tu.h"

struct M4RankTwoOutput {
  double* data;
};

namespace asc {

template <>
struct ExpressionAdapter<::M4RankTwoOutput> {
  using value_type = double;
  static constexpr rank_t rank = 2;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;

  static constexpr std::array<extent_t, 2> Shape(
      const ::M4RankTwoOutput&) noexcept {
    return {1, 1};
  }

  static double Read(const ::M4RankTwoOutput& output,
                     std::span<const index_t, 2>) {
    return output.data[0];
  }

  static bool MayAlias(const ::M4RankTwoOutput&, AliasToken) { return false; }
};

template <>
struct ExpressionPlacementAdapter<::M4RankTwoOutput> {
  static constexpr MemorySpace Space(const ::M4RankTwoOutput&) noexcept {
    return MemorySpace::kHost;
  }

  static ExpressionAliasMetadata Alias(
      const ::M4RankTwoOutput& output) noexcept {
    return ExpressionAliasMetadata(output.data, output.data, sizeof(double));
  }
};

template <>
struct WritableExpressionAdapter<::M4RankTwoOutput> {
  static constexpr std::array<extent_t, 2> Shape(
      const ::M4RankTwoOutput&) noexcept {
    return {1, 1};
  }

  static ExpressionAliasMetadata Alias(
      const ::M4RankTwoOutput& output) noexcept {
    return ExpressionPlacementAdapter<::M4RankTwoOutput>::Alias(output);
  }

  static constexpr bool IsUnique(const ::M4RankTwoOutput&) noexcept {
    return true;
  }

  static void Write(::M4RankTwoOutput& output, std::span<const index_t, 2>,
                    double value) {
    output.data[0] = value;
  }
};

}  // namespace asc

void AttemptWrongRank(const asc::ExecutionContext& context,
                      asc::CsrView<const double> matrix,
                      const M4CompileVector<const double>& input,
                      M4RankTwoOutput& output) {
  (void)asc::Spmv(context, 1.0, matrix, input, 0.0, output);
}

int main() { return 0; }
