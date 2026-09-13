# Sparse archive and preview example

This installed C++20 consumer links only `ASC::sparse`. It reads an independent
COO text fixture, explicitly converts to CSR/CSC with a caller memory resource,
prints stored values, saves and reloads every kind as ASC text and binary, and
checks exact structure/value encoding. A stored negative zero survives. A
corrupt binary checksum is rejected without changing the existing CSR values.
No Dense, Random, CUDA or LAPACK target is required. Conversions are explicit;
reading does not densify or silently change storage kind.

After installing ASC with P03 support, configure this directory independently
with `ASCCpp_DIR` pointing to the installed CMake package:

```sh
cmake -S examples/sparse_array_io -B /external/sparse-archive-example \
  -DASCCpp_DIR=/external/asc-prefix/lib/cmake/ASCCpp
cmake --build /external/sparse-archive-example --parallel 2
ctest --test-dir /external/sparse-archive-example \
  --no-tests=error --output-on-failure
```

CTest uses its external build directory for output. Manual execution requires
`--truncate EXISTING_OUTPUT_DIRECTORY` and explicitly overwrites
`coo.asc.txt`, `csr.asc.txt`, `csc.asc.txt` and their `.asc.bin` counterparts.
File conveniences check write/flush/close but do not promise atomic replacement
or crash durability. The example's standard streams and filesystem paths may
allocate; they are caller conveniences, not the bounded codec allocation probe.
