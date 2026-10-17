# Experimental robust PPSVX installed example

This C++20 example uses only installed public headers and `ASC::dense_lapack`.
It constructs a strictly diagonally dominant Hermitian positive-definite
system with known solution (1, 2), solves it in N/E modes, inspects diagnostics,
then reuses the returned factors for a new RHS with solution (2, 4). It covers
four scalar types, both triangles and independent packed/full layouts.

First build and install ASC with the options and independently prepared provider
described in [the capability contract](../../docs/contracts/robust-ppsvx.md).
The install may be relocated before configuring this example:

```sh
cmake -S examples/robust_ppsvx -B /tmp/asc-robust-example \
  -DCMAKE_CXX_COMPILER=g++-11 \
  -DASCCpp_DIR=/path/to/relocated-asc/lib/cmake/ASCCpp \
  -DASC_CPP_LAPACK_ROOT=/path/to/prepared-provider/prefix \
  '-DASC_CPP_LAPACK_RUNTIME_LIBRARIES=/absolute/libgfortran.so.5;/absolute/libquadmath.so.0'
cmake --build /tmp/asc-robust-example
ctest --test-dir /tmp/asc-robust-example --no-tests=error --output-on-failure
```

Use the actual runtime files and package directory of the prepared installation.
Their contents must match its attested dependency identity. ASC does not bundle
these files, search a machine-specific evidence directory or enable Fortran in
the consumer. Copying this example alone out of the repository is supported.

The named implementation is opt-in and experimental. Its success does not
resolve the separately retained Reference PPSVX extreme-value failures.
