# Milestone 7 Production Self-Review

Status: production implementation complete; independent verification and
integration remain lead-owned

Date: 2026-07-28

## Implemented scope

- `sparse_cuda`: move-only cuSPARSE context, canonical host-to-device CSR
  clone, explicit CSR SpMV workspace query/use, deterministic unit-stride
  `CUSPARSE_SPMV_CSR_ALG2`, positive nonunit-stride project kernel, and bounded
  same-structure sparse evaluation.
- `random_cuda`: asynchronous Philox4x32-10 raw-word fill into a capacity-
  carrying device `MutableMemoryView`.
- `random_dense_cuda`: asynchronous, layout-independent Uniform01 float/double
  fill for unique device DenseViews of rank zero through eight.
- `random_sparse_cuda`: exact-count canonical coordinate/value generation with
  no computational allocation. The project algorithm uses result coordinate
  storage while selecting and sorting ordinals, then launches one decoding
  kernel per dimension. It has no rank cap.

No cuRAND, Thrust/CUB, Dense dependency from Sparse, hidden transfer,
conversion, packing, densification, precision change, or fallback was added.

## Sparse ownership evolution

Raw device sparse views remain untrusted because their canonical structure
cannot be inspected without synchronization. Host-validated views and
provider-created owners carry trusted-canonical provenance.

`CoordinateView::RebindValues` and
`CompressedSparseView::RebindValues` preserve immutable structure and its
provenance while accepting distinct value storage. Checked rebinding and view
creation reject null, overflowing, or structure-overlapping value spans.
Exact full-span in-place rebinding is legal; partial overlap with the existing
value span is rejected.
Provider validation also rejects all pairwise overlap among structure and
value spans.

## Validation and lifetime

Operations validate checked shape/count/span arithmetic, declared placement,
alignment, CUDA device identity, canonical provenance, format, type, shape,
stride, overlap, workspace size/placement/overlap, and random-offset overflow
before enqueue. External non-owning views retain the inherited precondition
that the declared span describes truthful valid storage; CUDA Runtime exposes
the allocation's device identity but not a portable allocation-end query.

Zero work returns an already-complete event. If event recording fails after a
submission, or a later launch/provider call fails after earlier work may have
been submitted, the implementation drains the context stream before local
owners can release storage. Callers otherwise keep contexts, resources,
owners, views, workspace, and referenced bytes alive through completion.

## Determinism and performance limits

Raw and Dense random kernels map logical positions directly to explicit Philox
word offsets, independent of launch partition. Sparse generation uses the same
priority/ordinal ordering and word-consumption contract as the CPU facet. Its
workspace-free selection is intentionally bounded in memory but costs
`O(logical_size * exact_count + exact_count^2)` scalar selection/insertion
work, plus `O(rank * exact_count)` coordinate decoding. It is correctness-
oriented and makes no speedup claim.

CSR SpMV performs deterministic sequential accumulation per row in the
nonunit-stride project kernel. Unit stride delegates to the frozen cuSPARSE
ALG2 choice under a context mutex, with caller-observable workspace.

## Production checks

- CUDA 12.9.86, GCC 11.4, architecture 86, static Debug production targets:
  pass.
- Strict Clang 19 C++20 parse with `-Wall -Wextra -Werror` for all four public
  provider headers: pass.
- `clang-format-19 --dry-run --Werror` over the production scope: pass after
  final formatting.
- Focused GPU runtime facets: sparse CUDA, raw random CUDA, and Dense random
  CUDA pass. Sparse-random parity and rank-nine behavior pass; the initial
  verifier revision incorrectly counted zero-byte Core buffers as live
  non-null allocations and was returned to the verification owner for
  correction.

## Remaining review risks

- cuSPARSE behavior and workspace size remain toolkit/architecture dependent;
  the API exposes the exact queried size and records the tested environment.
- Pageable canonical host CSR input may be internally staged by CUDA Runtime;
  `CudaCloneCsr` is the milestone's explicit staging operation.
- Sparse exact-count generation is deliberately slow for large logical
  domains or counts because hidden computational workspace is forbidden.
