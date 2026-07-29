# Internal asc-cpp architecture analysis

**Role:** Designer A, independent internal architecture analyst
**Date:** 2026-07-26
**Scope:** read-only first pass required by runbook Section 16.2
**Repository:** `/home/yicai/AI4SciComp/asc-cpp`

## 1. Executive conclusion

The current `asc-cpp` working tree is an intentional clean restart, not a
partially functioning library. It retains documentation, CI text, the license,
and repository metadata, but it has no live CMake project, production headers,
compiled sources, examples, or tests. The 208 tracked deletions must remain
deleted. Consequently, no current API, component, package, CPU backend, or GPU
backend exists to build or validate.

The retained documentation describes a substantial but superseded
five-component implementation:

```text
core, utilities, array, linalg, random
```

That implementation was committed at
`33b261ea33616a6395c4ad3b20646093103344f7` and is available only as historical
prior art through `git show HEAD:<path>`. Its strongest ideas and evidence are
worth retaining selectively:

- signed 64-bit metadata and checked shape/layout arithmetic;
- status/result transport and release-active public contracts;
- move-only, single-space RAII buffers;
- explicit execution contexts and events;
- distinct mutable/const dense views;
- serial reference BLAS kernels with shape, alias, and allocation tests;
- explicit Philox key/counter state, independently calculated vectors, and
  deterministic logical traversal;
- transactional configuration and option parsing;
- monotonic timer behavior;
- header isolation, ODR, dependency scans, installed-component consumers, and
  relocation tests.

The old architecture cannot be restored as the target. Its `array` component
combined dense, sparse, and expression responsibilities; its `linalg`
component combined dense and sparse algebra; its `random` base target depended
on array storage; its expression system named concrete storage and selected
execution; and optional CUDA/Eigen/MKL dependencies leaked through broad public
targets. Those choices directly contradict the current six-module contract.

The restart should therefore use historical code and tests as a behavior and
design catalogue, not as a directory-level restoration. The only admissible
production graph is:

```text
core
├── utilities
├── expression
├── dense      (also depends on expression)
├── sparse     (also depends on expression)
└── random

random + dense  -> random-owned dense generation facet
random + sparse -> random-owned sparse generation facet
```

`array` and `linalg` must not reappear as top-level modules. Dense and sparse
must each own their storage, evaluator, and CPU/GPU linear algebra.

## 2. Evidence rules and terminology

This report distinguishes three evidence classes:

1. **Current fact** means a file or Git fact observed in the working tree on
   2026-07-26.
2. **Retained historical statement** means a claim in a currently retained
   document. Such a statement is not proof that the described implementation
   still exists.
3. **Historical prior art** means content inspected explicitly with
   `git show HEAD:<path>` or `git grep ... HEAD`. It is not live production
   code and is not an applicable repository instruction.

Everything under **Proposal** is an architectural recommendation, not a fact,
approval, or implementation authorization.

The deleted `AGENTS.md` and `generator.md` were not read or restored. Their
`HEAD` versions were not treated as applicable. No other designer's analysis
or provenance output was read.

## 3. Verified repository state

### 3.1 Identity and Git state

Current facts:

| Field | Verified value |
| --- | --- |
| checkout root | `/home/yicai/AI4SciComp/asc-cpp` |
| remote | `git@github.com:AI4SciComp/asc-cpp.git` |
| GitHub visibility | private |
| branch | `main` |
| `HEAD` | `33b261ea33616a6395c4ad3b20646093103344f7` |
| upstream | `origin/main` at the same commit |
| worktrees | one, at the checkout root |
| tags | none |
| open GitHub issues | none returned |
| open GitHub pull requests | none returned |
| working-tree change class | exactly 208 tracked deletions; no other status entries |
| deletion size | 62,687 lines |

The repository has only two commits:

```text
33b261e Updated at 2026-07-24 10:26:51
6e4579b Initial commit
```

The initial commit contains `.gitignore` and `LICENSE`. Commit `33b261e`
introduced the documented five-component implementation and its supporting
files. The current working tree intentionally deletes all executable parts of
that commit.

### 3.2 Current retained files

There are 28 retained files:

- `.github/workflows/ci.yml`;
- `.gitignore`;
- `LICENSE`;
- `README.md`;
- 24 files under `docs/`.

The retained documents consist of API, architecture, build, backend, testing,
module, migration, and five-component design material. They are valuable
historical evidence but are not a current implementation contract where they
conflict with the six-module runbook.

