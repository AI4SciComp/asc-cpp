# MdeCpp migration analysis for the clean `asc-cpp` restart

Status: Designer B proposal

MdeCpp evidence commit: `f6294e9079262682ce63ae7ff2d8a643e658bf5d`

MdeCpp evidence branch/remote: `main`, `git@github.com:escapetiger/MdeCpp.git`

asc-cpp baseline commit: `33b261ea33616a6395c4ad3b20646093103344f7`

asc-cpp baseline branch/remote: `main`, `git@github.com:AI4SciComp/asc-cpp.git`

## 1. Scope and evidence discipline

This report treats MdeCpp as a behavior and test catalogue, not as a directory
tree to copy. The approved asc-cpp architecture has exactly six public
modules:

1. `core`
2. `utilities`
3. `expression`
4. `dense`
5. `sparse`
6. `random`

There is no destination `array` or `linalg` module. Dense and sparse storage,
evaluation, and linear algebra remain independent. `expression` is
storage-neutral. Base `random` depends only on `core`; storage integration is
provided through separate `random_dense` and `random_sparse` facets.

The inspection used only tracked content at the pinned commits, plus the
current worktree status. In particular:

- MdeCpp's tracked `Makefile` is modified locally. It was not read as pristine
  evidence and must not be overwritten.
- MdeCpp's `/legacy/` and `/develop/` directories are ignored by the tracked
  `.gitignore`; any local content under them is non-reproducible and was not
  used as evidence.
- Local MdeCpp build and testing artifacts are not commit evidence.
- The tracked `miniapps/vpfp/vpfp1d` symlink points to an absolute local build
  path and is broken. It is a packaging defect, not a reusable artifact.
- The asc-cpp worktree intentionally deletes the former build, source, test,
  contributor, and third-party-notice files. This report does not restore or
  infer approval from any deleted implementation.
- The retained asc-cpp README and design/module/migration documents describe a
  historical five-component implementation with `array` and `linalg`. They
  are historical prior art and do not override the six-module restart
  contract.

No MdeCpp production or test source should be copied into the Apache-2.0
asc-cpp repository under the current evidence. The default migration mode is
an independently written implementation of approved behavior, with independently
derived expected results. The detailed license and attribution gates are in
`provenance-review.md`.

## 2. Executive disposition

MdeCpp contains useful behavioral coverage but the wrong dependency topology
for the restart.

| MdeCpp area | Approved destination | Primary disposition |
| --- | --- | --- |
| `generic/core` | `core` | Redesign contracts and diagnostics; preserve failure categories as test ideas |
| `generic/device` | `core` | Redesign around explicit resources, contexts, transfers, and events |
| `generic/utility` | `utilities`, with configuration value model in `core` | Split and redesign |
| `generic/array/expr.h` | `expression` | Redesign as storage-neutral protocols and nodes |
| dense parts of `generic/array` | `dense` | Redesign owners, views, mappings, evaluators, and dense linalg |
| sparse parts of `generic/array` | `sparse` | Redesign sparse formats, builders, evaluators, and sparse linalg |
| dense parts of `algebra` | `dense` | Relocate algorithm contracts and provider adapters |
| sparse parts of `algebra` | `sparse` | Relocate algorithm contracts and provider adapters |
| base engines and scalar distributions in `random` | `random` | Redesign with explicit state and reproducibility contracts |
| dense-output samplers in `random` | `random_dense` facet | Redesign without a base-random-to-dense dependency |
| possible future sparse generation | `random_sparse` facet | New design; MdeCpp supplies no sufficient implementation evidence |
| `functional`, `geometry`, `integrate`, `mesh`, `fem`, `odeint`, finite-element analysis | `asc-xde` | Relocate behavior catalogue |
| kinetic simulation models and reusable schemes | `asc-kinetic` | Relocate behavior catalogue |
| visualization, applications, experiments, and workflow benchmarks | `asc-lab` | Relocate or retain only as evidence |
| monolithic build and umbrella header | none | Reject |

