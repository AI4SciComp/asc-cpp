# Codex implementation prompt: extract `asc-cpp` carefully from MdeCpp

Revision: 4.0  
MdeCpp audit commit: `471e2a41b4ac3703d118db87a1e2a827d95ef652`  
Audit date: 2026-07-20

Copy this entire document into Codex while its working directory is the
`AI4SciComp/asc-cpp` repository. This prompt is an implementation contract, not
merely a design suggestion.

## Role and objective

You are the principal C++/CMake engineer implementing `AI4SciComp/asc-cpp`.
Migrate the reusable foundation of `escapetiger/MdeCpp` into a clean,
installable, component-oriented C++ library. Reuse the completed shared CMake
infrastructure in the sibling `../asc-cmake` repository. Preserve useful
behavior and history where practical, but establish a clean public API in the
single namespace `asc`.

The first release contains exactly five production modules:

1. `core`, merging MdeCpp `generic/core` and `generic/device`;
2. `utilities`, migrated from `generic/utility`;
3. `array`, migrated from `generic/array`;
4. `linalg`, migrated from MdeCpp `algebra`;
5. `random`, migrated from MdeCpp `random`.

Do not migrate MdeCpp by copying complete top-level directories. The current
MdeCpp tree mixes infrastructure, approximation, geometry, physics, and
applications. `asc-cpp` receives only domain-neutral C++ scientific-computing
primitives.

The audited disposition is:

- `generic/core`, `generic/device`, `generic/utility`, `generic/array`,
  `algebra`, and `random` are candidates for `asc-cpp`, subject to the per-file
  dependency audit below;
- `geometry` belongs to `asc-xde` because it models computational domains,
  measures, point membership, boundaries, and coordinate maps used by equation
  discretizations;
- `functional/operator.h`, `functional/linear.*`, and `functional/basis.*`
  belong to `asc-xde` because they define mappings, derivatives/Jacobians,
  forward/backward actions, bases, orthogonality, interpolation, and function
  approximation;
- `functional/nonlinear.h` currently defines a Maxwellian distribution and
  belongs to `asc-kinetic`, not `asc-xde` or `asc-cpp`;
- `miniapps/vpfp` is a kinetic application and belongs to `asc-kinetic` or an
  integration/example repository such as `asc-lab`, not `asc-cpp`;
- MdeCpp `examples`, `miniapps`, `legacy`, experimental `dev`, and future
  physics/approximation layers are not copied into `asc-cpp` automatically.

Do not build, install, namespace-convert, delete, or otherwise modify code
assigned to another repository while implementing `asc-cpp`. Produce a
handoff manifest for it instead.

## Non-negotiable constraints

- Put every production C++ declaration and definition in `namespace asc`.
- Do not introduce module namespaces such as `asc::array` or `asc::linalg`.
- Do not add `namespace mdecpp = asc` or another compatibility namespace.
- Remove production occurrences of `namespace mdecpp`, `mdecpp::`, old public
  include paths, and public `MDECPP_` macros/include guards.
- Use `ASC_` for public macros and `ASC_CPP_` where a project-specific prefix is
  clearer.
- Use modern target-based CMake. Never set global include directories, global
  compile definitions, or global compiler flags for library requirements.
- Keep the module dependency graph acyclic.
- Classify ownership by semantic responsibility and dependency direction, never
  merely by the current MdeCpp directory name.
- Do not introduce `geometry`, `functional`, `operator`, `basis`, `Maxwellian`,
  PDE/XDE, mesh, discretization, weak-form, boundary-condition, time-integration,
  or physics APIs into `asc-cpp`.
- Do not copy private implementation details from `asc-cmake` into this repo.
  Consume its documented public CMake API.
- Do not invent an `asc-cmake` function. Inspect the local sibling repository
  and use the API that actually exists.
- Do not modify `../asc-cmake` or the MdeCpp source repository.
- Avoid new mandatory third-party dependencies. Optional CUDA, OpenMP, Eigen,
  BLAS, and LAPACK support must remain genuinely optional.
