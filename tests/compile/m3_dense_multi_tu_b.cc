#include <array>

#include "asc/core/types.h"
#include "m3_dense_multi_tu.h"

int M3DenseMultiTuB() {
  const std::array<asc::extent_t, 2> shape = {3, 2};
  const std::array<asc::index_t, 2> coordinate = {2, 1};
  return M3DenseOffset(shape, coordinate) == 5 ? 13 : -1;
}
