# asc-cpp

`asc-cpp` is the C++20 numerical foundation of AI4SciComp. Its implementation
preserves the selected design and behavior of MdeCpp while presenting a smaller,
installable package in namespace `asc`.

The architecture is compositional: descriptors define extents, mappings turn
logical coordinates into storage offsets, views borrow typed element access,
and owners manage storage independently of execution choice. The repository is
moving to that canonical model in reviewed milestones. Linalg M1 provides
seven canonical serial-reference BLAS operations over canonical Array views.
Random M1 adds an explicit Philox4x32-10 key/counter engine, a unit-uniform
transform, and deterministic serial bulk fill. Broader algebra and inherited
Random samplers remain on the retained MdeCpp-derived compatibility path.

## Functionality

| Component | CMake target | Main functionality | Direct dependency |
| --- | --- | --- | --- |
| `core` | `ASC::core` | Canonical types, status/contracts, host resources, move-only buffers, and serial contexts/events; retained diagnostics and CPU/OpenMP/CUDA memory/dispatch compatibility | C++ standard library |
| `array` | `ASC::array` | Canonical mixed 64-bit extents, checked mappings, typed non-owning views, and a move-only host tensor owner; retained dense/sparse/expression arrays remain for compatibility | `ASC::core` |
| `utilities` | `ASC::utilities` | Typed configuration values backed by standard containers, command-line option parsing, and timers | `ASC::core` |
| `linalg` | `ASC::linalg` | Canonical `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, and `Gemm` through a compiled serial-reference provider; broader algebra, factors, solvers, and Eigen/MKL integration remain compatibility-only or deferred | `ASC::array`, `ASC::core` |
| `random` | `ASC::random` | Canonical Philox4x32-10 counter generation, `Uniform01<float/double>`, and explicit-context deterministic fill over Array views | `ASC::array`, `ASC::core` |
| `cpp` | `ASC::cpp` | Convenience target, `<asc/cpp.h>` aggregate, and inherited Linalg/Random compatibility headers | All components |

Geometry, integration, meshes, finite elements, ODE solvers, kinetic models,
analysis, and visualization remain outside this repository. See the
[architecture](docs/architecture.md) and
[migration inventory](docs/migration/inventory.md). Developers extending a
component should also read the [build-system architecture](docs/build-system.md).
The canonical Array contract and its compatibility boundary are in
the [Array module guide](docs/modules/array.md) and
[Array migration guide](docs/migration/array.md).
The canonical Linalg contract and legacy-family classification are in the
[Linalg module guide](docs/modules/linalg.md) and
[Linalg migration guide](docs/migration/linalg.md).
The canonical Random reproducibility contract and inherited-sampler boundary
are in the [Random module guide](docs/modules/random.md) and
[Random migration guide](docs/migration/random.md).

## Prerequisites

Required:

- a C++20 compiler;
- CMake 3.25 or newer;
- `asc-cmake` 0.1, installed or present as the sibling directory
  `../asc-cmake`;
- a CMake build tool such as Make or Ninja;
- GoogleTest only when `ASC_CPP_BUILD_TESTING=ON`.

Optional:

- OpenMP for the threaded loop backend;
- CUDA Toolkit for CUDA memory and kernel execution;
- Eigen 3.3 or newer for Eigen conversion and solver adapters;
- Intel oneAPI MKL, together with Eigen, for MKL-backed Eigen adapters.

The default library build is CPU-only and requires no numerical third-party
package. The canonical Linalg M1 path uses its compiled
`serial-reference` provider. BLAS and LAPACK are not separate build
dependencies; Eigen/MKL options affect the explicit legacy adapter and solver
path and do not become canonical M1 providers.
Canonical Random M1 is likewise serial and, in the default CPU-only build,
requires no standard distribution or optional provider library.

## Installation

Install `asc-cmake` first, or keep its checkout next to this repository. If it
is installed in a non-system prefix, add that prefix to `CMAKE_PREFIX_PATH`.

### Personal computer

On a machine where `/usr/local` is writable through `sudo`:

```bash
git clone https://github.com/AI4SciComp/asc-cpp.git
cd asc-cpp
cmake -S . -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_BUILD_EXAMPLES=OFF
cmake --build build/release --parallel
sudo cmake --install build/release
```

Only installation needs elevated permission. Add
`-DCMAKE_INSTALL_PREFIX=/absolute/prefix` and omit `sudo` for any writable
prefix.

### Supercomputer or other unprivileged machine

Load the compiler/CMake modules supplied by the site, then install to a private,
versioned prefix:

```bash
module load gcc cmake  # Example only; use the site's module names.

asc_cpp_prefix="$HOME/.local/opt/asc-cpp/0.1.0"
asc_cmake_prefix="$HOME/.local/opt/asc-cmake/0.1.0"
cmake -S . -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$asc_cpp_prefix" \
  -DCMAKE_PREFIX_PATH="$asc_cmake_prefix" \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_BUILD_EXAMPLES=OFF
cmake --build build/release --parallel "${SLURM_CPUS_PER_TASK:-2}"
cmake --install build/release
```

No administrative permission is required. Put the two prefixes in a site/user
module, toolchain file, job script, or untracked `CMakeUserPresets.json` rather
than committing machine-specific paths.

## Use from another repository

```cmake
cmake_minimum_required(VERSION 3.25)
project(MySimulation LANGUAGES CXX)

