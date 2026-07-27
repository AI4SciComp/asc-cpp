# asc-cpp

`asc-cpp` is the C++20 scientific-computing foundation for AI4SciComp.

## Current status

This branch is the unreleased `0.9.0` candidate for
**Milestone 8: packaging/API/performance/downstream hardening**. It preserves
all six base modules, two random-owned storage facets, the provider-free
aggregate, and six optional CUDA provider facets:

| Build target | Build/install target | Direct ASC dependency |
| --- | --- | --- |
| `asc_core` | `ASC::core` | none |
| `asc_utilities` | `ASC::utilities` | `ASC::core` |
| `asc_expression` | `ASC::expression` | `ASC::core` |
| `asc_random` | `ASC::random` | `ASC::core` |
| `asc_dense` | `ASC::dense` | `ASC::core`, `ASC::expression` |
| `asc_sparse` | `ASC::sparse` | `ASC::core`, `ASC::expression` |
| `asc_random_dense` | `ASC::random_dense` | `ASC::random`, `ASC::dense` |
| `asc_random_sparse` | `ASC::random_sparse` | `ASC::random`, `ASC::sparse` |
| `asc_cpp` | `ASC::cpp` | all six modules and both random facets |
| `asc_core_cuda` | `ASC::core_cuda` | `ASC::core` |
| `asc_dense_cuda` | `ASC::dense_cuda` | `ASC::dense`, `ASC::core_cuda` |
| `asc_sparse_cuda` | `ASC::sparse_cuda` | `ASC::sparse`, `ASC::core_cuda` |
| `asc_random_cuda` | `ASC::random_cuda` | `ASC::random`, `ASC::core_cuda` |
| `asc_random_dense_cuda` | `ASC::random_dense_cuda` | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` |
| `asc_random_sparse_cuda` | `ASC::random_sparse_cuda` | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` |

The provider-free components use only the C++20 standard library. The optional
CUDA facets require CUDAToolkit 12 or newer and use the CUDA Runtime, cuBLAS,
cuSPARSE, and original project kernels. The hardened surface includes:

- [transactional command-line parsing and monotonic timing](docs/modules/utilities.md);
- a [storage-neutral expression protocol and safe pointwise nodes](docs/modules/expression.md);
- an [explicit-state Philox4x32-10 engine and exact unit-uniform transforms](docs/modules/random.md);
- [dense host storage, storage-neutral evaluation, reductions, and serial
  reference linear algebra](docs/modules/dense.md); and
- [canonical coordinate/CSR/CSC storage, sparse evaluation, conversion, and
  serial reference CSR SpMV](docs/modules/sparse.md); and
- [explicit-state deterministic dense fills and exact-count canonical sparse
  generation](docs/modules/random.md).

There is no local configuration-file parser, mutable/default random state,
optimized CPU provider, general sparse GPU algebra, factorization, or solver.
A package lookup without components requests the provider-free `cpp`
aggregate. Milestone completion, publication, and release are separate
approval gates.

## Architecture

The approved architecture has exactly six modules:

| Module | Permitted direct asc-cpp dependencies |
| --- | --- |
| `core` | none |
| `utilities` | `core` |
| `expression` | `core` |
| `dense` | `core`, `expression` |
| `sparse` | `core`, `expression` |
| `random` | `core` |

Two random-owned integration facets connect storage without adding modules:

- `random_dense` depends on `random` and `dense`;
- `random_sparse` depends on `random` and `sparse`.

The `cpp` component is a convenience aggregate, not a seventh module.
The approved [architecture blueprint](docs/development/asc-cpp-architecture/architecture-blueprint.md)
and [dependency decision](docs/development/asc-cpp-architecture/decisions/0001-six-module-graph.md)
define the hard boundaries.

## Configure and test

Requirements:

- CMake 3.25 or newer;
- a C++20 compiler;
- released ASCCMake `v0.1.0`, commit
  `8a7dcbad3a97267cce59810aff24de800a3497a7`;
- a CMake build tool.

