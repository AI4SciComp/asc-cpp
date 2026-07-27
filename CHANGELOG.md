# Changelog

This file records user-visible asc-cpp changes. Milestone completion,
publication, and release are separate decisions.

## Unreleased

### Milestone 8 packaging/API/performance/downstream hardening

- Advanced the package candidate to `0.9.0` without adding a product target,
  public C++ API, operation, or direct dependency.
- Derived installed component closures and export names from the frozen
  component manifest, reducing package/source graph drift while preserving
  isolated component lookup and provider-free default consumption.
- Added deterministic public-header, package-target, component, and
  shared-symbol observations plus independent installed-package,
  compile/object-size, and asc-xde-shaped downstream probes.
- Added explicit support, API compatibility, package capability, extension,
  downstream integration, and performance guidance for the complete
  Milestones 1--7 surface.

### Milestone 8 evidence limits

- The `0.9.x` package remains an unreleased hardening candidate; no cross-0.x
  ABI promise or absolute cross-machine performance threshold is introduced.
- Symbol and object-size observations detect unexpected drift but do not turn
  internal support symbols or compiler output into a public API guarantee.
- The downstream trial is an asc-cpp-owned isolated fixture shaped like
  `asc-xde`; the sibling repository is not modified.
- Hosted CI and remote publication remain pending. This entry is not a release
  record.

### Milestone 7 sparse CUDA and random CUDA facets

- Advanced the package candidate to `0.7.0`.
- Added optional compiled `ASC::sparse_cuda`, `ASC::random_cuda`,
  `ASC::random_dense_cuda`, and `ASC::random_sparse_cuda` facets with their
  exact approved owner-specific dependency closures.
- Added explicit canonical host-CSR cloning, deterministic float/double CSR
  SpMV, and bounded structure-preserving coordinate/CSR sparse evaluation.
- Added bit-identical asynchronous CUDA Philox raw-word generation, dense
  Uniform01 fill, and exact-count canonical coordinate generation.
- Kept CUDA discovery conditional, provider SDK types out of public headers,
  random storage facets independently consumable, and `ASC::cpp`
  provider-free.

### Milestone 7 evidence limits

- Unit-stride SpMV uses `CUSPARSE_SPMV_CSR_ALG2` with queried caller-owned
  workspace; positive nonunit strides use the original project kernel with
  zero workspace.
- Raw external CUDA allocations have Runtime pointer classification but no
  CUDA-Runtime allocation-bound query; exact subspan bounds are enforced for
  allocations owned by `CudaMemoryResource`.
- A trusted device CSC staging producer is not in the approved M7 surface, so
  the CSC evaluator success path remains skipped rather than inferred.
- Hosted GPU CI, multi-device hardware, non-Linux CUDA, and later sparse or
  distribution capabilities remain outside local evidence.
- This entry describes an unreleased candidate and is not a release record.

### Milestone 6 GPU core and dense

- Advanced the package candidate to `0.6.0`.
- Added optional compiled `ASC::core_cuda` and `ASC::dense_cuda` facets with
  exact direct ASC edges `core` and `dense + core_cuda`.
- Added explicit CUDA pinned/device/managed resources, owned nonblocking
  streams, stream-scoped copies, queryable completion events, and synchronous
  named dense clone across accessible spaces.
- Added bounded float/double rank-zero-through-eight pointwise CUDA evaluation
  and asynchronous Copy/Scal/Axpy/Gemv/Gemm; cuBLAS is confined to compatible
  column-major Gemv/Gemm.
- Selected the shared CUDA runtime for every CUDA-enabled producer target so
  core and dense provider facets observe one runtime and error state.
- Kept CUDA discovery conditional, package components isolated, and
  `ASC::cpp` provider-free.

### Milestone 6 evidence limits

- Pageable-host CUDA copies may stage or block the host call; the returned
  event is the completion contract, and pinned memory is required for a
  stronger host-asynchronous transfer expectation.
