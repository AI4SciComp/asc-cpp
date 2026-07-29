#include <type_traits>

#include "asc/dense.h"

using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

static_assert(std::is_nothrow_move_constructible_v<
              asc::DenseArray<double, MatrixExtents>>);
static_assert(std::is_trivially_copyable_v<asc::DenseView<double, 2>>);

int main() { return 0; }
