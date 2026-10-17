# Checked selected matrix scaling

P09.required.lascl adds SLASCL, DLASCL, CLASCL and ZLASCL through24 public
query/execute declarations. These are four catalogue routines, with13 storage
modes each: G/L/U/H full matrices in both layouts, B/Q symmetric or Hermitian
band storage in both layouts, and Z in the existing column-major GBTRF storage.
The four rows are registered callable-unverified. Integrated build, safety and frozen-source package checks have passed on the
scoped GNU/Linux static/shared actual-ABI profiles. No
verified Reference credit or full-program acceptance is claimed.

## Reviewed source and API

The four pinned Reference-LAPACK3.12.1 definitions and actual installed
declarations were checked in `lascl-prerequisite-01`. Both prepared providers
export the required routines. The adapter uses the pinned LAPACK declarations
directly, as existing selected-copy adapters do: the LAPACKE row interface
allocates/transposes storage and does not provide ASC's selected-cell packing
contract. Four compile-time interception signature checks include the real
integer width and character length. No upstream algorithm is copied, modified
or distributed. The dependency and floating-point environment are unchanged.

Queries inspect metadata only. Plans bind scalar, provider, storage pattern,
shape, bandwidths and strides. CFROM/CTO are execution inputs, so a matching
plan can be reused with different factors. CFROM must be nonzero and non-NaN;
CTO must be non-NaN. Infinities are admitted by the pinned API. Empty shapes
and equal finite factors complete locally after metadata, scale and workspace
validation, with no numeric access or native INFO. Both infinite factors do
not take the local identity path.

Column layout is direct and requires no scratch. Row layout reserves explicit
live scalar packing: M*N entries for full storage, (KD+1)*N for band storage.
Only selected entries are packed and published. Imaginary diagonal components
are ordinary stored input values, with no normalization or conjugation.
GBTRF reserved factor-fill rows, unused band corners, complement and padding
are unchanged. There is no band densification, implicit allocation or transfer.
ASC makes two selected-entry passes on row paths; the provider may make
multiple scaling passes depending on exponent range. Column paths do not
scan numeric input before the foreign call.

The pure count helper protects actual native loop terminals, bandwidth
expressions, leading dimensions and aggregate workspace bytes. Nonempty M/N
and stored row counts are strictly below the signed ABI maximum. General-band
KL+KU+1+M is checked before native entry. Packed and direct offsets stay within
validated containing arrays. Boundary tests use pure integers rather than
fabricated large objects. Reachable matrix spans, active workspace, report
and plan metadata must satisfy the existing disjointness contract.

Full-width INFO starts at the actual signed minimum. Zero means ordinary
provider completion, not a finiteness certificate. Missing, partial-width or
nonzero INFO is a provider defect. Direct column partial writes remain visible
with unusable validity; row publication is withheld and caller values remain
unchanged. Preflight rejection preserves numeric buffers. Immutable matching
plans may be reused; concurrent calls require independent mutable operands,
workspace, reports and appropriate provider context state.

## Numerical and observation evidence

The finite matrix is unchanged from initial candidate execution: four scalars,
13 storage modes, M in0/1/2/5, N in0/1/3/5 where valid, and eight ordinary,
zero, identity, subnormal and extreme scale pairs, each with changed-sign plan
reuse. The independent long-double oracle computes the ratio without working-
precision overflow and checks every selected component within8epsilon times
the expected magnitude plus one minimum subnormal. The test requires enough
long-double exponent range at compile time; it does not skip an unsupported
oracle. This oracle currently qualifies the admitted GNU/Linux profiles only.
Four real-provider threads use independent contexts and mutable state. Serial
calls audit allocation; the global test allocation counter is excluded from
threaded calls.

The separate exceptional test makes108 adapter/direct comparisons per scalar,
covering infinite scale branches, signed zero, infinity and quiet NaN matrix
values in both layouts. These are fidelity checks outside the finite-final-
result property. For example, CFROM=-1, CTO=+Inf and A=1 produce +Inf and INFO0
in the pinned provider: its CTO fixed-point branch chooses CTO as multiplier.
ASC exposes that result. This is not claimed to be the signed limiting value
or counted as finite mathematical acceptance; no diagnostic is clamped and no
finite test is converted to a provider-fidelity pass.

Fault tests exercise all13 storage modes and four scalars with real-provider,
missing INFO, positive/negative INFO and partial-width INFO controls. Queries,
stale plans, invalid scales, invalid enums, one-byte-short workspace and
aliasing are checked against their metadata-only/no-mutation contracts.

The Linux observer protects initialized aligned containing arrays with
PROT_NONE. Query, stale, invalid-factor, exact short-workspace and local paths
run while numeric and scratch pages are inaccessible. A direct-column entry
interception restores access at the actual foreign boundary. Row input packing
is a legitimate pre-entry read and has an explicit faulting control. Unused
cells remain protected through the actual provider call. Each run requires232
intentional read controls,24 legitimate input-read controls,52 active calls,
124 local completions and56 unused-cell cases. Faulting children disable core
dumping only to avoid the existing WSL crash collector delay; no fault handler
resumes an invalid access. Valid object lifetimes and bounds are retained.
This observes ASC access to protected regions and the actual entry boundary;
it does not instrument the Fortran provider or runtime. Final bytes alone are
not used as absence-of-read evidence.

