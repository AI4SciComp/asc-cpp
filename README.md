# ASCCpp

ASCCpp 0.9.0 is a portable C++20 scientific-computing foundation with explicit
ownership, failure, memory-placement, execution, numerical, and package
contracts. It provides provider-free reference implementations suitable for
correctness, integration, and research infrastructure.

## Release boundary

Supported CPU modules are Core, Utilities, Expression, Dense, Sparse, and
Random, including the Random Dense/Sparse storage facets. Dense BLAS Levels
1--3 and Sparse BLAS are serial correctness-reference implementations; they
are not optimized OpenBLAS, BLIS, oneMKL, or vendor providers.

CUDA source and package components are retained as **experimental** in 0.9.0.
They are not part of the supported matrix without the complete hosted
real-NVIDIA release gate on the exact release commit.

## Components

| Component | CMake target | Purpose | 0.9.0 status |
| --- | --- | --- | --- |
| `core` | `ASC::core` | status/result, configuration, I/O, types, memory, execution | supported |
| `utilities` | `ASC::utilities` | command-line parsing and timing | supported |
| `expression` | `ASC::expression` | expression/writable customization and aliasing | supported |
| `dense` | `ASC::dense` | dense arrays/views/evaluation and BLAS | supported reference |
| `sparse` | `ASC::sparse` | coordinate/CSR/CSC/evaluation and Sparse BLAS | supported reference |
| `random` | `ASC::random` | deterministic engines, distributions, seed and QMC | supported |
| `random_dense` | `ASC::random_dense` | deterministic dense storage adapters | supported |
| `random_sparse` | `ASC::random_sparse` | deterministic sparse storage adapters | supported |
| `cpp` | `ASC::cpp` | all supported provider-free components | supported |
| `*_cuda` | corresponding `ASC::*_cuda` | explicit CUDA provider facets | experimental |

## Requirements

- CMake 3.25 or newer
- a C++20 compiler
- `ASCCMake 0.1.0` at the exact identity in
  [`docs/installation.md`](docs/installation.md)
- CUDA Toolkit 12 only when experimental CUDA components are enabled

Public distribution remains blocked until the recorded `asc-cmake` tag is
publicly immutable and the final hosted release gates pass. No private token is
part of the build or CI design.

## Build, test, and install

Build out of the source tree. If `ASCCMake 0.1.0` is installed, CMake finds it;
otherwise the top-level build uses the pinned public bootstrap once its
upstream repository is public.

```bash
cmake --preset test-release
cmake --build --preset test-release --parallel 2
ctest --preset test-release --no-tests=error --output-on-failure
cmake --install ../asc-cpp-build/test-release --prefix /tmp/asccpp-prefix
```

To use a prepared local checkout without network access:

```bash
cmake --preset test-release \
  -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE=/path/to/asc-cmake
```

## Installed-package consumer

```cmake
cmake_minimum_required(VERSION 3.25)
project(consumer LANGUAGES CXX)

find_package(ASCCpp 0.9.0 EXACT CONFIG REQUIRED COMPONENTS dense random)
add_executable(consumer main.cc)
target_link_libraries(consumer PRIVATE ASC::dense ASC::random)
target_compile_features(consumer PRIVATE cxx_std_20)
```

See the six [compiled installed-package examples](examples/) for Core, Dense
BLAS, Sparse conversion/BLAS, Random/QMC, storage adapters, and experimental
CUDA completion.

## Documentation and project policy

- [Getting started](docs/getting-started.md)
- [Installation and immutable dependencies](docs/installation.md)
- [Strict API reference entry point](docs/api/mainpage.md)
- [Support matrix](docs/support-matrix.md)
- [Package capabilities](docs/package-capabilities.md)
- [`linalg` to `blas` migration](docs/migration/linalg-to-blas.md)
- [Changelog](CHANGELOG.md) and [0.9.0 release notes](release/release-notes-v0.9.0.md)
- [Citation](CITATION.cff), [references](docs/references.md), and [provenance](docs/provenance/)
- [Security](SECURITY.md), [contributing](CONTRIBUTING.md), and [governance](GOVERNANCE.md)

Licensed under Apache-2.0. Third-party data and notices are recorded in
[`THIRD_PARTY_NOTICES`](THIRD_PARTY_NOTICES) and
[`data/random/LICENSE.joe-kuo`](data/random/LICENSE.joe-kuo).
