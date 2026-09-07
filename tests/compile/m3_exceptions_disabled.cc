#include <type_traits>

#include "asc/core/extents.h"
#include "asc/core/types.h"
#include "asc/dense.h"  // NOLINT(misc-include-cleaner): umbrella-header probe
#include "asc/dense/array.h"
#include "asc/dense/view.h"

using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

static_assert(std::is_nothrow_move_constructible_v<
              asc::DenseArray<double, MatrixExtents>>);
static_assert(std::is_trivially_copyable_v<asc::DenseView<double, 2>>);

int main() { return 0; }