find_package(ASCCpp 0.1 CONFIG REQUIRED COMPONENTS linalg)

add_executable(simulation main.cc)
target_link_libraries(simulation PRIVATE ASC::linalg)
```

Configure with the installation prefix if it is not in CMake's default search
path:

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="$HOME/.local/opt/asc-cpp/0.1.0"
cmake --build build --parallel
```

Canonical Linalg M1 program:

```cpp
#include <asc/array.h>
#include <asc/linalg.h>

#include <utility>

int main() {
  using Shape = asc::Extents<2>;
  using Vector = asc::Tensor<double, Shape>;

  auto x_result = Vector::Create(Shape{});
  auto y_result = Vector::Create(Shape{});
  if (!x_result.ok() || !y_result.ok()) return 1;

  Vector x = std::move(x_result).value();
  Vector y = std::move(y_result).value();
  auto x_view = x.View();
  auto y_view = y.View();
  if (!x_view.ok() || !y_view.ok()) return 1;

  x_view.value()(0) = 1.0;
  x_view.value()(1) = 2.0;
  y_view.value()(0) = 3.0;
  y_view.value()(1) = 4.0;

  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const asc::Status status =
      asc::Axpy(context, 2.0, x_view.value(), y_view.value());
  return status.ok() && y_view.value()(0) == 5.0 &&
                 y_view.value()(1) == 8.0
             ? 0
             : 1;
}
```

Existing code may continue to include narrow legacy headers such as
`<asc/linalg/blas.h>`; those context-free overloads are not the canonical M1
contract. `<asc/linalg.h>` intentionally does not
re-export Array ownership, so code constructing `Tensor` or `TensorView`
operands includes `<asc/array.h>` explicitly while still linking only
`ASC::linalg`.

Canonical Random code requests component `random`, includes both
`<asc/array.h>` and `<asc/random.h>`, and supplies literal `RandomKey` and
`RandomCounter` values. The Random umbrella does not re-export Array ownership.
Inherited headers such as `<asc/random/halton.h>` remain available through the
`ASC::cpp` compatibility surface; they are not supplied by the minimal
`ASC::random` component.

A canonical fill always makes random state explicit:

```cpp
const asc::Status random_status = asc::FillRandom(
    asc::ExecutionContext::Serial(), destination_view,
    asc::Uniform01<double>{}, asc::RandomKey{0x72616e646f6d5f31ULL},
    asc::RandomCounter{0, 0});
```

Here `destination_view` is a writable canonical Array view. See the Random
module guide for a complete owner/view example and the exact logical
partition guarantee.

Applications may link `ASC::cpp` and include `<asc/cpp.h>`. Reusable libraries
should select the narrowest component target and header.

## Build options

| Option | Default | Meaning |
| --- | --- | --- |
| `ASC_CPP_PRECISION` | `double` | Selects the library-wide `asc::real_t` as `single` or `double`. |
| `ASC_CPP_ENABLE_ASSERTIONS` | `ON` | Enables ASC runtime assertions. |
| `ASC_CPP_ENABLE_EXCEPTIONS` | `ON` | Uses exceptions for recoverable ASC errors. |
| `ASC_CPP_ENABLE_OPENMP` | `OFF` | Enables OpenMP dispatch and exports `OpenMP::OpenMP_CXX`. |
| `ASC_CPP_ENABLE_CUDA` | `OFF` | Enables the CUDA language, memory backend, kernels, cuBLAS, and cuSPARSE. |
| `ASC_CPP_ENABLE_EIGEN` | `OFF` | Enables the compatibility `<asc/linalg/eigen.h>` adapter and exports Eigen; it does not add a canonical provider. |
| `ASC_CPP_ENABLE_MKL` | `OFF` | Enables the compatibility Eigen/MKL path; requires the Eigen option and does not add a canonical provider. |
| `ASC_CPP_WARNINGS_AS_ERRORS` | `OFF` | Promotes project warnings to errors. |
| `ASC_CPP_ENABLE_SANITIZERS` | `OFF` | Enables address and undefined-behavior sanitizers. |

For CUDA, compile translation units that instantiate ASC device algorithms as
CUDA (normally use a `.cu` suffix). Set `CMAKE_CUDA_ARCHITECTURES` for the GPUs
that will run the result. Full backend details are in
[optional backends](docs/optional-backends.md).

## Development and quality checks

```bash
cmake --preset strict
cmake --build --preset strict --parallel
ctest --preset strict

cpplint --recursive include/asc src tests examples
```

The migrated MdeCpp behavioral suite contains more than 500 CPU tests. The
repository additionally checks every public header in isolation, multi-TU ODR
safety, every installed component, package relocation, and unknown-component
rejection. See [testing](docs/testing.md) and the [API map](docs/api.md).

## Provenance and license

The selected `generic/core`, `generic/device`, `generic/utility`,
`generic/array`, `algebra`, and `random` sources were adapted at source level
from the project owner's MdeCpp repository at commit
`f6294e9079262682ce63ae7ff2d8a643e658bf5d`. Namespace, include paths, package
boundaries, build system, install interface, and identified correctness or
cpplint defects were changed for `asc-cpp`. The precise disposition is recorded
in [docs/migration/inventory.md](docs/migration/inventory.md).

`asc-cpp` carries the Apache License 2.0 in [LICENSE](LICENSE). Notices for
adapted random-generator material are in
[THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES).
