# Changelog

All notable changes to the clean restart are recorded here. Milestone
completion and release publication require separate approval.

## Unreleased

### Advanced samplers and storage adapters (Issue 15)

- Added allocation-free generic pseudo and Dense Latin/Halton/Hammersley/Sobol
  fills over caller-owned views and explicit workspaces.
- Added clean-room lower-Cholesky preparation, multivariate-normal Dense
  sampling, and bounded normalized-Gaussian unit-sphere sampling on the
  portable serial CPU path.
- Added structure-preserving coordinate/CSR/CSC value fills and average-linear
  exact-count Sparse ordinal selection into caller-owned candidate/output
  workspace while retaining the combined owner generator unchanged.
- Added deterministic known-answer, parameter/failure, padded-layout,
  statistical, allocation, package/relocation consumer, CUDA-context rejection,
  crosswalk, documentation, and correctness-guarded performance checks.

No target, dependency, storage customization protocol, factorization package,
GPU implementation, compressed random output, hidden allocation/workspace,
transfer, synchronization, fallback, or global state is added. MdeCpp remains
behavioral evidence only; no GPL-covered code, data, tests, benchmarks, or
prose is reused.

### Dense BLAS Level 3 (Issue 9)

- Added all 30 frozen classic dense BLAS Level 3 S/D/C/Z rows on the portable
  serial CPU path and explicit CUDA provider: Gemm, Symm, Hemm, Syrk, Herk,
  Syr2k, Her2k, Trmm, and Trsm.
- Added common checked row/column-major matrix descriptors with explicit side,
  triangle, transpose/conjugation, and unit-diagonal controls.
- Added CPU/CUDA conformance, invalid-input, degenerate, edge-value,
  no-allocation, packaging-consumer, generated coverage, documentation, and
  representative Gemm benchmark checks.

No Sparse BLAS or optimized CPU provider is included. Successful Level 3
calls add no hidden allocation, transfer, packing, synchronization, or
fallback; CUDA returns an explicit completion event and retains only the
established error-path stream drain.

### Dense BLAS Level 2 (Issue 8)

- Added all 66 frozen classic dense BLAS Level 2 S/D/C/Z rows on the portable
  serial CPU path and explicit CUDA provider: general, symmetric, Hermitian,
  triangular, band, packed, solve, and rank-update families.
- Added checked caller-owned full, general-band, triangular-band, and packed
  matrix descriptors with row- and column-major layouts, plus transpose,
  conjugate-transpose, triangle, and unit-diagonal controls.
- Added CPU/CUDA conformance, signed-stride, invalid-input, edge-value,
  allocation, packaging-consumer, generated coverage, public example, and
  representative Gemv latency/estimated-bandwidth benchmark evidence.

No Level 3 routine or optimized CPU provider is included. Successful Level 2
calls add no hidden allocation, transfer, packing, synchronization, or
fallback; CUDA returns an explicit completion event and retains only the
established error-path stream drain.

### Dense BLAS Level 1 remediation (Issue 7)

- Added the complete frozen classic dense BLAS Level 1 real and complex
  families on the portable serial CPU path and the explicit CUDA provider,
  including mixed/extended dot variants.
- Added checked caller-owned signed-stride vector descriptors, asynchronous
  caller-owned CUDA scalar results, and explicit device workspace for
  zero-based `CudaIamax` result conversion.
- Added CPU/CUDA conformance, negative-stride, invalid-input, extreme-value,
  allocation, packaging-consumer, documentation-example, coverage-manifest,
  and representative benchmark evidence.

No Level 2 or Level 3 routine or optimized CPU provider is included.
Successful Level 1 calls add no hidden allocation, transfer, synchronization,
or fallback; the established post-enqueue CUDA failure drain remains explicit
in the error contract.

### Milestone 8: packaging/API/performance/downstream hardening

- Advanced the unreleased package candidate to 0.9.0 without adding or
  changing a product target, direct dependency, numerical capability, or
  provider.
- Hardened component/version metadata, exact transitive package closures,
  copied build-tree and relocated installed consumption, repeated component
  lookup, installed-header equivalence, and provider-disabled isolation.
- Added deterministic public-header, target/component, shared-symbol, and
  bounded local ABI inventories plus compile-time/object-size and existing
  runtime performance observations.
