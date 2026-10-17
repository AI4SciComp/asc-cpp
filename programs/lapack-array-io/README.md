# LAPACK and array I/O implementation program

Program `ASC-CPP-LAPACK-IO` follows the complete [runbook](runbook.md).
This directory records development, not release or capability claims.

Read [state.json](state.json), [blockers](blockers.md), the current Git diff,
and the exact next runbook package before resuming. Package dependencies and
all P00–P11 requirements remain mandatory. A generated declaration or upstream
routine record is not implemented or verified ASC capability.

The integration branch is now local `develop`, using the existing sibling
`asc-cpp-lapack-array-io` worktree. The feature history starts at
`46412183b2ae86101b2361c52376a8db8efff264`; its last numerical implementation
checkpoint is `df91a449037b5f16945f6aa6bf3602c3481970f7`. The original `asc-cpp`
release worktree and its user changes are preserved. Local logs, dependencies,
builds and installed prefixes live under sibling `asc-cpp-evidence/lapack-array-io`.

Start with the [2026-10-17 integration checkpoint](
integration-checkpoint-2026-10-17.md). It records the maintainer-authorized local
integration, the byte-preserved provider-metadata draft, and the current
continuation order. Earlier next-task prose and merge-authorization statements
describe their dated checkpoints. The complete numerical and release
requirements remain in force.

See [baseline](baseline.md), [decisions](decisions.md), [API notes](api-notes.md)
and [verification summary](verification-summary.md). Actual source identities,
manifest hashes and executed test counts are recorded as they become available.

The [2026-09-10 subset checkpoint](stabilization-review.md) records
completed native/array-I/O technical acceptance and passing provider-free
hosted CI; owner/provenance acceptance and the full Reference program remain
open. Read its current section before the historical slices.
All 2,113 Reference routines remain required. Local development integration does
not close full-program acceptance or authorize tagging and publication.
