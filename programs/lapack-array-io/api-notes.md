# LAPACK and array API notes

The complex Dense owner foundation, LAPACK descriptors/workspace/reports and
bounded value printing are committed. Production ASC text/binary readers and
writers plus native and reference LU are implemented in the integration slice.
Actual verification identities and remaining gates are recorded separately in
[state.json](state.json) and [verification-summary.md](verification-summary.md).

Provider interfaces use explicit Dense-owned overloads, typed arguments, ASC
64-bit dimensions and caller-owned conversion/workspace storage. Reports remain
accessible after numerical failure. Base Dense has no foreign symbol dependency.
`ASC::dense_lapack` is optional, absent from `ASC::cpp` and common umbrellas.
The current checked provider profile remains incomplete but supports both
layouts for its registered LU and equilibration routes;
neither its upstream dependency tests nor its ABI probes imply full coverage.

Storage-neutral codecs belong to Core; Dense and Sparse each own their array
formats. Printing, ASC text, ASC binary and Matrix Market are distinct APIs.
No dense/sparse sibling dependency or implicit device transfer is permitted.

Native `Getrf`/`Getrs` operate on all four numerical scalars, both matrix
layouts and N/T/C reusable solves. The raw factorization report and a successful
`LapackLuFactorView` are distinct: singular raw factors cannot be promoted to a
successful solve object. The expert reference-only expansion is kept in separate
files and gains no ledger credit before implementation and actual tests.

Prepared Dense/Sparse readers borrow the source, metadata and bounded scratch;
they retain their parsing position. Text and binary entry points are explicitly
named. Owner loads accept a resource and limits, while Dense `Into` and Sparse
values-only `Into` use explicit disjoint staging and commit only after complete
validation. Sparse structure comparison is exact, never checksum-only. Save
wrappers require explicit truncation intent and checked close; examples run
against installed components. P10 now implements native Dense array and Sparse
coordinate Matrix Market interchange; complete mode/platform acceptance remains
open. See `matrix-market-review.md` and the current `stabilization-review.md`.

New array-stream errors retain ErrorCode/native code without copying arbitrary
source/sink strings. Internal failed-Result moves preserve long owner-resource
diagnostics without changing public Status/Result accessors. These bounded
allocation guarantees are tested by prepared long failures, not code-only probes.

Per-routine contracts must be read, frozen and tested before generating bindings.
See the complete runbook, not these notes, for all P00–P11 requirements.