The most important blockers are:

- MdeCpp's repository license is GPLv3 while asc-cpp is Apache-2.0.
- MdeCpp has no tracked notice or provenance inventory beyond its root
  `LICENSE` and a few comments embedded in `random/generator.h`.
- MdeCpp's Sobol implementation is evidently derived from John Burkardt's
  MIT-licensed Sobol source, but the local file omits the upstream licensing
  and author block.
- The provenance and license of `random/data/sobol.txt` and the generated
  `sobol.bin` are not recorded.

These blockers prohibit source adaptation. They do not prevent clean-room
architecture work or behavioral reimplementation from independently approved
specifications.

## 3. Dependency findings

### 3.1 The source is monolithic

The top-level `CMakeLists.txt:296-325` appends all source directories into
single `SOURCES` and `HEADERS` lists and builds one `mdecpp` target. The source
directory list includes every layer from `generic` through `simulate`.
`mdecpp.h:12-69` re-exports the same stack through one umbrella header.
`tests/CMakeLists.txt` builds directory-grouped tests against that combined
library, not independent public components.

This build is useful as negative dependency evidence:

- it does not prove that any proposed asc-cpp component can configure, compile,
  install, or be consumed independently;
- it allows transitive includes to conceal ownership errors;
- it couples optional providers to a common target;
- it cannot validate the required mutual independence of `utilities`,
  `dense`, and `sparse`.

The asc-cpp restart should begin with dependency-negative build tests, narrow
public headers, and package-consumer tests for each approved component.

### 3.2 Shared vocabulary versus owned storage

MdeCpp centralizes shapes, layouts, dense arrays, sparse arrays, and expressions
inside `generic/array`. That is the central migration seam.

The restart should place only truly storage-neutral vocabulary in `core`, for
example:

- checked index and extent scalar types;
- rank and shape descriptors;
- checked size/offset arithmetic;
- memory-space identifiers, resources, buffers, execution contexts, and
  events;
- explicit status/result and contract categories.

Dense and sparse must each own their storage-specific mappings, views,
containers, evaluators, and linalg entry points. A shared concept is admitted
to `core` or `expression` only if neither storage module is required to define
or use it.

### 3.3 Exact destination graph

The migration mapping assumes these direct dependencies and no others:

| Module/facet | Permitted direct asc-cpp dependencies |
| --- | --- |
| `core` | none |
| `utilities` | `core` |
| `expression` | `core` |
| `dense` | `core`, `expression` |
| `sparse` | `core`, `expression` |
| `random` base | `core` |
| random-owned dense generation facet | `random`, `dense` |
| random-owned sparse generation facet | `random`, `sparse` |

Thus `dense` cannot use `utilities`, `sparse`, or `random`; `sparse` cannot
use `utilities`, `dense`, or `random`; and `expression` cannot use
`utilities`, either storage module, or `random`. The random-owned facets are
integration targets, not seventh or eighth architectural modules.

Candidate installed target names are `ASC::core`, `ASC::utilities`,
`ASC::expression`, `ASC::dense`, `ASC::sparse`, `ASC::random`,
`ASC::random_dense`, and `ASC::random_sparse`. An optional `ASC::cpp`
convenience umbrella is not a module and must never be required by a narrower
component. Public APIs remain in `namespace asc`; module identity belongs in
headers and targets rather than public nested namespaces.

### 3.4 I/O seam

MdeCpp mixes file and stream behavior into utilities and numerical containers,
and its Sobol reader consumes a raw build/source-tree binary. Preserve the
useful read/write/serialization failure cases, but divide ownership as follows:

- `core` owns storage-independent byte/text source and sink contracts, safe
  handle ownership, and any approved portable scalar/metadata encoding;
- `utilities` owns concrete approved local-file parsing;
- `dense` and `sparse` own their storage formats, metadata, and serialization
  adapters;
