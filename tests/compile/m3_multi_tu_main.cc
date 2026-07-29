#include "m3_multi_tu.h"

int main() {
  return M3DenseOwnerRank() == 2 && M3DenseEvaluationRank() == 2 ? 0 : 1;
}
