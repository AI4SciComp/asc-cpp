# Installed LU and array-I/O example

This deliberately small double-precision application demonstrates independent
native and explicitly enabled Reference-LAPACK routes. It is not a full-family
or full-profile LAPACK coverage claim.

Configure outside the ASC source tree, against an installed prefix:

```sh
cmake -S examples/lapack_array_io -B /absolute/external/example-build \
  -DCMAKE_PREFIX_PATH=/absolute/installed/asc-prefix \
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
  -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
cmake --build /absolute/external/example-build
ctest --test-dir /absolute/external/example-build --no-tests=error \
  --output-on-failure
```

Only `ASC::dense` is required. The CMake project has C++ as its sole language.
To include the reference route, use a separately prepared provider-enabled ASC
installation and explicitly add `-DASC_CPP_EXAMPLES_ENABLE_LAPACK=ON`. That
configuration additionally requests and links `ASC::dense_lapack`; it does not
download or compile a provider. The application queries each reference
operation and supplies bounded, aligned workspace bytes itself.

The command-line shape is:

```text
asc_cpp_example_lapack_array_io native A.asc B.asc corrupt.asc X.ascb --truncate
asc_cpp_example_lapack_array_io reference A.asc B.asc corrupt.asc X.ascb --truncate
```

`--truncate` explicitly permits creation or destructive truncation of `X.ascb`.
Do not select an input path as the output. No atomic replacement or crash
durability is promised. CTest uses distinct output paths in its external build
directory; inputs are copied from the three committed fixtures.

The input matrix has conventional rows `(0,2,1)`, `(1,-2,0)`, `(3,1,4)`.
The two independently derived solution columns are `(1,2,-1)` and `(-2,0,3)`.
The application preserves original owners, factors a clone once, solves both
right-hand sides together and repeats that solve from preserved B with the same
factor, checks a scaled infinity-norm residual
and the known solution, prints numerical/provider diagnostics and bounded X
values, then saves and reloads X with exact binary value-bit comparison.
A small residual alone is not a forward-accuracy or conditioning certificate.

The corrupt fixture contains every value but an invalid final terminator.
Its staged read must leave the existing solution unchanged. A separate singular
matrix demonstrates a numerical failure and rejection of a reusable successful
factor, never a fabricated solution. Every I/O, descriptor, query, numerical,
flush and checked-close status is handled. No test-private support is used.
