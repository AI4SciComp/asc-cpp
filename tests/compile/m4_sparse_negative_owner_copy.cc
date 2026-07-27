#include "asc/sparse/compressed.h"

void CopyOwner(asc::CsrArray<double> owner) {
  asc::CsrArray<double> copy(owner);
  static_cast<void>(copy);
}

int main() { return 0; }
