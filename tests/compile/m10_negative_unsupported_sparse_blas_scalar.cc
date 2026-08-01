#include "asc/sparse/blas.h"

asc::SparseBlasVectorView<int> unsupported;

int main() { return unsupported.size() == 0 ? 0 : 1; }
