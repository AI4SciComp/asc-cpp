#include <cstddef>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/evaluate.h"
#include "asc/dense/view.h"
#include "m3_multi_tu.h"

std::size_t M3DenseEvaluationRank() {
  using View = asc::DenseView<double, 2>;
  using EvaluationResult = decltype(asc::Evaluate(
      std::declval<const asc::ExecutionContext&>(), 1.0, std::declval<View>()));
  static_assert(std::is_same_v<EvaluationResult, asc::Status>);
  return View::kRank;
}
