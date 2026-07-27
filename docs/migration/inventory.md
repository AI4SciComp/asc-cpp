# MdeCpp migration inventory

> **Superseded historical document.** This file describes the deleted implementation at historical HEAD `33b261ea33616a6395c4ad3b20646093103344f7`; it is retained only for auditability and is not current API, build, package, or implementation guidance. See the [approved Stage A six-module blueprint](../development/asc-cpp-architecture/architecture-blueprint.md).

## Audit metadata

- Reference tree: `/home/yicai/repo/MdeRepo/MdeCpp`
- Reference revision: `f6294e9079262682ce63ae7ff2d8a643e658bf5d`
- Earlier revision named by `generator.md`: `471e2a41`
- Migration date: 2026-07-20
- Destination namespace: `asc`

At the repository owner's direction, this implementation is a source-level
selection and adaptation of the domain-neutral MdeCpp foundation. It is not a
clean-room reimplementation. Namespace, paths, build/install structure, package
components, and defects listed below were changed in asc-cpp.

## Selected production code

| MdeCpp source | asc-cpp destination | Result |
| --- | --- | --- |
| `generic/core/*` | `include/asc/core`, `src/core` | Deep-copied and namespace/include adapted |
| `generic/device/*` | `include/asc/core`, `src/core` | Deep-copied; device and memory remain part of core |
| `generic/utility/*` | `include/asc/utilities`, `src/utilities` | Selected as the initial baseline; class names and forwarding `optparser` spelling retained through the M1 redesign below |
| `generic/array/*` | `include/asc/array`, `src/array` | Deep-copied as the retained compatibility baseline; canonical Array M1 is a separate asc-cpp design |
| `algebra/*` | `include/asc/linalg` | Deep-copied and directory renamed as the retained compatibility baseline; canonical Linalg M1 is a separate asc-cpp design |
| `random/*.{h,cc}` | `include/asc/random`, `src/random` | Deep-copied and adapted |
| `random/data/sobol.txt` | `src/random/sobol_data.inc` | Exact direction data compiled into the library |
| selected `tests/generic`, `tests/algebra`, `tests/random` | matching `tests` directories | Deep-copied, namespace/include adapted, grouped by component |

The source manifest intentionally excludes MdeCpp's `functional`, `geometry`,
`fem`, `integrate`, `mesh`, `odeint`, `simulate`, `analyze`, `visualize`,
miniapps, benchmarks, experimental `develop` code, legacy build machinery, and
generated configuration. Their ownership is summarized in
[handoff.md](handoff.md).

## Canonical Core M1 addition

Core M1 adds a new serial foundation beside the selected MdeCpp code:

- signed 64-bit metadata aliases;
- stable status/result values and release-active contracts;
- explicit memory spaces and a shareable host resource;
- move-only RAII `Buffer<T>` ownership;
- immutable explicit `ExecutionContext` and completed `Event` values.

These interfaces derive from the approved asc-cpp architecture and are not a
source migration from MdeCpp. They do not internally depend on the legacy
`Memory<T>`, `MemoryManager mm`, `Device`, or `forall` runtime. See
[Core migration](core.md) for the exact staged mapping.

## Canonical Utilities M1 redesign

Utilities M1 retains the selected MdeCpp class names while replacing inherited
implementation hazards with asc-cpp contracts:

- standard-container configuration values with status-oriented validation,
  transactional loading, an explicit unknown-key policy, and canonical
  serialization;
- owned CLI names and values, transactional argv parsing, negative numeric
  values, explicit help sinks, and a forwarding `optparser.h` spelling;
- monotonic per-instance timing with defined empty queries and lossless
  statistics beyond the historical fixed capacity;
- a canonical `<asc/utilities.h>` umbrella and an isolated compatibility
  translation unit for historical error/global-output behavior.

These status, grammar, transaction, dependency, and timer contracts come from
the approved asc-cpp architecture rather than MdeCpp. Diagnostics/tracing are
deferred. See [Utilities migration](utilities.md) for the exact API mapping.