- Follow the checked-in formatting/lint configuration and Google C++ style when
  no more specific repository rule exists.
- Do not commit, push, publish a release, or open a pull request unless asked.

## Phase 0: inspect before editing

First inspect and report:

1. `AGENTS.md`, `CONTRIBUTING.md`, `README.md`, existing source layout,
   `CMakeLists.txt`, presets, workflows, and the working-tree status in
   `asc-cpp`;
2. the public modules, functions, examples, templates, tests, and package rules
   in `../asc-cmake`;
3. MdeCpp's root and module CMake files and all files under `generic/core`,
   `generic/device`, `generic/utility`, `generic/array`, `algebra`, `random`,
   `geometry`, `functional`, and relevant tests/examples/miniapps;
4. every cross-module include and every optional backend dependency;
5. existing tests and examples that specify observable behavior.

Locate MdeCpp in this order:

- a user-provided local path;
- `../MdeCpp`;
- a read-only clone into a temporary directory, if network access is available.

If the source cannot be obtained, stop and explain the blocker. Do not fabricate
implementations from filenames. If `../asc-cmake` is unavailable, stop and ask
for its location or release/tag. If the `asc-cpp` tree contains unrelated user
changes, preserve them and work around them; ask before overwriting a conflict.

Before editing, create a migration inventory containing old path, new path,
destination repository, owning module, direct internal/third-party dependencies,
public/private status, template/compiled status, maturity, known defects, and
related tests. Every inspected MdeCpp production file must have one disposition:

```text
MIGRATE_TO_ASC_CPP
HANDOFF_TO_ASC_XDE
HANDOFF_TO_ASC_KINETIC
HANDOFF_TO_ASC_LAB
DEFER_LEGACY_OR_EXPERIMENTAL
```

No file may be silently omitted. When ownership is ambiguous, defer it and
record the question; do not pull it into the lowest-level library “just in
case.”

## Architectural ownership test

Apply these questions to every candidate file:

1. Can the API be useful without knowing about differential equations,
   computational domains, bases, discretization, or a physical model?
2. Does it depend only on language/runtime facilities or other `asc-cpp`
   primitives?
3. Would `asc-cpp` remain conceptually coherent if `asc-xde` and
   `asc-kinetic` did not exist?
4. Is the abstraction stable enough to become a low-level public dependency?
5. Does moving it downward avoid a dependency cycle, rather than merely moving
   domain vocabulary into the foundation?

Normally migrate a file to `asc-cpp` only if all five answers support doing so.
Use these ownership definitions:

| Repository | Owns | Must not own |
|---|---|---|
| `asc-cpp` | domain-neutral types, memory/execution, arrays, generic linalg, general RNG/sampling, small general utilities | computational domains, bases, PDE operators, physical distributions/applications |
| `asc-xde` | domain geometry, function/operator abstractions, bases/approximation, discretization-facing mathematical structures | device allocation internals, general containers, physics-specific models |
| `asc-kinetic` | Maxwellians, distribution functions with kinetic semantics, collision/transport models, VPFP application logic | general arrays/linalg, equation-agnostic build infrastructure |
| `asc-lab` | cross-repository demonstrations, experiments, benchmarks, end-to-end workflows | canonical low-level library implementation |

The desired repository dependency direction is:

```text
asc-cmake
   ↑
asc-cpp
   ↑
asc-xde
   ↑
asc-kinetic
   ↑
asc-lab (integration and experiments)
```

An optional dependency may skip a layer, but dependencies must never point
downward from `asc-cpp` to `asc-xde` or `asc-kinetic`.

## Audited MdeCpp routing decisions

At the audit commit, use the following as the default decision. Re-inspect the
latest local/remote commit and revise individual rows only with written evidence.

