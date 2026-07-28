#include "asc/core.h"
#include "asc/dense.h"
#include "asc/expression.h"
#include "asc/random.h"
#include "asc/random/dense.h"
#include "asc/random/sparse.h"
#include "asc/sparse.h"
#include "asc/utilities.h"

int AscM8ProviderFreeObservation() {
  const auto result = asc::GeneratePhilox4x32Word(1, 2, 3);
  return static_cast<int>(result & 0x7FFFFFFFU);
}
