# ASCCpp package capabilities

Status: unreleased `0.9.0` Milestone 8 candidate

Date: 2026-07-27

ASCCpp installs a CMake config package with explicit components. A component
request is a capability request: it imports that target and its exact
transitive ASC target closure. It does not enable a provider, add an
unimplemented operation, or make every ASCCpp target available.

## Minimal use

Request the smallest component that owns the API being used:

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_consumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense)

add_executable(my_consumer main.cc)
target_link_libraries(my_consumer PRIVATE ASC::dense)
```

The imported target propagates its include directories, C++20 requirement,
static/shared usage definitions, and transitive ASC link interface. Do not
hard-code ASCCpp library filenames, include paths, `ASC_*_STATIC_DEFINE`
macros, or transitive libraries.

Use either an explicit package directory:

```sh
cmake -S consumer -B consumer-build \
  -DASCCpp_DIR="/absolute/prefix/lib/cmake/ASCCpp" \
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
  -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON
```

or a prefix:

```sh
cmake -S consumer -B consumer-build \
  -DCMAKE_PREFIX_PATH="/absolute/prefix" \
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
  -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON
```

Milestone validation does not write or consult the CMake user package
registry.

## Package metadata

After `find_package`, the config package exposes:

| Variable | Meaning |
| --- | --- |
| `ASCCpp_VERSION` | exact configured package version, `0.9.0` for this candidate |
| `ASCCpp_KNOWN_COMPONENTS` | every component name understood by this package generation |
| `ASCCpp_AVAILABLE_COMPONENTS` | components built and exported by this particular package |
| `ASCCpp_<component>_FOUND` | CMake Boolean for a requested or loaded component |
| `ASCCpp_FOUND` | overall result after required-component and imported-target checks |
| `ASCCpp_NOT_FOUND_MESSAGE` | diagnostic text when the request cannot be satisfied; prose is not stable API |

`ASCCpp_KNOWN_COMPONENTS` for the candidate is:

```text
core
utilities
expression
dense
sparse
random
random_dense
random_sparse
core_cuda
dense_cuda
sparse_cuda
random_cuda
random_dense_cuda
random_sparse_cuda
cpp
```

A CUDA-disabled package has these available components:

```text
core;utilities;expression;random;dense;sparse;
random_dense;random_sparse;cpp
```

A successfully configured CUDA-enabled package adds all six `_cuda`
components. Availability says that the target exists in that package; it is
not real-device runtime or parity evidence.

Do not assume an unrequested `ASCCpp_<component>_FOUND` variable is defined.
Test membership in `ASCCpp_AVAILABLE_COMPONENTS` when inspecting the complete
package inventory, and request the component when a target is required.

## Version requests

The generated package version file uses CMake `SameMinorVersion`
compatibility. For an installed `0.9.0` package:

| Request | Result |
| --- | --- |
| `find_package(ASCCpp 0.9 CONFIG ...)` | compatible |
| `find_package(ASCCpp 0.9.0 CONFIG ...)` | compatible |
| `find_package(ASCCpp 0.9.0 EXACT CONFIG ...)` | exact |
| request newer than `0.9.0` | incompatible because the installed package is older |
| request for another minor such as `0.8` or `0.10` | incompatible |
| request for another major | incompatible |

The package's version comparison does not make `0.9.0` a released artifact.
It describes how this candidate's generated metadata behaves.

## Components and closures

The closure column lists all imported ASC targets. The direct-dependency
column is the target's public link interface, not a flattened closure.

| Requested component | Imported target closure | Exact direct ASC dependencies |
| --- | --- | --- |
| `core` | `core` | none |
| `utilities` | `core;utilities` | `core` |
| `expression` | `core;expression` | `core` |
| `dense` | `core;expression;dense` | `core;expression` |
| `sparse` | `core;expression;sparse` | `core;expression` |
| `random` | `core;random` | `core` |
| `random_dense` | `core;expression;random;dense;random_dense` | `random;dense` |
| `random_sparse` | `core;expression;random;sparse;random_sparse` | `random;sparse` |
| `cpp` | all six provider-free modules, both random storage facets, `cpp` | all six modules and both facets |
| `core_cuda` | `core;core_cuda` | `core` |
| `dense_cuda` | `core;expression;dense;core_cuda;dense_cuda` | `dense;core_cuda` |
| `sparse_cuda` | `core;expression;sparse;core_cuda;sparse_cuda` | `sparse;core_cuda` |
| `random_cuda` | `core;random;core_cuda;random_cuda` | `random;core_cuda` |
| `random_dense_cuda` | `core;expression;random;dense;random_dense;core_cuda;random_cuda;random_dense_cuda` | `random_dense;random_cuda;core_cuda` |
| `random_sparse_cuda` | `core;expression;random;sparse;random_sparse;core_cuda;random_cuda;random_sparse_cuda` | `random_sparse;random_cuda;core_cuda` |

All imported targets use the `ASC::` namespace. The corresponding build-tree
targets and installed targets have the same public names.

`ASC::cpp` deliberately excludes every provider. An application needing the
provider-free aggregate and a provider requests both components and links both
targets explicitly.

## Required, optional, and unknown components

An unknown required component makes `ASCCpp_FOUND` false. A known but
unavailable required component, such as `core_cuda` in a CUDA-disabled
package, also makes the package not found. No placeholder target is created.

Optional misses remain nonfatal:

```cmake
find_package(
  ASCCpp 0.9 CONFIG REQUIRED
  COMPONENTS dense
  OPTIONAL_COMPONENTS dense_cuda
)