Current absences are decisive:

```text
CMakeLists.txt             absent
CMakePresets.json          absent
cmake/                     absent
include/                   absent
src/                       absent
tests/                     absent
examples/                  absent
AGENTS.md                  absent
generator.md               absent
THIRD_PARTY_NOTICES        absent
```

Therefore:

- the current tree cannot configure or build;
- no current public header can be consumed;
- no historical test result can be reproduced from this tree;
- the retained CI workflow cannot run successfully;
- retained README installation and consumption commands are presently false
  as operational instructions.

`git diff --check` passed before this report was added. It validates the
deletion patch's whitespace only; it is not a build or behavior check.

### 3.3 Toolchain facts relevant to future validation

The current host has CMake 4.1.2, GCC 11.4.0, CUDA 12.9, and an NVIDIA GeForce
RTX 3060 Laptop GPU with driver 576.83. System BLAS/LAPACK and CUDA cuBLAS,
cuSOLVER, cuSPARSE, and cuRAND libraries are visible. `clang++`, Ninja,
`clang-format`, and `clang-tidy` were not found in `PATH`.

These are inventory facts only. They establish neither provider support nor
GPU test evidence. No new asc-cpp code was configured, compiled, or executed.

## 4. Reconstructed original intent versus present actual state

### 4.1 Original committed intent

Retained documents and historical `HEAD` files agree on the former product
intent:

- a C++20 scientific-computing foundation in flat namespace `asc`;
- an installable `ASCCpp` package;
- five production components (`core`, `utilities`, `array`, `linalg`,
  `random`) plus an `ASC::cpp` umbrella;
- a canonical serial path added beside broad MdeCpp-derived compatibility
  code;
- an incremental migration away from global memory/device state;
- CPU-only defaults with optional OpenMP, CUDA, Eigen, and MKL compatibility;
- component-level header, dependency, ODR, package, and relocation tests.

The historical direct target graph was:

```text
utilities -> core
array     -> core
linalg    -> array, core
random    -> array, core
cpp       -> core, utilities, array, linalg, random
```

The historical package declared components:

```text
core utilities array linalg random cpp
```

This was a coherent migration graph for the former five-component plan, but it
is not the current required architecture.

### 4.2 Present actual state

The present state contains no migration facade and no canonical path. The old
implementation has been intentionally removed wholesale. The word
"implemented" in retained module, migration, testing, and README documents is
therefore historical.

There is also no need to preserve source compatibility with the deleted
five-component surface unless a later owner decision introduces such a
requirement. A clean restart should avoid recreating two permanent type
hierarchies or an `ASC::array`/`ASC::linalg` compatibility layer by default.

### 4.3 Retained-document contradictions

The following retained statements conflict materially with current facts or
the current runbook:

| Retained statement | Conflict |
| --- | --- |
| Exactly five production components are approved. | The current owner contract requires exactly six modules: `core`, `utilities`, `expression`, `dense`, `sparse`, and `random`. |
| `ASC::array` owns dense, sparse, and expressions. | Dense, sparse, and expression are now separate modules with hard dependency ceilings. |
| `ASC::linalg` owns dense and sparse algebra. | Dense and sparse must each own their own CPU/GPU algebra; there is no top-level linalg module. |
| `ASC::random` directly depends on `ASC::array`. | Random base must depend only on core; storage integration belongs to separate random-owned facets. |
| `ConfigValue` and its collection live entirely in utilities. | Core now owns the storage-independent configuration model/schema; utilities owns concrete parsing and precedence. |
| `asc::detail` is the sanctioned internal namespace. | Current policy requires internal implementation namespace names to contain `internal`. |
| Exception translation can be a normal public compatibility path. | Current direction is no public production exceptions, subject to the error ADR. |
| CPPLINT's historical 100-column configuration is repository style. | The current contract requires current Google style and its 80-column guidance unless new checked-in formatting establishes an approved alternative. The old `CPPLINT.cfg` is deleted. |
| Optional CUDA requirements can propagate publicly from core. | Optional vendor headers/libraries must not leak into common public headers or unrelated base components. |
| The documented default/strict/39-test suites pass. | All build and test files are currently deleted, so those results are historical and cannot be rerun. |

## 5. Mapping historical work to exactly six modules

This mapping is exhaustive at the architectural-owner level. Facets are
listed under their owning module and do not create additional modules.

### 5.1 `core`

**Historical prior art**

