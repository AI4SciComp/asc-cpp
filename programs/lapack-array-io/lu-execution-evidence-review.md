# Basic LU execution identity and partial fixture records

The maintained schema2 ledger now binds the existing S/D/C/Z GETRF, GETRF2,
GETF2, GETRS, GETRI and GESV delivery to completed execution from a separate
source expectation. It preserves the native20 execution identity. No numerical
source, fixture, tolerance, public API, provider or route status changes.

The bounded Release rerun passes all27 configured processes in each actual ABI,
with zero skips: public consumer237, normal-return292, reference LU293–296,
INFO297–300, expert INFO301–304, pivots305–308, layouts309–312, expert LU313–316
and counts341. These are the actual configured listings retained in
`master-continuation-20260910-01/lu-execution-binding-{lp64,ilp64}-01`, not
permanent test numbering. The maintained compact
[LP64](../../docs/contracts/lapack-lu-release-lp64-executed.json) and
[ILP64](../../docs/contracts/lapack-lu-release-ilp64-executed.json) artifacts
retain every selected command, configured ID, result and raw-input hash.

Execution source is commit `c01abddf739551d8512520f71711491d314b2c77` plus the
retained patch, exact tree `777aad8584547809aaa3fe87e203921ad635959d`, patch
SHA256 `5c7d5c111058736f28d3faeeb0e54b0afed80d69d5645b23f4b272f42958905d`.
`execution-ledger-source-01` preserves the patch and identities. It adds only
the first evidence-validator/tests/contract edits to that commit; C++ inputs
are unchanged. `lu-normalization-review-01` compares the selected LU inputs
against the actual immutable tree. Later normalization/tooling/record changes
are not relabeled as that whole-tree execution. Earlier six-profile, TSan and
relocated install evidence remains bound to its own inputs in the
[delivery review](general-lu-delivery-review.md).

The actual GNU11.4 Release/static providers retain Reference-LAPACK3.12.1
commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`.
LP64 attestation is
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`;
global ILP64 attestation is
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
The latter uses the prepared Fortran integer8 ABI. The records retain actual
archive/header checksums, CMake and compile-command hashes. This execution
does not instrument provider code.

Each ABI has184 explicitly partial assignments covering96 mode IDs across24
routine entries. They map the inspected `examples/general_lu/main.cc` loops
to ordinary reconstruction/pivots, solution/residual, immutable factor reuse
and inverse products. Each assignment records its precise fixture and
`class_complete:false`. The single bounded dyadic fixture does not close every
required class. Guard checks do not claim all aliasing cases, preferred inverse
scratch does not claim minimum scratch, and root public-example execution does
not claim installation. Fault-wrapper test processes retain their actual pass
results but receive no real numerical assignment. Mathematical failures in
other LU/expert, PT, PPSVX and GEDMDQ records remain required genuine failures.

The maintained normalizer and migration generator reject missing/duplicate or
zero selections, preserve failure and skip outcomes, reject metadata overrides
of execution, and bind each route to explicit source expectations. The migration
changes selectors in36 rows (20 native and24 Reference routes with8 shared
rows); it preserves contracts, all old covered cases and source identities.
The only updated field within the old identity is the current mapping-index
hash, with the exact prior expectation and record hash in the bridge. That
index extension is not a claim that a later index existed at execution time.

`execution-ledger-tests-05` passes111 tooling tests normally and under Python
optimization; `execution-migration-04` is the final reviewed migration output.
The unchanged denominator is2113:36 callable-unverified,342 in progress,
1735 not started, zero verified Reference; native20 remains verified.
Full Reference class normalization and required platform admission remain
actionable programme work. This closes the distinct-source ledger mechanism
and initial honest LU observations, not P04 or P11 acceptance.