First configure the exact ASCCMake release as a build-tree package:

```bash
cmake -S /absolute/path/to/asc-cmake \
  -B /absolute/path/to/asc-cmake-build \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF
```

Then configure, build, and test asc-cpp:

```bash
cmake --preset test-debug \
  -DASCCMake_DIR=/absolute/path/to/asc-cmake-build
cmake --build --preset test-debug
ctest --preset test-debug
```

The configure step calls:

```cmake
find_package(ASCCMake 0.1.0 EXACT CONFIG REQUIRED)
```

`ASCCMake_DIR` must name a build or installation directory containing that
release's `ASCCMakeConfig.cmake`. A canonical lookup path such as
`CMAKE_PREFIX_PATH` may be used instead. asc-cpp does not download ASCCMake.

Install to an explicit prefix:

```bash
cmake --preset install-test \
  -DASCCMake_DIR=/absolute/path/to/asc-cmake-build
cmake --build --preset install-test
ctest --preset install-test
cmake --install build/install-test \
  --prefix "/absolute/path/to/asc-cpp prefix"
```

Available presets are `dev-debug`, `dev-release`, `test-debug`,
`test-release`, `install-test`, `test-shared`, `test-asan-ubsan`, and
`test-cuda`.
Development and test presets enable warnings-as-errors; the sanitizer preset
requests AddressSanitizer and UndefinedBehaviorSanitizer together. It runs the
instrumented in-tree component, contract, compile, and architecture tests;
package and isolated-consumer tests run separately from a non-instrumented
build so sanitizer link requirements never become an exported consumer
dependency.

To build all six approved CUDA facets, opt in explicitly and provide the standard
CMake architecture list:

```bash
cmake --preset test-cuda \
  -DASCCMake_DIR=/absolute/path/to/asc-cmake-build \
  -DCMAKE_CUDA_ARCHITECTURES=86
cmake --build --preset test-cuda
ctest --preset test-cuda
```

CUDA-disabled configuration performs no CUDA language or toolkit discovery.

## Consume one component

An installed consumer requests the narrowest available component explicitly.
For example, a command-line application uses:

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_consumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_sparse)

add_executable(my_consumer main.cc)
target_link_libraries(my_consumer PRIVATE ASC::random_sparse)
```

Configure the consumer with the installed prefix:

```bash
cmake -S /absolute/path/to/consumer \
  -B /absolute/path/to/consumer-build \
  -DCMAKE_PREFIX_PATH="/absolute/path/to/asc-cpp prefix"
cmake --build /absolute/path/to/consumer-build
```

Use `<asc/sparse.h>`, `<asc/dense.h>`, `<asc/utilities.h>`,
`<asc/expression.h>`, `<asc/random.h>`, or `<asc/core.h>` for one complete
module surface, or include the self-contained topic header required by the
source file. Random storage generation is opt-in through
`<asc/random/dense.h>` with `ASC::random_dense` or
`<asc/random/sparse.h>` with `ASC::random_sparse`; the base random umbrella
does not include them. `ASC::dense` and `ASC::sparse` each bring exactly
`ASC::core` and `ASC::expression` transitively. No request imports a sibling
outside its frozen closure.

Provider facets are requested independently:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS sparse_cuda)
target_link_libraries(my_gpu_target PRIVATE ASC::sparse_cuda)
```

Each CUDA component imports only its frozen closure. In particular,
`sparse_cuda` imports `sparse` and `core_cuda`, while the dense and sparse
random CUDA facets import their provider-free storage facet, `random_cuda`,
and `core_cuda`; they do not import `dense_cuda` or `sparse_cuda`.
Provider-free requests do not import a CUDA facet or discover CUDAToolkit. The
package intentionally rejects:

- unknown required components; and
- known provider components that were not built into the package.

No-component lookup and explicit `cpp` requests import the complete
provider-free surface. An unavailable optional component reports its component
`_FOUND` value false without invalidating an otherwise successful required
`core` request.

## Project options