- `include/asc/core/types.h`: signed 64-bit `index_t`, `extent_t`,
  `stride_t`, `nnz_t`.
- `status.h`, `contracts.h`: status/result values and release-active
  contracts.
- `memory_space.h`, `memory_resource.h`, `buffer.h`: explicit space/resource
  vocabulary and move-only single-allocation ownership.
- `execution_context.h`, `event.h`: explicit serial context and completion
  vocabulary.
- Legacy `memory.h`, `device.h`, `forall.h`, `cuda.h`, `globals.h`, and
  `error.h`: broad global compatibility runtime.

**Useful work**

- The 64-bit metadata aliases and checked arithmetic tests match the new
  direction.
- `Buffer<T>` explicitly rejects implicit copying, mirroring, execution
  preference, and raw-pointer conversion.
- The serial context represents unavailable backends explicitly rather than
  silently falling back.
- Status values preserve a provider name and native diagnostic code.
- Historical tests cover allocation failure, rollback, alignment, zero size,
  move behavior, independent contexts, and header isolation.

**Work to reject or redesign**

- Global `MemoryManager mm`, `Device::device_singleton`, global streams/error
  policy, mutable `UseDevice`, automatic mirroring, and manual `Delete`.
- Public CUDA wrappers and SDK exposure from the base core target.
- The old broad core umbrella and compatibility path.
- `asc::detail`/`details` internal namespaces.

**New gaps**

- The required core configuration value/schema model.
- Storage-independent source/sink and safe file-resource I/O.
- Explicit device, pinned, and managed resources; copies; streams; real
  asynchronous events; and provider-error boundaries.
- Approved shape/rank/extent vocabulary shared by dense and sparse.
- Error and ABI decisions.

### 5.2 `utilities`

**Historical prior art**

- Transactional typed configuration parsing and canonical serialization.
- Command-line parsing with negative numeric values, unknown-option
  diagnostics, rollback, dynamic help text, and owned option names.
- `std::chrono::steady_clock` timer with defined empty queries and lossless
  accumulated statistics.
- `carray.h` and related light array wrappers existed under the old array
  component.

**Useful work**

- Transactional parser behavior and focused malformed/boundary tests.
- Standard-container values eliminated the former dependency on numerical
  arrays.
- Timer state and monotonic-clock behavior are suitable behavior-level
  references.

**Work to move or redesign**

- `ConfigValue`, schema/default/required semantics, validation results, and
  effective-value provenance belong in core.
- File and argv syntax, precedence, help, and diagnostics remain in utilities.
- Historical polymorphic option classes and raw bound-output pointers should
  not be adopted without an API/lifetime review.
- `carray.h` may contribute only genuinely lightweight C-array/`std::array`/
  `std::span` wrappers. Numerical ownership/allocation helpers do not belong
  here.

**New gaps**

- A clean interface from utilities parsers to the core configuration model.
- A frozen local-file format and precedence/provenance contract.
- Response-file, Unicode/path, environment-input, and security decisions.

### 5.3 `expression`

**Historical prior art**

- `include/asc/array/expr.h` and `mobject*.h`.
- CRTP expression nodes, scalar/unary/binary operations, by-value nested-node
  capture, and elementwise assignment tests.

**Useful work**

- Tests for nested temporaries and by-value expression-node capture are useful
  lifetime evidence.
- The historical separation between expression construction and some
  destination traversal suggests useful operation vocabulary.

**Work to reject**

The historical expression implementation is not storage-neutral:

- `ExpressionLike` requires inheritance from the ASC CRTP base.
- Concepts explicitly name `DenseMArrayLike` and `SparseMArrayLike`.
- Sparse terminals retain a concrete sparse object pointer and expose missing
  entries as logical dense-order zeros.
- `EvalTo` performs destination evaluation inside the expression layer.
- Evaluation reads `UseDevice`, calls `Read`/`ReadWrite`, and selects
  `ASC_FORALL_SWITCH`.
- Compatibility checks compare flattened size rather than full shape.
- Nodes carry execution preference but not approved rank/shape, traversal,
  alias, or sparsity-effect metadata.

**New gaps**

- Non-intrusive structural customization for external types.
- Safe lvalue/rvalue capture and explicit dangling prevention.
- Shape/rank and scalar-promotion metadata.
- Sparsity-preserving/changing/densifying classification.
- Destination-required and alias/overlap metadata.
- Dense- and sparse-owned evaluators.

