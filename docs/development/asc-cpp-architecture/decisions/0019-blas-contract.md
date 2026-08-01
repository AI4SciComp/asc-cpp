# ADR 0019: dense and sparse BLAS contract

Status: Approved by Issue 5; implementation completed through Issue 10 and
independently audited by Issue 11

Date: 2026-07-31

Supersedes: ADR 0013 and ADR 0014 where they describe the linear-algebra
surface, provider plan, or deferred operation inventory

## Context

Before Issue 6, ASCCpp exposed a small float/double linear-algebra subset through
`asc/dense/linalg.h` and `asc/sparse/linalg.h`. The names do not distinguish
the BLAS layer from general linear algebra, and the existing subset is not an
auditable completeness definition. Dense BLAS and Sparse BLAS are separate
standards with different data models and operation families.

This decision is Class C. It freezes the future public surface, numerical and
execution semantics, provider boundaries, standards inventory, compatibility
route, coverage evidence, and implementation order before any rename or new
routine is implemented.

The approved six-module graph remains unchanged. Dense owns dense BLAS, Sparse
owns sparse BLAS, and neither module depends on the other. There is no
top-level BLAS module, shared backend module, provider registry, or seventh
module.

## Frozen primary baselines

The coverage contract does not follow a moving branch or vendor latest page.
It uses these immutable identities:

<!-- markdownlint-disable MD013 -->

| Baseline | Frozen identity |
| --- | --- |
| Netlib reference BLAS and CBLAS | Reference-LAPACK `v3.12.1`, commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, specifically `BLAS/SRC`, `CBLAS/include/cblas.h`, `CBLAS/src`, and `CBLAS/testing` |
| Netlib BLAS quick reference | `https://www.netlib.org/blas/blas.pdf`, SHA-256 `fc35d15e2853e56e8771eb1e85cf3af8fed70af89e9cdb60861901c41bf41ae6` |
| BLAS Technical Forum standard | final draft, Spring 2002 publication; complete PDF SHA-256 `7874962ae4dc753fcdd483ff0c69931227846632251f79a68f4f879348f321a6` |
| Sparse BLAS standard | BLAS Technical Forum Chapter 3 PDF, SHA-256 `50b54c41bc42efe771f7f5d78a64d02a9373dfe051255e3b4366813101375905` |
| NIST Sparse BLAS reference | ANSI C++ distribution 1.02, SHA-256 `2567fd5fbef04a7ec4f649e2809245badcf04c3a866c36639089729e7de41d0b` |
| CUDA toolkit documentation | archived CUDA Toolkit 12.9.1 cuBLAS and cuSPARSE documentation |
| Exercised CUDA libraries | CUDA compiler 12.9.86; cuBLAS 12.9.1.4; cuSPARSE 12.5.10.65 |
| C++ source policy | Google C++ Style Guide retrieved 2026-07-30, SHA-256 `f681e8c1b71ed5f2420555a28b7e7120f46914cfa126e9d8ec5e6e9851512caf`, plus repository executable policy |

<!-- markdownlint-enable MD013 -->

Netlib states that reference BLAS and CBLAS are now part of the LAPACK
release. The Reference-LAPACK release commit therefore supplies the immutable
source, C header, and test identity for both.

The coverage inventory excludes the non-operation support routines `lsame`,
`xerbla`, `xerbla_array`, `scabs1`, and `dcabs1`. It includes all 146 classic
BLAS operation/type source variants and records the four `gemmtr` routines
added to the frozen source as non-required extensions. Chapter 4 extended and
mixed-precision BLAS, half, bfloat16, tensor-core, batched, and other vendor
extensions require separate approval and rows.

## Dense inventory

The manifest contains one row per official routine and scalar combination.
The normalized families are:

<!-- markdownlint-disable MD013 -->

| Level | Families | Exact row count |
| --- | --- | ---: |
| 1 | `rotg`, `rotmg`, `rot`, `rotm`, `swap`, `scal`, `copy`, `axpy`, `dot`, `dotu`, `dotc`, `nrm2`, `asum`, `iamax` | 50 |
| 2 | `gemv`, `gbmv`, `hemv`, `hbmv`, `hpmv`, `symv`, `sbmv`, `spmv`, `trmv`, `tbmv`, `tpmv`, `trsv`, `tbsv`, `tpsv`, `ger`, `geru`, `gerc`, `her`, `hpr`, `her2`, `hpr2`, `syr`, `spr`, `syr2`, `spr2` | 66 |
| 3 | `gemm`, `symm`, `hemm`, `syrk`, `herk`, `syr2k`, `her2k`, `trmm`, `trsm` | 30 |
| extension | `gemmtr` | 4 |