| MdeCpp area | Destination | Reason/action |
|---|---|---|
| `generic/core/*` | `asc-cpp/core` | Fundamental concepts, types, errors, casts, numeric helpers |
| `generic/device/*` | `asc-cpp/core` | Foundational memory and execution backend; merge into core |
| `generic/utility/*` | `asc-cpp/utilities` | General timer and option parser |
| `generic/array/*` | `asc-cpp/array` | Domain-neutral multidimensional containers and expressions |
| `algebra/*` | `asc-cpp/linalg` | Domain-neutral array algebra and optional numerical adapters |
| `random/*` | `asc-cpp/random` | General pseudo/quasi-random generation and sampling |
| `geometry/geometry.h` | `asc-xde/geometry` | Computational-domain interface and boundary queries |
| `geometry/orthotope.h` | `asc-xde/geometry` | Domain bounds/faces and boundary membership |
| `geometry/ball.h` | `asc-xde/geometry` | Domain measure, boundary membership, coordinate maps |
| `functional/operator.h` | `asc-xde/operators` | Mapping, derivative, Jacobian, JVP/VJP abstraction |
| `functional/linear.*` | `asc-xde/operators` | Constant/linear/affine mappings built on operator abstraction |
| `functional/basis.*` | `asc-xde/approximation` | Polynomial/basis/interpolation and mass structures |
| `functional/nonlinear.h` | `asc-kinetic/distributions` | Maxwellian is a kinetic distribution, not a generic nonlinear operator library |
| `miniapps/vpfp/*` | `asc-kinetic` or `asc-lab` | Kinetic end-to-end application |
| `legacy/*`, `dev/*` | defer | Review separately; never migrate by default |

Important audit findings that must not be copied blindly:

- `geometry/CMakeLists.txt` currently lists no geometry headers or sources, so
  these headers are not an integrated, package-tested module;
- `geometry/geometry.h` refers to a type `T` that is not declared by the class
  template as currently written;
- geometry headers rely on project macros/types without consistently including
  their declaring headers, so header self-containment must be repaired in the
  future `asc-xde` migration;
- exact floating-point comparisons in `Orthotope::IsOnBoundary` require an
  explicit tolerance policy before becoming a stable public API;
- documentation/examples in `ball.h` contain namespace/name inconsistencies;
- the functional CRTP hierarchy mixes virtual capability methods with static
  dispatch and contains partially implemented/default derivative paths; its API
  must be stabilized in `asc-xde`, not frozen through an asc-cpp migration;
- `functional/nonlinear.h` is named generically but contains a specifically
  kinetic Maxwellian; classify by the actual class semantics;
- the root MdeCpp documentation describes future geometry/approximation/physics
  layering; aspirational or in-development code is not automatically release
  quality.

Do not fix these cross-repository files during the asc-cpp task. Capture them in
`docs/migration/handoff.md` with source commit, destination, dependencies,
defects, tests to port, and recommended next action.

## Required repository layout

Adapt only where the actual `asc-cmake` contract requires it:

```text
asc-cpp/
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/
│   ├── ASCCppConfig.cmake.in
│   └── dependencies.cmake
├── include/asc/
│   ├── core/
│   ├── utilities/
│   ├── array/
│   ├── linalg/
│   ├── random/
│   └── asc.h
├── src/
│   ├── core/
│   ├── utilities/
│   ├── array/
│   ├── linalg/
│   └── random/
├── tests/
│   ├── core/
│   ├── utilities/
│   ├── array/
│   ├── linalg/
│   ├── random/
│   └── package/
├── examples/
├── docs/
├── LICENSE
└── README.md
```

Headers intended for users belong under `include/asc/<module>/`. Private
headers should remain under `src/` or another non-installed private directory.
All public includes must use forms such as:

```cpp
#include <asc/core/device.h>
#include <asc/array/marray.h>
#include <asc/linalg/blas.h>
```

Do not use relative traversal between public headers.

## Template and metaprogramming architecture

MdeCpp is template- and metaprogramming-heavy. Do not force it into a
conventional declaration-in-`include/`, definition-in-`src/` split. In C++, a
consumer normally needs the complete definition of a function or class template
at the point of instantiation. The installed public header tree is therefore
both the API and much of the implementation.

Classify every migrated file and symbol before deciding its destination:

