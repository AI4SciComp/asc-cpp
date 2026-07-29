# ADR 0014: sparse-owned algebra with neutral vector operands

Status: Proposed at Architecture Checkpoint A

## Context

Sparse algebra must not live in dense or a seventh linalg module. SpMV and
similar operations naturally interact with dense vectors, but a concrete dense
dependency is forbidden.

## Decision

Sparse owns sparse operation contracts, reference kernels, and providers. Its
initial algebra capability is:

- canonical CSR serial reference SpMV for float/double;
- structural/correctness operations for COO/CSR/CSC.

SpMV accepts storage-neutral expression-level readable and writable rank-one
descriptors. A dense view can satisfy those protocols when both headers are
present, but sparse neither includes nor links dense. The caller supplies the
destination.

Before invocation sparse validates dimensions, structure, sortedness/
uniqueness, index base/width, scalar/compute type, destination uniqueness/
overlap, memory accessibility, provider capability, and workspace.

No hidden densification, format conversion, packing, allocation, transfer,
narrowing, synchronization, or fallback is allowed. Conversion and workspace
are explicit operations.

SpMM, triangular solve, preconditioners, and iterative/direct solvers are
deferred. `sparse_cuda` may later own a selected cuSPARSE subset after exact
format/algorithm/workspace/determinism and hardware evidence. No optimized CPU
sparse provider is approved yet.

## Consequences

Neutral operands preserve dense/sparse independence but require careful
compile-time customization and installed-consumer tests. The first capability
is intentionally narrow.

## Verification

Consume sparse with dense disabled; instantiate SpMV with an external toy
vector and later a dense view; test malformed structures before provider calls,
no-densification/allocation, reference values, capability truthfulness, and
real-hardware parity for optional providers.