<!-- markdownlint-enable MD013 -->

The Level 1 `dot` family includes `sdot`, `ddot`, mixed-accumulation `dsdot`,
and extended `sdsdot`. Unconjugated and conjugated complex dot products remain
separate `dotu` and `dotc` rows. The scalar names in the manifest distinguish
input, coefficient, and result precision where one prefix alone is
insufficient.

All classic rows are required for dense completion. The four `gemmtr` rows are
visible but `not-applicable` to the classic completion claim; admitting that
extension requires a later contract update. No extension substitutes for a
classic row.

## Sparse inventory and applicability

The Sparse BLAS baseline contains exactly 79 C-interface routines:

<!-- markdownlint-disable MD013 -->

| Group | Families | Type expansion | Rows |
| --- | --- | --- | ---: |
| Level 1 compute | `usdot`, `usaxpy`, `usga`, `usgz`, `ussc` | S, D, C, Z | 20 |
| Level 2 compute | `usmv`, `ussv` | S, D, C, Z | 8 |
| Level 3 compute | `usmm`, `ussm` | S, D, C, Z | 8 |
| construction begin | `uscr_begin`, `uscr_block_begin`, `uscr_variable_block_begin` | S, D, C, Z | 12 |
| insertion | `uscr_insert_entry`, `uscr_insert_entries`, `uscr_insert_col`, `uscr_insert_row`, `uscr_insert_clique`, `uscr_insert_block` | S, D, C, Z | 24 |
| construction end | `uscr_end` | S, D, C, Z | 4 |
| property and lifetime | `usgp`, `ussp`, `usds` | scalar-neutral | 3 |

<!-- markdownlint-enable MD013 -->

The compute families are applicable. ASCCpp will express them over
caller-owned sparse descriptors and storage-neutral dense-vector or
dense-matrix descriptors rather than exposing the standard's integer handle.
This preserves the forbidden Dense-to-Sparse dependency boundary.

The 43 construction, property, and lifetime routines remain in the crosswalk
as `not-applicable` BLAS calls:

- point construction, insertion, completion, and immutable descriptor
  lifetime are owned by `CoordinateBuilder`, coordinate/CSR/CSC owners, and
  their views;
- block and variable-block construction are unavailable because ASCCpp owns no
  BSR or variable-block format;
- properties are typed immutable metadata rather than mutable integer
  properties;
- C++ owner lifetime replaces an explicit public destroy routine.

This is a deliberate API mapping, not an incomplete hidden handle layer.
Provider-native handles and descriptors are private to explicit provider
contexts.

The owned storage contract is:

- general-rank canonical coordinate storage;
- rank-two canonical CSR and CSC;
- zero-based signed 64-bit ASCCpp indices and nonzero counts;
- immutable sorted unique finalized structure;
- explicit duplicate reject/sum and explicit-zero keep/drop policy during
  finalization;
- no BSR, SELL, unsorted finalized descriptor, duplicate-bearing finalized
  descriptor, or one-based public descriptor.

Compute operations accept empty rows, columns, and matrices when their shapes
are valid. Operations requiring triangular structure reject a descriptor that
does not explicitly declare and validate triangle, diagonal, and symmetry
metadata. Singular triangular systems return a failure before publishing a
successful result.

Sparse matrix addition and sparse matrix multiplication are not in the frozen
79-routine BLAST/NIST contract and are not admitted by this decision. They
require a later design covering result structure, allocation, workspace,
duplicate/zero policy, symbolic analysis, and backend capability. A vendor
API alone does not define ASCCpp semantics.

## Public API and rename decision

Issue 6 implements the approved breaking rename in the next pre-1.0 minor:

- `include/asc/dense/linalg.h` becomes `include/asc/dense/blas.h`;
- `include/asc/sparse/linalg.h` becomes `include/asc/sparse/blas.h`;
- `src/dense/linalg.cc`, `src/sparse/reference_linalg.cc`, internal namespaces,
  CMake source/file-set entries, tests, documentation, and non-historical
  policy references use `blas`;
