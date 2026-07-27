# Milestone 7 Portability, GPU, and Performance Review

Status: Accepted after corrective review, with disclosed portability limits

Date: 2026-07-27

Role: independent portability/GPU/performance reviewer

## Review boundary

This review is separate from production implementation, independent numerical
verification, documentation/API review, and lead integration. Its only write
scope is this file. Production, tests, benchmarks, CMake, package files, and
all other reports were inspected read-only.

The review covers the frozen Milestone 7 contract, ownership ledger, ADRs
0008, 0012, 0014, 0015, 0017, and 0018, all M7 public headers and compiled
sources, provider-neutral sparse ownership changes, verifier-owned tests and
benchmarks, target definitions, component closures, and the other independent
reviews. No MdeCpp or deleted asc-cpp implementation/test was inspected.

Primary provider facts were cross-checked against the CUDA Toolkit 12.9
[cuSPARSE documentation](https://docs.nvidia.com/cuda/archive/12.9.2/cusparse/index.html).
In particular, non-transpose `CUSPARSE_SPMV_CSR_ALG2` is the selected
deterministic CSR algorithm, `cusparseSpMV` is asynchronous, CSR requires
caller-visible external storage reported by `cusparseSpMV_bufferSize`, and
Compute Sanitizer may report a documented false race for `beta == 0`. That
last provider caveat is why the local sanitizer gate uses `memcheck`, not a
racecheck result as a correctness oracle for cuSPARSE.

## Reviewed environment

```text
Branch: feature/asc-cpp-m7-gpu-sparse-random
HEAD baseline: 33b261ea33616a6395c4ad3b20646093103344f7
CMake: 4.1.2
GNU host compiler: 11.4.0
Clang header compiler: 19.0.0
CUDA compiler/toolkit: nvcc 12.9.86 / CUDA 12.9
Compute Sanitizer: 2025.2.1.0
Driver: 576.83
GPU: NVIDIA GeForce RTX 3060 Laptop GPU
Compute capability / compiled architecture: 8.6 / 86
Diagnostic build: Release, static ASC libraries, shared CUDA Runtime
Warnings as errors: enabled
```

No commit, push, merge, tag, release, branch deletion, or remote action was
performed.

## C++20 and host portability

The four public provider headers expose no CUDA, cuSPARSE, cuRAND, Thrust, or
other SDK type. Their templates use C++20 concepts and fixed-width ASC
metadata. Standalone strict Clang 19 parsing passed for all headers and the
combined ownership/API contract with:

```sh
for f in \
  tests/compile/m7_sparse_cuda_header.cc \
  tests/compile/m7_random_cuda_header.cc \
  tests/compile/m7_random_dense_cuda_header.cc \
  tests/compile/m7_random_sparse_cuda_header.cc \
  tests/compile/m7_cuda_contracts.cc
do
  clang++-19 -std=c++20 -fsyntax-only -Iinclude \
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow \
    -Werror -fno-exceptions "$f"
done
```

Result: pass. Classification: `compile-tested`.

Repository `clang-format-19 --dry-run --Werror` checking passed for the
production headers, C++ sources, CUDA sources, and both revised benchmark
sources after correction.

The CUDA ABI boundary uses signed 64-bit `index_t`, `extent_t`, `stride_t`, and
`nnz_t`, matching `CUSPARSE_INDEX_64I`. Every provider narrowing reviewed is
guarded:

- rank is checked at no more than eight before its `uint8_t` plan field is
  consumed;
- nonnegative signed sizes are checked before conversion to `uint64_t` or
  `size_t`;
- allocation, element-span, stride, and byte arithmetic uses checked
  multiply/add operations before kernel launch;
- kernel grid counts are capped at 65535 before conversion to `unsigned int`;
  and
- cuSPARSE receives native signed-64 dimensions and indices, so no 32-bit
  provider narrowing occurs.

The CUDA kernels use fixed-width unsigned arithmetic for Philox wraparound.
Uniform01 scaling uses exactly representable binary powers and does not depend
on contraction, standard-library distributions, or host rounding modes. The
random bit-identity claim is therefore portable across the reviewed CPU/GPU
implementation boundary, subject to the actual parity evidence below.

GCC 11.4 plus nvcc 12.9 compiled all four CUDA facets with C++20 and warnings
as errors. Clang CUDA, MSVC/nvcc, AppleClang, 32-bit hosts, big-endian hosts,
and non-NVIDIA GPU toolchains were not available locally and are `skipped`.
The code uses no GNU-only extension in a public header, but those unavailable
matrices remain evidence gaps rather than inferred support.

## Address, placement, and allocation safety

Every nonempty M7 device span reaches
`internal_core_cuda::ValidateCudaMemory`, which establishes CUDA pointer type,
context device identity, declared memory space, and—when the allocation came
from `CudaMemoryResource`—the exact registered allocation and interior
subspan. Type alignment, `uintptr_t` end arithmetic, element/stride span, and
pairwise overlap are checked before enqueue.

There is one unavoidable approved-dependency limitation. CUDA Runtime 12.9
has no general Runtime allocation-range query. `cuMemGetAddressRange` belongs
to the CUDA Driver API, and adding the Driver API solely for this query is not
approved by the frozen dependency graph. Consequently:

- ASC-owned `CudaMemoryResource` allocations have exact bound checking;
- arbitrary external Runtime device pointers have placement, device,
  alignment, and representable-address checks; but
- the true end of an arbitrary external allocation or logical suballocation
  remains a truthful caller-supplied span contract.

This is not classified as full allocation-bound evidence for external
storage. It remains the principal disclosed M7 memory-safety risk.

Trusted sparse provenance is non-user-settable. Canonical host views become
trusted only after content validation; CSR clone and sparse-random owner
factories preserve that provenance for device structure. Raw device sparse
views remain untrusted and are rejected before provider access. Provenance
does not imply asynchronous readiness: the producer event or same-stream
ordering and all owner/resource lifetimes remain mandatory.

## CUDA execution, errors, and asynchronous lifetime

All operations use the explicit execution context and preserve the caller's
previous current CUDA device through `CudaDeviceGuard`. Random and project
kernel launchers clear stale Runtime launch state immediately before enqueue.
Provider errors carry stable ASC categories, provider name, and signed native
code.

The corrective review found and resolved two cuSPARSE-device issues:

1. the first unit-stride workspace query/launch implementation did not retain
   the context device guard around descriptor creation, workspace query, and
   `cusparseSpMV`; and
2. it did not isolate `PendingCudaEvent::Record` from an unrelated prior
   Runtime last-error.

The final implementation holds both the sparse-context mutex and context
device guard across descriptor/query/launch work, clears prior Runtime error
immediately before `cusparseSpMV`, drains the context stream if cuSPARSE
reports failure after submission may have begun, and lets the pending-event
record path drain on event-record failure.

The random and pointwise/project SpMV launchers likewise drain the context
stream on launch failure. `PendingCudaEvent::Record` drains if work was
submitted but no usable completion event can be published. CSR clone retains
the latest copy event and waits it on a later copy or final event-publication
failure before the partially constructed owner is destroyed. These narrow
failure-only waits prevent use-after-free without adding a success-path
device-wide synchronization.

Zero-work raw, dense, sparse-random, pointwise, and empty-row SpMV paths return
an already-complete event before kernel/event enqueue. Completion events retain
provider completion state but not user arrays, workspaces, resources, or
external streams. Event destruction does not establish storage lifetime.

`SparseCudaContext` is move-only. Its corrected move assignment releases the
replaced handle through the old context's guarded destructor. If device
selection itself fails during a no-throw destructor, the implementation
deliberately leaks the provider handle rather than destroying it on the wrong
device; this is a rare failure-path resource risk, not a data-correctness
fallback.

## cuSPARSE algorithm and workspace audit

Unit-stride CSR SpMV uses exactly:

```text
CUSPARSE_OPERATION_NON_TRANSPOSE
CUSPARSE_INDEX_64I offsets and indices
CUSPARSE_INDEX_BASE_ZERO
CUDA_R_32F or CUDA_R_64F storage/compute
CUSPARSE_SPMV_CSR_ALG2
```

`CudaCsrSpmvWorkspaceSize` is non-enqueuing and reports the exact size returned
by `cusparseSpMV_bufferSize`. `CudaCsrSpmv` recomputes the requirement under
the same guarded handle, rejects undersized, misplaced, or overlapping
workspace before enqueue, and requires the caller to keep that workspace alive
through completion. No preprocessing state or hidden provider allocation is
advertised.

Positive non-unit input or output stride is a separately documented original
ASC project-kernel path. Its query returns zero and launch requires an empty
workspace. This is an explicit metadata-selected capability split, not a
provider failure fallback. The kernel processes one row per logical worker,
uses signed-64 CSR metadata, rejects output overlap, and avoids reading prior
output when `beta == 0`.

NVIDIA's bitwise determinism statement for ALG2 is bounded by fixed algorithm,
hardware, toolkit/driver, and relevant data alignment. The review therefore
does not infer cross-version or cross-hardware bit identity for SpMV. The local
claim is repeatability and independent numerical parity in the recorded
environment.

## Random and sparse evaluator audit

Raw Philox and dense Uniform01 use grid-stride kernels with explicit ordinal
addressing, so launch partition does not change bits. Dense ordinal-to-physical
mapping is checked for rank zero through eight and honors the frozen
dimension-zero-fastest order for left, right, and unique padded layouts.

Sparse random intentionally uses one device thread, repeated priority scans,
and insertion sorting:

```text
time: O(exact_count * logical_size + exact_count^2 * rank)
result allocations: exactly coordinate and value buffers
computational workspace: none
```

The implementation is auditable and bit-stable but intentionally unsuitable
for large domains. This is a documented performance limit; no speedup or
scalability claim is supportable.

The bounded sparse evaluator accepts only shallow copy, negate, add, subtract,
and multiply over identical trusted structure. A corrective review found that
the first binary dispatcher also accepted nonzero rank-zero scalar add/subtract
forms, which densify implicit zeros. The final dispatcher accepts arbitrary
exact-type scalar multiplication but restricts scalar addition/subtraction to
exact zero; verifier rejection checks cover nonzero and scalar-left forms.
Partial value overlap, nesting, structure mismatch, untrusted provenance,
type/rank/shape/format mismatch, and rank above eight are rejected.

CSR evaluation is reachable from the explicit clone and coordinate evaluation
is reachable by combining `sparse_cuda` with the independently requested
`random_sparse_cuda` producer. There is no approved M7 producer for trusted
device CSC. Although the template surface accepts CSC, the CSC evaluator
success path is `skipped` and is a disclosed capability/evidence gap.

## Target, package, and linkage audit

Source and target inspection confirmed the exact direct ASC graph:

```text
ASC::sparse_cuda        -> ASC::sparse, ASC::core_cuda
ASC::random_cuda        -> ASC::random, ASC::core_cuda
ASC::random_dense_cuda  -> ASC::random_dense, ASC::random_cuda, ASC::core_cuda
ASC::random_sparse_cuda -> ASC::random_sparse, ASC::random_cuda, ASC::core_cuda
```

Only `ASC::sparse_cuda` adds private `CUDA::cusparse`. The other new facets use
the Runtime closure established by `ASC::core_cuda`; none adds cuRAND,
Thrust/CUB as an API dependency, cuSOLVER, dense CUDA, or a sibling provider.
`ASC::cpp` remains provider-free.

The diagnostic executable built from static ASC archives records dynamic
needs for `libcudart.so.12` and, only for sparse CUDA use,
`libcusparse.so.12`. Provider headers do not leak their SDK include paths or
types. Component config requests discover CUDAToolkit only when a requested
closure contains a CUDA facet.

This specialist review did not independently rerun the full static/shared
install, relocation, path-with-spaces, registry-disabled, or isolated-consumer
matrix. Those rows are `skipped` here and must use the lead's clean package
evidence; source inspection alone is not promoted to package runtime evidence.

## Executed GPU and sanitizer evidence

The independent verifier's isolated Release configuration and warning-clean
full build are accepted as:

| Evidence row | Result | Classification |
| --- | --- | --- |
| CUDA 12.9, architecture 86 configure | pass | `configure-tested` |
| all four M7 CUDA facets, headers, compile contracts, negative contracts, and consumers | pass | `compile-tested` |
| four real-device runtime suites with independent CPU/bit/numerical oracles | 4/4 pass | `parity-tested` |

After the portability corrections, this reviewer rebuilt the four runtime
executables and two benchmarks in
`/tmp/asc-cpp-m7-diag.NqVZ6k/build`. The focused runtime/benchmark CTest
initially exposed an invalid unsorted benchmark CSR fixture. The verifier
changed it to canonical sorted columns and the corrected sparse benchmark
passed. That finding is recorded below rather than hiding the superseded
failure.

Compute Sanitizer was run exactly as:

```sh
for exe in \
  tests/random_cuda/asc_random_cuda_test \
  tests/random_dense_cuda/asc_random_dense_cuda_test \
  tests/random_sparse_cuda/asc_random_sparse_cuda_test \
  tests/sparse_cuda/asc_sparse_cuda_test
do
  compute-sanitizer --tool memcheck --error-exitcode=99 \
    --leak-check=full "$exe"
done
```

Working directory:
`/tmp/asc-cpp-m7-diag.NqVZ6k/build`.

Result: all four reported `LEAK SUMMARY: 0 bytes leaked in 0 allocations` and
`ERROR SUMMARY: 0 errors`. Classification: `runtime-tested`. This is device
memory/leak evidence, not an additional numerical-parity claim.

AddressSanitizer and UndefinedBehaviorSanitizer are host instrumentation and
do not replace Compute Sanitizer for device accesses. They were not run by
this specialist build and are `skipped` here; the lead's provider-free and
host-validation sanitizer matrix is authoritative.

## Performance evidence

The corrected benchmarks perform warmup before timing, wait every asynchronous
operation, keep setup/transfer and oracle validation outside timed regions
except where the operation name explicitly includes sparse result allocation,
and reject an incorrect checksum. They are smoke observations, not speed
gates.

Exact local output:

```text
operation=csr_spmv scalar=float rows=1024 columns=1024 nnz=5120 warmups=3 repetitions=20 workspace_bytes=704 elapsed_us=1418 checksum=10240
operation=csr_spmv scalar=double rows=1024 columns=1024 nnz=5120 warmups=3 repetitions=20 workspace_bytes=752 elapsed_us=3137 checksum=10240
operation=raw_philox words=1048576 warmups=3 repetitions=20 elapsed_us=2915 checksum=2253713208691922
operation=dense_uniform01 scalar=float shape=1024x1024 layout=right warmups=3 repetitions=20 elapsed_us=3701 checksum=524733
operation=sparse_uniform01_including_output_allocation scalar=float shape=128x128 count=128 warmups=1 repetitions=5 elapsed_us=9047386 coordinate_checksum=16629 value_checksum=66.3448
```

Classification: `parity-tested`, because every benchmark performs an untimed
independent checksum/bit/coordinate oracle after real-device execution.
Elapsed values are single local observations and do not support a comparative
speedup, regression threshold, or cross-system conclusion. The sparse-random
time illustrates the documented single-thread complexity and must not be
generalized beyond this fixture.

## Findings and resolutions

### PORT-001 — arbitrary external allocation end cannot be proven

Initial severity: release-blocking under a literal full-bound claim.

Resolution: exact registry-backed subspan validation was established for
ASC-owned CUDA allocations. The only general allocation-range query would add
the unapproved CUDA Driver dependency, so arbitrary external bounds remain a
disclosed caller-span limitation.

Status: partially resolved; remaining risk accepted for checkpoint disclosure.

### PORT-002 — nonzero scalar add/subtract could densify

Initial severity: release-blocking sparse-contract defect.

Resolution: scalar add/subtract now requires exact zero; multiply retains
arbitrary exact-type scalar support. Independent positive/rejection cases were
added.

Status: resolved.

### PORT-003 — cuSPARSE work lacked a persistent context-device guard

Initial severity: release-blocking multi-device/provider error risk.

Resolution: query and launch now retain the context device guard and handle
mutex, isolate prior Runtime error state, and drain on post-submission failure.

Status: resolved by source inspection, local single-device execution, and
Compute Sanitizer. A real multi-device restoration test remains `skipped`.

### PORT-004 — benchmark coverage and oracle gaps

Initial severity: performance-evidence defect.

Resolution: SpMV now benchmarks float and double. Sparse-random coordinates
and values are checked against an independent Philox/priority oracle. The
first revised SpMV fixture was noncanonical because modulo-wrapped columns were
unsorted; the corrected fixture uses sorted unique columns and passes.

Status: resolved.

### PORT-005 — revised benchmark sources were not repository-formatted

Initial severity: required style-gate defect.

Resolution: the verifier formatted both revised benchmark files. This
reviewer reran `clang-format-19 --dry-run --Werror` on those exact files and
the command passed.

Status: resolved.

## Remaining risks and skipped matrices

- Arbitrary external CUDA allocation terminal bounds cannot be discovered with
  the approved Runtime-only dependency closure.
- Trusted sparse provenance does not prove an asynchronous producer has
  completed; misuse remains a caller lifetime/ordering error.
- Trusted device CSC has no approved producer, so CSC evaluator success is
  `skipped`.
- Multi-device current-device restoration and cross-device rejection were
  source-reviewed but real multi-GPU runtime evidence is `skipped`.
- MSVC/nvcc, Clang CUDA, AppleClang, 32-bit, big-endian, HIP, and SYCL are
  `skipped`.
- Shared-library, complete installed-package/relocation, and host
  ASan/UBSan rows are `skipped` in this specialist run and require lead
  evidence.
- cuSPARSE determinism is not claimed across toolkit, driver, hardware,
  algorithm, or alignment changes.
- Sparse random is correctness-first and has no supported large-domain
  performance or speedup claim.

No additional portability, GPU, or performance blocker remains in the
reviewed M7 scope. Final checkpoint acceptance remains conditional on the
lead's clean package, relocation, isolated-consumer, and host-sanitizer
matrix; the disclosed external-allocation, CSC, multi-device, toolchain, and
scalability limits above remain risks rather than inferred evidence.
