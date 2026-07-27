# Milestone 7 Independent Verification Review

Status: Accepted with one documented external-allocation limitation

Date: 2026-07-27

Scope: Independent verification of Milestone 7 sparse CUDA and random CUDA
facets. This review follows the verifier-owned design frozen before production
inspection in `verification-design.md`.

## Environment

```text
Branch: feature/asc-cpp-m7-gpu-sparse-random
HEAD: 33b261ea33616a6395c4ad3b20646093103344f7
CMake: 4.1.2
Host compiler: GNU C++ 11.4.0
CUDA compiler/toolkit: 12.9.86 / 12.9
GPU: NVIDIA GeForce RTX 3060 Laptop GPU
Compute capability: 8.6
Driver: 576.83
Build type: Release
```

No remote operation, commit, merge, tag, release, or branch deletion was
performed.

## Independent evidence

The following isolated configure succeeded:

```sh
cmake --preset test-cuda \
  -B /tmp/asc-cpp-m7-verification-n7zSsT \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
```

Classification: `configure-tested`.

The following isolated full build succeeded with warnings treated as errors:

```sh
cmake --build /tmp/asc-cpp-m7-verification-n7zSsT --parallel 4
```

It compiled all four M7 CUDA facets, runtime tests, public-header tests,
positive compile contracts, expected-failure compile contracts, and isolated
consumers. The only toolchain diagnostic was nvcc's forward-looking warning
that offline compilation below architecture 75 will be removed in a future
release. Classification: `compile-tested`.

All six verifier-owned negative translation units failed compilation as
required:

```text
m7_cuda_negative_copy.cc
m7_negative_random_result_copy.cc
m7_negative_dense_const_destination.cc
m7_negative_dense_integral.cc
m7_negative_sparse_random_result_copy.cc
m7_negative_sparse_integral.cc
```

After the PORT-002 scalar-effect and M7-VER-007 structure/value-overlap
corrections, the final focused real-GPU run was:

```sh
ctest --test-dir /tmp/asc-cpp-m7-verification-n7zSsT \
  -R '^asc_cpp\.(sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)\.runtime$' \
  --output-on-failure
```

Result: 4/4 passed, 0 failed, 0 skipped.

```text
asc_cpp.sparse_cuda.runtime          passed
asc_cpp.random_cuda.runtime          passed
asc_cpp.random_dense_cuda.runtime    passed
asc_cpp.random_sparse_cuda.runtime   passed
```

These are real-device results, not configure-only or emulated evidence.
Classification: `runtime-tested` and `parity-tested`.

Before the final corrections, a 47-test M7-labelled run reached the package
matrix after all header, compile-contract, negative-contract, three random
runtime, and all eight build-tree/installed-relocated M7 consumer tests had
passed. Its sparse runtime test failed eight verifier assertions because the
test incorrectly supplied nonempty workspace to the zero-workspace strided
path and constructed untrusted rebound destinations. Those test assumptions
were corrected. The run was then stopped while the aggregate component
package test was still executing; it is not reported as a clean full-suite
pass. The four corrected runtime tests above are the authoritative independent
runtime result. Clean aggregate package and sanitizer evidence remains the
lead's integration responsibility.

The final lead-integration build was also audited after its shared-core
corrections:

```sh
cmake --build /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build \
  --target asc_core_cuda_test asc_dense_cuda_storage_evaluate_test \
           asc_sparse_cuda_test asc_random_cuda_test \
           asc_random_dense_cuda_test asc_random_sparse_cuda_test \
  --parallel 4
ctest --test-dir /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build \
  -R '^asc_cpp\.(core_cuda\.runtime|dense_cuda\.storage_evaluate|sparse_cuda\.runtime|random_cuda\.runtime|random_dense_cuda\.runtime|random_sparse_cuda\.runtime)$' \
  --output-on-failure --no-tests=error
```

Result: build passed; the two corrected M6 tests and all four M7 runtime tests
passed, 6/6, with 0 failures and 0 skips in 1.37 seconds.

## Numerical and behavioral coverage

`ASC::random_cuda` passed an independently implemented Philox4x32-10 oracle,
public CPU parity, block/lane/tail and high-offset cases, zero count,
partitioned generation, repeatability, two contexts, offset overflow,
misalignment, host placement, and no-mutation failure checks. An overrun from
an ASC-owned CUDA allocation was rejected with `kMemoryAccess` and left the
allocation unchanged.

