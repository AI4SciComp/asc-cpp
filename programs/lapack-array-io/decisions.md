# Program decisions

## Resumed V8 identity audit, 2026-09-08

The preserved integration HEAD remains `b1b78d7`; all product files match
frozen V8 tree `37be13252b938e8761842ca6060d31bfb03cf9b6`. Read-only remote
inspection still finds `main`/`develop` at `46412183` and the feature head at
`b1b78d7`. No branch, index or existing implementation was reset or replaced.
Recovery diffs, worktrees and source identities are retained externally in
`resume-20260908-01/recovery.json`. Provider-prefix verification rechecks the
actual installed files against both original attestation identities.

Both live Google guides were retrieved again at 2026-09-07 18:08 UTC
(2026-09-08 local). C++ HTML SHA-256 is
`f681e8c1b71ed5f2420555a28b7e7120f46914cfa126e9d8ec5e6e9851512caf`;
Python HTML SHA-256 is
`9b02fa0d1aa05bfc8a4b5a95d3594665124f7a0414c82a69359f4a0b2f65e1c0`.
D003's compatibility exceptions remain scoped and unchanged.

Runbook section 2.5 supplies the parallel-owner direction for independent
Sylvester, indefinite-expert and SVD least-squares reviews. Review evidence
is separate from integrator execution. Expert acceptance gaps receive
additive tests; existing assertions and frozen failures remain preserved.

## V7 registration and exact foreign primitive check

Band12, classic indefinite18 and rank8 are integrated as distinct required
reference routes. All remain in-progress. Additive final-column permutation
APIs preserve prior factor enum values and LU behavior; no unpivoted-QR or
generic LDL provenance is fabricated. The root reviewed complete frozen slice
code/tests and added independent public consumers before combined verification.

The initial integrated test registration omitted the existing allocator-audit
support TU and linker wrappers for band/indefinite tests. Candidate01's failed
links are retained; candidate02 restores those dependencies without altering
test bodies. An expanded private-header strict lane also found the exact GNU
ILP64 primitive assertion uses `long`. Retain this compiler-emitted type check
with a documented, single-line `google-runtime-int` exception at the private
interoperability boundary. Do not replace it with a width-only check, weaken
ABI probes, or expose primitive foreign integers in supported headers. This
source amendment requires its own exact-identity strict/integration records;
the earlier owner freeze remains immutable.

While integration runs, the root's isolated `feature/lapack-p08-sylvester`
worktree begins the dependency-satisfied P08 ordinary Sylvester slice using
existing P01/P04 provider and workspace contracts. It owns only new optional
Sylvester header/adapter/count/test files and its review. No P07 or remaining
P04–P09 requirement is removed, no shared manifest is edited from that
worktree, and no new operation is credited before implementation and tests.

## V6 integration and reflector count correction

Root imported the separately frozen Cholesky-expert20 and least-squares12
actual source routes, then added public-only multi-scalar/multi-layout consumers.
All32 rows remain in_progress: declarations and owner diagnostics alone do not
establish complete capability. Optional headers, sources, component ownership,
installed profile and manifest consumers are updated together as v6; native
scope and the2113 denominator remain unchanged.

A source audit found the terminal strided BLAS cursor gap in existing QR and
new least squares. The correction binds effective foreign strides and actual
source branches, not unused ASC strides. The frozen old least-squares helper
fails an unchanged pure query regression; corrected arithmetic and count tests
pass. Fresh full-source verification remains mandatory. See
[the cursor review](reflector-cursor-review.md). No provider patch, fallback,
algorithm reassociation or weakening of numerical tests is authorized by this
implementation decision. Owner/license approval remains pending.

## D001: preserve the dirty release worktree

Observed original HEAD `31587a41ab8c949101480910df59266f1c6384fd` on
`release/0.9.0`, 318 modified tracked files plus untracked `CPPLINT.cfg` and
the supplied runbook. Tracked binary diff SHA-256:
`a9fd9651f9c3dc3050bc3c868852e92e9d55932a6e774e5ed288f3469d1f8657`.
No user changes are incorporated or discarded. Feature development follows
CONTRIBUTING.md's `develop` base. Fetched `main` and `develop` both identify
`46412183b2ae86101b2361c52376a8db8efff264`, matching the runbook observation.
The two additional release commits and uncommitted release edits remain outside
the program baseline; this is an explicit baseline delta, not a rollback.

