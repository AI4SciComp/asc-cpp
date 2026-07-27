# Milestone 7 Documentation and API Review

Status: Independent review complete; disclosed limitations remain

Date: 2026-07-27

Role: documentation and API reviewer

Writable scope:

```text
docs/modules/sparse.md
docs/modules/random.md
docs/development/asc-cpp-m7-gpu-sparse-random/documentation-api-review.md
```

## Review boundary

This review is distinct from production implementation, independent
verification, lead integration, and portability/GPU/performance review. It
uses the complete frozen Milestone 7 contract and ownership ledger, the
accepted ADRs, actual public headers and implementation, declared package
behavior, primary provider documentation, and independently compiled examples.

Production, tests, CMake, package files, architecture manifests, root
documentation, and every other specialist report are read-only to this role.
Public API defects are reported to the lead and production owner rather than
documented around.

The review does not inspect or copy MdeCpp, the user-deleted asc-cpp
implementation or tests, third-party sample source, or historical CUDA
wrappers.

## Authoritative material read

- the complete frozen M7 milestone contract and ownership ledger;
- approved ADRs 0008, 0012, 0014, 0015, 0017, and 0018;
- the complete existing sparse and random module guides;
- the M7 preflight and independent verification design;
- the current provider-neutral and M6 CUDA public headers needed to establish
  ownership, memory, execution, and expression behavior;
- the current
  [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html);
  and
