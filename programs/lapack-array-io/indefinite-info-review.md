# Classic indefinite INFO publication correction

Status: bounded correction implemented and locally verified. The eighteen
S/D/C/Z SYTRF/SYTF2/SYTRS and C/Z HETRF/HETF2/HETRS rows retain their
incomplete family status. This continues their existing integrated APIs,
backend, exported symbols and installed example.

Both shared foreign call sites initialized INFO to zero. A provider that
left that argument unwritten appeared successful and allowed publication
of packed factors, public pivots or a packed RHS. A 32-bit zero write also
appeared successful in the actual little-endian ILP64 profile. The correction
initializes the slots to the full provider-width minimum sentinel, so the
existing interpreter rejects those returns as provider defects with unusable
output and retains the signed INFO value. Valid native INFO, numerical
arithmetic, UPLO, symmetry, Hermitian diagonal handling and workspace sizes
are unchanged. Empty operations still make no foreign call and have no INFO.

The new wrappers execute all eighteen actual native routines with their
original operands and full workspace, isolating the fault to INFO publication.
The real native return must remain zero. Omitted writes are rejected in both
ABIs; a 32-bit zero write is a valid LP64 control and an ILP64 defect. Eighteen
compile-time checks compare complete wrapper signatures to the pinned header
or the existing compiler-derived private declarations, including CHARACTER
length and actual integer and complex argument types.

All six scalar/symmetry classes exercise both triangles, blocked/unblocked
factorizations, factor layouts, independent RHS layouts and empty operations.
Queries and executions check allocations, scratch guards and call counts.
Packed output and public pivots remain unpublished on a provider defect;
direct foreign writes cannot be rolled back. Solves preserve borrowed factors
and pivots. The previous numerical, preflight, provider-fault, arithmetic-bound
and ABI tests remain required and unchanged.

## Evidence and limits

Raw evidence is retained under
`master-continuation-20260910-01/continuation-20260912-01`, relative to the
existing external programme evidence root. No worktree or provider replacement
was created. Before correction, `indefinite-info-static-release-*-before`
records six failed processes and 560 failed assertions in LP64, and six failed
processes and 1,120 failed assertions in ILP64, with zero skips. The first
corrected six-process runs pass in both ABIs.

`indefinite-info-final-audit-02/audit.json` binds the final source and actual
provider identities to twelve static/shared Debug/Release/ASan+UBSan profiles,
each passing 23/23, and four relocated installed consumers, each passing 1/1.
All have zero skips. Six strict translation-unit checks, formatting, public
header modes, coverage, backlog, documentation consistency, links and the
actual maintained CI selector audit pass. Doxygen reports 140 headers, 2,422
members and zero warnings. Sanitizers cover ASC and its tests; the pinned
foreign provider remains uninstrumented. No TSan result is claimed here.

The public-header checksum and 66 affected artifact indexes advance atomically
through `docs/contracts/lapack-indefinite-info-evidence-extension.json`: eighteen
classic rows, 24 dependent expert rows and 24 PB/GB registration hashes.
Historical execution records, route states and native20 remain unchanged.
The exact eighteen pinned source identities are retained in
`indefinite-recovery-01/upstream-rows.json`.

Initial strict checks found one missing direct include and two unused test
includes. Their failures remain preserved. Successful early profiles and
consumers are also retained; the final audit uses the refreshed profiles with
exact final source hashes. Its first attempt rejected a stale shared Debug
test-source hash, prompting the two final shared Debug checks. No assertion,
test case, numerical tolerance or checker was weakened.

This correction does not close normalized routine/mode contracts, extreme
range acceptance, concurrent reuse, broader provider/platform admission or
full execution records. The [original source review](indefinite-review.md)
retains its bounded numerical and ABI distinctions. Classic indefinite
contract and numerical completion is the next independent task; the recorded
PTTRS, GB and diagnostic-family mathematical blockers remain open.

The subsequent [contract and range continuation](indefinite-continuation-review.md)
reuses this correction's completed evidence and records the remaining
mathematical failures separately.
