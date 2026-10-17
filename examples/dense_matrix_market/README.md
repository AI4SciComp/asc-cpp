# Dense Matrix Market interchange example

This standalone C++20 consumer requests only `ASC::dense`. It rejects Sparse,
Random, LAPACK, CUDA and unrelated utility imports, and checks that no foreign
language was enabled. It reads a hand-derived Hermitian array fixture, verifies
all four values independently, writes the lower triangle, reloads into the other
physical layout, and prints a bounded preview. A truncated payload demonstrates
destination rollback with explicit typed staging; source progress is not undone.

After installing ASC with Matrix Market support:

```sh
cmake -S examples/dense_matrix_market -B /external/dense-matrix-market-example \
  -DASCCpp_DIR=/external/asc-prefix/lib/cmake/ASCCpp
cmake --build /external/dense-matrix-market-example --parallel 2
ctest --test-dir /external/dense-matrix-market-example \
  --no-tests=error --output-on-failure
```

CTest writes only in its external build directory. Manual execution requires
`--truncate EXISTING_OUTPUT_DIRECTORY` and explicitly overwrites
`dense-hermitian.mtx`. File conveniences check write, flush and close but do not
promise atomic replacement or crash durability. Standard streams and filesystem
paths in this example may allocate; the codec's bounded allocation tests are
separate. See the [interchange guide](../../docs/matrix-market.md) for limits,
conversion rules and the distinction from native ASC archives.