- CUDA Toolkit 12.9 Runtime and
  [cuSPARSE documentation](https://docs.nvidia.com/cuda/archive/12.9.2/cusparse/index.html)
  for pointer classification, stream/event behavior,
  signed 64-bit CSR descriptors, SpMV algorithms, workspace, and
  determinism.

The current Google guide targets C++20, requires self-contained headers, and
prefers standard concepts to equivalent traits. The cuSPARSE 12.9
documentation states that `CUSPARSE_SPMV_CSR_ALG2` with non-transpose CSR is
the deterministic SpMV choice and that CSR SpMV requires explicit external
storage reported by `cusparseSpMV_bufferSize`.

## Findings

### M7-DOC-01: zero-count raw fill records a CUDA event

Initial severity: release-blocking asynchronous-contract defect

Initial evidence: the first implementation of `CudaFillPhilox4x32` created and
recorded a pending CUDA event after its kernel launcher returned immediately
for `word_count == 0`. The frozen contract requires exact no-op behavior before
enqueue, and the independent verification design explicitly treats zero count
as an enqueue-free success.

Required resolution: return a valid, already-complete event for zero work
without recording work on the CUDA stream. The existing public event type has
no provider-accessible completed-event factory, so production reported the
required core-internal integration point to the lead rather than documenting
the event record as acceptable.

Resolution: the lead added a core-internal completed-event factory. Raw and
dense zero-size paths and sparse zero-count paths now return that event before
creating or recording a CUDA event.

Status: resolved in production; independent runtime instrumentation pending.

### M7-DOC-02: device allocation bounds were not established

Initial severity: release-blocking validation and memory-safety defect

Initial evidence: the initial raw random provider checked nullability,
alignment, representable pointer-end arithmetic, CUDA pointer classification,
and device identity, but the shared M6 memory helper did not establish that
the complete requested byte range remained within the underlying CUDA
allocation.

Required resolution: establish allocation bounds without adding an unapproved
provider dependency, or disclose the exact unsupported case.

Resolution: CUDA Runtime 12.9 has no `cudaMemGetAddressRange`; that name is not
a Runtime API. The only general query is Driver API
`cuMemGetAddressRange`, and CUDA Driver is not an approved M7 dependency. The
lead extended the core CUDA validator with an internal registry, so every
ASC-owned `CudaMemoryResource` allocation and interior subspan receives exact
bound checking. Arbitrary external Runtime pointers receive placement,
device, alignment, and representable-address checks, but their declared size
remains a caller contract.

Status: resolved for ASC-owned storage; arbitrary external allocation bounds
remain a disclosed M7 contract/evidence limitation.

### M7-DOC-03: dense device alignment and lifetime wording

Initial severity: release-blocking validation defect plus public-contract
clarity

Initial evidence: `CudaFillDenseUniform01` accepts a caller-created
`DenseView`. View creation proves nullability, representable span arithmetic,
and a unique mutable mapping, but does not prove that the data pointer is
aligned for `float` or `double`. The first provider implementation did not add
that check. Its declaration comment also required the copied, non-owning view
descriptor to remain alive rather than naming the backing device storage.

Required resolution: validate scalar alignment and every available
allocation-bound check before enqueue. State that backing device storage and the
context remain alive through completion; keeping a view variable alive does
not own or extend that storage.

Resolution: production added type alignment and core memory validation. Exact
registered bounds apply to ASC-owned storage; the M7-DOC-02 limitation applies
to arbitrary external storage. The declaration now names backing device
storage rather than the copied view descriptor.

Status: resolved subject to the disclosed external-allocation limitation.

### M7-DOC-04: provider-neutral sparse owner names a random CUDA implementation

Initial severity: architecture dependency defect

Initial evidence: the first device-owner enabling change in
`<asc/sparse/coordinate.h>` forward-declared and friended
`internal_random_sparse_cuda::CoordinateArrayFactory`. That makes the
provider-free sparse owner explicitly know a downstream random provider
implementation, even though the target graph correctly points from
`random_sparse_cuda` to sparse.

Required resolution: expose the minimum generic sparse-internal owner factory
or access helper needed for explicit device allocation, then have the random
CUDA facet use it. A provider-neutral sparse header must not name an
implementation namespace owned by a downstream provider facet.

Resolution: production replaced both downstream/provider friends with generic
`internal_sparse_coordinate::ArrayFactory` and
`internal_sparse_compressed::ArrayFactory` access. Provider-neutral owners no
longer name the optional CUDA implementation.

Status: resolved in production; dependency scan pending.

### M7-DOC-05: sparse-random async lifetime and full cost were implicit

Initial severity: public documentation defect

Initial evidence: the first `CudaGenerateSparseUniform01` declaration
documented the priority-selection cost and absence of hidden workspace, but
did not state the caller's asynchronous lifetime obligations. It also omitted
the canonical coordinate insertion-sort term from the overall cost.

Required resolution: document that the device resource, returned owner and
its backing storage, and context remain alive through completion; borrowed
views do not extend owner lifetime. Record the complete low-workspace bound as
`O(exact_count * logical_size + exact_count^2 * Rank)` time, two explicit
result-buffer allocations, and no computational workspace.

Resolution: the public declaration now states the full bound, exact result
allocations, no workspace, and the context/resource/owner lifetime
obligations.

Status: resolved in production; guide cross-check pending.

### M7-DOC-06: rank greater than eight wrote past a fixed plan array

Initial severity: release-blocking host memory-safety defect

Initial evidence: the first coordinate `CudaEvaluate` template copied every
destination extent into a fixed eight-element plan array without rejecting a
compile-time rank greater than eight.

Required resolution: reject unsupported rank before constructing or writing
the bounded plan.

Resolution: production added a compile-time rank branch returning
`kUnsupported` before the plan or fixed array is touched.

Status: resolved in production; independent compile/runtime review pending.

### M7-DOC-07: sparse context move assignment leaks a cuSPARSE handle

Initial severity: release-blocking ownership defect

Initial evidence: `SparseCudaContext` initially defaulted move assignment.
Overwriting a live context destroyed its `unique_ptr<SparseCudaContextState>`,
but that state destructor did not call `cusparseDestroy`; only the outer
context destructor performed handle release.

Required resolution: make move assignment release the destination's existing
handle on its owning device before taking the source state, while preserving
valid moved-from behavior and the no-throw move contract.

Resolution: production replaced default assignment with a no-throw
temporary-and-swap implementation. The temporary receives the old destination
state and its destructor selects the old context device and destroys that
handle.

Status: resolved by source inspection; runtime allocation/handle
instrumentation remains verifier evidence.

### M7-DOC-08: arbitrary device sparse views lack canonicality validation

Initial severity: release-blocking contract and memory-safety defect

Initial evidence: device `CoordinateView` and `CompressedSparseView` creation
checks metadata and address arithmetic but cannot inspect device structure.
The first M7 SpMV and evaluator implementations validated allocation spans,
placement, shape, type, and overlap but did not establish canonical offsets,
indices, or coordinates before compute. Malformed external CSR could therefore
reach cuSPARSE or the strided project kernel, and a malformed evaluator
destination could be accepted.

Required resolution: introduce a lead-approved canonicality token or API
restriction, or another mechanism that establishes canonical device structure
without hidden synchronization and before destination mutation. Documentation
must not turn this missing validation into a caller precondition because the
frozen contract requires the operation to reject malformed structure.

Lead decision: provider-neutral views carry internal, non-user-settable
canonical provenance. Host views become trusted only after host content
validation. Raw device views remain untrusted. Provider factories for CSR clone
and random coordinate generation publish trusted views. Sparse CUDA rejects an
untrusted device view before enqueue.

The decision makes CSR and coordinate evaluation safe without a hidden wait,
but no approved M7 operation produces trusted device CSC. The CSC evaluator
success path is therefore unreachable and must be classified **skipped** at
Publication Checkpoint B. Adding CSC staging or a user-asserted token is not
approved.

Status: canonical provenance and trusted view rebinding implemented and
independently exercised; CSC remains a disclosed frozen-contract capability
gap and is classified **skipped**.

### M7-DOC-09: SpMV provider-path documentation was inaccurate

Initial severity: API and provider-evidence defect

Initial evidence: the initial public comment said `CudaCsrSpmv` enqueued
`CUSPARSE_SPMV_CSR_ALG2` for every supported vector. The implementation used
that deterministic cuSPARSE algorithm only for unit-stride vectors; positive
nonunit strides dispatched an original project kernel and required zero
workspace.

Required resolution: obtain lead confirmation that the bounded project-kernel
path is within the frozen cuSPARSE SpMV contract, then document the two paths,
workspace behavior, arithmetic/reproducibility boundary, and evidence
separately. Otherwise restrict or redesign the API.

Lead decision: retain and document the explicit two-path capability. Unit
stride uses deterministic `CUSPARSE_SPMV_CSR_ALG2` with queried caller-owned
workspace. Positive nonunit stride uses an original deterministic project
kernel and requires an empty workspace. Neither path selects the other as a
runtime fallback.

Status: resolved in the public comment, module guide, implementation, and
independent test split.

### M7-DOC-10: sparse provider header failed strict Clang parsing

Initial severity: required compile-contract defect

Initial evidence: Clang 19 with the repository warning set and
`-Werror -fno-exceptions` rejected the standalone sparse CUDA provider header
because a generic `SetTerminal` overload named an unused `destination`
parameter.

Required resolution: remove the unused parameter name or explicitly consume
it, then rerun standalone strict GCC and Clang header probes.

Status: resolved; standalone strict GCC/Clang confirmation belongs to final
compile evidence.

### M7-DOC-11: trusted sparse values needed a safe rebinding path

Initial severity: release-blocking API usability defect

Initial evidence: independent verification found that two trusted owners
cannot share structure pointers, while a single owner's original view shares
its value span. The initial overlap rules therefore made every non-no-op
bounded evaluator expression unreachable.

Resolution: provider-neutral coordinate and compressed views now provide
checked `RebindValues` over their existing immutable canonical structure.
Exact-length value spans preserve read-only canonical provenance, while
partial overlap is rejected. Exact in-place elementwise operands are safe and
supported. Scalar multiplication accepts any exact-type scalar; scalar
addition/subtraction accepts only exact zero so implicit zero entries are not
densified. The internal trusted-construction entry point is private to sparse
owner types, so callers cannot forge provenance.

Status: resolved in API and independent evaluator tests.

## Evidence boundary

Documentation uses exactly `configure-tested`, `compile-tested`,
`runtime-tested`, `parity-tested`, or `skipped`. Header parsing and installed
consumer compilation are compile evidence only. Toolkit detection is
configure evidence only. Runtime and parity claims require real-device
execution, with parity requiring an independent CPU, numerical, or bit oracle.
No label is inferred from another.