- Destroying or replacing a live dense CUDA context destroys its cuBLAS handle
  and may device-wide synchronize; successful submitted operations remain
  asynchronous and event-tracked.
- Dot, Nrm2, reductions, factorization, solvers, mixed precision, complex
  values, sparse/random CUDA, native handles, and external streams remain
  deferred.
- Hosted GPU CI remains pending; exact local configure/compile/runtime/parity,
  sanitizer, Compute Sanitizer, package, and benchmark evidence belongs in
  Publication Checkpoint B.
- This entry describes an unreleased candidate and is not a release record.

### Milestone 5 random storage generation

- Advanced the package candidate to `0.5.0`.
- Added header-only `ASC::random_dense` and `ASC::random_sparse` facets with
  the exact direct edges `random + dense` and `random + sparse`.
- Added deterministic explicit-state serial host dense uniform fill for
  `float` and `double`, with checked word advancement, logical
  dimension-zero-fastest traversal, partition equivalence, and no operation
  allocation.
- Added deterministic exact-count canonical sparse coordinate generation with
  separate structure/value domains, checked offset advancement, explicit
  output resource ownership, and only the declared coordinate/value output
  allocations.
- Added the provider-free `ASC::cpp` aggregate and made no-component package
  lookup succeed, while preserving isolated base and facet component closures.

### Milestone 5 evidence limits

- The sparse selection rule is a deterministic pseudorandom reference
  algorithm, not a mathematical-uniform-subset claim.
- Generation is serial host-only; mutable/default engines, additional
  distributions, density mode, compressed output, optimized CPU providers,
  and all GPU providers remain deferred.
- CUDA and all other GPU evidence are exactly `skipped`.
- Hosted GCC, Clang, MSVC, and AppleClang results remain pending until the
  branch is published and CI runs.
- This entry describes an unreleased candidate and is not a release record.

### Milestone 4 sparse CPU

- Advanced the package candidate to `0.4.0`.
- Added compiled `ASC::sparse` with exactly `ASC::core` and
  `ASC::expression` as direct dependencies and no dense dependency.
- Added general-rank canonical coordinate construction with explicit duplicate
  and zero policies, rank-two canonical CSR/CSC owners and views, and all six
  named coordinate/CSR/CSC conversions.
- Added storage-neutral writable and placement expression customization,
  span-aware conservative alias metadata, destination-structure sparse
  evaluation, and deterministic serial CSR SpMV for `float` and `double`.
- Added sparse compile contracts, malformed-metadata and rollback checks,
  conversion and numerical oracles, allocation instrumentation, negative
  compilation, build/install/relocation, subproject, and isolated-consumer
  gates.

### Milestone 4 evidence limits

- Sparse ownership, conversion, evaluation, and algebra are serial host-only;
  random storage facets, SpMM, solvers, optimized CPU providers, and all GPU
  providers remain deferred.
- Successful evaluation and SpMV perform no hidden allocation, workspace,
  packing, densification, conversion, transfer, synchronization, dispatch, or
  fallback.
- CUDA and all other GPU evidence are exactly `skipped`.
- Hosted GCC, Clang, MSVC, and AppleClang results remain pending until the
  branch is published and CI runs.
- This entry describes an unreleased candidate and is not a release record.

### Milestone 3 dense CPU

- Advanced the package candidate to `0.3.0`.
- Added compiled `ASC::dense` with exactly `ASC::core` and `ASC::expression`
  as direct dependencies.
- Added checked rank-static left, right, and explicit non-negative-stride
  mappings; trivially copyable mutable/const views; rank-preserving subviews;
  and move-only host owners with explicit clone and transactional
  discard-resize.
- Added storage-neutral expression evaluation, rank-zero scalar expansion,
  deterministic sum/minimum/maximum reductions, conservative overlap
  rejection, and zero hidden allocation, packing, transfer, synchronization,
  or fallback.