| Code category | Required location and treatment |
|---|---|
| Public class/function/variable template | `include/asc/<module>/`; definition visible to consumers |
| Concept, type trait, policy, expression template | `include/asc/<module>/`; normally header-only |
| `constexpr`/`consteval` algorithm | Public header when part of the API or required by templates |
| Small non-template function intentionally `inline` | Public header |
| Template implementation separated for readability | Installed `include/asc/<module>/detail/*_impl.h`, included by its public header |
| Public template implementation helper | Installed `detail/` header, but treated as non-API |
| Non-template declaration | Public header if users need it |
| Non-template definition | `src/<module>/*.cc` |
| Private non-template helper | `src/<module>/` and not installed |
| Stable large lookup table | Prefer one definition in `src/`, exposed through a non-template accessor; use an inline variable only with justification |
| Explicit template instantiation | `src/<module>/*.cc` or `.cu`, plus matching `extern template` declaration where useful |
| CUDA kernel/template required by callers | A CUDA-consumable installed header or a deliberately closed explicit-instantiation boundary |
| Generated configuration/export header | Generated into the build include tree and installed |

Use this pattern when separating a template for readability:

```cpp
// include/asc/array/marray.h
#ifndef ASC_ARRAY_MARRAY_H_
#define ASC_ARRAY_MARRAY_H_

#include <cstddef>

namespace asc {

template <typename T>
class MArray {
 public:
  T& operator[](std::size_t index);

 private:
  T* data_ = nullptr;
};

}  // namespace asc

#include <asc/array/detail/marray_impl.h>

#endif  // ASC_ARRAY_MARRAY_H_
```

```cpp
// include/asc/array/detail/marray_impl.h
#ifndef ASC_ARRAY_DETAIL_MARRAY_IMPL_H_
#define ASC_ARRAY_DETAIL_MARRAY_IMPL_H_

namespace asc {

template <typename T>
T& MArray<T>::operator[](std::size_t index) {
  return data_[index];
}

}  // namespace asc

#endif  // ASC_ARRAY_DETAIL_MARRAY_IMPL_H_
```

The public header owns inclusion of its implementation header. Documentation
must tell users not to include `detail/` directly. A `detail` path communicates
API instability; it does not mean the file can be omitted from installation.

Never move an unrestricted template definition only into a `.cc` file. That is
valid only when the supported type set is deliberately closed. For explicit
instantiation:

1. document the supported types;
2. put `extern template` declarations in the appropriate installed header when
   they provide a real compile-time/size benefit;
3. provide exactly one matching explicit instantiation definition in a compiled
   source;
4. export the instantiation correctly for shared-library builds;
5. test every supported instantiation from an external consumer;
6. ensure unsupported types either instantiate from a visible definition or
   fail with a clear compile-time diagnostic—never an unexplained link error.

Do not use explicit instantiation merely to make the project look like a
compiled library. Prefer open genericity unless compile-time measurements,
binary size, proprietary implementation concerns, or backend boundaries justify
a closed type set.

### Header self-sufficiency and ODR safety

Every installed public header must compile as the first include in a minimal
translation unit. It must include what it uses and must not rely on umbrella
header include order. For template definitions:

- avoid non-inline definitions with external linkage in headers;
- use `inline` variables for header-defined shared variables when appropriate;
- use `constexpr` rather than macros for typed constants where possible;
- ensure function-local statics and registration mechanisms are ODR-safe;
- do not put `using namespace` directives in headers;
- minimize platform and optional-backend headers in the common public surface;
- keep diagnostics and constraints close to the public template using concepts
  or focused `static_assert` messages;
- preserve ADL behavior when moving declarations into namespace `asc`.

Add automated header self-containment tests, preferably generated from the
installed header manifest. Compile representative template use from at least two
translation units and link them together to expose ODR violations.

### CMake modeling of template-heavy modules

Choose target type from actual contents, independently for every module:

- use an `INTERFACE` library when a module has no compiled source;
- use a normal static/shared library when it contains non-template definitions,
  explicit instantiations, backend adapters, lookup tables, or runtime code;
- do not add empty `.cc` files to force an `INTERFACE` module into a compiled
  target;
