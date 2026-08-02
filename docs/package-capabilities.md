# ASCCpp package capabilities

Status: unreleased `0.9.0` Issue 14 Feature Gate B candidate

Date: 2026-08-02

An ASCCpp CMake component is a capability request. It imports the named target
and its exact transitive `ASC::` closure. It does not enable a provider, add an
operation, or make every package target available.

Issues 13 and 14 add `asc/random/generator.h`, `asc/random/seed.h`, and
`asc/random/quasi.h` to the existing `random` header file set plus compiled
seed, engine, and QMC symbols in the existing library. The Joe--Kuo dataset,
license, and third-party notice are installed as documentation/data artifacts.
No component, target, dependency, package variable, or CUDA claim is added;
the exact `random -> core` closure is unchanged.

## Minimal consumption

Request the smallest component that owns the API:

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_consumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense)

add_executable(my_consumer main.cc)
target_link_libraries(my_consumer PRIVATE ASC::dense)
```

The imported target propagates include directories, C++20, linkage-specific
definitions, and transitive ASC links. Do not hard-code include directories,
library filenames, `ASC_*_STATIC_DEFINE`, or transitive libraries.

Select the package explicitly and disable the user package registry:

```sh
cmake -S consumer -B consumer-build \
  -DASCCpp_DIR="/absolute/prefix/lib/cmake/ASCCpp" \
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
  -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON
```

`CMAKE_PREFIX_PATH=/absolute/prefix` is also supported. Quoting is required
when a path contains spaces.

## Metadata variables

After a successful lookup the config exposes:

| Variable | Meaning |
| --- | --- |
| `ASCCpp_VERSION` | exact installed package version, `0.9.0` for this candidate |
| `ASCCpp_KNOWN_COMPONENTS` | ordered architecture-controlled inventory of all 15 recognized components |
| `ASCCpp_AVAILABLE_COMPONENTS` | components built and exported by this producer package |
| `ASCCpp_<component>_FOUND` | truth value for a requested component or a component loaded through its transitive closure |

Do not infer availability from a target guessed by name. Query
`ASCCpp_AVAILABLE_COMPONENTS`, request the component, then consume the imported
target. An unrequested component outside the loaded closure need not have a
`FOUND` variable. `AVAILABLE_COMPONENTS` describes what the producer exported;
it does not assert that a particular consumer can discover an optional
component's external provider dependency.

The ordered known inventory is:

```text
core;utilities;expression;dense;sparse;random;
random_dense;random_sparse;cpp;
core_cuda;dense_cuda;sparse_cuda;random_cuda;
random_dense_cuda;random_sparse_cuda
```

A CUDA-disabled package makes exactly the first nine components available. A
CUDA-enabled package makes all fifteen available.

## Direct edges and loaded closures

| Requested component | Direct ASC dependencies | Exact transitive `ASC::` closure, including request |
| --- | --- | --- |
| `core` | none | `core` |
| `utilities` | `core` | `core`, `utilities` |
| `expression` | `core` | `core`, `expression` |
| `dense` | `core`, `expression` | `core`, `expression`, `dense` |
| `sparse` | `core`, `expression` | `core`, `expression`, `sparse` |
| `random` | `core` | `core`, `random` |
| `random_dense` | `random`, `dense` | `core`, `expression`, `dense`, `random`, `random_dense` |
| `random_sparse` | `random`, `sparse` | `core`, `expression`, `sparse`, `random`, `random_sparse` |
| `cpp` | all provider-free targets | all nine provider-free targets |
| `core_cuda` | `core` | `core`, `core_cuda` |
| `dense_cuda` | `dense`, `core_cuda` | `core`, `expression`, `dense`, `core_cuda`, `dense_cuda` |
| `sparse_cuda` | `sparse`, `core_cuda` | `core`, `expression`, `sparse`, `core_cuda`, `sparse_cuda` |
| `random_cuda` | `random`, `core_cuda` | `core`, `random`, `core_cuda`, `random_cuda` |
| `random_dense_cuda` | `random_dense`, `random_cuda`, `core_cuda` | `core`, `expression`, `dense`, `random`, `random_dense`, `core_cuda`, `random_cuda`, `random_dense_cuda` |
| `random_sparse_cuda` | `random_sparse`, `random_cuda`, `core_cuda` | `core`, `expression`, `sparse`, `random`, `random_sparse`, `core_cuda`, `random_cuda`, `random_sparse_cuda` |

A no-component lookup is equivalent to required component `cpp`. It remains
provider-free.

## Required, optional, and repeated lookup

- An unknown required component makes `ASCCpp_FOUND` false.
- A known but unavailable required component makes `ASCCpp_FOUND` false.
- A missing optional component is nonfatal and its `FOUND` value is false.
- If a known optional CUDA component is present in the producer package but
  its CUDAToolkit dependency cannot be discovered, that optional component and
  its unloaded CUDA closure remain not found. Required provider-free
  components and `ASCCpp_FOUND` remain usable.
- A successful lookup includes exports in dependency order.
- Repeated lookups may extend the loaded closure; they must not corrupt an
  already loaded target, partially import a failed optional provider closure,
  or silently load an unrelated component.

For example:

```cmake
find_package(
  ASCCpp 0.9 CONFIG REQUIRED
  COMPONENTS dense
  OPTIONAL_COMPONENTS core_cuda
)
```

The CPU Dense closure remains usable both when `core_cuda` was not built and
when it is available in the ASCCpp package but CUDAToolkit discovery is
disabled or fails. In both cases `ASCCpp_core_cuda_FOUND` is false. Changing
`OPTIONAL_COMPONENTS` to required `COMPONENTS` makes the lookup fail instead.

`SameMinorVersion` accepts a non-`EXACT` `0.9` request against `0.9.0`.
`EXACT 0.9.0` succeeds. `0.9.1`, `0.8`, `0.10`, and `1.0` do not match this
candidate.

## CUDA discovery and static/shared linkage

CUDAToolkit is discovered only when a required or optional requested
transitive closure contains a CUDA facet. Provider-free lookup succeeds even
when `CMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE`. A failed discovery for an
optional-only CUDA closure is contained as described above; it does not make
an otherwise successful required provider-free closure unavailable.

The exact `ASC::` closure is separate from the global population of CMake
targets. `FindCUDAToolkit` may define helper `CUDA::` targets that are not
ASCCpp components and are not linked by an ASCCpp target.

Shared provider libraries privately link their implementation SDK targets.
Static exports may expose those private dependencies as linkage-only
requirements so the final executable links successfully. The consumer must
still link only the requested `ASC::` target and let CMake carry the correct
closure.

## Package modes and relocation

The same metadata and target graph are checked for:

- the configured build-tree package;
- a copied build-tree package;
- the installed package;
- a copied/relocated install prefix;
- package and prefix paths containing spaces;
- static and shared producer builds; and
- CUDA-disabled and CUDA-enabled producer builds where available.

These are required evidence dimensions, not a claim that every Cartesian
combination was run. The checkpoint names each exact compiler, configuration,
linkage, and CUDA selection that was exercised.

An installed config or imported target must not contain an absolute source or
build path. Build-tree packages naturally reference their build outputs and
are movable only through the separately checked copied-build-tree contract.

ASCCMake is required to build ASCCpp itself but is not discovered by the
installed ASCCpp package. CUDA-free installed consumers require only CMake, a
C++20 toolchain, and the requested ASCCpp artifacts.

See [support matrix](support-matrix.md) for evidence boundaries and
[downstream integration](downstream-integration.md) for the isolated consumer
workflow.