| Option | Default | Purpose |
| --- | --- | --- |
| `ASC_CPP_BUILD_TESTING` | top-level follows `BUILD_TESTING`; subproject `OFF` | Build architecture, component, and package tests |
| `ASC_CPP_INSTALL` | top-level `ON`; subproject `OFF` | Install available components and package metadata |
| `ASC_CPP_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings from asc-cpp-owned targets as errors |
| `ASC_CPP_ENABLE_CUDA` | `OFF` | Build all six optional CUDA facets |
| `ASC_CPP_ENABLE_ADDRESS_SANITIZER` | `OFF` | Instrument asc-cpp-owned targets with ASan |
| `ASC_CPP_ENABLE_UNDEFINED_SANITIZER` | `OFF` | Instrument asc-cpp-owned targets with UBSan |
| `ASC_CPP_ENABLE_THREAD_SANITIZER` | `OFF` | Instrument asc-cpp-owned targets with TSan |
| `ASC_CPP_ENABLE_LEAK_SANITIZER` | `OFF` | Instrument asc-cpp-owned targets with LSan |

ThreadSanitizer cannot be combined with AddressSanitizer or LeakSanitizer.
These development settings are target-local and are not exported to
consumers. `BUILD_SHARED_LIBS` selects shared or static compiled components;
`asc_expression` remains an interface target. No asc-cpp-specific linkage
option exists.

## Evidence boundary

`core_cuda` owns explicit pinned/device/managed resources, a nonblocking CUDA
stream per execution context, byte copies, and completion events.
`dense_cuda` owns device pointwise evaluation plus asynchronous
Copy/Scal/Axpy/Gemv/Gemm for the documented float/double subset. It has no
sparse/random edge and `ASC::cpp` remains provider-free.
`sparse_cuda` owns explicit canonical host-CSR clone, bounded
structure-preserving sparse evaluation, and float/double CSR SpMV.
`random_cuda`, `random_dense_cuda`, and `random_sparse_cuda` own raw Philox
words, layout-independent dense Uniform01 fill, and exact-count canonical
coordinate generation respectively.

An asynchronous result means work is ordered on the explicit stream and is
represented by an event; it is not a blanket guarantee that the API call
returns before pageable-host staging completes. Use pinned host memory when
strong host-asynchronous transfer behavior matters. Operations perform no ASC
fallback or implicit device-wide synchronization. `DenseArray::Clone` is the
explicit synchronous transfer operation. Destroying or replacing a live
`DenseCudaContext` destroys its cuBLAS handle and may device-wide synchronize.

The hardening guarantees and limits are documented in the
[support matrix](docs/support-matrix.md), [API compatibility policy](docs/api-compatibility.md),
[package capability guide](docs/package-capabilities.md),
[extension guide](docs/extension-guide.md),
[downstream integration guide](docs/downstream-integration.md), and
[performance guide](docs/performance.md). The complete local validation and
independent review results are recorded at Publication Checkpoint B. Hosted
GCC, Clang, MSVC, and AppleClang results are not claimed before the
unpublished branch runs in CI.

## Documentation, provenance, and license

Start at the [documentation index](docs/README.md). The frozen
[Milestone 8 contract](docs/development/asc-cpp-m8-hardening-downstream/milestone-contract.md)
is the scope authority for this candidate. The Milestone 7 CUDA clean-room
boundary remains recorded in the
[Milestone 7 provenance record](docs/development/asc-cpp-m7-gpu-sparse-random/provenance-record.md);
the random-storage and Philox records remain authoritative for their work.

`asc-cpp` is licensed under the [Apache License 2.0](LICENSE). MdeCpp is used
only as behavior and test-category evidence under the approved clean-room
policy; its production code, tests, literal corpora, and generated data are
not copied. See
[ADR 0017](docs/development/asc-cpp-architecture/decisions/0017-third-party-provenance.md).

Contribution expectations are in [CONTRIBUTING.md](CONTRIBUTING.md). Report
suspected security issues using [SECURITY.md](SECURITY.md).