- umbrella headers include the new paths;
- operation names such as `Copy`, `Axpy`, `Gemm`, and `Spmv` remain BLAS
  operation names in the flat `asc` namespace;
- public argument types acquire owner-qualified BLAS names where needed, such
  as `DenseBlasTranspose` and `SparseBlasTranspose`, so Dense and Sparse remain
  independent in the flat namespace.

There is no forwarding header, alias, duplicate implementation, deprecated
target, component, or deprecation window. The repository is an unreleased
`0.9.0` candidate, and the approved compatibility policy permits a documented
pre-1.0 minor break. Migration notes must give mechanical include/type/source
renames while preserving legitimate prose such as “linear algebra.” Historical
records remain historical and are not blindly rewritten.

The rename introduces no `ASC::blas` component or target. Existing component
ownership remains `ASC::dense`, `ASC::sparse`, `ASC::dense_cuda`, and
`ASC::sparse_cuda`.

## Common operation semantics

Every public operation documents its mathematical formula and validates its
complete contract before destination mutation or provider submission.

### Types and indices

- Required scalar families are `float`, `double`, `std::complex<float>`, and
  `std::complex<double>` where the frozen routine exists.
- Mixed dot rows preserve their frozen input, accumulation, and result types.
- cv-qualified, integral, half, bfloat16, and provider-specific scalar types
  are unsupported unless a separate manifest row approves them.
- Public logical extents, strides, and sparse indices use checked ASCCpp
  64-bit types.
- Every provider narrowing, byte count, leading dimension, increment, band
  width, packed length, and workspace calculation is checked before use.

### Layout and stride

- Dense matrices support explicit `LayoutLeft` and `LayoutRight` when the
  operation row says so. `LayoutStride` is supported only when its mapping can
  be represented without packing by the selected backend.
- Dense vector operations support nonzero signed increments in the BLAS
  contract, including negative increments. A view describes the logical first
  element; validation proves the complete addressed span. Current Dense views
  defer negative strides, so those rows cannot become `verified` until the
  view or operation-descriptor design supports them safely.
- Zero increments are invalid. Empty logical vectors do not dereference their
  pointer.
- Banded and packed routines use operation-specific public descriptors; they
  are not disguised ordinary dense matrices.
- Row-major mappings are semantic mappings, not implicit transpose tricks
  visible to callers. Adapters may transform parameters without data movement.

### Operation flags

- Transpose has `none`, `transpose`, and `conjugate-transpose`; real
  conjugate-transpose is mathematically equivalent to transpose but remains a
  valid explicit request where the row permits it.
- Triangle is `upper` or `lower`; diagonal is `unit` or `non-unit`; side is
  `left` or `right`.
- Symmetric and Hermitian routines read only the declared triangle.
  Hermitian diagonal imaginary parts follow the frozen routine contract.
- Unit-diagonal triangular routines do not read stored diagonal values.
- Invalid enumerators return `kInvalidArgument`.

### Scalars, empty inputs, and floating point

- `alpha == 0` prevents reads of multiplicative input operands when the frozen
  BLAS contract permits it.
- `beta == 0` prevents reads of the prior destination.
- Empty and degenerate shapes perform required validation but do not
  dereference empty storage.
- The portable reference path fixes an explicit traversal order per routine.
  It does not promise cross-compiler bit identity.
- `Nrm2` uses scaled sum-of-squares. Dot and reduction accumulation type is
  declared by the row.
- NaN, infinity, signed zero, subnormal, and extreme-magnitude tests compare
  documented invariants or justified error bounds; they do not use one
  universal epsilon.
- CPU/GPU parity uses operation-, type-, size-, and conditioning-aware forward
  or residual error bounds. Each test records the oracle and tolerance
  derivation.

### Aliasing and mutation

- Exact in-place forms are allowed only where the frozen routine defines them.
- `Copy` permits identical source/destination as a no-op and rejects unsafe
  partial overlap.
- Vector rotations, swaps, triangular operations, rank updates, and other
  mutating routines document their permitted exact aliases.
- Matrix/vector and matrix/matrix outputs conservatively reject overlap with
  read operands unless a row proves a safe standard in-place form.
- Validation, capability, workspace, and alias checks complete before
  mutation. An asynchronous provider submission failure follows the existing
  stream-drain recovery rule when work may already have been enqueued.