## Execution checkpoint

Raw records are under `master-continuation-20260910-01/`. Both
`lascl-branch-{lp64,ilp64}-05` pass14/14 candidate tests. Earlier initial,
fault-only, observation and style attempts remain preserved. The reviewed
integration snapshot is `lascl-integration-inputs-06`; it includes mechanical
strict-check fixes, precise reserved-fill documentation and an invalid-enum
regression. Only12 original source/test/example files were imported; the
external compilation driver was excluded.

The maintained `examples/matrix_scale` uses public headers and
`ASC::dense_lapack` only. It scales subnormal integer coefficients to ordinary
values without forming an overflowing reciprocal, exercises all scalars and
seven storage patterns, reuses plans with changed factors and checks guards.
Its integrated execution passes in all six static profiles and both shared
Release/Debug profiles. All four actual-ABI/static/shared installed and
relocated package executions pass. No historical evidence path is present in the maintained consumer.

This is an implementation self-review, not owner/provenance approval.
Native20/array-I/O, robust PPSVX and original Reference mathematical failures
retain their separate identities and acceptance states. Wider admission and
normalized complete Reference evidence remain programme requirements.

Both `lascl-{debug,release,sanitizer}-{abi}-integrated-01` profiles pass14/14;
`lascl-tsan-{abi}-integrated-01` pass4/4 with real-provider concurrency. ASC and
test code are instrumented; provider and runtime code are not. Strict product,
exception and public-example checks pass in style04. The intentional invalid-
enum regression requires a localized analyzer annotation:99 is representable
in the enum's fixed uint8_t base but intentionally outside its API options.
The assertion remains active. Its final strict rerun is recorded separately.

`lascl-documentation-01` passes strict Doxygen, links, documentation consistency
and whitespace. `lascl-registration-01` preserves all three schema2 execution
records and both source identity catalogs, changing only their current index
hash under a complete baseline bridge. The annotation's test-file identity has
its own retained amendment02. Contracts, execution outcomes, selector allowlists
and native20 rows are unchanged. Counts are2113 required,40 callable-unverified,
342 partial,1731 not started and0 verified Reference;68 contracts are reviewed
and382 Reference rows are registered.

The exact integrated tree `b52fa952ccec39d08eebd8cd53f3f412bb48494e` is frozen
in `lascl-frozen-product-01`, archive SHA256
`2a4d55bc26f84f5920a20c34cdb93bceef894f89be30323d366cc5c45af6cd43`.
All four `lascl-fresh-{static,shared}-{abi}-01` profiles pass21/21 LASCL,
header, architecture and configuration checks, plus the full relocated package
1/1. Static package times are262.376/263.211seconds; shared233.603/233.794.
The package executes the maintained public example from copied source, checks
its exact shared provider/runtime closure and rejects dependency bundling.
Existing28 provider isolation/control cases and provider-free component checks
remain active. `lascl-package-audit-01` binds consumer binaries and raw logs.

Both shared Debug and ASC-only sanitizer profiles pass14/14; shared TSan
passes4/4 per actual ABI. `lascl-shared-safety-audit-01` confirms no skips or
sanitizer/race diagnostics. Provider/runtime code remains uninstrumented. `lascl-executed-input-comparison-01`
compares all eight earlier static profile inputs: only the three analyzer
comment lines differ. The C++ statements, assertions, fixtures and tolerances
are identical. `lascl-root-to-frozen-comparison-01` matches1135 build/API/test/
example/ABI files. `lascl-native-subset-reuse-01` preserves the20 native rows,
original execution record and selected native/I/O source identities; it is not
whole-tree hosted certification. Final strict checks, formatting and scoped
coverage/backlog checks pass in style05, format-final01 and record-checks02.

The local engineering slice is complete within that scope. Fresh hosted checks
for its eventual feature commit remain separate from the passing CI/CodeQL and
118/123-per-profile family records for129bba18 (the five retained PT/SGEDMDQ
mathematical failures). No later whole-tree hosted pass is inferred. Source and
signature/padding/observation requirements, all ordinary finite assertions,
platform guards and the2113 denominator remain unchanged. The independent
GBRFS guarded-diagnostic oracle correction is recorded externally and is not
part of this LASCL implementation or its mathematical acceptance.

## Hosted inventory correction

Revision `dbc924d7aaada7f51dc1359739f7b54f981af6af` has a fresh CodeQL pass
(run34567090338), but CI34567090343 failed. The GCC11 Release log identifies
five inventory checks out of294: the new optional LASCL header was omitted
from the independent Dense dependency and hardening header lists. The reviewed
correction adds exactly that header, changes the explicit135-header oracle,
and retains both exact-equality checks and the unchanged provider-free list.
`lascl-header-oracle-fix-01` preserves original files and amendment identities.
`lascl-header-fix-provider-free-01` configures and builds a fresh GNU11 Release
provider-free checkout profile and passes11/11 actual configured checks and
fixtures, including build/copy/install/relocation, zero skips. No product
source or provider change was needed. The original hosted failure is retained;
new hosted checks must confirm the corrected revision.