- every binary format states version, endianness, sizes, overflow behavior,
  and short-read/write behavior.

Core must never know a concrete dense or sparse type. A numerical format must
not be inferred from native object layout or an absolute source-tree path.

## 4. Module-by-module analysis

### 4.1 `core`

#### Useful behavior

`generic/core` provides a compact catalogue of:

- checked casts;
- numeric aliases and traits;
- failure categories and assertion/verification sites;
- string helpers;
- shape validation and overflow-sensitive size calculation in
  `generic/array/mshape.h`;
- host/device allocation, ownership, reference, move, and copy scenarios in
  the memory tests.

Those are useful contract and test ideas.

#### Required redesign

`generic/device/device.h:53-68,133-158` explicitly defines a process-wide,
non-thread-safe singleton. `generic/device/device.cc:30-75` configures it from
environment variables and mutates the global memory manager.
`generic/device/device.h:161-199` chooses a memory class from that global state
and lets `Read`, `Write`, and `ReadWrite` trigger synchronization implicitly.
This is incompatible with the approved explicit runtime model.

The restart should instead require:

- an explicit memory resource for allocation;
- an explicit execution context for work submission;
- explicit copy/transfer operations;
- explicit event/lifetime relationships;
- no mutable process-global backend selection;
- no public CUDA, MPI, or vendor header leakage through common headers.

`generic/core/error.h` chooses exception versus abort/assert behavior at compile
time. `generic/core/globals.h` exposes global diagnostic streams. Preserve the
failure taxonomy, but decide status/result, exception, and diagnostic-sink
policy through an ADR and explicit objects.

MdeCpp shapes use `int` extents and accept either wholly static or wholly
dynamic shapes. Preserve rank-zero, zero-extent, negative-extent, overflow,
and indexing test cases, but design the approved checked index width and mixed
extent model independently.

#### Provider boundary

`generic/device/cuda.h` includes CUDA vendor headers directly. CUDA support
belongs behind a `core` provider facet or private implementation boundary.
Common `core` headers must remain usable when no GPU SDK is installed.

### 4.2 `utilities`

#### Configuration

`generic/utility/config.h:19-35` includes `generic/array/marray.h` and stores
`DVector` and `DMatrix` directly in the configuration variant. This creates a
forbidden utilities-to-storage dependency.

Split the behavior:

- a storage-independent configuration value/schema model belongs in `core`;
- text/file parsing and user-facing configuration helpers belong in
  `utilities`;
- numerical containers are represented by standard-library sequences or a
  serialization boundary, not dense storage types.

`generic/utility/config.cc:43-52` silently truncates `double` to `int`.
`LoadFromFile` mutates entries one line at a time and admits unknown keys
through `SetConfig`. The new contract should specify exact conversions,
unknown-key policy, duplicate-key policy, validation ordering, and
transactional rollback.

#### Command-line parsing

`generic/utility/optparser.h:147-154` stores the short option in a five-byte
buffer and truncates it. The parser's token classification treats a following
`-...` token as another option, which prevents ordinary negative numeric
values from being consumed as values. It also lacks an explicit contract for
unknown positional tokens, duplicate options, `--`, and transactional failure.

Retain the behavioral categories from `tests/generic/unit_test_optparser.cc`,
then add independent tests for those missing cases. Do not retain the bound
raw-pointer API as the canonical interface.

#### Timers and small arrays

The timer uses `steady_clock`, which is the right clock category, but stores a
fixed 128 samples. Empty `Average`/`Last` access is unchecked, and
`Timer::Stop` increments the offset before returning the indexed sample
(`generic/utility/timer.cc:14-22`). Preserve start/stop/lap/reset concepts and
derive state-machine tests; redesign storage and error behavior.

`CArray` can be evaluated as an independent convenience wrapper, but it must
earn its place over `std::array`/`std::span`. `UArray` is an owning,
resizable, serializable container used pervasively by dense and sparse source.
Moving it wholesale into `utilities` would recreate forbidden storage edges.
Dense and sparse should use their own private storage or standard containers.

