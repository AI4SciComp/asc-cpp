# Explicit mixed-precision general solve

This public consumer selects the pinned Reference provider and calls the actual
DSGESV/ZCGESV APIs. It checks the solution, INFO, ITER and selected precision,
then reuses returned working-precision LU factors for another RHS when the
documented conversion-range fallback occurs. Low-precision success preserves A
and does not certify its original values as factors. Both scalar types and
independent row/column layouts run; complex matrices have nonzero imaginary
entries. No private headers or source-tree paths are needed.

Configure against an installed ASC package and a separately installed, matching
admitted provider. The provider and Fortran runtimes are not bundled:

```sh
cmake -S . -B /tmp/asc-mixed-example \
  -DASCCpp_DIR=/path/to/relocated/asc/lib/cmake/ASCCpp \
  -DASC_CPP_LAPACK_ROOT=/path/to/matching/provider \
  '-DASC_CPP_LAPACK_RUNTIME_LIBRARIES=/path/to/libgfortran;/path/to/libquadmath'
cmake --build /tmp/asc-mixed-example
ctest --test-dir /tmp/asc-mixed-example --no-tests=error --output-on-failure
```

Use the compiler/runtime profile admitted by that package. ITER describes the
provider's stopping or fallback branch; it is not an independent certificate of
forward accuracy or conditioning. The maintained mathematical tests separately
check accuracy, including extreme scalar systems. This optional provider
consumer does not change the provider-free Dense or array-I/O dependencies.
