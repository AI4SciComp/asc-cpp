# Installation and immutable dependencies

## Requirements

ASCCpp 0.9.0 requires CMake 3.25+, C++20, and `ASCCMake 0.1.0` exactly. CUDA
Toolkit 12 is required only for experimental CUDA builds.

The approved ASCCMake identity is:

| Field | Value |
| --- | --- |
| repository | `https://github.com/AI4SciComp/asc-cmake` |
| version/tag | annotated `v0.1.0` |
| commit | `8a7dcbad3a97267cce59810aff24de800a3497a7` |
| source URL | `https://api.github.com/repos/AI4SciComp/asc-cmake/tarball/8a7dcbad3a97267cce59810aff24de800a3497a7` |
| source SHA-256 | `67765391bef06c6c9a1a0c43e934d0db7a9c52876e66e0cf644042eeb0c2a5c9` |

The public repository, annotated tag, commit, and anonymous source archive
were reverified against these identities. No `ASC_CMAKE_READ_TOKEN` or other
private credential is part of ordinary CI.

## Preparing the dependency

Install or configure the exact package before disconnecting the network, or
provide a verified source checkout:

```bash
cmake -S /path/to/asc-cmake -B /tmp/asccmake-build \
  -DBUILD_TESTING=OFF -DASC_CMAKE_BUILD_TESTING=OFF -DASC_CMAKE_INSTALL=OFF

cmake -S . -B /tmp/asccpp-build \
  -DASCCMake_DIR=/tmp/asccmake-build \
  -DASC_CPP_ENABLE_CUDA=OFF
```

For a prepared source checkout, set
`FETCHCONTENT_SOURCE_DIR_ASCCMAKE=/path/to/asc-cmake`. When no package/source
override is supplied, a top-level build downloads only the pinned URL and
checks its SHA-256. Provider-free consumers perform no network access after
this dependency-preparation step.

## Build and install

```bash
cmake --preset test-release
cmake --build --preset test-release --parallel 2
ctest --preset test-release --no-tests=error --output-on-failure
cmake --install ../asc-cpp-build/test-release --prefix /path/to/prefix
```

The install contains libraries, all public headers, component-isolated CMake
metadata, `LICENSE`, `THIRD_PARTY_NOTICES`, and Joe--Kuo data/license. Package
tests relocate a prefix containing spaces and disable package registries to
detect hidden source/build dependencies.

## Experimental first-party robust PPSVX

The [robust PPSVX contract](contracts/robust-ppsvx.md) defines the limited
Linux x86_64 GNU 11.4 static development profile. The feature defaults to OFF.
It uses an independently prepared, attested Reference-LAPACK 3.12.1 context
while executing a separately named original numerical algorithm. Preparing the
provider does not approve its redistribution or bundle it with ASC.

For each desired integer ABI, choose a fresh external directory and build the
exact locked provider with the maintained tool (32 for LP64, 64 for global
ILP64). The tool retains commands, real upstream test results and attestation:

```sh
python3 -B tools/lapack/prepare_reference.py \
  --work-dir /tmp/asc-provider-32 --integer-bits 32 --jobs 2
cmake -S . -B /tmp/asc-robust-32 \
  -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11 \
  -DCMAKE_Fortran_COMPILER=gfortran-11 -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON -DASC_CPP_ENABLE_LAPACK=ON \
  -DASC_CPP_ENABLE_EXPERIMENTAL_ROBUST_PPSVX=ON \
  -DASC_CPP_LAPACK_INTEGER_BITS=32 \
  -DASC_CPP_LAPACK_ROOT=/tmp/asc-provider-32/prefix \
  -DASC_CPP_LAPACK_ATTESTATION=/tmp/asc-provider-32/attestation.json \
  '-DASC_CPP_LAPACK_RUNTIME_LIBRARIES=/absolute/libgfortran.so.5;/absolute/libquadmath.so.0'
cmake --build /tmp/asc-robust-32 --parallel 2
ctest --test-dir /tmp/asc-robust-32 --show-only=json-v1 -R robust_ppsvx
ctest --test-dir /tmp/asc-robust-32 --no-tests=error --output-on-failure \
  -R 'robust_ppsvx|dense_lapack.package_integration'
cmake --install /tmp/asc-robust-32 --prefix /tmp/asc-robust-install-32
```

Replace the runtime placeholders with the compiler's actual shared runtime
files. See [the public example](../examples/robust_ppsvx/README.md) for a C++-only
consumer of the relocated installation. The selected tests above concern the
first-party capability and package; the separately required Reference numerical
checks remain genuine failures and are not converted into expected successes.
