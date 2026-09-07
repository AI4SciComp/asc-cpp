# Program decisions

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
