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
| source SHA-256 | `73299eca4b80b8a5571622636fe12c88f67602eba4e99f24368d5405b9bc0021` |

Public publication is blocked until the repository/tag/source URL are
anonymously accessible and their identities are reverified. No
`ASC_CMAKE_READ_TOKEN` or other private credential is part of ordinary CI.

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