### 4.3 `expression`

`generic/array/expr.h` is not storage-neutral:

- it includes device execution and concrete array/layout concepts;
- terminals expose `UseDevice`, `Read`, `HostRead`, and concrete `At`
  operations;
- `Expression::EvalTo` selects host/device execution and writes a concrete
  destination (`generic/array/expr.h:184-227`);
- dense and sparse categories are recognized in one concept layer;
- sparse objects can be traversed as a dense logical object, leaving
  densification and complexity behavior implicit.

Retain only independently expressed semantic ideas:

- composable unary and binary nodes;
- scalar terminals;
- rank/shape propagation;
- broadcasting compatibility;
- result-type promotion;
- value capture of nested expression nodes.

The new module should define storage-neutral protocols, nodes, and metadata.
It must not allocate, transfer, synchronize, select a backend, or know a dense
or sparse concrete type. Dense and sparse own their respective terminal
adaptation and evaluation. Tests must include an external toy type, node
lifetime checks, compile-time rejection, aliasing, and proof that sparse
evaluation does not silently densify.

### 4.4 `dense`

The dense source contains valuable semantic coverage:

- rank and shape construction;
- row-major, column-major, and strided mappings;
- owning storage and non-owning references;
- transpose, permutation, slicing, and nested views;
- broadcasting and elementwise expressions;
- reductions, copy/move/resize, and serialization;
- CPU/GPU parity scenarios.

The storage model must nevertheless be redesigned. `DenseMArray` combines
owner and view modes, `MArrayView` is an alias of a `DenseMArray` specialization,
and access is tied to the global `Memory`/`Device` synchronization system.
The restart needs separate owner and view types, explicit lifetime rules,
checked logical mappings, explicit resources/contexts, and dense-owned
evaluators.

Dense linalg comes from the dense portions of `algebra/blas.h`,
`algebra/decomp.h`, `algebra/lapack.h`, and `algebra/eigen.h`. It remains in
`dense`; there is no linalg module. Provider adapters for a serial reference
implementation, Eigen, BLAS/LAPACK, MKL, or CUDA are private or opt-in dense
facets and must not create a dependency on `sparse`.

Preserve test ideas for:

- layout/transposition/conjugation combinations;
- exact small BLAS references;
- decomposition reconstruction and orthogonality;
- solver residuals, multiple right-hand sides, factor reuse, singular and
  rectangular cases;
- zero extents, invalid shapes, overflow, aliasing, and allocation failure;
- view lifetime and explicit transfer behavior.

Expected numerical values should come from independent mathematics or approved
provider references, not copied GPL-covered assertions.

### 4.5 `sparse`

MdeCpp demonstrates useful COO, CSR, and CSC behaviors. Its builder sorts
entries, combines duplicates, and removes zeros; tests cover construction,
format conversion, transpose, slices, diagonal extraction, arithmetic, and
multi-rank cases.

The implementation boundary is unsuitable:

- `generic/array/concepts.h` specializes dense and sparse types together;
- `generic/array/marray.h` re-exports both families;
- sparse storage uses the shared `UArray`;
- expression and algebra layers inspect both storage families;
- rank-zero and rank-one aliases are exposed although `SparseMArray` requires
  rank at least two;
- many operations normalize through COO and execute only on the host without
  an explicit complexity/provider contract.

The `sparse` module should independently own:

- sparse index and format descriptors;
- immutable/validated views;
- builders and canonicalization policy;
- format conversion;
- sparse expression terminals/evaluation;
- sparse matrix/vector algorithms and solver/provider adapters.

Duplicate handling, explicit-zero policy, sortedness, index base, integer
width, invalid structure detection, and conversion complexity must be public
contracts. Sparse must not return or accept dense-owned types merely for
convenience; cross-storage conversion requires a separately owned integration
surface.

