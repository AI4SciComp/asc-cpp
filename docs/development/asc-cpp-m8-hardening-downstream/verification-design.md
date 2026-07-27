# Milestone 8 Independent Verification Design

Status: Implemented; lead integration requested

Date: 2026-07-27

## Scope and independence

This verification was designed from the frozen M8 contract, ownership ledger,
approved roadmap, ADR 0018, declared public file sets, installed package
contract, and current public headers. It does not copy a production inventory
or derive expected package facts from generated exports. It does not inspect
or reuse deleted legacy implementation or tests.

The real `asc-xde` repository is used only as a read-only provenance anchor.
Its approved state is the clean skeletal commit
`abcb29b51f22f40afd7f174707b7ccf83c32d4bf`; no implementation or test is
copied from it.

## Independent oracles

### Package version and metadata

`tests/hardening/package_metadata` is an isolated CMake consumer. It requests
`core` plus every other component as optional so every
`ASCCpp_<component>_FOUND` result must be defined and truthful. Its hard-coded
oracle contains the 15 approved components and the exact CPU/CUDA availability
sets. It also verifies the corresponding target presence and the toolkit
targets required by the complete CUDA closure.

The required version cases are:

| Request | Mode | Oracle |
| --- | --- | --- |
| `0.9.0` | exact | accept |
| `0.9` | `SameMinorVersion` | accept |
| `0.9.1` | newer patch than candidate | reject |
| `0.8` | different pre-1.0 minor | reject |
| `1.0` | different major | reject |

The rejected cases inspect `ASCCpp_CONSIDERED_VERSIONS` when CMake provides it,
so a false pass caused by looking at an unrelated package cannot satisfy the
probe.

### Installed header and public API equivalence

`tests/hardening/check_installed_headers.cmake` compares the installed header
tree with a hard-coded 37-header provider-free manifest or 49-header
CUDA-enabled manifest. Missing and unexpected files fail independently. It
also rejects the deleted aggregate/compatibility headers `asc/array.h`,
`asc/asc.h`, `asc/cpp.h`, and `asc/linalg.h`.

`tests/hardening/installed_api` is an isolated installed-package consumer. The
CPU case requests only `cpp`; the CUDA case requests `cpp` and all six CUDA
facets. It includes each provider-free aggregate, conditionally includes all
provider headers, checks representative ownership/concept contracts, links
the public targets, runs, and records representative `sizeof` and `alignof`
observations. A shared-package run also acts as an independent link-time check
that the support symbols needed by the exercised public templates are visible.

### Compile-time and object-size observation

`benchmarks/hardening/public_header_compile_probe.cc` includes all
provider-free public aggregates and instantiates representative dense, sparse,
expression, and random contracts. The CMake script
`run_compile_object_probe.cmake` compiles it directly as C++20 three times,
measures each compile through `cmake -E time`, verifies a nonempty and
repeatably equal object size, and writes a CSV report.

Timing is an observation, not an absolute gate. Object size is required to be
stable only across repetitions from the same compiler, command, source, and
environment. Compiler-to-compiler size differences are expected.

### asc-xde-shaped trial

`tests/downstream/asc_xde_trial` requests only `dense`, whose approved closure
is `core`, `expression`, and `dense`. Configuration rejects every unrelated
ASC target, every CUDA target, and any CUDAToolkit discovery.

The executable performs two public-only forward-Euler workflows:

- scalar ODE `y' = -2y`, `y(0) = 1`, `dt = 0.1`, with exact oracle `0.8`; and
- a five-point one-dimensional diffusion state with a three-point Laplacian,
  `dt = 0.25`, exact next state `{0, 1, 1.5, 1, 0}`, and checksum `3.5`.

Storage uses `DenseArray`, element access uses `DenseView`, and updates use
public expression construction and `Evaluate`. Compile-time
`__has_include` negatives reject the deleted `asc/array.h` and `asc/linalg.h`.

`run_asc_xde_trial.cmake` configures, builds, and runs the fixture in an
isolated path containing spaces with the user package registries disabled. It
checks the real asc-xde HEAD and complete worktree status before and after the
trial and fails on any change.

## Required lead integration

The lead-owned CMake integration should register:

1. the metadata consumer for all five version cases against build-tree and
   installed package configurations, and for CPU and CUDA where applicable;
2. exact installed-header checks after CPU and CUDA installations;
3. installed API consumers for static/shared CPU and static/shared CUDA
   packages;
4. GCC and Clang compile/object probes with three repetitions and build-tree
   report outputs;
5. asc-xde trials against build-tree, copied build-tree, installed, relocated,
   and path-with-spaces packages; and
6. labels `hardening`, `package`, `consumer`, `downstream`, `performance`, and
   `milestone-8` as appropriate.

Each nested configure must pass `ASCCpp_DIR` explicitly and disable both CMake
user and system package registries. Multi-config builds must forward the active
configuration. The asc-xde runner additionally needs the read-only sibling
repository path.

## Acceptance

Every correctness probe is pass/fail. Compile timing and object size are
observations, except that failed compilation, empty objects, or an object-size
change between identical repetitions fails. A missing compiler, provider,
platform, or topology is skipped and recorded; it is never converted into
provider evidence.
