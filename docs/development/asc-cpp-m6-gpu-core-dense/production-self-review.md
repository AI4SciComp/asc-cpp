# Milestone 6 production self-review

## Scope

This production slice implements the approved GPU core and dense contracts
without changing shared root CMake/package ownership, tests, or later-milestone
facets.

Owned implementation files:

- `include/asc/core/execution.h`
- `include/asc/core/memory.h`
- `include/asc/core/providers/cuda.h`
- `include/asc/core/providers/cuda_export.h`
- `include/asc/dense/array.h`
- `include/asc/dense/providers/cuda.h`
- `include/asc/dense/providers/cuda_export.h`
- `src/core/execution.cc`
- `src/core/execution_internal.h`
- `src/core/cuda/cuda.cc`
- `src/core/cuda/provider_internal.h`
- `src/dense/cuda/cuda.cc`
- `src/dense/cuda/kernels.cu`
- `src/dense/cuda/kernels_internal.h`

## Public contract implemented

- Provider-neutral `ExecutionContext` state dispatch and move-only
  `CompletionEvent` query/wait semantics.
- CUDA device discovery, explicit CUDA execution-context creation, event
  recording, and pinned-host/device/managed memory resources.
- Provider-aware asynchronous `CopyBytes` with explicit memory-space and device
  validation.
- Uninitialized dense allocation and provider-neutral clone-by-copy support.
- Move-only `DenseCudaContext` bound to the execution context's stream and
  device.
- CUDA dense evaluation for `float` and `double`, rank 0 through 8, limited to
  the approved terminal/scalar/negate/one-level binary expression grammar.
- CUDA copy, scale, AXPY, GEMV, and GEMM for the approved rank, scalar, layout,
  transpose, and non-overlap contracts.

CUDA SDK types do not appear in common public headers or public signatures.
Provider declarations first appear in their provider headers, with separate
export macros for `asc_core_cuda` and `asc_dense_cuda`.

## Dependency and lifetime review

- Common core remains independent of the CUDA SDK; provider state is opaque.
- Dense CUDA depends directly on dense, core CUDA, CUDA Runtime, and cuBLAS.
- Dense and sparse remain independent, and the expression module remains
  storage-neutral.
- Execution state is shared so an outstanding event retains its stream.
- Event destruction does not synchronize.
- CUDA resources, streams, events, and cuBLAS handles retain their owning
  device ordinal and select/restore the caller's current device for teardown.
- A moved-from `DenseCudaContext` has a defined execution-context observer and
  rejects execution with `kInvalidState`.

## Safety, error, and asynchronous-work review

- Logical sizes, byte spans, strided offsets, address ends, and cuBLAS integer
  widths are checked before narrowing or submission.
- Every algebra wrapper rejects a declared placement other than exactly
  `MemorySpace::kDevice` and rejects non-unique input or output mappings before
  opaque launch. Cross-device access is rejected.
- Core copy validation queries CUDA pointer attributes for every declared
  space. A pageable-host view accepts only unregistered host memory (including
  the CUDA-version-specific `cudaErrorInvalidValue` classification) and rejects
  CUDA-tracked pinned, device, or managed allocations mislabeled as host.
- Partial output overlap is rejected for Copy and AXPY. Exact-self Copy is the
  approved no-op, and exact same-index AXPY is approved when source and
  destination have identical data, shape, strides, and validated device
  placement; that AXPY case executes the kernel and is not treated as a no-op.
  GEMV and GEMM reject every output overlap with an input.
- Events are allocated before work submission.
- Kernel launches use a capped grid-stride geometry. Quotient/remainder block
  calculation and distance-before-increment termination avoid host and device
  signed overflow at the representable size boundary. Source-private
  host/device `constexpr` helpers expose the exact block-count and ordinal-step
  algorithms for allocation-free boundary verification.
- CUDA Runtime sticky errors already reported by an earlier operation are
  consumed and are not attributed to a later valid operation. Post-submission
  launch/copy checks use resetting error queries. Event-query `not ready` is
  handled as a pending result and consumed before restoring the caller's
  current device.
- A kernel, copy, event-record, or cuBLAS failure that may follow accepted work
  triggers a failure-only synchronization of the affected context stream
  before returning without a completion event. The original operation's
  provider/native diagnostic is preserved; handled CUDA drain errors are
  consumed before current-device restoration. No device-wide synchronization
  is used.
- Exact-self CUDA copies still validate provider accessibility and return a
  stream-ordered event.
