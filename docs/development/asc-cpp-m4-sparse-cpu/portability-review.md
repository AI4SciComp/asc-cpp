# Milestone 4 Portability, GPU, and Performance Review

Status: Accepted after resolution of release-blocking findings

Date: 2026-07-26

Branch: `feature/asc-cpp-m4-sparse-cpu`

Role: independent portability/GPU/performance reviewer

## Review boundary

This review was read-only except for this report. It covered the complete
runbook, frozen Milestone 4 contract and ownership ledger, accepted ADRs for
the module graph, package, source, errors, metadata, memory/execution,
ownership, expressions, sparse storage, sparse algebra, mixed storage,
provenance, and release boundaries, the production self-review, public
headers and compiled source, tests, benchmark, package integration, and
module documentation.

No MdeCpp source or test, user-deleted implementation, provider SDK example,
or third-party sparse implementation was used. No production, test, CMake,
package, manifest, or other report was edited by this role.

## Environment inspected

```text
host:          Linux 6.18.33.2-microsoft-standard-WSL2 x86_64
CPU:           11th Gen Intel Core i7-11800H, 16 logical CPUs
CMake:         4.1.2
GCC:           11.4.0
Clang:         19.0.0
clang-format:  19.0.0
clang-tidy:    unavailable locally
Ninja:         unavailable locally
CUDA toolkit:  12.9, nvcc 12.9.86
GPU:           NVIDIA GeForce RTX 3060 Laptop GPU, 6144 MiB
driver:        576.83
```

CUDA, cuSPARSE, cuBLAS, BLAS, and LAPACK availability is host inventory only.
None is a Milestone 4 dependency or evidence level.

## Findings and resolutions

### M4-PORT-01: identity-only alias metadata missed partial overlap

Initial severity: release-blocking correctness and undefined-behavior risk

Initial evidence: generic SpMV validation compared only a readable operand's
identity token with the writable destination's identity token. Independently
created dense views over partially overlapping ranges had different starting
identities. Both overlap directions could therefore pass validation, allowing
an early output write to overwrite an input used by a later row. Matrix-value
and dense-output partial overlap had the same defect.

Resolution:

- expression now provides checked `AliasToken::FromAddressSpan` and symmetric
  `AliasTokensMayOverlap`;
- root dense, coordinate, CSR, and CSC views publish their complete validated
  value byte span;
- their readable adapters use symmetric span-aware comparison;
- dense subviews retain their root span, preserving conservative behavior;
- identity equality remains source-compatible and identity-only;
- sparse does not include or link dense.

Independent audit covered equal, disjoint, partial-overlap, point-in-span,
zero-byte, and identity-only semantics. Regression tests cover both directions
of independently created dense input/output overlap and both directions of
matrix-values/dense-output overlap, with unchanged destinations on rejection.
The correction is storage-neutral and preserves the six-module graph.

Disposition: resolved.

### M4-PORT-02: volatile placement/writable constraint mismatch

Initial severity: release-blocking public constraint defect

Initial evidence: adapter detection removed cv/ref qualifiers, so a
top-level-volatile descriptor could satisfy `PlacedReadableExpression` and
`WritableExpression`, but query and write bodies could not bind that object to
the non-volatile adapter signature. Failure therefore occurred inside a public
function body instead of at constraint selection.

Resolution: `PlacedReadableExpression` rejects top-level volatile descriptors;
`WritableExpression` inherits that rejection. Const readable placement remains
supported, while top-level const remains non-writable.

Disposition: resolved.

### M4-DOC-06: base readable constraint had the same volatile mismatch

Initial severity: release-blocking public constraint defect

Independent audit confirmed that base `ReadableExpression` also removed
cv/ref qualifiers without rejecting top-level volatile. This affected shape,
read, alias, and sparse-evaluation calls even when placement was not queried.

Resolution: base `ReadableExpression` now rejects top-level volatile and
const-volatile descriptors through `std::remove_reference_t`. Positive const
readable behavior is unchanged. Compile contracts cover all four readable,
placed-readable, and writable cases.

Disposition: resolved.

## C++20 and platform portability

The reviewed code uses C++20 facilities available in GCC 11, Clang 19,
AppleClang, and Visual Studio 2022: concepts, `std::span`, `std::array`,
`std::construct_at`, type traits, and checked project vocabulary. It contains
no C++23 construct, compiler extension, module, exception-based public API,
provider header, platform-sized sparse index, or global mutable production
state.

Public headers are self-contained `.h` files with full guards and direct
includes. The one compiled source is `.cc`. The flat `asc` namespace and
`internal_*` implementation namespaces match ADR 0003.

The static/shared design is coherent:

- `asc_sparse` is a real compiled library whose only compiled behavior is the
  float/double reference SpMV kernel;