- do not create separate public CMake targets for `detail` implementation
  headers;
- list public and implementation headers through `FILE_SET HEADERS` when the
  supported CMake baseline and `asc-cmake` API permit it; otherwise use the
  installation helper supplied by `asc-cmake`;
- give header-only targets compile features, include directories, compile
  definitions, and transitive dependencies through their `INTERFACE`
  properties;
- give compiled targets the same usage requirements through `PUBLIC` properties
  when their headers expose them.

Expected initial character, subject to source inspection:

| Module | Expected form |
|---|---|
| `core` | Hybrid: templates/concepts in headers; errors, device/runtime and selected memory operations compiled |
| `utilities` | Mostly compiled, with small templates/inline helpers in headers |
| `array` | Predominantly header-only because containers and expressions are templates |
| `linalg` | Predominantly header-only generic algorithms plus optional compiled BLAS/LAPACK/backend adapters |
| `random` | Header templates plus compiled Sobol tables or other large fixed data/non-template algorithms |

Source inspection overrides this expectation. Record the reason for each final
target type in the migration inventory.

### Compile-time discipline

Metaprogramming can make build times and diagnostics unacceptable. Preserve
correctness first, then apply measured improvements:

- keep umbrella headers convenient but do not use them internally;
- include the narrowest public header needed;
- forward-declare only where legal and beneficial;
- avoid repeating expensive detection machinery across unrelated headers;
- prefer C++ concepts to deeply nested SFINAE when the selected language
  standard supports them and behavior is unchanged;
- do not rewrite stable template machinery solely for style;
- capture at least one clean-build timing/baseline and report it, but do not set
  an arbitrary CI performance gate in this task;
- never add a precompiled-header requirement to the installed consumer API.

### CUDA and templates

Inspect MdeCpp's host/device annotations and instantiation model carefully.
Ordinary CPU consumers must not need the CUDA language or CUDA headers when CUDA
is disabled. When CUDA is enabled:

- isolate CUDA-only parsing and runtime dependencies;
- propagate required compile definitions through the owning target;
- ensure installed template headers are valid under the intended host compiler
  and CUDA compiler paths;
- do not expose an implementation that requires downstream `.cu` compilation
  without documenting and testing that contract;
- prefer a compiled backend boundary when it avoids leaking CUDA into generic
  consumers, unless generic device templates fundamentally require visibility;
- test at least one template instantiation through the CUDA path when a toolkit
  and runner are available.

## Source migration map

### `core`

Merge these MdeCpp areas:

```text
generic/core/globals.*
generic/core/error.*
generic/core/casts.*
generic/core/concepts.h
generic/core/typedefs.h
generic/core/operators.h
generic/core/numeric.*
generic/device/cuda.*
generic/device/device.*
generic/device/forall.h
generic/device/memory.*
generic/device/memory_impl.h
```

`core` owns fundamental types and concepts, checked casts, errors, numeric
helpers, execution/backend selection, device abstractions, memory ownership and
synchronization, host/device annotations, and `forall` dispatch. It must not
include headers from any other asc-cpp module.

### `utilities`

Migrate `generic/utility/optparser.*` and `generic/utility/timer.*`.
`utilities` may depend on `core`; it must not pull in array, linalg, or random.

### `array`

Migrate MdeCpp's array containers, shapes, layouts, indexes, iterators, views,
expressions, dense/sparse storage, and umbrella header, including the families
`carray`, `uarray`, `mshape`, `mlayout`, `mindex`, `miterator`, `mobject`,
`dsmarray`, `spmarray`, `spmlayout`, `spmindex`, `spmiterator`, `expr`, and
`marray`. Keep template implementation headers installed when public templates
need them. `array` depends only on `core`.

### `linalg`

Rename the MdeCpp `algebra` module to `linalg`. Initially migrate `blas.h`,
`lapack.h`, `decomp.h`, and optional `eigen.h` without gratuitous API redesign.
It owns numerical array operations, reductions/products, matrix multiplication,
decomposition and solve facilities, and optional backend adapters. `linalg`
depends publicly on `array`; it must not depend on random or utilities.