- Pageable host transfers use CUDA Runtime stream/event completion semantics;
  the Runtime may stage or block the host call. Pinned-host, device, and managed
  storage provide the intended strong asynchronous path.

## Review findings resolved

- Restored the approved serial predecessor behavior for unsupported dependency
  chaining.
- Added checked address arithmetic instead of relying on unchecked view overlap
  helpers.
- Precreated completion events and drained failure paths to prevent live work
  escaping without a handle.
- Rejected unapproved unary-scalar and nested expression forms.
- Removed provider functions as unannotated friends from the common header,
  fixing Windows DLL import/export declaration order.
- Removed internal plan types from exported helper signatures.
- Checked every provider-width conversion before a synthetic pointer can reach
  CUDA access validation.
- Made cuBLAS handle destruction device-correct.
- Made stream, event, memory, and cuBLAS teardown abandon cleanup if the owning
  device cannot be selected, avoiding destruction or free on the wrong current
  device under an unrecoverable provider failure.
- Replaced overflow-prone launch geometry and grid-stride increments.
- Isolated CUDA sticky error state across sequential failed and valid
  operations.
- Reconciled the dense-algebra alias wording with its operation-specific
  contract: exact-self Copy and exact same-index AXPY are the allowed
  same-index forms, partial Copy/AXPY overlap is rejected transactionally, and
  GEMV/GEMM reject every output/input overlap.
- Preserved view placement and uniqueness validation before linalg plan
  lowering, which otherwise discards those descriptor properties.

## Production validation

CPU-only regression:

```text
cmake -S . -B /tmp/asc-cpp-m6-prod-cpu.Xgfy70/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
PASS

cmake --build /tmp/asc-cpp-m6-prod-cpu.Xgfy70/build --parallel 4
PASS

ctest --test-dir /tmp/asc-cpp-m6-prod-cpu.Xgfy70/build \
  -R '^asc_cpp\.(core\.|dense\.)' --output-on-failure
PASS: 20/20
```

CUDA configure:

```text
cmake -S . -B /tmp/asc-cpp-m6-prod-audit.KJ9l13/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
PASS: CMake 4.1.2, GCC 11.4, CUDA compiler 12.9.86, architecture 86
```

Build and focused validation after the final review fixes:

```text
cmake --build /tmp/asc-cpp-m6-prod-audit.KJ9l13/build --parallel 4
PASS

ctest --test-dir /tmp/asc-cpp-m6-prod-audit.KJ9l13/build \
  -R '^asc_cpp\.(core_cuda\.runtime|dense_cuda\.(storage_evaluate|linalg|concurrency|compile_contract|multi_tu))$' \
  --output-on-failure
PASS: 6/6
```

The focused runtime set includes transfer/event behavior, dense storage and
expression results, CPU/CUDA numerical parity for the exercised linear algebra,
exact same-index in-place AXPY, partial-overlap transactionality, inverse
memory-label and linalg placement/uniqueness rejection, allocation-free launch
geometry boundaries, independent-stream concurrency, sequential CUDA error
isolation, header contracts, and multi-translation-unit linkage. Multi-device
teardown runtime coverage was skipped because the host exposes one CUDA device.

Formatting:

```text
clang-format-19 --dry-run --Werror <owned C++ and CUDA files>
PASS

git diff --check -- <owned files>
PASS
```

Evidence classification for this production slice:

- CUDA provider: configure-tested, compile-tested, runtime-tested, and
  parity-tested on NVIDIA architecture 86.
- Multi-device behavior: compile-tested; runtime skipped (one device present).
- Other GPU providers: skipped; they are outside Milestone 6.

The independent verifier and lead integrator own the final clean-tree,
sanitizer, package, relocation, isolated-consumer, and full-label conclusions.

## Residual risks

- Only one NVIDIA device and one CUDA architecture were available locally.
- No Windows CUDA toolchain or shared-library runtime was available locally;
  declaration-order portability is source/compile-contract reviewed.
- CUDA and cuBLAS teardown failures cannot be reported from `noexcept`
  destructors; on device-selection failure teardown avoids acting on the wrong
  device.
- Pageable-host asynchronous behavior remains subject to CUDA Runtime staging
  rules.
- Performance validation is a smoke baseline, not a release performance claim.

## Provenance

The implementation is clean-room work from the approved ADRs and official
CUDA Runtime/cuBLAS contracts. No deleted legacy CUDA source or MdeCpp
implementation was copied. MdeCpp provenance remains governed by the frozen
Milestone 6 provenance record.
