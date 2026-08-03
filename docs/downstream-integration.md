# Downstream integration

Status: `0.9.0` release contract

Date: 2026-08-03

Downstreams should consume the installed or explicitly selected build-tree
CMake package, request the smallest component set, and link only imported
targets. ASCCpp does not support copying headers/libraries into an application
or editing generated target files.

Random engines and quasi-random sequences install the stateful engines, scalar distributions, generator
composition, explicit random-device wrapper, and storage-neutral QMC through
the existing `random` component. Random storage adapters installs only updated headers in the
existing `random_dense` and `random_sparse` components. The licensed Joe--Kuo
input and notice remain package documentation; QMC execution reads only its
compiled table. No package target, component closure, provider discovery rule,
or external dependency changes.

Random release contract introduces no downstream migration. The completion audit verifies
that the existing build-tree, install/relocation, and asc-xde-shaped consumers
remain registered evidence for the approved Random surface.

## Choose the owning component

| Downstream need | Component / target |
| --- | --- |
| Core status, metadata, memory, I/O, execution | `core` / `ASC::core` |
| command-line configuration or timer | `utilities` / `ASC::utilities` |
| storage-neutral expression protocol only | `expression` / `ASC::expression` |
| host Dense storage/evaluation/algebra | `dense` / `ASC::dense` |
| host Sparse storage/evaluation/Sparse BLAS | `sparse` / `ASC::sparse` |
| explicit seeds, engines, scalar distributions, QMC, raw Philox/`Uniform01` | `random` / `ASC::random` |
| host Dense pseudo/QMC/normal/sphere adapters | `random_dense` / `ASC::random_dense` |
| host Sparse value/structure/combined adapters | `random_sparse` / `ASC::random_sparse` |
| complete provider-free surface | `cpp` / `ASC::cpp` |
| CUDA capability | the corresponding explicit `*_cuda` component/target |

Requesting `dense` loads only `ASC::core`, `ASC::expression`, and
`ASC::dense`. It does not discover CUDAToolkit or load Sparse, Random, or an
optional provider.

## Minimal project

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_xde_consumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense)

add_executable(my_xde_consumer main.cc)
target_link_libraries(my_xde_consumer PRIVATE ASC::dense)
```

Configure with an explicit package location and no user package registry:

```sh
cmake -S . -B build \
  -DASCCpp_DIR="/absolute/prefix/lib/cmake/ASCCpp" \
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
  -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Use quotes for paths containing spaces. A relocatable install must continue to
work after the complete prefix is copied without retaining source/build
absolute paths.

## The release hardening asc-xde-shaped trial

The default downstream trial is wholly owned by asc-cpp and does not discover,
read, require, or modify a sibling checkout. The
[`tests/downstream/asc_xde_trial.cc`](../tests/downstream/asc_xde_trial.cc)
fixture models the dependency shape of a small ODE/PDE consumer. It requests
only:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense)
target_link_libraries(asc_xde_trial PRIVATE ASC::dense)
```

The fixture performs one explicit-Euler step over a two-dimensional host Dense
state:

```text
derivative = -decay * state
next = state + timestep * derivative
```

It checks every result value and a reduction checksum using only public
headers and targets. It also verifies:

- the exact imported target closure is `ASC::core`, `ASC::expression`, and
  `ASC::dense`;
- CUDAToolkit discovery is disabled and no `CUDA::` target appears;
- deleted `asc/array.h` and `asc/linalg.h` compatibility headers are absent;
- build-tree, installed, relocated, and path-with-spaces package use.

This is a package/dependency/API trial. It is not an asc-xde solver, production
feature, migration-complete claim, or evidence about any real asc-xde
revision.

## Optional real-repository audit

Inspection of a real asc-xde checkout is a separate read-only audit and is
disabled by default. It must be explicitly enabled with all three cache
entries:

```sh
cmake -S . -B build \
  -DASC_CPP_ENABLE_ASC_XDE_REPOSITORY_AUDIT=ON \
  -DASC_CPP_ASC_XDE_REPOSITORY="/absolute/path/to/asc-xde" \
  -DASC_CPP_ASC_XDE_EXPECTED_COMMIT="<full-approved-commit>"
```

`ASC_CPP_ASC_XDE_REPOSITORY` and `ASC_CPP_ASC_XDE_EXPECTED_COMMIT` have empty
defaults. Enabling the audit without an explicit repository path and exact
expected commit is a configure error; asc-cpp never substitutes `../asc-xde`
or the checkout's current `HEAD`. The registered repository-audit trial checks
the commit and exact worktree status before and after its isolated scratch
work and fails if either changes. It does not require or clean a pristine
checkout, authorize a write to that repository, or turn repository inspection
into a default package test.

## CPU and CUDA consumers

A CPU consumer should keep CUDA disabled at package-production time unless it
also distributes CUDA facets. Even a CUDA-enabled ASCCpp package remains
provider-free for a CPU-only component request.

A CUDA consumer requests a facet explicitly:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_cuda)
target_link_libraries(my_gpu_target PRIVATE ASC::random_cuda)
```

That lookup discovers CUDA Toolkit 12+ because the selected closure contains a
CUDA facet. It does not authorize implicit device selection, transfer,
synchronization, or fallback. The application creates the required execution
context/resource, supplies truthful live views and capacities, and retains all
storage/context/workspace until completion.

## Integration checklist

- Include only installed public headers owned by requested components.
- Link only imported `ASC::` targets.
- Check every `Status` and `Result<T>`.
- Keep `MemoryResource` objects alive longer than their allocations.
- Keep view storage and asynchronous operands alive through completion.
- Do not assume a provider-neutral umbrella includes a provider header.
- Test installed and relocated packages with the registry disabled.
- Record provider skips and evidence labels honestly.
- Do not use implementation namespaces, generated exports, or deleted legacy
  headers.

## Pre-1.0 upgrade checklist

For each ASCCpp minor update:

1. pin and review the minor rather than accepting an unconstrained `0.x`;
2. read the changelog, migration notes, support matrix, and remaining risks;
3. rebuild every translation unit and shared-library boundary;
4. rerun numerical, random-bit, package, relocation, and provider tests;
5. revalidate every enabled provider on real hardware; and
6. regenerate comparable local ABI and performance observations.

See @ref md_docs_2package-capabilities "package capabilities", @ref
md_docs_2api-compatibility "API compatibility", and @ref
md_docs_2extension-guide "extension guidance".