Do not mechanically rename public functions merely because `algebra` became
`linalg`. A later API cleanup can split an over-broad `blas.h` after migration
tests exist.

### `random`

Migrate generator, permutation, sampler, pseudo, Latin hypercube, Halton,
Hammersley, Sobol, normal, and spherical facilities. Base generators use
`core`; array-valued samplers use `array`. Link `linalg` only if inspected code
actually calls its public API. Never create a reverse dependency from core,
array, or linalg to random.

### Rejected asc-cpp candidates

Do not create an asc-cpp module named `geometry`, `functional`, `operators`,
`basis`, `approximation`, or `distributions` in this task. In particular:

- do not move `Operator` into `core` merely because it is templated;
- do not move basis functions into `linalg` merely because they use matrices;
- do not move `Ball` or `Orthotope` into `array` merely because they store
  vectors;
- do not move `Maxwellian` into `random` merely because it is related to a
  probability density;
- do not use an umbrella `asc.h` to re-export APIs owned by other repositories.

Metaprogramming technique does not determine repository ownership. A CRTP
class can still be domain-level code.

## Required CMake target model

Create one target per component plus an umbrella interface target. A component
may be `INTERFACE` or compiled according to the template rules above. Follow the
actual naming helper supplied by `asc-cmake`, but the externally visible result
must be equivalent to:

| Build target | Build-tree and installed alias |
|---|---|
| `asc_core` | `ASC::core` |
| `asc_utilities` | `ASC::utilities` |
| `asc_array` | `ASC::array` |
| `asc_linalg` | `ASC::linalg` |
| `asc_random` | `ASC::random` |
| `asc_cpp` (INTERFACE) | `ASC::cpp` |

`ASC::cpp` links all five components and exists for convenient full-library
consumption. Users must also be able to link only the components they need.

The intended graph is:

```text
ASC::core
├── ASC::utilities
├── ASC::array
│   ├── ASC::linalg
│   └── ASC::random
└── ASC::random (for scalar-only facilities)
```

Express only direct dependencies in CMake and use `PUBLIC`/`PRIVATE` according
to whether public headers expose them. Do not encode dependencies twice.

Each component target must declare:

- its complete source/header list, including installed template implementation
  headers, using the asc-cmake convention;
- build and install include interfaces;
- the repository's selected C++ standard through target features;
- appropriate visibility/export definitions for compiled symbols and explicit
  instantiations if shared builds are supported;
- only target-specific warnings, definitions, and backend flags.

Support static builds at minimum. Support shared builds if `asc-cmake` already
provides the export machinery. Do not hard-code Linux-only paths or flags.

## Configuration options

Use the option helpers and naming conventions already defined by `asc-cmake`.
At minimum support the capabilities below when they are meaningful in the
migrated code:

```text
ASC_CPP_BUILD_TESTING
ASC_CPP_BUILD_EXAMPLES
ASC_CPP_ENABLE_OPENMP
ASC_CPP_ENABLE_CUDA
ASC_CPP_ENABLE_EIGEN
ASC_CPP_ENABLE_BLAS
ASC_CPP_ENABLE_LAPACK
ASC_CPP_WARNINGS_AS_ERRORS
ASC_CPP_ENABLE_SANITIZERS
```

Defaults should permit a clean CPU-only build with no optional numerical
backend. Unsupported option combinations must fail at configure time with an
actionable message. Do not expose an option for `functional`.

For optional dependencies:

- use imported targets such as `OpenMP::OpenMP_CXX`, `Eigen3::Eigen`,
  `BLAS::BLAS`, and `LAPACK::LAPACK` when available;
- keep dependency discovery in a focused CMake file;
- propagate a dependency publicly only when installed public headers require it;
- ensure the installed package repeats required discovery with
  `find_dependency`;
- provide compile-time feature macros only through the owning target;
- do not download dependencies during a normal configure unless asc-cmake has a
  deliberate, user-controlled policy for it.

CUDA must be optional. A CPU-only machine must configure, compile, test,
install, and consume the package without a CUDA toolkit. Isolate CUDA sources
and CUDA runtime calls behind the core backend boundary.