## D002: authority and provenance

The user authorized the runbook's bounded six-module development direction,
new Dense-owned optional provider, native kernels and array formats. This is
architecture direction approval; it does not assert external reviewer or
license/redistribution approval. ADR 0017 requires reviewer approval for
source/data adaptation; the provenance review requires owner acceptance of a
new notice inventory. Prepare dependencies externally and write ASC code
independently. Record a concrete redistribution decision when material exists.

## D003: compatibility and style

Preserve C++20, public filesystem-based File APIs, flat `asc`, explicit memory
and execution contexts, and the ExpressionAdapter customization contracts.
These are narrow repository compatibility rules when applying the live Google
C++ and Python guides, retrieved 2026-09-07. Preserve existing BLAS enum values,
Random engine/state/sampling semantics and the six-module dependency graph.
No release version/SONAME or historical evidence is changed by this program.

## D004: explicit provider overloads

Freeze explicit native and Dense-provider overloads using typed ASC arguments.
The optional facet owns foreign types and interoperability. Base native calls
cannot discover or fall through to a provider; `ASC::cpp` stays provider-free.
Freeze concrete descriptor and workspace signatures during P01 before bindings
are generated. Unsupported native capability stays distinct from a required
reference-provider gap.

## D005: keep program records out of release documentation

The existing documentation-consistency and release-state checks forbid a
`docs/development` tree. The runbook specifies that path only as a suggested
default. Durable development records therefore live at
`programs/lapack-array-io`; `.gitattributes` excludes this directory from source
archives. The original supplied runbook remains untouched, and its durable copy
retains its recorded hash. Normative contracts and the capability evidence
schema remain under `docs/contracts`, including files required by archive
tests. No failing test is disabled and no historical release claim is changed.

## D006: evidence is configuration-specific

The frozen provider-free baseline and complex-scalar snapshots have independent
external evidence indexes. The externally prepared LP64 reference library's
111 passing upstream CTests verify that dependency configuration, not any ASC
wrapper. All 2113 required numerical rows remain unimplemented in ASC until
their checked bindings and actual ASC execution evidence exist. Optional XBLAS
and true ILP64 remain required gates, regardless of the first LP64 success.

## D007: successful requests are distinct from live allocations

`max_allocations` counts every successful MemoryResource request, including
zero-byte requests made by the existing owner factories. Actual null empty
storage does not imply a live allocation or a required Deallocate call.
Sparse test-resource bookkeeping must distinguish these quantities rather
than changing Core/owner behavior to satisfy an empty-case assertion. Resource
failure, total-request, temporary/final-storage and leak checks remain required.

## D008: strict input budgets and unseekable EOF

Whole-file validation needs one spare byte of input budget for its EOF probe:
ByteSource provides no nonconsuming peek, so reading at an exhausted cap could
consume a forbidden trailing byte. A framed read may finish at its exact cap.
The text/binary contracts state this explicitly and boundary tests cover it;
the wire grammar and checksums do not change. File conveniences require
explicit truncation intent and checked close, not a durability promise.

## D009: bound newly introduced stream diagnostics

The new array display and persistence boundaries retain a source/sink error's
stable ErrorCode and native integer code, but do not copy its unbounded owning
message/provider strings. Bounded phase and byte/value progress remain in the
array reports. This policy is needed to prevent hidden heap allocations when
a custom source/sink moves a prebuilt long failure into a borrowed-stream call.
It is not a change to existing Core Status, Result, WriteAll or File behavior.
Implementation and adversarial allocation tests are required before closure.

Resource-factory diagnostics follow a distinct preserving route: unsupported
internal Result access moves an already-failed Status through Buffer and Dense/
Sparse owner factories. Public Status/Result accessors and error contents remain
unchanged. Tests move prebuilt long message/provider strings into allocation
failures, including zero-byte requests, and check both retained diagnostics and
zero hidden allocations. The first integrated Dense test exposed a second copy
in the initialized factory; its correction passed the three-test resource
subset. Final frozen-tree regression remains required. This does not claim
every existing Core validation diagnostic is allocation-free.

