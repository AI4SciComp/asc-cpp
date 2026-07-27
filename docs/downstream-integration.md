# Downstream integration

Status: unreleased `0.9.0` Milestone 8 candidate

Date: 2026-07-27

Downstreams consume installed/public headers and imported targets. They should
request the smallest explicit component set that owns their workflow and
should not rely on the ASCCpp source tree, implementation namespaces, deleted
compatibility headers, or ambient CUDA discovery.

## Select components from APIs

Common CPU-only choices are:

| Downstream need | Request and link |
| --- | --- |
| status, configuration, byte I/O, host memory/execution | `core` / `ASC::core` |
| command line or timer | `utilities` / `ASC::utilities` |
| external expression adapters only | `expression` / `ASC::expression` |
| dense storage/evaluation/algebra | `dense` / `ASC::dense` |
| sparse storage/conversion/SpMV | `sparse` / `ASC::sparse` |
| raw Philox/Uniform01 | `random` / `ASC::random` |
| dense random fill | `random_dense` / `ASC::random_dense` |
| sparse random generation | `random_sparse` / `ASC::random_sparse` |
| all provider-free product surfaces | `cpp` / `ASC::cpp` |

For a mixed dense/sparse ODE or PDE application, request both storage
components instead of the aggregate when utilities and random generation are
not used:

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_xde_application LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense sparse)

add_executable(my_xde_application main.cc)
target_link_libraries(
  my_xde_application
  PRIVATE
    ASC::dense
    ASC::sparse
)
```

Their closures load core and expression exactly once. Sparse remains
independent of dense; the application explicitly owns the decision to use
both.

## Package locations

An installed consumer normally uses `CMAKE_PREFIX_PATH`:

```sh
cmake -S downstream -B downstream-build \
  -DCMAKE_PREFIX_PATH="/absolute/ASCCpp prefix" \
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
  -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON
cmake --build downstream-build
ctest --test-dir downstream-build --output-on-failure
```

For an exact generated package directory, set `ASCCpp_DIR` instead. Do not set
both to disagreeing installations.

The same consumer must work after copying the complete installed prefix:

```sh
cmake -S downstream -B relocated-build \
  -DASCCpp_DIR="/absolute/relocated prefix/lib/cmake/ASCCpp" \
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
  -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON
cmake --build relocated-build
ctest --test-dir relocated-build --output-on-failure
```

Paths containing spaces are supported. Quote shell arguments and CMake path
values; do not escape spaces into a different literal path.

Static and shared installations are separate packages. Select one complete
prefix and link imported targets. Do not combine a config/targets file from
one with libraries from the other.

## Ownership in application code

Declare resources before owners so reverse destruction order releases owners
first:

```cpp
asc::HostMemoryResource resource;
auto state = StateArray::Create(extents, resource);
auto operator_matrix =
    asc::CsrArray<double>::Create(shape, offsets, indices, values, resource);
```

The resource is not owned by either returned array. Views borrowed from an
array do not keep the array or resource alive. A successful resize or
move-assignment can invalidate views of the replaced allocation.

Expression nodes borrow lvalue operands. Keep both the view descriptor and its
storage valid until synchronous evaluation returns. For asynchronous CUDA
operations, keep storage, resources, workspaces, and provider contexts valid
until the returned completion event completes.

## Optional CUDA use

Request a provider explicitly:

```cmake
find_package(
  ASCCpp 0.9 CONFIG REQUIRED
  COMPONENTS dense sparse dense_cuda sparse_cuda
)

target_link_libraries(
  my_gpu_application
  PRIVATE
    ASC::dense_cuda
    ASC::sparse_cuda
)
```

That request discovers CUDAToolkit 12 or newer. A CPU-only request does not.
Do not make CUDA an unconditional downstream dependency when the CPU
configuration does not use a CUDA facet.

Provider calls never migrate CPU owners implicitly. Applications create an
explicit CUDA execution context and memory resource, stage or clone storage
through named operations, retain all referenced objects through completion,
and copy results through an explicit context when host access is needed.

GPU evidence from the ASCCpp checkpoint describes the tested ASCCpp
environment. A downstream with a different toolkit, GPU, driver, scalar,
layout, workload, or topology must collect its own evidence. Use exactly
`configure-tested`, `compile-tested`, `runtime-tested`, `parity-tested`, or
`skipped`.

## Milestone 8 asc-xde trial

The repository inspected for the trial is the clean skeletal `asc-xde`
repository at:

```text
abcb29b51f22f40afd7f174707b7ccf83c32d4bf
```

It has no production build/API surface to migrate. Milestone 8 therefore uses
an asc-cpp-owned, isolated, asc-xde-shaped fixture. The fixture:

- requests an explicit minimal provider-free component set;
- includes only installed/public ASCCpp headers;
- compiles and runs a small ODE/PDE-oriented storage/evaluation workflow;
- exercises the build-tree package, installed package, relocated prefix, and
  paths containing spaces;
- runs with CUDA unavailable and proves the CPU closure does not discover a
  CUDA SDK; and
- does not include deleted `asc/array*`, `asc/linalg*`, or another legacy
  compatibility header.

This is a dependency and API trial. It does not add an asc-xde solver, define
asc-xde production APIs, claim migration completion, or modify the sibling
repository. The real asc-xde commit and unchanged repository state are
recorded in the Milestone 8 checkpoint.

## Pre-1.0 upgrade checklist

For every ASCCpp minor update:

1. pin and review the new minor rather than accepting an unconstrained `0.x`;
2. read the changelog, migration notes, support matrix, and remaining risks;
3. configure with the CMake user package registry disabled;
4. rebuild all downstream translation units and shared-library boundaries;
5. rerun CPU numerical and random-bit checks;
6. rerun every enabled provider on real hardware and record skips honestly;
7. verify build-tree/install/relocation and paths containing spaces; and
8. regenerate local ABI/performance observations in the same environment.

See [package capabilities](package-capabilities.md) for exact closures and
[API compatibility](api-compatibility.md) for what a version request does and
does not promise.