## Package and installation contract

Install public headers, component targets, umbrella target, export set, version
file, and package configuration. The installed consumer experience must support:

```cmake
find_package(ASCCpp CONFIG REQUIRED)
target_link_libraries(app PRIVATE ASC::cpp)
```

and component use:

```cmake
find_package(ASCCpp CONFIG REQUIRED COMPONENTS core array linalg)
target_link_libraries(app PRIVATE ASC::linalg)
```

Choose one canonical package spelling and use it consistently; if existing
`asc-cmake` policy mandates a different package name, document that decision and
update the examples coherently. Unknown or unavailable components must produce a
clear configure-time error. Relocation must work: no source/build absolute path
may leak into installed CMake files or headers.

The public umbrella `include/asc/asc.h` may include all stable module umbrella
headers. Do not force optional dependency headers into basic `ASC::core`
consumers.

## Namespace, identifiers, and compatibility migration

Perform the namespace change semantically, not as an unsafe global text replace:

1. change namespace declarations and qualified names to `asc`;
2. update friend declarations, explicit instantiations, ADL-dependent helpers,
   stream operators, specializations, macros, and generated/config headers;
3. update include paths and documentation examples;
4. rename old MdeCpp include guards to an `ASC_..._H_` pattern;
5. preserve C++ standard-library specializations in `namespace std` where they
   are legally required, referring to `asc` types;
6. format all changed C++ files;
7. scan the production/install tree for forbidden residues.

Tests may mention `mdecpp` only when explicitly verifying that legacy names do
not leak into the installed API. Third-party vendored files, if any, must not be
rewritten.

Use a small number of stable module umbrella headers if helpful, but do not
create a second parallel public header hierarchy. Do not retain forwarding
headers under the old MdeCpp paths unless explicitly requested.

## Tests

Port behavior-bearing MdeCpp tests first, then add architecture tests. Use the
test facilities supplied by `asc-cmake`; do not add a second framework casually.

At minimum cover:

- core errors, casts, numeric helpers, CPU device selection, memory ownership,
  copy/move behavior, and CPU `forall`;
- utilities option parsing and timer invariants without timing-flaky thresholds;
- array construction, shapes/layouts/indexing, views, iteration, expressions,
  dense/sparse behavior, and ownership;
- linalg representative elementwise operations, dot/product/matmul,
  decomposition/solve behavior where implemented, dimensions, and failures;
- random deterministic seeded behavior, ranges, permutation validity, Sobol and
  other quasi-random reference points, and distribution shape;
- build-tree use of every component target;
- direct compilation of every installed public header as the first include;
- representative template instantiation in two translation units followed by a
  successful link, guarding against ODR defects;
- external use of any explicitly instantiated types and, where applicable, a
  deliberately unsupported-type diagnostic;
- install to a temporary prefix and configure/build/run a separate consumer for
  both `ASC::cpp` and selected components;
- a negative component test for an unknown component;
- CPU-only configuration with all optional dependencies disabled.

Use exact reference values/tolerances appropriate to the numeric type. Seed all
stochastic tests. Sanitizer and warning-as-error builds must not be required for
ordinary users, but should be available to CI.

## CI and presets

Integrate with the current repository workflow rather than replacing it blindly.
Provide developer presets using the asc-cmake conventions for at least:

- default CPU debug/developer build;
- release build;
- strict warnings;
- sanitizer build where supported;
- optional backend builds only when CI runners support them.

CI should configure, build, test, install, and run the external consumer test.
Use a modest compiler/OS matrix appropriate for a solo-maintained project.
Pin action major versions and enable caching only after correctness is clear.

## Documentation

Update the README with:

- project scope and the five modules;
- dependency diagram;
- supported compiler/CMake requirements derived from actual configuration;
- clone/configure/build/test/install commands;
- examples using both `ASC::cpp` and component targets;
- optional backend flags;
- namespace and include examples;
- a migration table from MdeCpp paths/names to asc-cpp;
- a cross-repository disposition table for geometry, functional, and the VPFP
  miniapp;
