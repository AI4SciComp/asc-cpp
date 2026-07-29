#include "m5_random_facets_odr.h"

bool CheckM5OdrSparse() {
  asc::HostMemoryResource resource;
  auto generated = GenerateM5OdrSparse(resource, 7);
  return generated.ok() && generated->array.nnz() == 2 &&
         generated->next_structure_offset == 15 &&
         generated->next_value_offset == 9;
}