if(ASCCpp_dense_cuda_FOUND)
  target_link_libraries(my_consumer PRIVATE ASC::dense_cuda)
endif()
```

The required provider-free component remains usable if the optional CUDA
facet is absent. Downstream logic should test the component variable before
referencing the optional target.

## CUDA discovery boundary

A request whose closure contains a CUDA component invokes
`find_dependency(CUDAToolkit 12)`. The consumer environment must make the
matching CUDAToolkit config/find-module inputs available. The closure imports
only the `ASC::` targets in the requested component closure. ASCCpp separately
fixes each imported target's exact link interface: direct public ASC edges are
always present; a static export may retain its private provider library as a
`LINK_ONLY` entry, while a shared export does not expose that private link
entry. The provider implementation dependencies are:

- `core_cuda` reaches CUDA Runtime;
- `dense_cuda` reaches CUDA Runtime and cuBLAS;
- `sparse_cuda` reaches CUDA Runtime and cuSPARSE; and
- random CUDA facets reach CUDA Runtime through `core_cuda`.

`FindCUDAToolkit` may define additional `CUDA::` targets while satisfying that
dependency. Those helper/imported targets are CMake toolkit state, not ASCCpp
components. Their existence neither adds an `ASC::` target to the requested
closure nor adds them to an ASCCpp target's link interface. Inspect the
ASCCpp imported target's `INTERFACE_LINK_LIBRARIES` when auditing the link
contract; do not treat the complete set of targets created by
`FindCUDAToolkit` as that interface.

A provider-free request does not discover CUDAToolkit even when the
installation contains CUDA components. The base modules and `ASC::cpp` remain
usable on a machine without a CUDA SDK.

CUDA is a producer build option, not a consumer-side target mutation.
`ASC_CPP_ENABLE_CUDA` controls which targets are built when configuring
ASCCpp; setting it in an unrelated installed consumer does not add components
to an existing package.

## Build tree, install tree, copies, and relocation

The candidate exports the same component names from:

- its generated build-tree package directory;
- a copied build tree whose generated package references remain valid;
- an installed prefix;
- a copied or relocated installed prefix; and
- source/build/install paths containing spaces.

Installed package files must not expose an absolute source or build path.
Relocation preserves the prefix-relative imported locations. Moving only one
library or targets file out of its package layout is not a supported
relocation.

Static and shared packages are separate producer builds. A consumer must not
mix target files or libraries from the two builds. Likewise, a copied build
tree is a test of the generated build-tree export, not a substitute for an
installable redistribution artifact.

## Isolated consumption

Each component is tested from an isolated consumer that includes only public
headers and links only its requested target. This checks that dependency
closure is encoded by targets rather than by ambient source-tree include paths
or previously loaded components.

For application guidance, see [downstream
integration](downstream-integration.md). For the distinction between package,
source, ABI, provider, and numerical compatibility, see [API
compatibility](api-compatibility.md).