- a clear statement that functional/geometry code is intentionally assigned to
  higher-level repositories rather than missing accidentally.

Document why core and device were merged, why algebra became linalg, and why a
flat `asc` namespace is used. Avoid claiming features that tests do not prove.

## Implementation sequence

Work in reviewable stages and keep the tree buildable after each stage:

1. inspect repositories and write the migration/dependency inventory;
2. write `docs/migration/handoff.md` for all non-asc-cpp code before copying
   production sources;
3. establish root CMake/package skeleton using the actual asc-cmake API;
4. migrate `core` and its tests;
5. migrate `utilities` and tests;
6. migrate `array` and tests;
7. migrate `linalg` and tests;
8. migrate `random` and tests;
9. add umbrella target/header;
10. implement install/export/component package tests;
11. add presets, CI, documentation, formatting, and residue scans;
12. run the complete verification matrix and report results.

Do not migrate all modules in one unverified mechanical step. When old code has
undefined behavior, portability problems, or ambiguous ownership, preserve the
observable intent, add a regression test, and explain the correction.

## Required verification commands

Use repository presets where available; otherwise run their equivalent. The
final verification must include:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
cmake --install <build-directory> --prefix <temporary-prefix>
```

Then configure, build, and run a standalone consumer against only the temporary
installation. Also run formatting/linting configured by the repositories and
search production paths for legacy residue, for example:

```sh
rg -n 'namespace[[:space:]]+mdecpp|mdecpp::|MDECPP_' include src cmake CMakeLists.txt
rg -n 'geometry/|functional/|miniapps/vpfp|class[[:space:]]+(Operator|Basis|Maxwellian|Ball|Orthotope)' include src cmake CMakeLists.txt
```

The first command must have no unexplained matches. The second must not show a
production API assigned to asc-xde/asc-kinetic; documentation handoff references
outside those searched production paths are expected. Search generated installed
files for absolute source/build paths.

If a requested backend cannot be tested locally, state precisely which backend
was not tested and why; do not imply success.

## Acceptance criteria

The task is complete only when all of these are true:

- exactly `core`, `utilities`, `array`, `linalg`, and `random` are production
  modules;
- all unrestricted template definitions needed by consumers are present in the
  installed include tree;
- installed `detail/*_impl.h` files are complete and reachable only through
  their owning public headers in normal use;
- header self-containment and multi-translation-unit ODR tests pass;
- core combines the former MdeCpp core and device facilities;
- linalg contains the migrated algebra facilities;
- public declarations and definitions use only namespace `asc`;
- all six public CMake targets (`ASC::core`, `ASC::utilities`, `ASC::array`,
  `ASC::linalg`, `ASC::random`, and `ASC::cpp`) work in the build tree and after
  installation;
- dependencies follow the required acyclic graph;
- CPU-only build/test/install/consume succeeds without optional packages;
- requested optional configurations that are locally available succeed;
- package components work and relocation is verified;
- tests cover preserved MdeCpp behavior and module boundaries;
- no production functional target/header/option was introduced;
- no production geometry, operator/basis, Maxwellian, or VPFP API was introduced;
- every audited MdeCpp production area has a recorded destination and
  `docs/migration/handoff.md` captures non-asc-cpp ownership and known defects;
- no unexplained `mdecpp` namespace, old include path, or public `MDECPP_` residue
  remains;
- documentation matches actual commands and tested features;
- the working tree contains no accidental build artifacts.

## Final response format

At completion, report concisely:

1. the inspected asc-cmake API and how it was used;
2. the source migration performed per module;
3. the cross-repository handoff decisions and any ambiguity discovered against
   the audited commit;
4. the resulting targets and dependency graph;
5. namespace/include/API migration decisions;
6. files or major areas changed;
7. every verification command and its result;
8. untested optional backends, remaining risks, and deliberate follow-up work;
9. confirmation that no commit, push, or sibling-repository modification was
   made unless separately authorized.

Do not declare completion if any acceptance criterion is unmet. Leave a clear,
actionable blocker report instead.