### 4.6 `random`

#### Base module

`random/generator.h` provides candidate engine behavior for SplitMix64, PCG32,
xoroshiro64*, and xoroshiro128+, together with standard-library distributions.
These algorithms carry upstream attribution/license obligations, and their
local code must not be copied under the current provenance record.

Independently designed base random should:

- depend only on `core`;
- expose explicit engine/key/counter or seed/state values;
- make sequence-version guarantees explicit;
- avoid hidden global engines and implicit entropy seeding;
- separate nondeterministic seed acquisition from deterministic engines;
- avoid promising cross-platform sequences from implementation-defined
  standard distributions;
- define serialization, partitioning/jump, overflow, and concurrency behavior.

`Generator::Reset` entropy-seeds through its default `DeviceSeed`, while
same-seed tests exercise only local determinism. That is too weak for a
scientific reproducibility contract.

#### Storage facets

MdeCpp samplers include the combined array layer and, for normal/spherical
sampling, algebra operations. The approved split is:

- `random`: engines, scalar transforms/distributions, indexing/counter policy;
- `random_dense`: dense fills and dense-output samplers;
- `random_sparse`: sparse pattern/value generation.

The base target must never include dense or sparse headers. A sampler that
allocates a dense result or performs dense linalg belongs to `random_dense`.
MdeCpp supplies no adequate sparse-random implementation, so
`random_sparse` needs a new specification before implementation.

#### Low-discrepancy state

`random/permutation.cc:18-20,122-124` uses function-local singleton caches for
prime and permutation data. Although permutation generation is seeded
deterministically, the caches are hidden mutable process state. Preserve only
the mathematical test ideas; expose caller-owned immutable tables or explicit
cache objects if the feature is approved.

Sobol is blocked separately. The local implementation has recognizable
Burkardt-derived structure and commentary, but omits the upstream attribution
block; the direction-data origin is not recorded. Do not copy the
implementation, table, binary, converter, or expected vectors derived solely
from them. A future Sobol feature must start from a named, approved
specification or authoritative upstream release with notices and table
provenance preserved.

## 5. Higher-level repository mapping

The following MdeCpp directories do not belong in asc-cpp:

| MdeCpp source | Destination | Reason |
| --- | --- | --- |
| `functional/` | `asc-xde` | Operator and basis abstractions are equation/discretization concerns |
| `geometry/` | `asc-xde` | Geometric domains and intersections support discretizations |
| `integrate/` | `asc-xde` | Quadrature and integration rules sit above storage kernels |
| `mesh/` | `asc-xde` | Mesh, grid, graph, boundary, and timeline models are discretization infrastructure |
| `fem/` | `asc-xde` | Finite-element spaces, elements, assembly, and approximation |
| `odeint/` | `asc-xde` | Time integrators and splitting schemes |
| `analyze/finite_element_metric.h` and related discretization metrics | `asc-xde` | Metrics depend on equation/discretization objects |
| `simulate/kinetic.h`, reusable kinetic state/scheme/model pieces | `asc-kinetic` | Domain-specific reusable kinetic layer |
| `visualize/` | `asc-lab` | Workflow/output concern, not a numerical foundation primitive |
| `examples/`, `miniapps/`, workflow-level `benchmarks/` | `asc-lab` | Applications, experiments, and reproducible demonstrations |

If a higher repository later needs a small generic primitive, that requirement
should be proposed downward through an approved asc-cpp contract. It should
not cause an entire higher-level abstraction to be imported into asc-cpp.

## 6. Test-evidence catalogue

MdeCpp tests are GPL-covered source and must not be copied verbatim into
asc-cpp. They remain useful as a checklist for independent specifications.

