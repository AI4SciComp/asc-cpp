#include <cstddef>

#include "asc/sparse/compressed.h"

using M4InvalidCompressedView =
    asc::CompressedSparseView<double,
                              static_cast<asc::SparseCompressedFormat>(255)>;

int main() { return static_cast<int>(sizeof(M4InvalidCompressedView)); }