- Added asc-xde-shaped isolated downstream trials using only the minimal
  public CPU package surface. Default validation is synthetic and independent
  of sibling checkout state; a separate explicit path-and-commit opt-in audits
  a real asc-xde repository without writing it.
- Corrected post-enqueue CUDA failure draining, complete sparse structural
  alias rejection, checked Timer and integral-reduction arithmetic, optional
  CUDA component lookup without a toolkit, guarded test workspaces, truthful
  no-device skips, and exact-environment ABI baseline selection.
- Added reviewed support, compatibility, package-capability, extension,
  downstream, and performance documentation and a full locally available
  GCC/Clang/static/shared/sanitizer/CUDA validation matrix.

Milestone 8 is not a release or a 1.0 compatibility claim. Hosted GPU,
multi-device, Windows, and macOS results remain separate evidence and are not
inferred from local Linux validation.

### Milestone 7: sparse CUDA and random CUDA facets

- Added opt-in `asc_sparse_cuda` / `ASC::sparse_cuda` with explicit host-CSR
  cloning, deterministic unit-stride cuSPARSE CSR SpMV, a zero-workspace
  positive-nonunit-stride project kernel, and bounded structure-preserving
  sparse evaluation for `float` and `double`.
- Added opt-in `asc_random_cuda` / `ASC::random_cuda` for the approved
  Philox4x32-10 word mapping, plus `asc_random_dense_cuda` /
  `ASC::random_dense_cuda` and `asc_random_sparse_cuda` /
  `ASC::random_sparse_cuda` for deterministic device-resident storage
  generation with explicit address advancement.
- Advanced the unreleased package candidate to 0.7.0. Each CUDA component
  loads only its exact component closure; provider-free requests and
  `ASC::cpp` remain free of CUDA discovery and target edges.
- Added real-hardware structural, numerical, and bit-parity validation,
  independent Philox and sparse-structure oracles, exact package/relocation
  consumers, concurrency and lifetime checks, and bounded performance
  observations.

No cuRAND, Thrust, CUB, implicit transfer, hidden fallback, later provider,
Milestone 8 hardening, or release publication is included.

### Milestone 6: GPU core and dense

- Added opt-in `asc_core_cuda` / `ASC::core_cuda` with explicit CUDA device
  discovery, pinned/device/managed resources, owned nonblocking streams,
  asynchronous copy submission, and move-only completion events.
- Added opt-in `asc_dense_cuda` / `ASC::dense_cuda` with a move-only cuBLAS
  context, bounded pointwise evaluation, and selected
  Copy/Scal/Axpy/Gemv/Gemm operations for `float` and `double`.
- Added extents-first `DenseArray::CreateUninitialized` for exact
  unique/exhaustive storage without a host initialization pass, while
  retaining host-only value-initializing `Create`.
- Advanced the unreleased package candidate to 0.6.0. CUDA provider exports,
  CUDAToolkit discovery, and provider dependencies exist only when the package
  is built with `ASC_CPP_ENABLE_CUDA=ON` and a provider component is requested;
  `ASC::cpp` remains provider-free.
- Added real-hardware Core/Dense CUDA validation, CPU/GPU numerical parity,
  provider header/ODR/negative compile contracts, CUDA component consumers,
  relocation and disabled-provider isolation, concurrency/lifetime checks,
  and benchmark evidence.

No Sparse CUDA, Random CUDA, HIP, SYCL, native-handle adoption, hidden
transfer/workspace/fallback, or later-milestone implementation is included.

### Milestone 5: random storage generation

- Added `asc_random_dense` / `ASC::random_dense`, an allocation-free
  interface facet that fills caller-provided Dense views from explicit
  Philox stream, subsequence, and word offsets in logical coordinate order.
- Added `asc_random_sparse` / `ASC::random_sparse`, an exact-count canonical
  coordinate generator with independent structure and value address domains,
  deterministic priority selection, and explicit caller-owned allocation.
- Added the provider-free `asc_cpp` / `ASC::cpp` convenience aggregate after
  all six base modules and both approved Random facets became available.
- Advanced the unreleased package candidate to 0.5.0 with independently
  consumable, relocatable Random Dense and Random Sparse components.

No new engine, distribution, entropy source, mutable generator, third-party
dependency, optional provider, GPU target, compressed Sparse generation, or
later-milestone behavior is included.

### Milestone 4: sparse CPU semantics

