# Build-system architecture

asc-cpp adopts the strongest part of MdeCpp's CMake design: ownership follows
the source tree. A component's build description sits beside its implementation
instead of contributing source and header lists to a growing root file. The
design is adapted to asc-cpp's installable, component-oriented package rather
than copying MdeCpp's monolithic target and directory-global build flags.

## Directory responsibilities

```text
CMakeLists.txt                  project policy and orchestration
cmake/dependencies.cmake       focused third-party discovery
cmake/ASCCppTargetHelpers.cmake
                                common compiled/interface target policy
cmake/ASCCppConfig.cmake.in    installed package dependency metadata
config/CMakeLists.txt          generated configuration header
src/CMakeLists.txt             dependency-ordered component composition
src/<component>/CMakeLists.txt target, files, and direct dependencies
examples/CMakeLists.txt        example executables
tests/CMakeLists.txt           tests and package-consumer checks
```

The root file owns settings that affect the complete build: public options,
precision validation, CUDA language enablement, the `asc-cmake` entry points,
package installation, and whether examples and tests are included. It does not
list component implementation files.

`config/CMakeLists.txt` generates `<asc/config/_config.h>`. The generated file
is an implementation detail of the stable `<asc/core/config.h>` facade, but is
part of the core target's public header file set so installed consumers receive
the exact ABI configuration used to build the libraries.

`src/CMakeLists.txt` expresses construction order and creates the `ASC::cpp`
umbrella. Every component subdirectory owns:

- its compiled or interface target and `ASC::` build-tree alias;
- implementation sources and the complete public `FILE_SET`;
- only its direct component dependencies;
- optional libraries whose interfaces are exposed by that component.

The order is deliberately readable as the dependency graph: core first,
independent utilities and array layers next, then linalg, then random. CMake
target dependencies, rather than this textual order, remain the source of truth
for the actual build graph.

## Shared target policy

`ASCCppTargetHelpers.cmake` contains the small amount of policy common to all
components: exported names, C++20, build/install include interfaces, project
warnings for implementation compilation, shared-library definitions and
runtime paths, and CUDA compilation requirements. It has separate helpers for
compiled and interface targets so private warning flags never leak into a
consumer.

The helpers complement the public `asc-cmake` API. Project warnings and
sanitizers come from `asc_add_project_options`; installation, export files, and
relocatable package configuration come from `asc_install_package`; test
registration comes from `asc_register_test`. asc-cpp does not duplicate those
facilities locally.

## Linalg M1 target transition

Linalg M1 changes `asc_linalg` from an interface library to a normal compiled
component configured with the compiled-target helper. Its local
`src/linalg/CMakeLists.txt` owns:

- the canonical and compatibility public-header file set;
- capability/runtime translation and compiled serial-reference sources;
- direct public links to exactly `asc_array` and `asc_core` in the base build;
- conditional Eigen/MKL usage requirements needed only by explicit legacy
  compatibility headers.

Public constrained operation facades remain header-visible, but dispatch to
compiled `float` and `double` kernels. The installed
`detail/reference_kernels.h` declarations support that dispatch and are not a
user API. No public provider target, SDK type, global source list, or root-level
implementation source is introduced.

Canonical header-isolation tests link `ASC::linalg` rather than the aggregate.
The dependency-boundary gate inspects both the public source includes and the
base target edges. The installed Linalg-only consumer explicitly includes both
`<asc/array.h>` for operand construction and `<asc/linalg.h>` for operations,
then checks that `find_package(ASCCpp COMPONENTS linalg)` and linkage to only
`ASC::linalg` supply the compiled library plus its two ASC dependencies after
relocation. Linalg does not re-export Array ownership declarations.

## Random M1 target separation

Random M1 keeps `asc_random` as a normal compiled target while narrowing its
public file set and base usage requirements. `src/random/CMakeLists.txt` owns:

- the canonical `<asc/random.h>` umbrella, key/counter types, engine,
  distribution, fill, and installed template validation helper;
- the compiled Philox4x32-10 definition;
- the inherited permutation and Sobol definitions needed by aggregate
  compatibility callers;
- direct public links to exactly `asc_array` and `asc_core`; and
- no direct Linalg, aggregate, or optional-SDK requirement.

The inherited Random headers are installed through the `asc_cpp` aggregate
file set during the compatibility window. Their compiled definitions remain in
`asc_random` so aggregate callers link one implementation, but the headers and
symbols are not part of the minimal component's canonical API contract. This
preserves physical include paths for existing aggregate consumers without
making them package-component `random` header promises.

The project-wide compiled-target helper and `ASC::core` still propagate legacy
OpenMP/CUDA compilation and public SDK requirements when those options are
enabled. The default build has the intended minimal boundary, while full
transitive SDK isolation waits for cross-module provider hardening. Random
does not interpret those options as providers.

Canonical header-isolation and dependency tests link only `ASC::random`. The
installed Random-only consumer explicitly includes `<asc/array.h>` for view
construction and `<asc/random.h>` for generation, requests package component
`random`, links only `ASC::random`, and verifies a frozen output after
relocation. The inherited Random regression links `ASC::cpp`.

## Adding or changing a component

For an existing component, edit only its local `src/<component>/CMakeLists.txt`
when adding a source or public header. Keep headers in the target's named
`public_headers` file set so installation and IDE metadata remain synchronized.

For a new component:

1. create `src/<component>/CMakeLists.txt` and declare one target;
2. use the compiled or interface target helper and add an `ASC::<component>`
   alias;
3. declare the component's public header file set and direct target links;
4. add the directory to `src/CMakeLists.txt` in a readable dependency order;
5. add the target to `asc_install_package` and the package-component mapping;
6. add build-tree isolation and installed-consumer tests;
7. update the README functionality table and architecture graph.

Do not communicate through directory scope with accumulated `SOURCES` or
`HEADERS` variables. Do not add global include directories, compiler options,
or link libraries. Those MdeCpp-era techniques are unnecessary now that each
module is a real exported target with explicit usage requirements.
