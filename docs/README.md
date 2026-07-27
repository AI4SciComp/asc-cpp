# asc-cpp documentation

## Current implementation

The unreleased `0.9.0` candidate contains all six base modules, both approved
random storage facets, the provider-free aggregate, and six optional CUDA
provider facets:

```text
ASC::core
ASC::utilities
ASC::expression
ASC::random
ASC::dense
ASC::sparse
ASC::random_dense
ASC::random_sparse
ASC::cpp
ASC::core_cuda
ASC::dense_cuda
ASC::sparse_cuda
ASC::random_cuda
ASC::random_dense_cuda
ASC::random_sparse_cuda
```

The first nine targets are provider-free C++20 foundations described in the
[core](modules/core.md), [utilities](modules/utilities.md),
[expression](modules/expression.md), [random](modules/random.md), and
[dense](modules/dense.md), and [sparse](modules/sparse.md) module guides.
`dense` and `sparse` each depend directly on exactly `core` and the
storage-neutral `expression`; utilities, expression, and random otherwise
remain mutually independent and depend directly only on core.

The random facets are independently consumable and preserve the dense/sparse
separation. Every CUDA facet has the exact owner-specific closure recorded in
the dependency manifest. CUDA is opt-in and `cpp` remains provider-free.

Milestone 8 hardens this unchanged product surface and is governed by:

- the [frozen milestone contract](development/asc-cpp-m8-hardening-downstream/milestone-contract.md);
- the [ownership ledger](development/asc-cpp-m8-hardening-downstream/ownership.md);
- the [Milestone 7 CUDA provenance record](development/asc-cpp-m7-gpu-sparse-random/provenance-record.md); and
- the implementation, verification, documentation/API, dependency, and
  portability review records in that directory as they are completed.

The candidate must finish local validation and independent review at
Publication Checkpoint B. It is not yet a published release.

## Architecture

The authoritative design package is
[`development/asc-cpp-architecture/`](development/asc-cpp-architecture/):

- [architecture blueprint](development/asc-cpp-architecture/architecture-blueprint.md);
- [dependency manifest](development/asc-cpp-architecture/dependency-manifest.yaml);
- [capability manifest](development/asc-cpp-architecture/capability-manifest.yaml);
- [ASCCMake 0.1.0 consumption contract](development/asc-cpp-architecture/asc-cmake-consumption.md);
- [backend capability matrix](development/asc-cpp-architecture/backend-capability-matrix.md);
- [testing strategy](development/asc-cpp-architecture/testing-strategy.md);
- [release roadmap](development/asc-cpp-architecture/release-roadmap.md); and
- [accepted architecture decisions](development/asc-cpp-architecture/decisions/).

The architecture has exactly `core`, `utilities`, `expression`, `dense`,
`sparse`, and `random`. Plans and capability rows do not imply
implementation. This milestone preserves all fifteen components listed above.
It adds no product target, dependency, or operation.

## Build and consume

The root [README](../README.md) gives the exact ASCCMake binding, configure,
build, test, install, and consumer commands. Consumers must request:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_dense)
target_link_libraries(my_target PRIVATE ASC::random_dense)
```

No-component lookup requests the available `cpp` aggregate.

Hardening guidance is organized in the [support matrix](support-matrix.md),
[API compatibility policy](api-compatibility.md),
[package capability guide](package-capabilities.md),
[extension guide](extension-guide.md),
[downstream integration guide](downstream-integration.md), and
[performance guide](performance.md).

## Historical documents

The current API map is `api.md`; the current module guides are
`modules/core.md`, `modules/utilities.md`, `modules/expression.md`,
`modules/random.md`, `modules/dense.md`, and `modules/sparse.md`.

The retained `architecture.md`, `build-system.md`, `optional-backends.md`,
`testing.md`, `design/`, `migration/`, and other files under `modules/`
describe the deleted five-component implementation at historical HEAD
`33b261ea33616a6395c4ad3b20646093103344f7`. Those files remain explicitly
marked superseded and are audit evidence only.