### 5.4 `dense`

**Historical prior art**

- Canonical `Extents`, left/right/stride mappings, `TensorView`, and
  move-only host `Tensor`.
- Legacy `DenseMArray`, `UArray`, views, slicing, indexing, expressions, and
  serialization behavior.
- Canonical BLAS1/2/3 facades and compiled serial kernels from the former
  linalg component.
- Broad decomposition/solver and Eigen tests.

**Useful work**

- Mixed static/dynamic extents and 64-bit checked mappings.
- Element constness in view types and one-way mutable-to-const conversion.
- Move-only ownership with explicit clone.
- Padded/noncontiguous mapping, alias, no-allocation, and transaction tests.
- Serial `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, and `Gemm` kernels,
  especially scaled sum-of-squares `Nrm2` and `beta == 0` no-read behavior.
- Reference residual/backward-error tests can be reassigned to dense.

**Work to reject or redesign**

- The `array` module boundary and every `ASC::array` target/include promise.
- Legacy dense ownership over global mirrored `Memory<T>`.
- Build-global default layout and implicit device preference.
- Host-only assumptions in canonical M1 as a completed design.
- Dense I/O that directly streams unversioned rank/extents/values without the
  required core I/O/error model.
- Former top-level `linalg` ownership.

**New gaps**

- Dense-owned expression evaluation and explicit alias/materialization policy.
- CPU provider adapter beyond the reference path.
- GPU storage/evaluation and cuBLAS/cuSOLVER facets.
- Workspace, packing, transfer, synchronization, and asynchronous-lifetime
  contracts.
- Approved reductions, factorization, solver, and tensor-operation scope.

### 5.5 `sparse`

**Historical prior art**

- `SparseMArray`, sparse layout maps, COO insertion/finalization, CSR/CSC
  conversion, sparse iteration, and Eigen adapter/solver tests.
- Empty compressed outer-pointer invariant.
- Tests for duplicates, sorting, conversion, elementwise behavior, and sparse
  solvers.

**Useful work**

- COO/CSR/CSC behavioral expectations and the empty `outer_extent + 1`
  zero-pointer table.
- Duplicate coalescing and sorted-index test cases.
- Sparse conversion and residual tests as behavior-level evidence.

**Work to reject or redesign**

- Dense and sparse storage were in the same target and shared `UArray`,
  `MObject`, expression, memory, and layout machinery.
- One mutable `SparseMArray` combined building, finalization, compressed
  storage, mutation, views, operations, and conversion.
- Most sparse metadata used `int`, including NNZ and index arrays.
- Routine arithmetic converted through COO and allocated temporary tuples/maps.
- Scalar addition/subtraction changed only stored entries, an ambiguity that
  must be classified explicitly because mathematically it can densify.
- Historical expression reads sparse objects in dense logical order.
- Sparse algebra lived in broad linalg/Eigen headers with dense types and
  provider-native APIs.

**New gaps**

- General-rank coordinate owner/view contract.
- Separate rank-two finalized compressed owners/views and immutable structural
  invariants.
- 64-bit index/base/ordering/duplicate/explicit-zero contracts.
- Sparse-owned evaluator and explicit densification policy.
- CPU reference/provider SpMV/SpMM/solver scope.
- GPU storage/evaluation and cuSPARSE provider boundary.
- Storage-neutral dense operands for mixed operations without a dense include.

### 5.6 `random`

**Historical prior art**

- `RandomKey`, `RandomCounter`, and compiled Philox4x32-10.
- Stateless `Uniform01<float/double>`.
- Logical-order `FillRandom` over canonical dense views.
- Stateful pseudo-random engines, Halton/Hammersley/Latin/Sobol/normal/
  spherical samplers, permutation caches, and compiled Sobol data.

**Useful work**

- The explicit key/counter pure-function design is the strongest reusable
  historical API concept.
- Nine independently calculated Philox raw-word vectors and exact unit
  transform values.
- Layout/partition invariance, overflow, no-allocation, rollback, and
  concurrent independent-fill tests.
- Large immutable sequence tables may be useful after provenance and
  representation review.

**Work to split or reject**

- Historical `ASC::random` linked `ASC::array`; that is forbidden for the new
  base target.
- Historical `FillRandom` belongs conceptually in a random-owned dense
  generation facet.
- No sparse generation facet existed.
- Mutable singleton caches, implicit entropy reseeding, standard-library
  distribution output promises, and covariance-taking samplers must not become
  the base design.
- Covariance factorization belongs to dense algebra, not random.
- Provider and internal code used `asc::detail`.

**New gaps**

- Base engine/distribution/generator concepts that include no storage header.
- Engine state/serialization/version and parallel splitting decisions.
- `random_dense` and `random_sparse` facets with deterministic traversal/state
  consumption.
- Sparse structure/count/density/duplicate/zero-generation contracts.
- Optional GPU generator provider and accurately scoped CPU/GPU reproducibility
  promises.

## 6. Coupling and defect findings

All items in this section are current retained-document facts or historical
prior-art facts, not assertions about live production code.

### 6.1 Hard graph violations in the historical implementation

1. **Dense, sparse, and expression were one component.** Historical
   `src/array/CMakeLists.txt` installed dense and sparse headers in
   `ASC::array`; `marray.h` included both dense and sparse storage.
2. **Expression depended on concrete storage.** Historical `expr.h` explicitly
   specialized on `DenseMArrayLike` and `SparseMArrayLike`.
3. **Expression owned evaluation and execution choice.** `EvalTo` selected
   host/device paths and wrote destinations.
4. **Dense and sparse algebra were one component.** Historical
   `ASC::linalg` installed both canonical dense BLAS and compatibility sparse
   Eigen/solver headers.
5. **Random base depended on storage.** Historical `ASC::random` linked
   `asc_array` publicly and its public fill-validation header included array
   concepts.
6. **Provider dependencies were too broad.** Historical core linked OpenMP
   and CUDA runtime, cuBLAS, and cuSPARSE publicly; the target helper changed
   every compiled source to CUDA language when CUDA was enabled.

### 6.2 Ownership, execution, and safety defects

- Legacy `Memory<T>` had default shallow copy/assignment, a destructor that did
  not free, implicit pointer conversions, mutable flags in const operations,
  and manual `Delete`.
- Global `MemoryManager mm`, a global device singleton, global diagnostic
  streams, and mutable error policy made independent contexts and concurrency
  difficult.
- `UseDevice` combined storage preference and execution selection.
- Historical expressions and algorithms could trigger access/synchronization
  through `Read`/`ReadWrite`.
- Public compatibility preconditions often used a build-switchable assertion
  path rather than an invariant-stable result.

The canonical historical M1 core/dense path corrected many of these issues,
but it coexisted with the old path and never completed CPU/GPU provider
isolation.

### 6.3 Shape, sparse, and numerical defects

- Legacy expression compatibility compared flat size, not full shape.
- Legacy array and sparse metadata remained predominantly 32-bit.
- Compressed sparse rank claims exceeded consistently implemented behavior.
- Sparse routine arithmetic hid COO conversion and allocation.
- Sparse and dense aliases were coupled to a build-global default layout.
- Historical Eigen binary I/O wrote native index/scalar representations with
  no portable version, endian, or short-I/O contract and even contained an
  inline known-bug comment for a sparse case.
- Broad linalg headers combined elementwise operations, contractions,
  decompositions, provider adapters, and solver policy.

### 6.4 Style and public-surface defects

- Historical accessors commonly used `GetSize`, `GetExtent`, `UseDevice`, and
  similar PascalCase spellings; current Google-style accessor policy requires
  a fresh naming decision.
- Internal namespaces included `detail`, `details`, and `asc::detail`; current
  policy requires names containing `internal`.
- Some include guards were not full-path guards, for example historical
  `ASC_TIMER_H_` and `ASC_GENERIC_SPMARRAY_IMPL_H_`.
- Large installed implementation headers included approximately 1,900-line
  BLAS, 1,600-line LAPACK, 1,700-line sparse implementation, and 900-line dense
  implementation files.
- Historical `*_impl.h` files conflict with the current instruction not to
  create legacy `-inl.h`-style public implementation partitions; template
  visibility should instead use deliberately named public/internal headers.

### 6.5 Build, package, test, and CI defects

- Historical component tests were a useful start, but compatibility header
  tests linked `ASC::cpp`, masking some transitive dependency problems.
- The old package had no expression, dense, sparse, random-dense, or
  random-sparse components.
- The old relocation test did not cover all current requirements, including a
  path containing spaces and the two random storage facets.
- The retained CI is Linux-only, has no Windows/macOS/sanitizer/GPU runtime
  job, uses mutable `actions/checkout@v4`, and clones the tip of
  `asc-cmake` rather than binding the verified release commit.
- The retained workflow references deleted presets, CMake files, source
  directories, and tests, so it is currently non-executable.

## 7. Historical test evidence worth preserving

Historical test results in retained documents are claims from the deleted
implementation, not current validation. Nevertheless, the following test
designs should be retained or independently re-derived:

| Evidence family | New owner |
| --- | --- |
| 64-bit aliases, checked arithmetic, status/result, contracts | core |
| allocation failure/rollback/alignment/move/exactly-once release | core |
| independent immutable contexts and unavailable backend behavior | core |
| transactional config parsing/serialization and negative CLI values | core model + utilities parser |
| monotonic timer empty/running/reset/statistics behavior | utilities |
| extents, layouts, padded strides, const views, explicit clone | dense, with shared vocabulary in core |
| nested temporary expression lifetime and negative constraints | expression |
| dense BLAS1/2/3 shapes, aliasing, allocation counts, deterministic order | dense |
| sparse empty/duplicate/order/conversion/solver residual behavior | sparse |
| Philox vectors and `Uniform01` exact values | random base |
| logical-layout and partition-invariant dense generation | random dense facet |
| header self-containment and multi-TU ODR | every module/facet |
| forbidden include/target scans | every module/facet |
| installed isolated consumers and relocation | every module/facet |

Tests must be rewritten around the six-module target graph. Historical tests
that require `ASC::cpp`, concrete dense types inside sparse tests, provider
types in common APIs, implicit transfers, or global state cannot be accepted as
contract tests without redesign.

## 8. asc-cmake facts

The relevant `asc-cmake` checkout is clean:

| Field | Verified value |
| --- | --- |
| repository | `/home/yicai/AI4SciComp/asc-cmake` |
| remote | `git@github.com:AI4SciComp/asc-cmake.git` |
| branch | `main` |
| release tag | `v0.1.0` |
| release commit | `8a7dcbad3a97267cce59810aff24de800a3497a7` |
| minimum CMake | 3.25 |
| package name | `ASCCMake` |

Verified public functions relevant to asc-cpp are:

```text
asc_target_enable_cxx20
asc_target_enable_warnings
asc_target_enable_sanitizers
asc_add_project_options
asc_register_test
asc_install_package
```

The package creates no compiled imported target and deliberately does not own
consumer feature options, dependencies, CUDA policy, source lists, component
semantics, or product configuration headers.

Historical asc-cpp used `asc_add_project_options`, `asc_register_test`, and
`asc_install_package`, plus local `asc_cpp_configure_*` wrappers. The restart
must not assume those deleted local wrappers are required or that asc-cmake
contains component-mapping helpers. Package-component validation remains an
asc-cpp config-template responsibility. Ordinary target definitions,
dependencies, file sets, aliases, and provider discovery should use standard
target-oriented CMake plus only the verified asc-cmake API.

## 9. Proposal: exact module and facet graph

This is the recommended graph for reconciliation. Target spellings are
candidates pending the package/target ADR, but module ownership and dependency
ceilings are fixed.

| Owner | Candidate exported target | Direct asc-cpp dependencies |
| --- | --- | --- |
| core | `ASC::core` | none |
| utilities | `ASC::utilities` | `ASC::core` |
| expression | `ASC::expression` | `ASC::core` |
| dense | `ASC::dense` | `ASC::core`, `ASC::expression` |
| sparse | `ASC::sparse` | `ASC::core`, `ASC::expression` |
| random | `ASC::random` | `ASC::core` |
| random dense facet | `ASC::random_dense` | `ASC::random`, `ASC::dense` |
| random sparse facet | `ASC::random_sparse` | `ASC::random`, `ASC::sparse` |
| convenience umbrella | `ASC::cpp` | all six base modules; integration facets only if the package ADR explicitly says so |

There are exactly six modules. The two random integration facets and the
umbrella are not modules.

Provider facets remain owned by core, dense, sparse, or random. Their exact
target names should not be frozen by this report. Dense CPU/GPU algebra
providers must not be shared with sparse providers through a seventh linalg
target. CUDA runtime belongs to a core facet; cuBLAS/cuSOLVER belong to dense;
cuSPARSE belongs to sparse; cuRAND or a project-owned device generator belongs
to random.

## 10. Proposal: responsibility and file boundaries

The restart should converge on these public roots:

```text
include/asc/core/
include/asc/utilities/
include/asc/expression/
include/asc/dense/
include/asc/sparse/
include/asc/random/
```

Compiled implementation should use matching owner roots:

```text
src/core/
src/utilities/
src/dense/
src/sparse/
src/random/
```

Expression may remain header-only if it contains only real public
templates/concepts. It must not receive a compiled target containing fake or
empty behavior.

A bounded semantic file proposal is:

| Module | Proposed public file families | Proposed private/compiled families |
| --- | --- | --- |
| core | version/features, types/checked arithmetic, shape vocabulary, status/result, configuration values/schema, I/O source/sink, memory space/resource/buffer, device/execution/event | host resources, file resources, error translation, CPU execution, optional runtime adapters |
| utilities | command line, approved config-file parser, precedence/help, timer, justified standard-array wrappers | parser and filesystem/platform implementation |
| expression | participation concepts, capture/holders, scalar/unary/binary/reduction nodes, shape/promotion/traversal/sparsity metadata, customization points | none unless a real storage-neutral compiled facility is approved |
| dense | owner, const/mutable view, layout/stride/slicing, evaluator, elementwise/reductions, dense algebra | CPU reference kernels, CPU providers, GPU evaluator and cuBLAS/cuSOLVER adapters |
| sparse | coordinate builder/owner/view, compressed matrix owner/view, conversion, evaluator, sparse algebra | CPU reference kernels/providers, GPU evaluator and cuSPARSE adapters |
| random | state/key/counter, engines, distributions, generator concepts, optional state serialization, dense/sparse facet headers | engine tables/implementations, dense/sparse generation implementations, optional GPU generator adapter |

Every public file must end in `.h`, every ordinary compiled C++ file in `.cc`,
and every public header must be self-contained with a full-path include guard.
Public declarations stay in flat `namespace asc`; non-public C++ namespaces
must contain `internal`.

## 11. Proposal: bounded restart sequence

No production implementation should begin before the architecture package and
required ADRs are approved.

After approval, the restart should be bounded as follows:

1. **Repository foundation:** root project, formatting/analysis configuration,
   exact asc-cmake v0.1.0 binding, package config, dependency-policy test
   harness, and consumer fixtures. Export no empty module targets.
2. **Core CPU foundation:** implement only the approved types, checked
   arithmetic/shape vocabulary, configuration model, status/error boundary,
   low-level I/O, host resource/buffer, and serial execution/event contracts.
3. **Independent foundations:** separate waves for utilities, expression, and
   random base. Each gets an isolated package consumer and forbidden-edge
   checks.
4. **Dense CPU:** adapt checked descriptor/view ideas and serial dense kernels
   only after expression is stable.
5. **Sparse CPU:** build independent coordinate/compressed storage,
   evaluation, and sparse algebra without a dense dependency.
6. **Random integration:** add dense and sparse generation as separate
   random-owned facets.
7. **GPU waves:** core runtime first, then dense, sparse, and random provider
   facets with real-hardware evidence.

Historical files should be dispositioned individually before reuse. Directory
restoration is inappropriate because most old files combine accepted behavior
with forbidden dependencies or obsolete public ownership.

## 12. Decisions still required before implementation

The following are material architecture blockers, not implementation details:

- package name, exact target/facet names, component request behavior, and
  umbrella semantics;
- public status/result and no-exception policy;
- configuration value/schema types, nesting, defaults, required values,
  unknown keys, provenance, and parser precedence;
- source/sink ownership and portable encoding;
- index/extent/rank/shape and checked arithmetic;
- memory spaces, resources, external ownership, explicit copies, device IDs,
  streams, events, asynchronous lifetimes, and fallback;
- dense owner/view/layout/copy/resize/alias contracts;
- sparse formats, builder/finalized invariants, index base/width, duplicates,
  explicit zeros, mutation, and densification;
- expression customization, capture, broadcasting, alias, traversal, and
  sparsity effects;
- dense and sparse algebra scopes and provider selection;
- mixed sparse/dense operation ownership without a direct module edge;
- random engines, state serialization, parallel splitting, traversal, sparse
  generation, and CPU/GPU reproducibility;
- third-party dependency, licensing, provenance, ABI, and release boundaries.

## 13. Prioritized risks and blockers

### P0 — blocks architecture approval or safe implementation

1. **Current tree is non-buildable.** This is intentional, but retained docs
   and CI must not be mistaken for live evidence.
2. **Five-component historical contracts conflict with the six-module owner
   contract.** Restoring `array` or `linalg` would immediately violate scope.
3. **Expression neutrality is unresolved.** The historical implementation is
   unusable as the new base because it names dense/sparse storage and evaluates
   destinations.
4. **Dense/sparse algebra ownership is unresolved at mixed-operation
   boundaries.** An ADR must prevent a hidden seventh module or sibling edge.
5. **Provenance must be revalidated before source adaptation.** Retained docs
   state source-level MdeCpp adaptation from
   `f6294e9079262682ce63ae7ff2d8a643e658bf5d`, while the historical
   `THIRD_PARTY_NOTICES` and all attributed source are currently deleted.
   Apache-2.0 is retained, but algorithm/file provenance and notices still need
   an approved disposition.

### P1 — high implementation and correctness risk

1. Reintroducing global/mirrored memory semantics through compatibility code.
2. Reusing flattened-size expressions without shape/sparsity metadata.
3. Reusing sparse arithmetic that hides COO conversion or densification.
4. Exposing provider headers/libraries through base components.
5. Treating a CUDA build as GPU runtime evidence without device execution.
6. Freezing a scalar/layout/index policy before vendor narrowing and downstream
   needs are reviewed.
7. Overloading core with unrelated helpers instead of the exact shared
   foundation.
8. Permanent compatibility with deleted PascalCase APIs, `asc::detail`, or
   five-component include paths.

### P2 — quality and delivery risk

1. Retained documentation is extensive but materially stale.
2. Current CI is both non-executable and below the required portability matrix.
3. Formatter/analyzer tools are absent from the current host `PATH`.
4. Historical public headers were large and template-heavy.
5. Package tests need paths-with-spaces, base/facet isolation, requested
   unavailable-provider failure, and both build/install graph auditing.

## 14. Evidence commands

Representative exact commands used for this report:

```bash
cd /home/yicai/AI4SciComp/asc-cpp
git rev-parse --show-toplevel
git rev-parse HEAD
git symbolic-ref --short HEAD
git status --short --branch
git diff --stat
git diff --name-status
git branch --all --verbose --no-abbrev
git worktree list --porcelain
git tag --list
git log --oneline --decorate -n 30
rg --files -uu -g '!.git/**'
git show --stat --oneline --summary 33b261ea33616a6395c4ad3b20646093103344f7
git show HEAD:CMakeLists.txt
git show HEAD:src/array/CMakeLists.txt
git show HEAD:src/linalg/CMakeLists.txt
git show HEAD:src/random/CMakeLists.txt
git show HEAD:cmake/ASCCppConfig.cmake.in
git show HEAD:include/asc/array/expr.h
git show HEAD:include/asc/core/buffer.h
git show HEAD:include/asc/core/memory.h
git show HEAD:include/asc/array/tensor_view.h
git show HEAD:src/linalg/providers/serial/reference_kernels.cc
git show HEAD:src/random/engines/philox.cc
git show HEAD:tests/random/unit_test_engine.cc
git show HEAD:tests/package_test.cmake
git grep -n -E '^#include [<"]asc/' HEAD -- include/asc
git grep -n -E 'namespace asc::detail|namespace detail' HEAD -- include/asc src
git diff --check
```

Remote issue/PR inventory:

```bash
gh repo view AI4SciComp/asc-cpp \
  --json nameWithOwner,url,defaultBranchRef,isPrivate,visibility,description
gh issue list --repo AI4SciComp/asc-cpp --state open --limit 100 \
  --json number,title,updatedAt,url
gh pr list --repo AI4SciComp/asc-cpp --state open --limit 100 \
  --json number,title,headRefName,baseRefName,updatedAt,url,isDraft
```

Relevant asc-cmake binding:

```bash
cd /home/yicai/AI4SciComp/asc-cmake
git rev-parse HEAD
git status --short --branch
git tag --list --sort=-version:refname
git log --oneline --decorate -n 10
rg -n '^(function|macro)\(' modules
```

## 15. Final assessment

There is useful engineering history, but no live implementation to preserve.
The right restart is selective and evidence-driven:

- retain the explicit-state, checked, component-tested ideas;
- reassign old array/linalg work to the six owners;
- reject concrete-storage expressions, dense/sparse entanglement, global
  execution/memory correctness, and broad provider exposure;
- treat every old test result as historical until rebuilt under isolated
  six-module targets;
- resolve the P0 ADR and provenance blockers before production code.

This analysis does not authorize restoring deleted files, creating production
targets, or implementing any module.
