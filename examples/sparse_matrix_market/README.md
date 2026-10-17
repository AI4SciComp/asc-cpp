# Sparse Matrix Market interchange example

This standalone C++20 consumer requests only `ASC::sparse` and rejects unrelated
Dense, Random, LAPACK and CUDA imports. It uses public headers only. A hand-derived
complex Hermitian coordinate fixture is independently assembled into each of
COO, CSR and CSC with an explicit host memory resource. Each owner is written in
lower-half Hermitian form, reloaded in its requested storage format, checked
against independent structure/value expectations, and previewed. No Dense
dependency, implicit conversion or densification is involved.

The rollback example supplies paired coordinate and typed value staging. A
malformed Hermitian diagonal after two valid records is rejected without changing
the existing CSR structure or any destination value bit. Staging and source
progress are not rolled back.

After installing ASC with Matrix Market support:

```sh
cmake -S examples/sparse_matrix_market -B /external/sparse-matrix-market-example \
  -DASCCpp_DIR=/external/asc-prefix/lib/cmake/ASCCpp
cmake --build /external/sparse-matrix-market-example --parallel 2
ctest --test-dir /external/sparse-matrix-market-example \
  --no-tests=error --output-on-failure
```

CTest writes only in its external build directory. Manual execution requires
`--truncate EXISTING_OUTPUT_DIRECTORY`, explicitly overwriting `coo.mtx`,
`csr.mtx` and `csc.mtx`. File conveniences check write/flush/close, but do not
promise atomic replacement or crash durability. The example's standard streams
and filesystem paths may allocate; this is an installed-use example, not a
bounded codec allocation probe. Matrix Market symmetry compression preserves
represented values, not every possible asymmetric stored-zero pattern; use
native ASC archives when exact archival structure preservation is required.