## D010: preserve the full denominator through partial implementations

Eight native LU contracts now have actual implementations and scoped passing
snapshot evidence. They remain implemented-unverified in the strict ledger
until contract-bound mode/evidence records close. The corresponding reference
rows remain in-progress while row-major and final evidence integration are pending.
No required upstream row is excluded to improve these counts. A development
subset must reject a requested incomplete full profile, not advertise it.

## D011: bound hostile size diagnostics without changing Core contracts

The new array parser paths use internal nonnegative AddSize/MultiplySize/
CastSize helpers that return code-only overflow. Calling public Core Checked*
and then dropping its message is too late to prevent its error allocation.
Dense owner reads also preflight the existing layout's actual forward/reverse
stride representability, including empty shapes. Core Extents already handles
zero extents in a full preliminary scan and is not changed. Sparse preserves
valid large empty COO shapes rather than imposing a false prefix-product rule.
Small real hostile headers, not invalid huge backing spans, exercise failures.

## D012: bind installed metadata and distinguish evidence terminology

A trusted generated ASC config now binds the exact provider metadata SHA256.
This detects stale/mixed metadata; it does not authenticate a package against
an attacker who changes both config and metadata. Negative required lookups
must fail at lookup and have a positive required control. An unrelated later
test failure is not proof that the lookup rejected the provider.

Coverage summaries distinguish not-started, in-progress, blocked and
implemented-unverified routes. Unverified does not mean no tests ran: scoped
passing artifacts can exist without complete mode/evidence closure. Neither
renaming counters nor recording those artifacts changes the 2113-row denominator
or awards verified coverage.

## D013: layout scratch uses the ASC size domain

`kLayoutConversion` counts describe caller-owned packing, not foreign LWORK.
Only that role is exempt from the provider integer maximum. Logical sizes,
checked byte products, alignment, capacities and disjointness remain checked;
all foreign-work roles retain their ABI bounds. A pure-capacity regression
above INT32_MAX failed against the prior implementation and now passes without
fabricating a huge live backing allocation. Each wrapper still checks its
own provider dimensions and intermediate formulas before foreign entry.

## D014: bind foreign leading dimensions and ASC strides separately

Row-major LU packing preserves mathematical orientation and passes a packed
leading dimension of max(1,rows). Query identities bind that ABI-bounded value
as a dimension and the original ASC stride as an option. This preserves plan
freshness without incorrectly narrowing an unused source stride. A real empty
LP64 descriptor with stride INT32_MAX+1 exposed the prior rejection; the same
test passes after this correction. Oversized actual foreign dimensions remain
errors. All simultaneous region products/totals are checked before dispatch.

Only documented outputs are unpacked after validated foreign INFO/pivots.
Singular factors remain inspectable; singular GESV leaves B unchanged, singular
GETRI leaves its factors unchanged, and provider defects do not publish packed
undefined results. The scalar, layout and integer regions stay disjoint.

## D015: distinguish finite-input estimate fidelity from mathematical success

GECON/GERFS/GESVX preserve exact source semantics, raw INFO and documented
diagnostics. Unscaled tiny finite systems expose independently reproduced
nonfinite FERR and zero RCOND even when the scale-invariant mathematical
quantities are finite. Those are retained unmet mathematical gates, not removed
fixtures or successes awarded to faithful error reports. Explicit GESVX FACT=E
can change the numerical path and succeeds on the recorded scalar fixture;
ASC never chooses it implicitly for FACT=N/F. No local provider patch, new
source identity, license/notice approval or full-family completion is implied.

## D016: queries also require exact foreign-intermediate bounds

A valid caller descriptor and ABI-sized N are not sufficient for a safe foreign
workspace query. Bounds include the pinned integer formula, floating conversion
and any intermediate return-helper conversion before entry. The independently
computed raw preferred count remains a lower bound even when the returned
floating value rounds downward. Extreme tests operate on private pure count
helpers; they must not forge huge nonempty backing allocations. Source-conditioned
vector and blocked-loop terminal arithmetic receives the same review.