### Errors, execution, and lifetime

- Recoverable failures use existing `Status`, `Result<T>`, and `ErrorCode`.
  Provider codes remain diagnostic details, not stable ASCCpp codes.
- The requested execution/provider context is authoritative. Unsupported and
  unavailable remain distinct.
- Serial reference operations are synchronous. Successful CUDA operations
  return a `CompletionEvent`; callers wait or query before reading results.
- No BLAS call silently allocates, packs, converts, transfers, densifies,
  synchronizes, changes precision, narrows, or falls back.
- Workspace size and alignment are queryable. Workspace is caller-owned unless
  an explicit reusable plan object owns provider workspace by documented
  construction.
- User operands, caller workspace, context, and provider plan outlive
  asynchronous completion. Events retain provider completion state, not user
  arrays.
- Provider contexts own cuBLAS/cuSPARSE handles, bind them to the context
  stream before submission, and serialize mutable handle state. Independent
  contexts may execute concurrently. A single context is safe for concurrent
  callers only through that internal serialization; no process-global handle
  or current stream exists.

## Backend architecture

### Portable reference CPU

Every required compute row has an allocation-free portable C++20 reference
implementation owned by Dense or Sparse. It is the correctness oracle and is
always available with a serial host context. It does not depend on Fortran,
CBLAS, Dense from Sparse, or an optional provider.

Reference kernels separate:

1. public descriptor and enum validation;
2. alias/span and checked-arithmetic validation;
3. operation-specific traversal;
4. result publication.

The reference code may use independently implemented formulas informed by the
frozen standard. It does not copy Reference-LAPACK or NIST implementation
text. Official test inputs may be adapted only after provenance and license
review.

### Optimized CPU adapters

The adapter boundary is approved, but no optimized CPU provider is approved.
OpenBLAS, oneMKL, Accelerate, and other libraries require a separate provider
ADR covering discovery, license, package isolation, integer ABI, threading,
complex ABI, row-major behavior, determinism, workspace, runtime evidence, and
fallback policy. Until then, `optimized_cpu` is `not-applicable` in coverage
rows and creates no dependency, target, option, or support claim.

An approved future adapter must live in an owner-specific optional facet,
translate only representable descriptors, and return unsupported rather than
pack or call the reference path.

### CUDA adapters

Dense CUDA maps representable rows through cuBLAS 12.9.1.4. Sparse CUDA maps
representable rows through cuSPARSE 12.5.10.65. Project CUDA kernels are
permitted only when their operation row explicitly names them and supplies the
same validation, numerical, allocation, completion, and parity evidence.

The common headers contain no CUDA type or include. Provider selection remains
explicit through `DenseCudaContext` or `SparseCudaContext`; there is no
automatic backend. Native descriptors and handles are private. Sparse
analysis, conversion, and workspace are explicit and never cached in a global
registry.

Each CUDA row records the exact vendor routine or project kernel, supported
layouts/formats, scalar and index widths, algorithm, determinism, workspace,
driver/toolkit/hardware evidence, and last verification date before it becomes
`verified`.

## Coverage manifest and generated report

`docs/development/asc-cpp-architecture/blas-coverage.yaml` is the source of
truth. It uses the repository's existing YAML format and contains:

- immutable baseline identities and checksums;
- exact approved backend identities;
- one coverage row per dense routine/type variant and per Sparse BLAS C
  routine;
- one dense-family-to-sparse crosswalk row per dense family;
- public API/header, implementation, and test links;
- backend and aggregate status;
- notes for exclusions and blocked work.

Allowed states are exactly:

```text
planned
implemented
verified
not-applicable
blocked
```

Only aggregate `verified` is complete. `implemented` means code exists but
lacks some declared conformance or environment evidence. `not-applicable`
requires a reason and is an explicit exclusion, not completion.

`cmake/GenerateBlasCoverage.cmake` validates the exact schema, allowed states,
unique keys, independently frozen dense/sparse/combined inventory identities,
independently frozen row counts, the family-crosswalk identity, required
links, and linked file existence. The manifest repeats those identities for
human and tool consumers, but changing the manifest declarations cannot
redefine the validator's Issue 5 contract. It deterministically generates
`docs/blas-coverage.md`. The architecture test regenerates into the build tree
and compares byte-for-byte with the committed report, so the manifest and
public report cannot drift. Negative architecture fixtures prove that
self-declared count changes, missing implementation links, partial backend
verification, and a stale generated report are rejected.