| Source tests | Preserve independently as |
| --- | --- |
| `tests/generic/unit_test_carray.cc`, `unit_test_uarray.cc` | zero-size, checked index, ownership/reference, copy/move, iterator, serialization cases |
| shape/index/layout coverage within generic tests | rank-zero, zero extent, invalid extent, overflow, permutation, row/column/stride mapping |
| `tests/generic/unit_test_marray.cc` | dense owner/view, slice, transpose, broadcast, reduction, copy/move/resize behavior |
| `tests/generic/unit_test_spmarray.cc` | COO/CSR/CSC validation, duplicate reduction, zero removal, conversion, transpose, slicing, diagonal, arithmetic |
| `tests/generic/unit_test_expr.cc` | unary/binary composition, scalar operands, broadcast, type promotion, destination evaluation |
| `tests/generic/unit_test_optparser.cc` | defaults, switches, long/short forms, file parsing, missing values, usage rendering |
| `tests/algebra/unit_test_blas.cc` | small exact vector/matrix operations and layout variants |
| `tests/algebra/unit_test_decomp.cc`, `unit_test_lapack.cc` | reconstruction, orthogonality, residual, singular/rectangular behavior |
| `tests/algebra/unit_test_eigen.cc` | provider conversion and solver behavior |
| `tests/random/unit_test_generator.cc` | same-state reproducibility, range, engine boundaries |
| `tests/random/unit_test_perm.cc`, `unit_test_sampler.cc` | permutation validity, low-discrepancy early points, reset/index behavior, shape/range constraints |
| `tests/common/common.h` | run the same public contract against each enabled provider and report unavailable providers as skipped |

The restart must add evidence absent or weak in MdeCpp:

- component-isolation configure/build/install/consume tests;
- forbidden-include and forbidden-link dependency tests;
- compile-fail concept tests;
- explicit provider capability and skip reporting;
- no-implicit-transfer tests;
- allocation-failure and transactional rollback tests;
- empty/rank-zero/overflow/bounds coverage;
- owner/view and asynchronous lifetime tests;
- deterministic random reference vectors from approved primary specifications;
- concurrency and state-partition tests;
- Sobol or other table checksums and origin/version validation if table-driven
  algorithms are approved.

MdeCpp's normal-distribution test estimates a mean from a nondeterministically
seeded generator. Such tests are statistical smoke tests, not reproducibility
evidence, and should use controlled seeds, tolerances, and non-flaky criteria
if retained.

## 7. Recommended migration sequence

1. Freeze the six component names and add build-time dependency guards before
   implementing APIs.
2. Approve core ADRs for status/result, contracts, index width, memory
   resources, execution contexts, events, transfers, and provider isolation.
3. Implement storage-neutral shape/index vocabulary in `core`.
4. Implement `utilities` independently and prove it does not include or link
   dense, sparse, or expression.
5. Implement storage-neutral expression protocols against an external toy
   type before adding dense or sparse adapters.
6. Implement dense storage, views, mappings, evaluator, and dense linalg as an
   isolated component.
7. Implement sparse storage, formats, builder, evaluator, and sparse linalg as
   a separate isolated component.
8. Approve random engine/version/provenance decisions; implement the
   core-only base target.
9. Add `random_dense` and, only after a new specification exists,
   `random_sparse`.
10. Re-express useful MdeCpp behavior as independent tests and verify every
    package component from an installed consumer.

## 8. Approval gates and stop conditions

Implementation must stop for the affected feature when any of these remains
unresolved:

- a requested source-derived implementation lacks a compatible license grant;
- an attribution or notice cannot be reconstructed from authoritative
  evidence;
- generated or table data lacks a traceable upstream version and license;
- a change would add an edge among `utilities`, `dense`, and `sparse`;
- `expression` would learn a concrete storage type or own execution;
- base `random` would include/link dense or sparse;
- a provider SDK would leak into a common public header;
- a proposed behavior depends on ignored, untracked, dirty, or broken local
  artifacts.

The architecture can continue around such a feature through a clean-room
specification, but the blocked source or data must not enter asc-cpp without
owner and provenance approval.