## D017: test refinement against conditioning and the stored system

Retain failed fixed-forward-error assertions and reproduce questionable results
with both an independent mathematical oracle and separately labeled direct
provider calls. The advanced installed LU consumer now checks exact dyadic
stored-system right-hand sides, widened backward residuals, finite diagnostics
and actual forward errors against FERR. A second well-conditioned fixture
retains the original tight forward assertion. Neither increasing a tolerance
to match one result nor dropping ill-conditioned fixtures is an acceptable fix.
See [lu-v4-integration-review.md](lu-v4-integration-review.md) for the concrete
condition8193 counterexample and retained raw evidence.

## D018: neutral workspace validity is not provider placement admission

Each explicitly selected provider context must admit every nonempty supplied
workspace region as well as operands before mutation or foreign execution.
The generic workspace validator keeps its broader documented policy. New
helper context tests exposed a missing adapter-specific check; the correction
and its regression must be propagated by an audited follow-up, not inferred
from generic host/pinned-host validation.

## D019: native Q conveniences do not replace exact reference experts

Native GEQRF provides separately named whole-factor Q formation/application.
Only its four exact factorization rows receive native implementation state.
Reference ORGQR/UNGQR/ORMQR/UNMQR are distinct actual foreign routines with
raw partial-reflector inputs, complete valid shape/side/transpose contracts,
actual workspace queries and independent mathematical tests. Their typed
exports and scoped evidence add partial reference rows, never inferred native
coverage or full-profile closure. Installed consumers must actually execute
the newly added routines through public headers and relocated component
metadata; compile-only declarations and older product results are not enough.

## D020: preserve actual zero-row provider execution when it has effects

GEQP3 with M=0 and N>0 still rearranges fixed-column metadata and can enter
nested QR routines. Local permutation emulation is not accepted as actual
provider capability. Use caller-owned N live scalar staging entries with
effective LDA=1; retain the original ASC stride in the plan key. Keep the raw
outer query result one distinct from safe execution minimum N, and check nested
query arithmetic/rounding and N+1 before entry. N=0 alone has a local return.
The isolated pre-correction candidate remains immutable and unregistered;
fresh actual-call evidence is required. This is an integration design decision,
not repository-owner or license approval.

## D021: preserve a documented numerical negative INFO separately

The exact four GESDD sources return INFO=-4 when their pre-scaling max norm
is NaN. This is a documented numerical-input outcome, not a generic checked
adapter argument defect. Root source/probe review approves an actual NaN-only
foreign call after ordinary structural/workspace validation, preserving raw
INFO=-4 as kNumerical/kPartialResult with unchanged numerical destinations
and no translated native_argument. Do not fabricate the diagnostic locally
or copy output-only staged S/U/VT on this return. Workspace scratch effects
are distinct from numerical destination rollback. Guarded all-job/layout and
real/imaginary-NaN tests are required before integration.

Actual Inf probes have unsafe non-return/STOP paths. Reject any meaningful
Inf component before mutation for both standard SVD drivers; GESVD rejects
NaNs as well. Empty and query paths retain their exact source behavior and
must not read nonexistent or output-only input. Finite complex components
with an overflowing source magnitude still require an explicit probe and
safe admission analysis. This is a checked-interface implementation decision,
not an upstream patch, owner approval, or closure of any required mode.

## D022: keep native pivot work in native INTEGER units

The initial GT slice direction incorrectly placed provider-width pivot entries
in kPivotConversion. Actual frozen ValidateAbiRegion requires that role's
entry size to be sizeof(index_t); the first LP64 execution correctly rejected
the incompatible plan with kConfiguration. Preserve that failed attempt and
do not alter the shared foundation to accommodate it.

The local correction follows existing LU experts: kInteger contains n live
native pivot integers, followed where needed by a separately bounded n-entry
native estimator segment. Validate the total and guard their live boundary.
kPivotConversion remains ASC64 conversion staging when actually required;
it is not reinterpreted as packed LP64 integers or silently rounded byte
slots. This corrects root's earlier implementation direction and preserves
the established public workspace and object-lifetime contracts.
