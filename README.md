# asc-cpp

`asc-cpp` is being rebuilt as the C++20 numerical foundation for
AI4SciComp. The current checkout is the unreleased **Milestone 8:
packaging/API/performance/downstream hardening** candidate. Milestone 8
hardens the Milestone 7 product surface without adding a product target,
operation, provider, or dependency.

## Available components

| Component | Build target | Consumer target | Kind | Direct ASC dependency |
| --- | --- | --- | --- | --- |
| Core | `asc_core` | `ASC::core` | Compiled | None |
| Utilities | `asc_utilities` | `ASC::utilities` | Compiled | `ASC::core` |
| Expression | `asc_expression` | `ASC::expression` | Interface | `ASC::core` |
| Dense | `asc_dense` | `ASC::dense` | Compiled | `ASC::core`, `ASC::expression` |
| Sparse | `asc_sparse` | `ASC::sparse` | Compiled | `ASC::core`, `ASC::expression` |
| Random | `asc_random` | `ASC::random` | Compiled | `ASC::core` |
| Random Dense | `asc_random_dense` | `ASC::random_dense` | Interface | `ASC::random`, `ASC::dense` |
| Random Sparse | `asc_random_sparse` | `ASC::random_sparse` | Interface | `ASC::random`, `ASC::sparse` |
| Aggregate | `asc_cpp` | `ASC::cpp` | Interface | All provider-free targets |
| Core CUDA | `asc_core_cuda` | `ASC::core_cuda` | Compiled, optional | `ASC::core`; private `CUDA::cudart` |
| Dense CUDA | `asc_dense_cuda` | `ASC::dense_cuda` | Compiled, optional | `ASC::dense`, `ASC::core_cuda`; private `CUDA::cublas` |
| Sparse CUDA | `asc_sparse_cuda` | `ASC::sparse_cuda` | Compiled, optional | `ASC::sparse`, `ASC::core_cuda`; private `CUDA::cusparse` |
| Random CUDA | `asc_random_cuda` | `ASC::random_cuda` | Compiled, optional | `ASC::random`, `ASC::core_cuda` |
| Random Dense CUDA | `asc_random_dense_cuda` | `ASC::random_dense_cuda` | Compiled, optional | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` |
| Random Sparse CUDA | `asc_random_sparse_cuda` | `ASC::random_sparse_cuda` | Compiled, optional | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` |

The provider-free surface uses only C++20 standard-library facilities:

- Utilities provides transactional command-line configuration and a checked
  monotonic timer.
- Expression provides a storage-neutral readable-expression customization
  protocol and safe pointwise negate, add, subtract, and multiply nodes.
- Dense provides checked left-, right-, and explicit-stride mappings,
  non-owning views, move-only ownership, expression evaluation, reductions,
  and allocation-free serial CPU reference algebra.
- Sparse provides explicit coordinate construction/finalization, canonical
  CSR/CSC ownership and views, named conversions, structure-preserving
  evaluation, and allocation-free serial CSR SpMV.
- Random provides the pure Philox4x32-10 raw-bit engine and exact scalar
  `Uniform01<float>` and `Uniform01<double>` transforms.
- Random Dense fills caller-provided Dense views in logical coordinate order.
- Random Sparse creates exact-count canonical coordinate arrays from separate
  structure and value address domains.
- Core CUDA supplies explicit CUDA resources, streams, asynchronous copies,
  and completion events without exposing CUDA SDK types in public signatures.
- Dense CUDA supplies bounded pointwise evaluation and selected
  Copy/Scal/Axpy/Gemv/Gemm operations for device-resident `float` and
  `double` views.
- Sparse CUDA supplies explicit canonical CSR staging, deterministic
  unit-stride CSR SpMV, positive-nonunit-stride CSR SpMV, and bounded
  structure-preserving evaluation.
- Random CUDA supplies the exact provider-free Philox word sequence on a CUDA
  device. Its Dense and Sparse facets preserve the Milestone 5 logical order,
  canonical structure, and explicit address contracts.

