# Getting started

ASCCpp consumers select only the package components they use. All public APIs
require C++20, use namespace `asc`, and report recoverable failures with
`Status` or `Result<T>`.

1. Prepare `ASCCMake 0.1.0` as described in [installation](installation.md).
2. Configure, build, test, and install ASCCpp using a release preset.
3. Use `find_package(ASCCpp 0.9.0 EXACT CONFIG REQUIRED COMPONENTS ...)`.
4. Link only the imported `ASC::*` targets.
5. Check every returned `Status`/`Result` before using an output.

The exact CPU and CUDA support boundary is in the
[support matrix](support-matrix.md). The provider-free CPU implementation is
synchronous. CUDA provider calls may return completion events and are
experimental in 0.9.0.

Build and run all examples against an installed package:

```bash
cmake -S examples -B /tmp/asccpp-examples \
  -DASCCpp_DIR=/path/to/prefix/lib/cmake/ASCCpp \
  -DBUILD_TESTING=ON
cmake --build /tmp/asccpp-examples --parallel 2
ctest --test-dir /tmp/asccpp-examples --no-tests=error --output-on-failure
```
