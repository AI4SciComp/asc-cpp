# Reference LU expert slice: first four additional operations

This is implementation and bounded verification evidence, not owner/license
approval or full P04 completion. It covers actual S/D/C/Z GETRF2, GETF2, GETRI
and GESV calls through the explicitly selected GNU Reference-LAPACK provider.
The complete upstream denominator remains unchanged. GESVX, GECON, GERFS,
GEEQU/GEEQUB, required helpers, row-major packing, full-profile and remaining
installed gates are separate work; their completion is not inferred here.

## Exact identities

The source pin, ABI argument, installed header hashes and runtime/compiler
restrictions in `provider-abi-review.md` also apply. Upstream is commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, Reference-LAPACK 3.12.1. The report's
source-input manifest digest is
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`.
Actual S/D/C/Z argument documentation and the implementation statements of
these source routines were inspected before binding; no LAPACKE-only
denominator or invented foreign declarations were used.

| Artifact | SHA256 |
| --- | --- |
| `include/asc/dense/providers/lapack_lu.h` | `07b95892b4ccf5b17fa9fa32686a018975b5ad583c5eeaae2df0773d7e416812` |
| `src/dense/lapack/reference_lu_expert.cc` | `7e7e581682538da99364192d998e3f9d1d0a9fa5f472a061b6364864622074f6` |
| `tests/dense_lapack/lu_expert_test.cc` | `3b3b1a651201e94ad058686002212b24a1ed5ed1ec639edd8ab71d33d8144edd` |
| `tests/dense_lapack/lu_expert_faults.h` | `da51202de40dbf22b26b19109d6ce494cb5c49fdea0fa942543d384a6d6a12e5` |
| `tests/dense_lapack/lu_expert_faults.cc` | `e794a5c3efbe16308da675747d19b81a06cd21f3d3fb93c6e5e479eada11033b` |
| Selected LP64 provider build | `7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c` |
| Selected true ILP64 provider build | `8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97` |

The independent immutable external evidence snapshot is
`lu-expert-frozen-IpzTaiUf`, relative to the program's recorded external evidence
root. Its `verified-source-identities.sha256` also identifies the compiled
existing provider/foundations and frozen Core Result header. It contains the
actual headers and test dependencies used, so later independent I/O header
edits do not silently change the numerical evidence.

## API and checked execution semantics

The new header is optional and Dense-owned, with supported declarations in flat
`asc`. The private implementation calls the authoritative pinned
`LAPACK_{s,d,c,z}{getrf2,getf2,getri,gesv}` declarations from `lapack.h`, using
the already probed GNU complex convention and LP64/true ILP64 integer selection.
There is no LAPACKE row-major branch, guessed mangling, generic void-pointer
public operation or process-wide error handler.

All operands are checked column-major host/pinned-host descriptors. Matrices
retain their explicit leading dimensions and padding; existing descriptor
construction requires `ld >= max(1,rows)` even for empty columns. Row-major
returns `kUnsupported` and remains a required unimplemented layout route.
Shape, placement, provider integer ranges, pivot metadata, plan identity,
workspace alignment/capacity and all live operand/workspace overlaps are
checked before numerical mutation. Reports reset before preflight.

GETRF2 and GETF2 expose formula-only workspace queries for provider-width
integer pivot conversion. All output pivots are checked in `[i+1,m]` before
publishing ASC signed 64-bit one-based sequential swaps. Positive INFO returns
the documented raw completed singular LU/pivots with `kNumerical`; negative
INFO is a provider defect, with the exact raw sign and foreign argument
position retained. Unexpected excessive positive INFO or corrupt pivots
produces `kProvider` and unusable output, without publishing malformed pivots.

GETRI consumes raw packed square LU plus a valid immutable raw LU pivot view;
it does not require a successful-factor wrapper, so true singularity remains
observable. Its query accepts the actual full matrix descriptor and caller
storage for a real n-entry provider-integer converted pivot array. A live
scalar WORK(1) is local to the query. No undersized unrelated factor or pivot
array is substituted. Mandatory query reports use e.g. `dgetri.query`, preserve
actual INFO, identify the provider call, and leave numerical validity
`kUnchanged`; A/pivots are untouched, while explicit conversion scratch may
change. A malformed query workspace fails before the provider call.

Pinned `SRC/ilaenv.f` selects GETRI NB=64. Every GETRI entry first checks that
`n*64` fits the selected provider integer, because the source computes that
product before returning from its workspace query. This guard is not used as a
replacement for the real `LWORK=-1` query. Returned WORK is checked for finite,
representable counts, and complex query imaginary components must be zero.
Minimum `max(1,n)` and actual preferred counts are bound into the copied plan
identity. Execution revalidates the plan without another foreign query and
uses the smaller of supplied scalar capacity and preferred capacity, never
below minimum. Caller scalar workspace contains live T objects; nonallocating
placement construction establishes provider-integer array lifetimes.

Successful GETRI replaces LU with the inverse and supplies no reusable LU tag.
The pinned TRTRI path checks every diagonal before inverse mutation; singular
GETRI therefore returns exact positive INFO with unchanged input. Empty GETRI
execution is a successful noncall with absent INFO; its workspace query is
still an actual provider query. GESV calls the actual driver, preserves raw
LU/pivots on singularity, and leaves B unchanged then. `n>0,nrhs=0` still
factors A, following the source; only `n=0` is a complete driver noncall.

## Executed numerical, defensive and allocation evidence

All following paths are relative to the external evidence root. No raw logs,
dependency builds or installed artifacts reside in source.

- `build-lu-expert.sh`: frozen standalone optimized build and four independent
  scalar processes per ABI; both actual builds passed
  `logs/p04-lu-expert-{lp64,ilp64}-compile-08.log` and
  `logs/p04-lu-expert-{lp64,ilp64}-test-{s,d,c,z}-08.log`.
- GETRF2/GETF2 exercise square, tall, wide and 67-square matrices at three
  power-of-two scales, with nontrivial row swaps, independent `P*A=L*U`
  reconstruction, raw pivots and padding guards. Every S/D/C/Z route executes.
- GETRI exercises 3-square and 67-square inverses at three scales with both
  minimum and queried preferred WORK; independent left and right products
  check inverse residuals. Queries preserve factors/pivots and guards. A
  double-specific test-only call counter proves execution did not requery.
- GESV exercises zero, one and three RHS, including actual factorization when
  RHS count is zero, independent expected solutions and original-A residuals.
  Singular/empty cases distinguish raw INFO from successful noncalls.
- Allocation probes surround complete ASC query/execution calls, first routine
  entries, repeated operations and structural/numerical errors. The C++ new
  probe and ELF wrappers observe zero calls to malloc/calloc/realloc/
  aligned_alloc/posix_memalign across ASC and statically linked provider
  objects. No numerical warm-up is excluded from those scopes.
- Tests reject missing/misaligned/overlapping/device workspace, stale leading
  dimensions, routine/count/limit tampering, invalid pivots, invalid layouts,
  shape and RHS mismatches, strided pivots and operand aliases. Structural
  failure leaves numerical/workspace snapshots unchanged with absent INFO.
- Test-only GNU link wrapping injects negative/excessive INFO, bad output
  pivots, NaN/infinite/negative/too-short GETRI query counts. These tests check
  defenses and publication rules, not upstream numerical capability.
- `logs/p04-lu-expert-frozen-{format,header,tidy}-01.log`: Clang 18.1.8 formatting,
  strict configured analysis and self-contained public-header compilation
  passed. The public-header test needs no foreign header include path.
- `build-lu-expert-asan.sh` and
  `logs/p04-lu-expert-{lp64,ilp64}-asan-{compile,test-s,test-d,test-c,test-z}-02.log`:
  all frozen S/D/C/Z ASan+UBSan processes passed. These builds instrument the
  provider adapters, foundations and tests, but link uninstrumented baseline
  Core/Dense and uninstrumented Fortran/BLAS archives. This is not whole-library
  or whole-provider sanitizer proof.

The archive closure audit is `audit-lu-expert-closure.py`, with actual archive
hashes, roots and external leaf symbols in
`provider-lu-{lp64,ilp64}-01/lu-expert-static-call-closure-02.json`. It excludes
only checked-unreachable XERBLA argument-error branches. Unlike the earlier
GETRF/GETRS-only closure, GETRI reaches `_gfortran_concat_string` through TRTRI.
That runtime leaf is recorded, not hidden by claiming no runtime calls.

The exact libgfortran runtime SHA256 is
`f7379c9331de9d66c03b439a070e8056b7419fc4213320d647952b998d026b30`. Full
disassembly in `logs/p04-lu-expert-runtime-concat-01.log` shows its 146-byte
`_gfortran_concat_string` function at `0x28c840` calls or tail-jumps only to
memcpy/memset, with no allocator or indirect call. The audit requires this
exact inspected runtime identity. Other external leaves are cabs/cabsf,
logf/lroundf, memcmp/memcpy/memset. This binary inspection supplements static
ELF allocation probes; it is not dynamic interposition inside every shared
runtime or a universal no-allocation guarantee for other LAPACK families.

## Remaining gates and next task

Parent integration still owns optional-component registration, installed
consumers, frozen ABI/public-header inventories and full regression evidence.
No PR or installed first-16 gate is claimed here. Continue actual GEEQU/GEEQUB
in their separately owned equilibration files, then GECON/GERFS/GESVX and
required helpers. Keep the first-16 frozen identities above stable while those
new files are tested. Native LU evidence remains distinct from these reference
calls; native Cholesky/QR and the complete array interchange program are not
closed by this report.
