#include <array>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/linalg.h"

struct ReadOnlyVector {};

namespace asc {

template <>
struct ExpressionAdapter<ReadOnlyVector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;
  static std::array<extent_t, 1> Shape(const ReadOnlyVector&) { return {1}; }
  static double Read(const ReadOnlyVector&, std::span<const index_t, 1>) {
    return 0.0;
  }
  static bool MayAlias(const ReadOnlyVector&, AliasToken) { return false; }
};

template <>
struct ExpressionPlacementAdapter<ReadOnlyVector> {
  static MemorySpace Space(const ReadOnlyVector&) { return MemorySpace::kHost; }
};

}  // namespace asc

static_assert(asc::PlacedReadableExpression<ReadOnlyVector>);
static_assert(!asc::WritableExpression<ReadOnlyVector>);

void MissingWritable(asc::CsrView<const double> matrix,
                     const ReadOnlyVector& input, ReadOnlyVector output) {
  static_cast<void>(asc::Spmv(asc::ExecutionContext::Serial(), 1.0, matrix,
                              input, 0.0, output));
}

int main() { return 0; }
