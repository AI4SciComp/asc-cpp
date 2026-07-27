#include "m4_sparse_multi_tu.h"

int main() {
  return M4CoordinateValue() == 7.0 && M4SpmvChecksum() == 23.0 ? 0 : 1;
}
