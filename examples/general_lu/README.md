# Installed general LU mode example

This example uses the existing checked optional Reference APIs for S/D/C/Z
GETRF, GETRF2, GETF2, GETRS, GETRI and GESV. It does not add another numerical
implementation. Independent padded A/RHS layouts, N/T/C solves, repeated solves
from returned factors, both inverse products and sequential-pivot reconstruction
are checked using a fixed well-conditioned fixture with nonzero complex parts.
The same public calls also run concurrently with independent caller state.

Linking uses `ASC::dense_lapack` and the system thread target. The externally
prepared pinned provider must match the installed ASC integer ABI. No private
headers or source/evidence paths are part of the consumer:

```sh
cmake -S examples/general_lu -B /tmp/asc-general-lu \
  -DCMAKE_PREFIX_PATH=/path/to/relocated-asc \
  -DASC_CPP_LAPACK_ROOT=/path/to/prepared-reference-lapack
cmake --build /tmp/asc-general-lu
ctest --test-dir /tmp/asc-general-lu --no-tests=error --output-on-failure
```

Supply the explicit Fortran runtime settings described in the provider
installation documentation. This optional-provider example is separate from
provider-free native/I/O examples. Its ordinary systems do not waive extreme
mathematical failures in other Reference drivers or certify unexecuted platforms.
