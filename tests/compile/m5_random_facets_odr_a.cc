#include <array>

#include "m5_random_facets_odr.h"

bool CheckM5OdrDense() {
  std::array<float, 4> storage{};
  auto next = FillM5OdrDense(storage, 7);
  return next.ok() && *next == 11;
}
