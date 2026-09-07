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
