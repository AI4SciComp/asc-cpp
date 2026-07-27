# Milestone 8 Contract — packaging/API/performance/downstream hardening

Status: Frozen

Date: 2026-07-27

Owner approval: Milestone 8, no corrections

Branch: `feature/asc-cpp-m8-hardening-downstream`

Baseline: cumulative, unpublished Milestones 0--7 working tree at unchanged
`HEAD` `33b261ea33616a6395c4ad3b20646093103344f7`

Candidate package version: `0.9.0`

## Purpose

Milestone 8 performs only the hardening named by the approved roadmap:

```text
packaging/API/performance/downstream hardening
exit gate: full matrix and asc-xde trial
```

It hardens the existing six provider-free modules, two random storage facets,
provider-free aggregate, and six optional CUDA facets. It introduces no new
product module, facet, provider, numerical operation, algorithm, scalar family,
runtime-rank owner, compatibility facade, or external runtime dependency.

Milestone completion and publication remain separate. `0.9.0` is an unreleased
candidate, not a release or a 1.0 compatibility claim.

## Governing architecture

The approved blueprint and ADRs 0001--0018 remain controlling, especially:

- ADR 0002: target and package naming;
- ADRs 0003 and 0004: dependency direction and independently consumable
  random storage facets;
- ADRs 0007--0015: the existing type, ownership, execution, expression,
  dense/sparse, and reproducibility contracts;
- ADR 0016: sanitizer and UB policy;
- ADR 0017: provenance and clean-room restrictions; and
- ADR 0018: milestone-gated pre-1.0 compatibility and release evidence.

Actual released ASCCMake 0.1.0 APIs remain mandatory. Standard CMake owns
multi-export component packaging that ASCCMake 0.1.0 does not provide. No
asc-cmake API may be imitated or modified in this milestone.

## Frozen product and dependency boundary

The product target set remains exactly:

```text
ASC::core
ASC::utilities
ASC::expression
ASC::dense
ASC::sparse
ASC::random
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

Their direct dependency edges remain exactly those in the approved dependency
manifest. `ASC::cpp` remains provider-free. CUDA stays default off. No
provider-free component discovers or links a provider SDK. CUDA Runtime,
cuBLAS, and cuSPARSE remain the only provider implementation dependencies.

No new public C++ API is planned. A production C++ edit is permitted only to
fix a hardening defect proven by an independent test and must preserve the
approved capability boundary. Package metadata and documentation may make
already-existing component/capability facts queryable; they may not advertise
an unavailable operation.

## Packaging hardening

The package candidate advances from `0.7.0` to `0.9.0` and retains
`SameMinorVersion` compatibility. The following existing CMake package
variables become reviewed, documented package metadata:

```text
ASCCpp_VERSION
ASCCpp_KNOWN_COMPONENTS
ASCCpp_AVAILABLE_COMPONENTS
ASCCpp_<component>_FOUND
```

Default lookup still requests `cpp`. Component lookup must:

- reject unknown and unavailable required components truthfully;
- keep quiet optional misses nonfatal;
- load the exact transitive ASC target closure;
- discover CUDAToolkit only for a requested CUDA closure;
- work from build tree, install tree, copied build tree, relocated prefix, and
  paths containing spaces;
- work as static and shared libraries on locally supported toolchains;
- avoid the CMake user package registry; and
- expose no source/build absolute path from an installed package.

Version tests must accept compatible `0.9.x` requests according to
`SameMinorVersion` and reject incompatible pre-1.0 minors/majors.

## API, ABI, and symbol hardening

The milestone records and mechanically checks:

- the complete public header/target/component manifest;
- standalone header self-containment, exceptions-disabled parsing, include
  order, multi-TU/ODR, ownership, and negative contracts;
- installed-header equivalence to the declared public file sets;
- shared-library symbol visibility and the intentional internal support
  symbols needed by public templates;
- target kind, output name, static-definition macro, C++20 propagation, and
  exact direct link interfaces; and
- local ABI observations for supported compiler/linkage combinations.

ASCCpp is pre-1.0. The symbol/API report is a hardening baseline and review
artifact, not a promise of ABI compatibility across different 0.x minors,
standard libraries, compilers, build modes, CUDA toolkits, or platforms.

## Performance hardening

Existing correctness-checked CPU and CUDA benchmarks remain the runtime probes.
M8 adds representative public-header compile-time/object-size observations and
an aggregate performance report. Measurements must record toolchain, build
mode, hardware/provider, workload, warmup, repetition count, synchronization
boundary, checksum, pass/fail/skip, and known noise.

No cross-machine absolute timing threshold, speedup claim, or unapproved
optimization is permitted. A local regression gate may compare repeated
measurements from the same build/environment with an explicit tolerance.

## Downstream asc-xde trial

The checked-in `asc-xde` repository is a clean, skeletal repository at
`abcb29b51f22f40afd7f174707b7ccf83c32d4bf`. M8 does not modify it or invent
asc-xde production APIs.

The approved trial is an isolated, asc-xde-shaped downstream fixture owned by
asc-cpp verification. It must:

- use only installed/public ASCCpp headers and targets;
- request an explicit minimal component set;
- compile and run a small ODE/PDE-oriented storage/evaluation workflow;
- exercise build-tree, installed, relocated, and path-with-spaces package use;
- prove no dependency on deleted `array`/`linalg` compatibility headers or
  optional CUDA SDKs for its CPU configuration; and
- record the real asc-xde repository commit and the fact that no downstream
  repository write occurred.

This is a dependency/API trial, not an asc-xde feature implementation or
migration-complete claim.

## Documentation hardening

M8 produces a reviewed support matrix, API/ABI policy, package-capability
guide, extension/downstream guide, performance envelope, migration note, and
unreleased `0.9.0` changelog entry. Claims must distinguish source, ABI,
numerical, random-bit, provider, file/schema, and package compatibility.

## Required local matrix

Within locally available infrastructure:

- GCC and Clang; Debug and Release; static and shared; CUDA disabled and
  enabled where the toolchain supports it;
- CUDA unavailable and CUDA-disabled isolation negatives;
- all package components, dependency closures, version requests, build-tree,
  installed, copied, relocated, and path-with-spaces cases;
- all isolated component consumers plus the asc-xde trial;
- ASan+UBSan and any additional supported sanitizer selection;
- shared symbol/API/ABI reports;
- representative compile-time/object-size and runtime performance probes;
- real-device CUDA runtime and CPU parity evidence; and
- final independent review after lead integration.

Hosted CI, unavailable compilers/platforms, and unavailable GPU topologies are
classified as skips, never inferred passes.

GPU evidence uses exactly:

```text
configure-tested
compile-tested
runtime-tested
parity-tested
skipped
```

## Explicitly out of scope

- any Milestone 9, 1.0, tag, release, ABI-stability, or support-window claim;
- publication, commit composition, push, PR, merge, tag, release, or branch
  deletion;
- changes to asc-xde, asc-kinetic, asc-lab, asc-cmake, or another repository;
- DLPack, forward AD, distributed adapters, new providers, OpenMP, Eigen,
  BLAS/LAPACK, oneMKL, TBB, SYCL, cuSOLVER, or CUDA Driver API;
- deleted legacy API compatibility, new solver/factorization surfaces,
  broadcasting, runtime-rank storage, or sparse densification;
- copied or mechanically translated MdeCpp/deleted asc-cpp implementation,
  tests, tables, vectors, or generated data; and
- performance changes without a frozen correctness oracle and independent
  evidence.

## Exit condition

Publication Checkpoint B is reached only when implementation, documentation,
clean local validation, four-role independent review, full locally available
matrix, package/relocation/consumer evidence, asc-xde trial, performance
evidence, remaining risks, and exact proposed remote/cleanup actions are
recorded. Work stops there.