`ASC::random_dense_cuda` passed bit-exact independent and provider-free CPU
parity for float and double, rank zero, zero extent, `LayoutLeft`,
`LayoutRight`, unique padded mappings, padding preservation, partitioned
generation, invalid destinations, and offset failures.

`ASC::random_sparse_cuda` passed the verifier's independently sorted
priority/ordinal oracle and provider-free CPU parity for rank zero through
three, empty, partial and full populations, float and double, exact count,
uniqueness, canonical coordinate order, offset consumption, allocation
rollback, and the unsupported-rank boundary.

`ASC::sparse_cuda` passed float/double host-to-device canonical CSR clone,
including empty CSR, signed-64 metadata preservation, rectangular SpMV,
positive non-unit vector strides and padding preservation, nontrivial
alpha/beta, `beta == 0` with NaN-initialized output, empty output, overlap and
shape failures, and evaluator numerical checks. The final test separately
exercises:

- the deterministic cuSPARSE unit-stride path and its queried explicit
  workspace, undersized/wrong-space/overlap rejection;
- the deterministic project-kernel positive-non-unit-stride path, whose
  workspace query returns zero;
- exact legal in-place evaluator updates; and
- distinct value buffers created through checked `RebindValues`, retaining
  trusted immutable sparse structure provenance.

Raw device CSR views constructed from public spans are deliberately untrusted
and are rejected before cuSPARSE is called. Device CSR owners returned by the
validated staging path carry canonical provenance.

## Findings and resolutions

### M7-VER-001 — supported evaluator operations were unreachable

The first review found a blanket sparsity-effect gate that rejected add,
subtract, multiply, and scalar forms even when the bounded structure-preserving
surface allowed them.

Resolution: production validation was narrowed to the approved bounded forms.
The independent evaluator tests now execute supported unary, binary, and
scalar forms. Resolved.

### M7-VER-002 — device CSR canonicality could not be established safely

The first review found that raw device metadata could reach SpMV without a
safe canonical-structure proof.

Resolution: canonical provenance is established by validated owner-producing
paths. Raw `CsrView`/`CoordinateView` construction remains untrusted and is
rejected. The verifier reconstructs a raw view from otherwise valid owner
pointers and confirms rejection. Resolved within the approved API.

### M7-VER-003 — arbitrary external allocation bounds are not discoverable

CUDA pointer classification reports placement and device but CUDA Runtime
12.9 exposes no general allocation-range query for every accepted external
pointer. Production added a registry for allocations owned by
`CudaMemoryResource`; exact subspan overruns of those allocations are rejected
and independently tested.

Resolution: resolved for ASC-owned allocations. Remaining limitation:
arbitrary external CUDA allocations can be placement/device-classified but
their true allocation end cannot always be proven. Callers of non-owning views
must supply a truthful span. This is a documented remaining risk, not claimed
as full external-bound validation.

### M7-VER-004 — SpMV algorithm/workspace behavior needed separation

The reviewed implementation uses deterministic `CUSPARSE_SPMV_CSR_ALG2` for
unit-stride vectors with an explicit caller-owned queried workspace. Positive
non-unit vector strides use the original deterministic project kernel and
require a zero-length workspace. There is no silent fallback between paths.

Resolution: the independent test now covers both paths and their distinct
workspace contracts. Resolved.

### M7-VER-005 — trusted structure could not initially be reused with values

The first evaluator test exposed that reconstructing a destination through raw
public spans correctly lost provenance, but there was no checked way to retain
immutable structure provenance with a distinct exact value buffer.

Resolution: checked `RebindValues` preserves only the validated immutable
structure provenance and requires an exact NNZ-length value span. Exact
value-span in-place terminals are also supported. Both cases pass independent
real-GPU tests. Resolved.

### M7-VER-006 / PORT-002 — densifying scalar expressions were accepted

The portability/API review correctly identified that adding a nonzero scalar
to a sparse terminal, subtracting a nonzero scalar from a sparse terminal, or
subtracting a sparse terminal from a nonzero scalar changes implicit zero
entries and therefore is not structure-preserving.

The verifier added both operand orientations and no-mutation checks. It also
requires sparse add/subtract zero, zero plus sparse, zero minus sparse
(negate), and multiply by an arbitrary scalar in either orientation to remain
accepted.

The pre-fix focused build succeeded, but:

```sh
ctest --test-dir /tmp/asc-cpp-m7-verification-n7zSsT \
  -R '^asc_cpp\.sparse_cuda\.runtime$' --output-on-failure
```

