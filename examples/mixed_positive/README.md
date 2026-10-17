# Installed mixed positive-definite solve example

This maintained example selects DSPOSV/ZCPOSV through the optional exported
`ASC::dense_lapack` target. It uses independent well-conditioned real and complex
Hermitian systems, both triangles, all independent A/B/X layouts, and ordinary
low success plus the prescribed conversion-range working fallback. It checks
INFO/ITER and solutions, then reuses the returned working Cholesky factors with
`LapackCholeskyFactorView` and `Potrs` for a second RHS. Low scratch factors are
not misrepresented as returned working factors.

Configure with an installed ASC prefix and an independently prepared compatible
pinned provider, using the documented `ASC_CPP_LAPACK_ROOT` and runtime settings:

```sh
cmake -S examples/mixed_positive -B /tmp/asc-mixed-positive \
  -DCMAKE_PREFIX_PATH=/path/to/relocated-asc \
  -DASC_CPP_LAPACK_ROOT=/path/to/prepared-reference-lapack
cmake --build /tmp/asc-mixed-positive
ctest --test-dir /tmp/asc-mixed-positive --no-tests=error --output-on-failure
```

The provider and Fortran runtimes are external dependencies of this explicitly
selected optional facet. No source-tree/private headers or historical evidence
paths are used by the consumer. The separate provider-free native/I/O consumers
remain independent. This ordinary example grants no acceptance to the retained
extreme-value failures of other Reference families.