File parsing, general broadcasting, entropy, additional distributions,
HIP, SYCL, and later-roadmap providers are not implemented.

## Consuming a component

Requirements:

- CMake 3.25 or newer;
- a C++20 compiler;
- released `ASCCMake` 0.1.0 when building asc-cpp; and
- a build tool supported by the selected CMake generator; and
- for CUDA providers only, CUDAToolkit 12 or newer, a CUDA compiler, and a
  caller-selected CUDA architecture.

Request only the components a target uses:

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_dense)

add_executable(my_consumer main.cc)
target_link_libraries(my_consumer PRIVATE ASC::random_dense)
```

Requesting `random_dense` loads only Random, Dense, Expression, and Core;
`random_sparse` loads only Random, Sparse, Expression, and Core. The two
facets do not load one another. A no-component lookup requests the
provider-free `cpp` aggregate. Required unknown components fail; an optional
unknown component does not invalidate a successful available request.
Requesting a `*_cuda` component is valid only for a package built with
`ASC_CPP_ENABLE_CUDA=ON`; only provider requests discover CUDAToolkit.
Provider closures are exact—for example, `random_dense_cuda` loads Random
Dense, Random CUDA, Core CUDA, and their provider-free prerequisites without
loading Sparse. `ASC::cpp` remains provider-free.

## Building and validating

Use an out-of-source build and supply the released ASCCMake package:

```sh
cmake -S . -B build/m8-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASCCMake_DIR=/absolute/path/to/asc-cmake/package
cmake --build build/m8-debug --parallel
ctest --test-dir build/m8-debug --output-on-failure
```

Enable the bounded CUDA provider facets explicitly:

```sh
cmake -S . -B build/m8-cuda \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/absolute/path/to/asc-cmake/package
cmake --build build/m8-cuda --parallel
ctest --test-dir build/m8-cuda --output-on-failure
```

`BUILD_SHARED_LIBS` selects shared or static Core, Utilities, Dense, Sparse,
and Random libraries. Expression, both Random storage facets, and the
aggregate remain interface targets in either mode. Development warning and
sanitizer controls are build-local and are not exported to consumers.

## Documentation and scope

Start with the [API map](docs/api.md) and module guides for
[Core](docs/modules/core.md), [Utilities](docs/modules/utilities.md),
[Expression](docs/modules/expression.md), [Dense](docs/modules/dense.md), and
[Sparse](docs/modules/sparse.md), and [Random](docs/modules/random.md). The
[documentation index](docs/README.md) links the frozen milestone contracts,
architecture decisions, and retained historical material.

Milestone 8 also records the [support matrix](docs/support-matrix.md),
[API and ABI policy](docs/api-compatibility.md),
[package capabilities](docs/package-capabilities.md),
[extension rules](docs/extension-guide.md),
[downstream integration trial](docs/downstream-integration.md), and
[performance envelope](docs/performance.md).

This clean restart follows the approved six-module architecture and
clean-room provenance policy. The Philox implementation is independently
derived from the frozen primary-paper contract; no MdeCpp, deleted asc-cpp,
Random123 implementation, or upstream vector corpus is copied. See the
[Milestone 2 provenance record][m2-provenance]. Dense is a clean-room
implementation of the approved Milestone 3 contract.
Sparse is a clean-room implementation of the approved Milestone 4 contract.
The Random storage facets are clean-room implementations of the approved
Milestone 5 contract.
The Core/Dense CUDA facets are clean-room implementations of the approved
Milestone 6 contract. The Sparse/Random CUDA facets are clean-room
implementations of the approved Milestone 7 contract.

Read [CONTRIBUTING.md](CONTRIBUTING.md) before making changes. Security
reporting guidance is in [SECURITY.md](SECURITY.md). The project is licensed
under [Apache License 2.0](LICENSE).

Milestone completion is not publication. This checkout is not a release.

[m2-provenance]: docs/development/asc-cpp-m2-independent-foundations/provenance-record.md
