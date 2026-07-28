#include "m4_multi_tu.h"

int main() {
  if (M4CoordinateFromFirstTranslationUnit() != 5.0) {
    return 1;
  }
  if (M4SpmvFromSecondTranslationUnit() != 11.0) {
    return 2;
  }
  return 0;
}
