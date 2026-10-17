# Explicit snapshot analysis with GEDMD

The public S/D/C/Z GEDMD adapters use the separately prepared, pinned optional
Reference provider. The caller explicitly selects scaling, SVD algorithm,
rank policy and explicit or factored Ritz vectors. Queries inspect metadata
only; caller-owned live typed workspace stages input and output. Rank and raw
INFO remain separate. INFO4 publishes warning results with a non-OK status;
nonconvergence preserves caller output. This is not a universal accuracy
certificate for arbitrary finite or extreme snapshots.

This example constructs independent well-conditioned snapshot pairs for a
known diagonal operator, checks eigenpairs, POD singular values, A*U, residuals
and reports for all four scalars/SVD algorithms, independent padded layouts,
minimum/preferred workspace, and repeated immutable plan use. It uses only
public headers and the exported `ASC::dense_lapack` target.

```sh
cmake -S examples/dmd -B /tmp/asc-dmd \
  -DCMAKE_PREFIX_PATH=/path/to/relocated-asc \
  -DASC_CPP_LAPACK_ROOT=/path/to/prepared-reference-lapack
cmake --build /tmp/asc-dmd
ctest --test-dir /tmp/asc-dmd --no-tests=error --output-on-failure
```

Supply the explicit runtime paths required by the provider contract. A matching
actual LP64 or global ILP64 provider remains separately installed. No provider
or runtime archive is bundled, and no private header, source-tree include path
or historical evidence directory is needed by this maintained consumer.