failed because production accepted all four tested densifying scalar forms
for both float and double and consequently mutated the destination.

Resolution: production now rejects the nonzero scalar forms that would modify
implicit zeros while retaining the approved zero add/subtract,
zero-minus-sparse negate, and arbitrary scalar multiply forms. The final
focused real-GPU test passes for float and double. Resolved.

### M7-VER-007 / M7-DOC-11 — structure/value overlap needed rejection

The API review found that sparse public view construction and checked
`RebindValues` must not permit a value byte span to overlap immutable
coordinate, offset, or index storage.

Resolution: sparse view construction now requires pairwise-disjoint structure
and value byte spans. The focused verifier intentionally rebinds a trusted CSR
owner's value span over its inner-index storage and confirms rejection before
evaluation. The final real-GPU test passes for float and double. Resolved.

## API and dependency review

The reviewed public provider surface remains confined to:

```text
<asc/sparse/providers/cuda.h>
<asc/random/providers/cuda.h>
<asc/random/providers/dense_cuda.h>
<asc/random/providers/sparse_cuda.h>
```

No public signature exposes a CUDA or cuSPARSE SDK type. The implementation
does not introduce cuRAND, Thrust/CUB as an API dependency, cuSOLVER, or
another third-party dependency. Sparse remains independent of dense.

The intended direct ASC dependencies reviewed against the frozen contract are:

```text
ASC::sparse_cuda        -> ASC::sparse, ASC::core_cuda
ASC::random_cuda        -> ASC::random, ASC::core_cuda
ASC::random_dense_cuda  -> ASC::random_dense, ASC::random_cuda, ASC::core_cuda
ASC::random_sparse_cuda -> ASC::random_sparse, ASC::random_cuda, ASC::core_cuda
```

cuSPARSE is private to `ASC::sparse_cuda`; the other facets use CUDA Runtime
through `ASC::core_cuda`. `ASC::cpp` remains provider-free.

## Consumers, packages, sanitizers, and performance

The first isolated run passed build-tree and installed-relocated consumers for
all four M7 facets, including paths containing spaces. Classification:
`compile-tested`.

The aggregate component package test was still running when the superseded
full run was stopped, so this verifier does not independently claim a complete
package matrix. The lead must report its clean integration run.

Sanitizer execution was not part of this CUDA verifier run. Classification:
`skipped` here; provider-free and host-testable validation sanitizer evidence
must come from the lead's clean matrix.

Verifier-owned smoke benchmark sources include warmups, completion waits
outside checksum work, repeated timed operations, independently checked
checksums, and exact problem sizes for:

```text
CSR SpMV: 1024 x 1024, 5 entries per row, float and double
Raw Philox: 2^20 words
Dense Uniform01: 1024 x 1024 float
Sparse Uniform01: 128 x 128, count 128, float
```

The sparse-random benchmark's untimed validation compares every coordinate and
value bit to the independent test-only Philox priority/ordinal oracle, rather
than relying on checksums. The canonical CSR fixture uses five sorted unique
columns per row. The verifier built and ran both benchmark executables:

```sh
cmake --build /tmp/asc-cpp-m7-verification-n7zSsT \
  --target asc_sparse_cuda_benchmark asc_random_cuda_benchmark --parallel 4
/tmp/asc-cpp-m7-verification-n7zSsT/tests/sparse_cuda/asc_sparse_cuda_benchmark
/tmp/asc-cpp-m7-verification-n7zSsT/tests/random_cuda/asc_random_cuda_benchmark
```

All checksum/oracle validations passed. Observed totals for the declared
repetition counts were:

```text
CSR SpMV float:    1,167 us; workspace 704 bytes; checksum 10,240
CSR SpMV double:   1,247 us; workspace 752 bytes; checksum 10,240
Raw Philox:        1,471 us; checksum 2,253,713,208,691,922
Dense Uniform01:   4,150 us; checksum 524,733
Sparse Uniform01:  4,101,647 us; coordinate checksum 16,629;
                   value checksum 66.3448
```

Classification: `runtime-tested`; no speedup claim is made.

## Independent disposition

The independent review accepts the corrected M7 CUDA runtime surfaces on the
tested NVIDIA compute-8.6 configuration. PORT-002 and M7-VER-007 are resolved
by the final focused run. The remaining documented risk is the inability to
prove the terminal bound of every arbitrary external CUDA allocation;
ASC-owned allocations are range-checked. Clean aggregate package and
sanitizer evidence must be taken from the lead's integration checkpoint
rather than inferred from this review.
