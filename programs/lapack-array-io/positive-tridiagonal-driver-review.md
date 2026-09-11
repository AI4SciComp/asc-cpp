# Positive-definite tridiagonal driver review

SPTSV, DPTSV, CPTSV and ZPTSV are four Reference catalogue entries. This
original checked adapter composes the pinned provider's driver through the
existing optional `ASC::dense_lapack` component. Registration, executed
engineering checks, numerical acceptance and owner approval remain separate.
The implementation does not change PTTRF/PTTRS, PTRFS, robust PPSVX, the
provider, numerical requirements, tolerances or the floating-point environment.

## Typed contract

The prerequisite review is
`master-continuation-20260910-01/ptsv-prerequisite-01/review.json` under the
existing external evidence root. It binds all four `SRC/?ptsv.f` hashes to
Reference-LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, and records the unchanged descriptor,
workspace, provider, PT factor and count-check dependencies.

| Routine | D storage | E and B storage | Native operation |
| --- | --- | --- | --- |
| SPTSV | float | float | Symmetric lower LDL-transpose |
| DPTSV | double | double | Symmetric lower LDL-transpose |
| CPTSV | float | complex float | Hermitian lower LDL-adjoint |
| ZPTSV | double | complex double | Hermitian lower LDL-adjoint |

Each signature has seven arguments: N, NRHS, D, E, B, LDB, INFO. There is no
UPLO, pivot or native workspace argument. The original descriptor contains
contiguous D[N] and lower E[max(N-1,0)]. Complex diagonal storage is real;
the upper entry of the same Hermitian matrix is the conjugate of lower E.
The complex drivers explicitly call PTTRS with Lower. The upstream prose
about regarding E as an upper factor does not remove that conjugation when
reusing factors for the same matrix.

`QueryPtsvWorkspace` inspects metadata without numeric reads or native entry.
Both full B layouts use explicit N*NRHS live scalar objects in the existing
layout-conversion region. D/E are passed directly. B is packed before entry
and published only after valid zero INFO. There are no hidden allocations.
Plans bind scalar, provider, N, NRHS, caller leading dimension and layout;
addresses and values may change. The existing PT count checker validates
native INTEGER terminal expressions using staged native LDB=N. Byte and
entry products are checked separately. No fictitious huge arrays are tested.

D/E/B, workspace and metadata obey existing disjointness rules. Stale plans,
undersized workspace, access and alias failures precede numeric reads/writes.
Metadata aliases preserve the report; other admitted preflight resets it.
N=0 completes locally without numeric reads or native INFO. N>0 with NRHS=0
still enters the provider and factors D/E; B and scratch have no active
numeric elements. Live dummy objects cover inactive native arguments.

INFO=1..N reports a nonpositive leading minor, with zero-based diagnostic
index INFO-1. D/E retain native partial results and B is preserved. The last
pivot may be nonpositive after a completed factorization. Invalid, unwritten
or out-of-range INFO is a provider defect: D/E writes remain and B is withheld.
INFO=0 publishes raw results; nonfinite factors or solutions produce a
numerical accuracy warning with documented partial validity. No clamp,
rescaling, fallback or mathematical success is inferred from INFO alone.

On finite successful completion, callers may use the existing factor view's
`FromRaw` with Lower and then PTTRS, or explicitly copy into the existing
factor owner and conjugate E for upper reuse. The PTTRF-report-bound factory
still rejects a PTSV report. No PTTRF execution certificate is manufactured.
Cost is O(N+N*NRHS), with O(N*NRHS) explicit temporary storage.

## Bounded engineering evidence

The initial candidates and all failed attempts are retained. Candidate01
could not compile its test against an older installed header set. Candidate05
could not compile its new observer because of an incorrect memory-view type.
Strict-check findings were corrected with direct includes, a split observer,
and existing localized GNU-wrapper/POSIX-header exceptions. No numerical
assertion or provider expression was changed.

`ptsv-external-final-audit-01/audit.json` binds candidate06's exact command
records, selected test IDs and raw observations in actual LP64 and true
ILP64. Each selected 11 tests: seven passed, four mathematical processes
failed, 24 assertions failed, and none skipped. All 32 raw observations per
ABI match candidate04. A later POSIX-header-only amendment passed strict
review and is being rebuilt in the integrated profiles.

Ordinary tests use independently constructed dyadic positive-definite
systems, orders 0/1/2/5/9, RHS counts 0/1/3, three scale exponents, both
padded layouts, factor reuse and immutable-plan reuse. They check known
solutions with unchanged 64-epsilon bounds, exact dyadic factors, guards,
allocation exclusion, exact one-byte-short rejection, stale plans, aliasing,
both native integer limits and actual failed pivots. The four-worker
real-provider checks use independent D/E/B, workspace, reports and provider
contexts; each worker has its own final failed pivot and preserved B.

The TLS foreign wrapper checks all four exact native prototypes and exercises
176 INFO/publication profiles, including partial-width INFO, missing INFO,
invalid INFO, failed pivots and nonfinite factors/solutions. Fault state is
isolated from ordinary concurrency. The partial-width check is qualified to
the admitted little-endian x86-64 profiles.

