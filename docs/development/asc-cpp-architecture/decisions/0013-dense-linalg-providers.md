# ADR 0013: dense-owned linear algebra and coarse provider dispatch

Status: Proposed at Architecture Checkpoint A

## Context

The restart has no linalg module. Dense must own its operation contracts and
providers without exposing SDKs or changing APIs when a provider is absent.

## Decision

The dense base target supplies deterministic serial reference implementations
for an initial float/double set:

```text
Copy, Scal, Axpy, Dot, Nrm2, Gemv, Gemm, selected reductions
```

Public operations take an explicit execution context and caller-provided
destinations. Validation, capability, shape, memory, alias, and workspace
checks complete before mutation. Reductions define accumulation order;
`Nrm2` uses scaled sum-of-squares; `beta == 0` does not read the prior
destination.

Dispatch occurs once per coarse operation. Provider capability keys include
operation, scalar/input/compute type, index width, rank, layout, transpose/
conjugate, memory spaces, device, algorithm, determinism, workspace, and
packing/conversion.

Providers cannot hide allocation, packing, transfer, precision change,
synchronization, or fallback. Unsupported cases return status.

No optimized CPU provider is approved at Checkpoint A. Detected Eigen,
BLAS/LAPACK, oneMKL, OpenMP, TBB, and SYCL are candidates. `dense_cuda` is the
first proposed GPU facet and privately owns cuBLAS/cuSOLVER use after real
runtime/parity evidence.

Factorizations, solvers, complex/mixed precision, and tensor contraction are
deferred to separately approved operation contracts.

## Consequences

The narrow reference path supplies correctness even without providers.
Provider-specific performance may wait. Public API/capability does not
disappear when a provider is disabled.

## Verification

Test every operation across supported layouts/shapes/aliases/extremes,
allocation/workspace transactions, independent oracles, advertised-versus-
invoked capability properties, installed provider isolation, and real GPU
parity before claiming CUDA.
