# Milestone 7 Production Self-Review

Status: Complete

Date: 2026-07-27

Scope: production implementation only

## Implemented surface

- `ASC::sparse_cuda`: move-only `SparseCudaContext`, explicit asynchronous CSR
  clone, trusted canonical sparse provenance, `RebindValues`, CSR SpMV
  workspace query and launch, and bounded coordinate/CSR/CSC evaluation.
- `ASC::random_cuda`: asynchronous Philox4x32-10 word fill with explicit
  stream, subsequence, offset, completion event, and next offset.
- `ASC::random_dense_cuda`: asynchronous float/double Uniform01 fill for
  unique rank-zero-through-eight device views in logical
  dimension-zero-fastest order.
- `ASC::random_sparse_cuda`: exact-count float/double canonical coordinate
  generation with independent structure/value domains, explicit result
  storage, no computational workspace, completion event, and next offsets.

The public provider headers contain no CUDA or cuSPARSE SDK type. No dependency,
later-milestone capability, hidden transfer, packing, format conversion,
precision conversion, entropy source, mutable random pool, or CPU fallback was
added.

## Sparse implementation notes

Provider-neutral sparse views now carry non-user-settable canonical provenance.
Raw device `Create` is untrusted. Internally allocated owners produce trusted
views; `RebindValues` preserves the immutable structure's provenance while
checking exact value length, address/alignment, and structure/value
disjointness. The internal trusted constructor is private to the corresponding
owner class; provider code has read-only trust access. Provenance is not a
readiness signal, so clone/random events and stream ordering still govern use.

Unit-stride CSR SpMV uses `CUSPARSE_SPMV_CSR_ALG2` and an exact non-enqueuing
workspace-size query. Positive non-unit vector strides use the original
deterministic ASC CUDA kernel and require zero workspace. Both paths reject
untrusted CSR provenance, incompatible shapes, invalid signed metadata,
misplaced spans, checked span overflow, and output overlap before mutation.
Exact `beta == 0` avoids reading output in the project kernel.

The bounded evaluator recognizes only shallow copy, negate, add, subtract, and
multiply nodes over identical trusted structure plus exact-type rank-zero
scalars. Exact in-place operands are safe and supported; partial overlap,
nested nodes, differing structure/type/rank/shape/format, and rank above eight
are rejected.

CSR clone failure paths retain or wait for the most recent copy event before
owner destruction. Kernel launch helpers and core pending-event recording
drain their stream on post-enqueue failure, preventing asynchronous use after
result storage destruction.

## Random implementation notes

All Philox kernels are original clean-room implementations of the approved
counter algorithm. Float consumes one word; double consumes adjacent high/low
words exactly as the provider-free contracts specify. Zero-work operations
return `CompletionAccess::Completed()` without a kernel or event enqueue.

Sparse random generation deliberately uses one device thread, repeated
priority scans, and in-place coordinate insertion sorting. Its work is
`O(exact_count * logical_size + exact_count^2 * rank)`. It makes exactly the
two result-buffer allocations and no hidden workspace allocation. This is a
correctness-first low-workspace implementation with an explicit performance
limitation.

## Validation performed

Commands run from the repository root:

```text
cmake --preset test-cuda \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
```

Result: pass (`configure-tested`), CUDA compiler 12.9.86, toolkit 12.9.86.

```text
cmake --build build/test-cuda \
  --target asc_sparse_cuda asc_random_cuda \
           asc_random_dense_cuda asc_random_sparse_cuda -j 4
```

Result: pass (`compile-tested`).

```text
ctest --test-dir build/test-cuda --output-on-failure \
  -R '^asc_cpp\.(sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)\.runtime$'
```

Result: pass, 4/4 (`runtime-tested`; the verifier owns exact independent parity
classification).

```text
clang++-19 -std=c++20 -fsyntax-only -Wall -Wextra -Wpedantic \
  -Werror -fno-exceptions ...
```

Result: pass for all four public provider headers and all four C++ provider
translation units. Repository `clang-format-19` was applied and
`git diff --check` passed for the production scope.

## Findings resolved

- Added a core completed-event path for exact no-enqueue zero work.
- Added explicit float/double alignment checks and checked span arithmetic.
- Replaced downstream-provider friendship in base sparse with sparse-owned
  internal factories/access.
- Prevented user forging of trusted canonical provenance.
- Made the frozen evaluator binary/scalar forms reachable despite generic
  expression sparsity categories.
- Added `RebindValues` and exact in-place safety so every advertised evaluator
  operation has a usable trusted representation.
- Replaced ambiguous workspace error-as-query with a non-enqueuing
  `CudaCsrSpmvWorkspaceSize`.
- Made live `SparseCudaContext` move assignment release the replaced cuSPARSE
  handle through the normal guarded destructor path.
- Serialized operations on a shared cuSPARSE handle.
- Checked asynchronous clone/launch failure lifetime safety.
- Restricted sparse add/subtract scalar operands to exact zero before enqueue;
  multiply continues to accept arbitrary exact-type scalars, and
  zero-scalar-minus-sparse remains valid over stored values.
- Guarded the full cuSPARSE descriptor/workspace query and unit-stride launch
  sections with the context device and cleared pre-existing CUDA Runtime error
  state immediately before `cusparseSpMV`.

## Remaining risk and integration needs

- CUDA Runtime 12.9 exposes pointer classification but no allocation-range
  query for arbitrary external CUDA allocations. ASC `CudaMemoryResource`
  allocations receive exact registry-backed subspan validation through
  `internal_core_cuda::ValidateCudaMemory`; external allocations retain a
  caller-supplied size contract. Adding CUDA Driver solely for
  `cuMemGetAddressRange` is not approved.
- Trusted provenance proves how immutable structure was created, not that an
  asynchronous producer has completed. Users must wait for its event or keep
  same-stream ordering and all owners/resources alive.
- Non-unit-stride SpMV evidence must be identified as the ASC project-kernel
  path, not as cuSPARSE ALG2.
- Sparse random's low-workspace algorithm is intentionally slow for large
  domains and must not support a speedup claim.
- Lead-owned CMake/package, complete clean matrices, sanitizer, relocation,
  isolated-consumer, and Publication Checkpoint B evidence remain lead
  integration responsibilities.

No commit, push, merge, tag, release, branch deletion, or remote action was
performed.