- static builds propagate `ASC_SPARSE_STATIC_DEFINE`;
- shared builds compile with `ASC_SPARSE_BUILDING_LIBRARY`;
- the two compiled overloads use `ASC_SPARSE_EXPORT`;
- GCC/Clang visibility attributes and Windows import/export declarations are
  isolated in `asc/sparse/export.h`;
- consumer-instantiated callbacks cross the compiled boundary as ordinary
  function pointers, without provider types.

Local hosted MSVC and AppleClang execution was unavailable. The checked-in CI
has Visual Studio 2022 x64 shared Debug/Release and macOS 15 arm64 AppleClang
jobs. Those remain future CI evidence, not local passes. Local clang-tidy and
Ninja were unavailable and are also reported rather than inferred.

## Overflow, pointer, lifetime, and UB audit

The sparse implementation consistently uses signed 64-bit core index, extent,
and NNZ vocabulary and checks conversion to `std::size_t`, sums, products,
byte counts, and address endpoints before publishing storage. External views
validate:

- recognized memory-space enumeration;
- non-negative shape and NNZ;
- non-null nonempty spans;
- alignment;
- representable byte and address ranges;
- disjoint structure/value spans; and
- host canonical structure when host dereference is permitted.

Loop bounds and `outer + 1`, `position + 1`, and binary-search midpoint
expressions stay within the previously validated signed ranges. Integral
duplicate addition is prevalidated with checked arithmetic before in-place
sorting or compaction. The same stable group order is used during validation
and mutation, so the unchecked extraction of a prevalidated sum cannot execute
signed overflow.

Owners begin trivial object lifetime with `std::construct_at`, are move-only,
and retain the non-owned `MemoryResource` used by their `Buffer`s. Partial
allocation failure is handled by local RAII. Views are trivially copyable,
non-owning descriptors; const propagation is one-way and structure is exposed
only as const.

The implementation cannot prove external allocation provenance, actual span
length, lifetime, or declared placement. Those remain explicit caller
preconditions. Non-host external structure cannot be dereferenced to prove
canonical order; all Milestone 4 algorithms reject it before access. A raw
pointer accessor is metadata, not host-dereference permission.

No sanitizer-visible defect was observed in the post-alias-fix focused SpMV
and concept executables under the verifier's ASan/UBSan build. The independent
verification and lead reports own the complete sanitizer matrix.

## Determinism, allocation, and algorithmic cost

Coordinate finalization is deterministic:

- stable lexicographic dimension order;
- insertion-order duplicate accumulation;
- checked integral addition;
- ordinary ordered floating addition; and
- explicit-zero removal only after duplicate handling.

The implementation deliberately trades time for no hidden workspace:

| Operation | Reviewed reference complexity | Computational allocation |
| --- | --- | --- |
| finalization | worst-case `O(rank * nnz^2)` | none beyond declared builder capacity |
| coordinate to CSR/CSC | `O(destination_outer * nnz)` | explicit destination buffers |
| CSR to coordinate | `O(rows + rank * nnz^2)` | explicit coordinate/value buffers |
| CSC to coordinate | `O(rows * (columns + nnz) + rank * nnz^2)` | explicit coordinate/value buffers |
| CSR to CSC | `O(columns * (rows + nnz))` | explicit offsets/indices/values |
| CSC to CSR | `O(rows * (columns + nnz))` | explicit offsets/indices/values |
| sparse evaluation | destination NNZ times expression lookup cost | none |
| CSR SpMV | `O(rows + nnz)` plus adapter costs | none |

Conversion loops write the requested destination format directly. They do not
route through an undisclosed coordinate owner, allocate dense-shape storage,
drop explicit zeros, combine entries, pack, transfer, synchronize, dispatch,
or fall back.

SpMV traverses rows and increasing canonical columns deterministically.
`beta == 0` branches before any output read. Successful evaluation and SpMV
contain no dynamic container, allocation, workspace, packing, conversion,
transfer, synchronization, provider selection, or dense traversal.

Dense subviews retain a root-span token and may conservatively reject
physically disjoint sibling views. This is an intentional false positive, not
a missed overlap or hidden copy.

## Benchmark evidence

The project-owned benchmark uses a canonical 128-by-128 tridiagonal CSR matrix,
382 stored entries, contiguous external vectors, five warmups, 200 measured
SpMV calls, global allocation instrumentation, a steady clock, and an observed
checksum. It has no speed threshold.

Direct execution of the post-fix GCC Debug executable:

```bash
/tmp/asc-cpp-m4-verifier.M9HB1A/build/tests/sparse/\
asc_sparse_allocation_free_benchmark
```

reported:

```text
compiler=GCC 11.4.0
configuration=Debug-like
backend=serial-reference
format=CSR-zero-based-canonical
shape=128x128
nnz=382
vector_layout=contiguous-external
iterations=200
operation=spmv
allocations_in_operations=0
elapsed_us=2595
checksum=382
```

This is functional allocation/no-densification evidence, not a stable
performance baseline. It is one aggregate observation under WSL2, has no
distribution, historical/provider comparison, or optimized Release claim, and
must not be used as a speed gate. The Publication Checkpoint may add a Release
observation with exact flags and CPU, but no provider comparison is applicable.

## Commands and independent results

Strict header and source checks passed with GCC 11 and Clang 19, both with
exceptions enabled and disabled:

```bash
<compiler> -std=c++20 [-fno-exceptions] -pedantic-errors \
  -Wall -Wextra -Wconversion -Wsign-conversion -Werror \
  -Iinclude -x c++ -fsyntax-only <new-or-modified-header>

<compiler> -std=c++20 [-fno-exceptions] -pedantic-errors \
  -Wall -Wextra -Wconversion -Wsign-conversion -Werror \
  -DASC_SPARSE_BUILDING_LIBRARY -Iinclude \
  -fsyntax-only src/sparse/reference_linalg.cc
```

The audited header set was:

```text
asc/expression/expression.h
asc/expression/writable.h
asc/expression.h
asc/dense/view.h
asc/sparse.h
asc/sparse/export.h
asc/sparse/coordinate.h
asc/sparse/compressed.h
asc/sparse/evaluate.h
asc/sparse/linalg.h
```

The M4 concept contract passed strict syntax with both compilers. The alias and
SpMV regression translation unit passed strict syntax with both compilers and
with exceptions disabled. Repository formatting passed:

```bash
clang-format-19 --dry-run --Werror \
  include/asc/expression/expression.h \
  include/asc/expression/writable.h include/asc/expression.h \
  include/asc/dense/view.h include/asc/sparse.h include/asc/sparse/*.h \
  src/sparse/reference_linalg.cc
```

Direct post-fix GCC runtime executions passed:

```text
asc_m4_sparse_contract:                    pass
asc_sparse_linalg_test:                    pass
asc_sparse_allocation_free_benchmark:      pass
allocations reported by benchmark:         0
```

Direct focused ASan/UBSan executions of `asc_sparse_linalg_test` and
`asc_m4_sparse_contract` returned zero with leak detection and halt-on-error
enabled.

## Dependency and provider isolation

`asc_sparse` links exactly:

```text
ASC::core
ASC::expression
```

Production sparse headers and source contain no utilities, dense, random,
retired array/linalg, aggregate, provider, CUDA, cuSPARSE, BLAS/LAPACK, Eigen,
oneMKL, OpenMP, TBB, HIP, or SYCL include or dispatch. Dense interoperability
is instantiated only in a development test or consumer translation unit
through expression-owned protocols.

The sparse package component expands to core, expression, and sparse only.
No provider discovery is present in root or sparse CMake. CPU-only
configuration therefore remains independent of locally installed GPU and CPU
provider libraries.

## GPU classification

Milestone 4 has no GPU target, source, option, discovery, compile, runtime
invocation, or parity operation. The detected CUDA toolkit and RTX 3060 are
inventory only.

**GPU evidence: skipped.**

This is not configure-tested, compile-tested, runtime-tested, or
parity-tested. GPU sparse storage and cuSPARSE work remain Milestone 7 scope.

## Residual limitations and risks

- An external adapter must prevalidate and retain a full span token, or use
  another conservative root identity that cannot miss overlap. The concepts
  cannot prove truthful external metadata, total writable coverage, unique
  indexing, or lifetime.
- Raw external pointers and non-host structural metadata retain documented
  caller obligations.
- Reference conversion and finalization algorithms are intentionally
  quadratic or repeated-scan correctness paths; no optimized CPU performance
  claim is appropriate.
- Only float and double serial CSR SpMV are implemented. CSC SpMV, transpose,
  SpMM, solvers, optimized providers, device storage, and asynchronous work are
  deferred.
- Local MSVC, AppleClang, clang-tidy, Ninja, ThreadSanitizer, and GPU runtime
  evidence is absent. Hosted platform results must come from CI after
  publication approval.
- Diagnostic `Status` strings may allocate on rejected calls. The successful
  numerical no-allocation claim does not cover diagnostics or an external
  adapter's undisclosed behavior.

## Final disposition

M4-PORT-01, M4-PORT-02, and M4-DOC-06 were release-blocking and are resolved.
No unresolved release-blocking portability, GPU-isolation, undefined-behavior,
or performance-contract finding remains in the reviewed Milestone 4 scope.
Final acceptance still depends on the lead's complete clean static/shared,
sanitizer, package, relocation, and isolated-consumer validation matrix.