## Canonical Array M1 addition

Array M1 implements the approved additive design for a new dense host path:

- fixed-rank, mixed static/dynamic extents with signed 64-bit metadata;
- checked left, right, and non-negative-stride mappings;
- default pointer access and element-typed non-owning views;
- move-only ownership over canonical Core `Buffer<T>`;
- explicit synchronous host clone through an `ExecutionContext`;
- structural tensor concepts and a canonical `<asc/array.h>` umbrella.

These interfaces derive from the asc-cpp architecture and are not source
migrations from MdeCpp. M1 does not rewrite inherited `UArray`, `DenseMArray`,
expressions, sparse arrays, or their linalg/random consumers. Focused,
compatibility, strict, sanitizer, dependency, and installed-package gates
verify the implemented boundary. See
[Array migration](array.md) for the exact mapping and compatibility boundary.

## Canonical Linalg M1 addition

Linalg M1 implements the approved additive design for a narrow canonical BLAS
path:

- exact-rank structural Linalg concepts over canonical Array views;
- explicit-context `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, and `Gemm`;
- checked shape, 64-bit metadata, host access, uniqueness, and alias rules;
- a queryable deterministic `serial-reference` capability;
- compiled `float` and `double` kernels with no operation-time allocation or
  transfer;
- a canonical `<asc/linalg.h>` umbrella and direct Core/Array dependencies.

These interfaces derive from the approved asc-cpp architecture and are not a
source migration from MdeCpp. M1 does not rewrite inherited pointwise
algorithms, contractions, decompositions, LAPACK-style wrappers, solver
hierarchies, or Eigen/MKL adapters. See [Linalg migration](linalg.md) for the
complete family classification and semantic mapping.

## Canonical Random M1 addition

Random M1 adds a new deterministic generation path beside the selected
MdeCpp sampler code:

- explicit fixed-width `RandomKey` and `RandomCounter` values;
- compiled, named, versioned Philox4x32-10 integer generation;
- ASC-owned `Uniform01<float/double>` transforms;
- context-first bulk fill over canonical writable Array views;
- logical layout/partition invariance and transactional status behavior; and
- a canonical `<asc/random.h>` umbrella with direct Core/Array dependencies.

These interfaces derive from the approved asc-cpp architecture and are not a
source migration from MdeCpp. Inherited Random headers move to aggregate
`ASC::cpp` ownership, while their permutation and Sobol definitions remain
compiled for compatibility. They are no longer part of the minimal
`ASC::random` header contract. See [Random migration](random.md) for the
complete family classification.

## Legacy behavior retained for compatibility

- host/device memory ownership, aliases, validity flags, and lazy transfer;
- runtime device policy with serial, OpenMP, and CUDA dispatch;
- `MShape` plus layout-map separation;
- multidimensional indices and iterators;
- CRTP `MObject` and expression evaluation;
- dense owners, strided views, and static/dynamic aliases;
- COO construction with CSR/CSC compressed layouts;
- generic BLAS/decomposition/LAPACK-style interfaces;
- optional Eigen and MKL adapters;
- pseudorandom and low-discrepancy sampler families.

“Retained” in this section records the current compatibility baseline; it does
not designate these mechanisms as the target architecture. Canonical Array,
Linalg, and Random M1 are additive, so broad compatibility Linalg/Random,
sparse, expression, and device-facing implementations continue to consume this
path through the aggregate during the migration.

## Deliberate adaptations and fixes

| Area | Change |
| --- | --- |
| Identity | `mdecpp`/`MDECPP` became `asc`/`ASC`; include roots and header guards changed accordingly. |
| Configuration | The stable `<asc/core/config.h>` facade is source-controlled, while build switches are generated from `config/cmake/config.h.in` into internal `<asc/config/_config.h>`, following MdeCpp's separation. |
| Core math | MdeCpp's `numeric.h` facility is published as `<asc/core/math.h>`; the temporary forwarding header remains for legacy Array compatibility, while canonical Array M1 may include only canonical Core headers. |
| Utilities | `ConfigVector` and `ConfigMatrix` use `std::vector`; canonical configuration and CLI failures return status; timer statistics are lossless; `<asc/utilities/optparser.h>` forwards to the canonical CLI header; `ASC::utilities` depends only on `ASC::core`, not `ASC::array`. |
| Linalg | A separate canonical umbrella and seven context-first operations use canonical views, stable status, direct Core/Array dependencies, and compiled serial-reference kernels; inherited algebra headers remain compatibility-only. |
| Random | A separate canonical umbrella exposes explicit key/counter Philox4x32-10 generation, an ASC-owned unit transform, and canonical-view fill; inherited sampler headers move to aggregate compatibility ownership. |
| Packaging | The monolithic legacy build became six exported `ASC::` CMake targets using `asc-cmake`. |
| Sobol | The exact MdeCpp direction table is compiled into `ASC::random`; no absolute source path or runtime data file remains. |
| Radical inverse | The overload accepting `base` now uses that argument instead of silently recomputing a prime from `dim`. |
| Sparse CSC ordering | Equal-column tuples use explicit lexicographic coordinates; the former `UArray < UArray` expression compared allocation addresses through pointer conversion. |
| Empty compressed arrays | Empty CSR/CSC objects initialize a zeroed outer-pointer table, making iteration and Eigen maps valid. |
| Dense static views | Cross-rank static views release the default constructor's temporary owned storage before becoming aliases. |
| Precision-generic scalars | Scalar `Pow` and `Clip` use the destination value type, so single-precision builds do not instantiate mismatched storage. |
| Header isolation | Sampler implementation headers include their base interface and every public header is compiled independently. |
| C++ diagnostics | Missing includes, duplicate includes, implicit `ConfigValue` conversions, C-style variable-size arrays, long lines, and warning-cleanliness issues were corrected. |
| Build ABI | Precision and enabled backends are generated into an installed configuration header; optional dependencies are rediscovered by the installed package; shared objects retain relocatable sibling-library lookup. |

## Test correspondence

The migrated GoogleTest sources are built as `asc-cpp.generic`,
`asc-cpp.algebra`, and `asc-cpp.random`. CPU builds execute more than 500 tests;
enabling Eigen expands the algebra group. asc-cpp adds dedicated Core and
Utilities suites plus header-isolation, dependency-boundary, ODR,
component-consumer, relocation, and unknown-component tests around that
behavioral baseline.

Array M1 adds focused extents, mapping, view, owner, release-contract,
dependency-boundary, and ODR executables linked to the minimal `ASC::array`
component, plus an installed Array-only consumer. The generic suite remains
the separate compatibility regression gate and cannot establish canonical
Array ownership or dependency claims.

Linalg M1 adds focused BLAS1, BLAS2, BLAS3, release-contract,
dependency-boundary, and ODR executables linked to the minimal `ASC::linalg`
component, plus an installed Linalg-only consumer. The existing algebra suite
remains the separate compatibility regression gate and cannot establish the
canonical Linalg view, alias, status, provider, or dependency contracts. The
canonical completion gates are recorded in [testing](../testing.md).

Random M1 adds focused engine, distribution, fill, release-contract,
dependency-boundary, and ODR executables linked to the minimal `ASC::random`
component, plus an installed Random-only consumer. The inherited Random suite
links `ASC::cpp` and remains a separate compatibility regression gate. The six
focused default tests, full 39-test default and strict suites, five applicable
exception/assertion-disabled tests, six focused sanitizer tests, dependency,
header, ODR, allocation, lint, and relocated-consumer gates passed.

## Licensing records

The destination license is [Apache-2.0](../../LICENSE). Algorithm-level
attributions retained from MdeCpp's random generators are collected in
[THIRD_PARTY_NOTICES](../../THIRD_PARTY_NOTICES) and preserved in source
comments. This file records provenance and engineering disposition; it does not
replace the repository's license terms.
