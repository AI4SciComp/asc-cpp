# Explicit selected matrix copy

The public S/D/C/Z LACPY adapters copy a full matrix or an explicitly selected
upper/lower trapezoid. Input and output may use independent padded layouts.
Unselected cells and padding stay unchanged. The native operation has no INFO
parameter; reports retain absent native INFO. No scaling, conjugation or
finiteness filtering is implied. Caller workspace contains live scalar arrays.

This maintained example uses public declarations and the exported
`ASC::dense_lapack` target, reuses plans and checks output/padding and reports.
From a prepared installed package:

```sh
cmake -S examples/matrix_copy -B /tmp/asc-matrix-copy \
  -DCMAKE_PREFIX_PATH=/path/to/relocated-asc \
  -DASC_CPP_LAPACK_ROOT=/path/to/prepared-reference-lapack
cmake --build /tmp/asc-matrix-copy
ctest --test-dir /tmp/asc-matrix-copy --no-tests=error --output-on-failure
```

Supply the explicit runtime paths required by the provider contract. The
matching actual LP64 or global ILP64 provider remains separately installed;
no provider or runtime archive is bundled. No private header, source-tree
include path or historical evidence path is needed by the consumer.