An `implemented` or `verified` row must name an existing public header,
implementation, conformance test, invalid-input test, and CPU/GPU test when a
CUDA backend is implemented or verified. Aggregate `verified` requires every
applicable declared backend to be `verified`; a reference CPU result cannot
complete a row whose CUDA backend remains planned or merely implemented.
Feature Gate B evidence must show that each declared verified backend actually
ran; detection or compilation alone is insufficient.

## Verification contract

Each compute row requires:

- independent formula or official reference-case conformance;
- zero, unit, rectangular, padded, non-unit-stride, and permitted
  negative-stride cases;
- supported row/column-major layouts and all operation flags;
- applicable upper/lower, unit/non-unit, side, band, and packed cases;
- `alpha`/`beta` zero and one, NaN, infinity, subnormal, and extreme values;
- allowed alias and rejected overlap;
- invalid shape, enum, placement, index-width, overflow, and workspace paths;
- exact allocation, transfer, synchronization, and fallback probes;
- static/shared build-tree and relocated installed consumers;
- CPU/GPU parity with explicit completion and tolerance justification.

Sparse compute adds empty rows/columns, canonical COO/CSR/CSC conversions,
duplicate and explicit-zero construction policy, malformed structures,
structural/numerical symmetry, singular triangular systems, and 32/64-bit
provider narrowing checks.

No performance claim precedes correctness. Later implementation issues record
Release flags, commits, hardware, dimensions, sparsity, warm-up, repetitions,
synchronization, statistics, allocations, transfers, correctness checks,
variance, and an approved threshold. Reference CPU is a correctness baseline,
not required to match vendor performance.

## Implementation sequence

1. Issue 6 performs only the approved breaking rename, migration notes,
   packaging/header tests, and manifest link updates.
2. Issues 7, 8, and 9 implement and verify dense Levels 1, 2, and 3.
3. Issue 10 implements and verifies the applicable Sparse BLAS compute
   families without introducing a Dense dependency.
4. Issue 11 independently audits the frozen inventories, manifest/API/source/
   test links, numerical evidence, packaging, documentation, and performance.

The first implementation branch, Issue 6, is expected to change only the
dense/sparse BLAS header and source paths, owning CMake file sets, umbrellas,
compile/runtime/hardening tests, public API and migration documentation, this
ADR where final names require correction, the manifest, and its generated
report. It must not add a BLAS routine, backend, target, component, dependency,
or compatibility forwarding layer.

## Consequences

- The required surface is large but reviewable by level and backend.
- Existing float/double operations begin as `implemented`, not `verified`,
  because the new frozen conformance contract has not yet been exercised.
- Negative vector strides require a safe descriptor/view solution before
  affected rows can be verified.
- Complex storage and operation APIs are required by classic parity and will
  be introduced in their level issues.
- Sparse standard construction remains fulfilled by typed storage APIs rather
  than duplicated handle calls.
- No optimized CPU provider is claimed.

## Resolved architecture decisions

- Breaking pre-1.0 rename, with no compatibility window.
- Flat `asc` namespace and existing Dense/Sparse component ownership.
- Reference CPU plus explicit owner-specific CUDA facets; no automatic
  backend or fallback.
- No optimized CPU provider in the approved contract.
- Classic 146 dense rows are required; four `gemmtr` rows are visible
  non-required extensions.
- All 36 Sparse BLAS compute rows are applicable; 43 storage-handle rows map
  explicitly to typed storage or unsupported block formats.
- SpAdd and SpGEMM are not admitted without a later architecture decision.
- Machine-readable YAML plus deterministic generated Markdown and CI drift
  enforcement.

No unresolved decision authorizes implementation outside the issue sequence.

## Verification

Run the manifest validator/generator directly, configure with the exact
ASCCMake 0.1.0 package, build with warnings-as-errors, run the architecture
test and complete available CPU suite, repeat shared/static and Release/Debug
as approved, then configure/build/test the CUDA row on the recorded toolkit
and GPU. Run formatter/linter checks applicable to changed Markdown, YAML, and
CMake. Review the full diff against `main`; do not infer a backend claim from
configure or compile evidence.