The observer protects real initialized containing arrays and intercepts the
actual ASC-to-provider entry. It uses fork and normal callbacks, with no
fault-handler recovery or fabricated pointers. Its 176 controls comprise 32
negative reads, 32 queries, 32 stale plans, eight exact short workspaces, 16
local completions, eight zero-RHS native paths, eight active native controls,
eight legitimate B input reads and 32 restored writes. D/E/B are legitimate
inputs during active computation; no output-only rule is imposed on them.
ASC preflight and local behavior are observed; provider/runtime internals are
not claimed instrumented. The maintained normal-return guard rejects
premature provider exit masquerading as a passed example.

## Required numerical disposition

| Cause and exact fixture | Required property | Observed ASC/direct provider | Decision |
| --- | --- | --- | --- |
| N=NRHS=1, A=B=a, E empty; a is denorm-min, twice denorm-min or min-normal/8, all four scalars and both B layouts | Exact X=1, tested at unchanged 64 epsilon | INFO=0; real X=Inf, complex X=(Inf,NaN); ASC numerical accuracy warning and raw partial output; direct typed PTSV gives the same values | Retain the failing gate. A separately authorized scaled PT solve strategy or provider decision is needed. The PPSVX-only strategy authorization does not cover this family. |

The driver reaches the already recorded PTTRS scalar path: PTTS2 evaluates
ONE/D before scaling B. The reciprocal overflows although B/D is exactly
one. Four failed processes represent 24 failed assertions, 12 failing
scalar/value cases, 24 layout executions and one arithmetic cause. The
max-finite fixture remains a passing control; direct fidelity is not
mathematical acceptance. Existing PT, PTCON and PTRFS causes remain separately
recorded in their reviews and the maintained owner packet.

## Integrated local delivery

`ptsv-root-import-01/manifest.json` identifies the original adapter, public
header, maintained tests, examples and export/package consumers. The frozen
source is tree `a34a722fc33ab4d6be840376a07907f49e1124b3`, archive SHA256
`40270605180af0daf91ee0bc44fce66d36790c45debfed8c08adcab76382be19`.
It records 1,184 build/API/test/example/package/ABI inputs. A reviewed test-only
companion changes the independent header count from 138 to 139 after the
single new PTSV header; the other 1,183 inputs remain byte-identical. The
frozen tree and original failed checks are preserved. This comparison does
not certify later narrative/index changes as an executed whole tree.

Static/shared Debug, Release and ASC-sanitizer configurations executed in
both actual ABIs. Each family selection passed seven of eleven processes,
retaining four required mathematical failures and 24 assertions. All 32 raw
observations per execution match the original candidate. Static/shared TSan
passed four real-provider ordinary/concurrent scalar processes per ABI.
No unexpected skip or sanitizer diagnostic occurred. Provider/runtime code
is uninstrumented. All six strict integrated source/test/example units pass.

Four fresh Release producers passed 14 of 18 checks, retaining only those
mathematical failures; both public-header compilation modes and the affected
architecture/configuration gates passed. All four install/relocate/package
runs passed. Copied public consumers solve all four scalars, inspect reports
and reuse factors through documented APIs. Eight symbols and exact installed
headers are audited. No provider/runtime files are bundled. All base
consumers and the five nonprovider shared libraries remain free of
LAPACK/Fortran runtime dependencies.

The initial installed-header selections passed seven fixture/M3 processes
and failed four header modes per configuration: the exact inventory count
had not advanced with the new enumerated header. The maintained three-literal
count/message correction is preserved separately in
`ptsv-header-count-amendment-01`; its exact amendment SHA256 is recorded in
`amendment-patch-sha256.txt`. Only the four failed checks were rerun, each with
fresh scratch paths and the actual selected commands. All sixteen amended
checks passed. Prior successful setup and M3 results are reused; original
failures remain genuine historical failures.

The earlier pushed guard revision `cc42d4ed295be1d1c48e9a9b1316c84238ccf080`
has four hosted family profiles at 164/181, seventeen unchanged required
mathematical failures and zero skips. All twelve guarded public examples
pass; each provider passes 111/111 and CodeQL passes. Eighteen CI jobs pass;
the documentation job fails on three example-only helper classes. Local
comment-only Doxygen internal sections correct that precise defect without
excluding a public ASC declaration or weakening a validator. Strict Doxygen
and its public coverage check now pass. These older hosted runs do not
certify PTSV or the documentation correction.

Exact commands, selected IDs, compiler/link settings and source/provider
identities are retained in `ptsv-static-profile-audit-01`,
`ptsv-shared-safety-audit-01`, `ptsv-shared-tsan-audit-01`,
`ptsv-fresh-producer-audit-01`, `ptsv-package-audit-01`,
`ptsv-final-local-audit-01` and `ptsv-delivery-checks-01`. All are under the
existing `master-continuation-20260910-01` evidence directory. Feature
synchronization is recorded in `ptsv-synchronization-01`; actual fresh hosted
run IDs are recorded in the PR/evidence rather than inserted as a new
self-identifying product candidate.

Four callable-unverified rows are registered under schema2 extension02.
Counts remain 2,113 required, 3,551 discovered and 1,438 excluded. There are
398 registered Reference routines: 56 callable-unverified and 342 partial;
1,715 are not started, 84 routine contracts are reviewed, and none is verified.
The accepted native20/array-I/O subset and separately selected robust PPSVX
capability retain their existing scoped milestones. Numerical acceptance,
complete normalized remaining programme evidence, unavailable provider
platforms and owner decisions remain separate obligations. PTSVX is the next
existing dependency-ready family; its prerequisite/source review is retained
in `ptsvx-prerequisite-01`, with no implementation credit yet.
