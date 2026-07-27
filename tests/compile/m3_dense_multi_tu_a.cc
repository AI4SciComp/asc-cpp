#include <array>

#include "asc/core/types.h"
#include "m3_dense_multi_tu.h"

int M3DenseMultiTuA() {
  const std::array<asc::extent_t, 2> shape = {2, 3};
  const std::array<asc::index_t, 2> coordinate = {1, 2};
  return M3DenseOffset(shape, coordinate) == 5 ? 11 : -1;
}