- Added serial reference `Copy`, `Scal`, `Axpy`, `Dot`, scaled `Nrm2`, `Gemv`,
  and `Gemm` for `float` and `double`, including arbitrary validated
  non-negative strides and transpose modes.
- Added dense compile contracts, negative compilation, numerical,
  transaction, overflow, allocation, benchmark, build/install/relocation,
  subproject, and isolated-consumer evidence.

### Milestone 3 evidence limits

- Dense owners, evaluation, and algebra are serial host-only; sparse storage,
  random storage facets, factorization/solver APIs, and optimized CPU
  providers remain deferred.
- CUDA and all other GPU evidence are exactly `skipped`.
- Hosted GCC, Clang, MSVC, and AppleClang results remain pending until the
  branch is published and CI runs.
- This entry describes an unreleased candidate and is not a release record.

### Milestone 2 independent foundation modules

- Advanced the package candidate to `0.2.0`.
- Added independent `ASC::utilities`, `ASC::expression`, and `ASC::random`
  components, each with exactly one direct ASC dependency: `ASC::core`.
- Added transactional command-line configuration parsing with deterministic
  help and explicit command-line origin metadata, including additive core
  origin-map validation, plus monotonic accumulated timing.
- Added a storage-neutral, non-intrusive expression customization protocol,
  safe value/reference capture, exact-shape pointwise nodes, scalar expansion,
  conservative alias metadata, and sparsity-effect metadata.
- Added a clean-room Philox4x32-10 raw-bit engine, explicit
  stream/subsequence/word-offset addressing, checked offset advancement, and
  exact `Uniform01<float>` and `Uniform01<double>` transforms.
- Added isolated build-tree, install-tree, relocation, path-with-spaces,
  static/shared, subproject, and consumer verification for each new component.

### Milestone 2 evidence limits

- No local configuration-file syntax, array convenience wrapper, expression
  evaluator, result storage, random storage-generation facet, mutable/default
  random engine, additional distribution, provider, or GPU implementation is
  included.
- CUDA and all other GPU evidence are `skipped` for this provider-free
  milestone.
- Hosted GCC, Clang, MSVC, and AppleClang results remain pending until the
  branch is published and CI runs.
- This entry describes an unreleased candidate and is not a release record.

### Milestone 1 core CPU foundation

- Advanced the package candidate to `0.1.0`.
- Added the real `asc_core` library with build-tree and installed target
  `ASC::core`; it has no direct ASC or external dependency.
- Added self-contained public headers for checked logical metadata and
  extents, status/result transport, release-active contracts, configuration
  validation, portable byte/text I/O, host memory ownership, and serial CPU
  execution.
- Added explicit development controls for warnings-as-errors and Address,
  UndefinedBehavior, Thread, and Leak sanitizers.
- Made `core` the only available package component. No-component lookup still
  requests the unavailable `cpp` aggregate, and all later components remain
  unavailable.
- Added isolated core build-tree, install-tree, relocation,
  path-with-spaces, static/shared, subproject, and consumer verification.

### Evidence limits

- There is no optimized CPU provider, GPU provider, numerical kernel,
  asynchronous execution, or performance claim.
- CUDA and all other GPU evidence are `skipped` for this provider-free
  milestone.
- Hosted GCC, Clang, MSVC, and AppleClang results remain pending until the
  branch is published and CI runs.
- This entry describes an unreleased candidate and is not a release record.

### Repository restart foundation

- Replaced stale five-component guidance with the approved six-module
  architecture: `core`, `utilities`, `expression`, `dense`, `sparse`, and
  `random`.
- Established the Milestone 0 repository, package, test, documentation, and
  CI foundation.
- Marked retained documents from historical HEAD
  `33b261ea33616a6395c4ad3b20646093103344f7` as superseded.

Future entries must distinguish implemented behavior from plans and record
component availability, compatibility changes, provider evidence, migration
notes, and known limitations.