- Added `asc_sparse` / `ASC::sparse`, with explicit-capacity coordinate
  construction, stable canonical finalization, explicit duplicate/zero
  policies, and move-only finalized ownership.
- Added canonical rank-two CSR and CSC owners/views and all named
  coordinate/CSR/CSC conversions without hidden COO round trips.
- Added storage-neutral placement/writable Expression customization and
  structure-preserving Sparse evaluation into caller-owned structures.
- Added allocation-free serial CPU CSR SpMV for `float` and `double` over
  external or explicitly included Dense vector adapters.
- Advanced the unreleased package candidate to 0.4.0. Sparse depends directly
  only on Core and Expression and never imports Dense.

No random storage facet, structural expression growth, densification, SpMM,
solver, optional provider, GPU implementation, or aggregate is included.

### Milestone 3: dense CPU semantics

- Added `asc_dense` / `ASC::dense`, with checked left-, right-, and
  explicit-stride mappings, non-owning views, rank-preserving subviews, and
  move-only ownership backed by Core memory resources.
- Added allocation-free expression evaluation and scalar sum, minimum, and
  maximum reductions with explicit alias handling.
- Extended the storage-neutral expression adapter with optional recursive
  execution-access validation so Dense rejects inaccessible direct or nested
  source views before destination mutation.
- Added allocation-free serial CPU reference implementations of copy, scale,
  AXPY, dot product, stable Euclidean norm, matrix-vector multiply, and
  matrix-matrix multiply for `float` and `double`.
- Advanced the unreleased package candidate to 0.3.0. Dense depends directly
  only on Core and Expression; all optional providers remain absent.

No sparse storage, random storage facet, optional provider, GPU
implementation, implicit transfer, synchronization, fallback, or aggregate
target is included.

### Milestone 2: independent foundation modules

- Added `asc_utilities` / `ASC::utilities`, with transactional schema-directed
  command-line configuration, deterministic help rendering, command-line
  origins, positional arguments, and a monotonic stateful timer.
- Added the header-only `asc_expression` / `ASC::expression` component, with a
  non-intrusive storage-neutral adapter protocol, explicit alias and sparsity
  metadata, safe operand capture, and exact-shape pointwise nodes with scalar
  expansion.
- Added `asc_random` / `ASC::random`, with a clean-room Philox4x32-10 pure
  raw-bit engine, explicit stream/subsequence/word-offset mapping, checked
  offset advancement, and exact scalar unit-interval transforms.
- Advanced the unreleased package candidate to 0.2.0 and made Core, Utilities,
  Expression, and Random independently consumable components.

The three new components depend directly only on Core and not on one another.
No configuration-file parser, writable storage, evaluator, result
materialization, general broadcasting, random fill, entropy, distribution
beyond scalar unit-uniform transforms, provider, or GPU implementation is
included.

### Milestone 1: core CPU foundation

- Added the provider-free `asc_core` library and `ASC::core` package
  component, using only C++20 standard-library facilities.
- Added stable error codes, value-oriented status and result handling, and
  release-active fatal contracts without a mutable global handler.
- Added signed 64-bit logical metadata, checked integral arithmetic, and
  mixed static/dynamic extents with validated logical sizes.
- Added recursive programmatic configuration values, schemas, transactional
  validation, origins, and sensitive-value redaction.
- Added partial byte-source/sink interfaces, exact-transfer helpers,
  move-only local files, bounded text helpers, and portable little-endian
  scalar encoding.
- Added aligned host byte ownership, non-owning byte views, an explicit serial
  execution context, overlap-safe host copies, and completed move-only events.
- Advanced the unreleased package candidate to 0.1.0 and added core-specific
  compile, runtime, package, relocation, and isolated-consumer validation.

### Milestone 0: architecture and repository foundation

- Approved the six-module Stage A architecture, dependency and capability
  manifests, 18 ADRs, implementation plan, roadmap, and provenance policy.
- Replaced the deleted implementation's build entry with a target-free CMake
  project bound to released `ASCCMake` 0.1.0.
- Added an unreleased `ASCCpp` 0.0.0 package skeleton that recognizes future
  component names but truthfully reports every component unavailable.
- Added architecture, package, installation, relocation, path-with-spaces,
  and package-registry isolation checks.
- Added portable CMake presets, formatting and analysis policy, and a
  least-privilege hosted CI design.

Milestone completion does not authorize publication or a release.
