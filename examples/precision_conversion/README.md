# Explicit matrix precision conversion

The public SLAG2D/DLAG2S/CLAG2Z/ZLAG2C calls convert full matrices through the
explicitly selected pinned provider. Both layouts may be chosen independently.
Caller workspace holds separate live input-packing and output-staging objects.
Narrowing may round or underflow. Native range failure preserves the entire
caller destination and remains visible in the report; there is no fallback.

This maintained example uses only exported public interfaces, demonstrates
query reuse and reports a real narrowing-range failure. No source-tree or
historical evidence paths are required. From a prepared installed package:

```sh
cmake -S examples/precision_conversion -B /tmp/asc-conversion \
  -DCMAKE_PREFIX_PATH=/path/to/relocated-asc \
  -DASC_CPP_LAPACK_ROOT=/path/to/prepared-reference-lapack
cmake --build /tmp/asc-conversion
ctest --test-dir /tmp/asc-conversion --no-tests=error --output-on-failure
```

Supply the explicit Fortran runtime paths required by the provider installation
contract. The matching actual provider ABI is required. Provider-free native
and array-I/O consumers remain separate.
