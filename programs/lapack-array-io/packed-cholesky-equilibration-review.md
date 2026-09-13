# Ordinary packed positive-definite equilibration candidate

The ten candidate07 files implement actual S/D/C/Z PPEQU and explicit
metadata-only queries under [the frozen admission](packed-cholesky-equilibration-admission-review.md).
They remain external and unregistered until PPSV v24 is committed. Both layouts
use the original packed storage and zero workspace: only diagonal positions
matter, and row storage uses the opposite native UPLO. A remains unchanged,
including ignored off-diagonal and complex imaginary diagonal components.
No implicit scaling, allocation, densification, fallback or native credit occurs.

Audit `ba536fec1735b00e007fed4a2500f414353f172d4902506a94e797188d1cac7f`
and its successful completion-record-02 seal bind 136 original records. Final
six GNU Debug/Release and ASC-only Clang19 sanitizer configurations each pass
5/5 tests, zero skips and no sanitizer diagnostics. They compile 60 fresh
adapter/test/support objects, explicitly reusing and rehashing the earlier 65
primary TUs per lane. The actual source has 1105 files: v24's 1095-file baseline
plus ten owned additions; inherited archive metadata describes that baseline,
not a new candidate archive. Foreign archives remain unsanitized. Fourteen
exact-input strict checks, four isolated normal/no-exception headers and final
ten-file formatting pass. The strict records come from candidate05 plus the
corrected fault test in candidate06, whose ten files equal candidate07 exactly.

Each lane passes 5,296 public workflows and 112,176 checks across all four
scalars, N0/1/2/3/5, both triangles/layouts, seven even dyadic exponents through
minimum-normal and near-maximum scales, repeated plans and ignored complex
imaginary NaNs. Independently specified squared-integer diagonals give known
scales/statistics and an independent S(i)^2*A(i,i)=1 check. Widened normalized
errors use scalar epsilon without an additive denominator. First/last zero,
negative and multiple nonpositive diagonals check raw S, complete AMAX and
unchanged SCOND. Real NaN/infinity cases check the documented late output
warning. Every A byte and all S/statistic guards are checked.

The 640 fault/reuse/alias/allocation profiles and 3,345 checks per lane cover
complete native argument pointers, order/effective UPLO/hidden length, full-width
negative/impossible/missing/partial INFO, synthetic positive partial output and
three injected INFO0 output warnings. Rejections cover flags, scale shape and
stride, host placement, every plan identity component, capacity/alignment fields,
workspace/operand/metadata aliases, and real S/SCOND/AMAX overlap. Structural
failure preserves every numeric byte and resets or preserves report according
to the metadata-alias rule. Scoped C++ and wrapped static-libc counters observe
no allocation on queries and native calls; other complete observations remain
separate requirements. Synthetic faults do not replace real numerical cases.

Linux protected-memory tests pass 72 profiles per lane: 48 queries with the
entire A, S and scalar-output containing arrays inaccessible; 16 local empty
executions while A/S remain protected; 16 empty stale-plan rejections with all
numeric arrays protected; 16 native calls with an actual ignored off-diagonal
page inaccessible; and eight complex native calls whose imaginary diagonal
component lies alone on an inaccessible page. The last case uses one real
containing complex array crossing a page boundary; real components remain
accessible. Placement array new establishes C++20 lifetimes, full checked
backing spans are real, and zero observed C++ allocations plus all input/output
guards are checked. This is Linux evidence for these observations, not universal
read/allocation/concurrency/platform closure. Pure admission's 12,501 source
count/offset checks and four complete signatures also pass per lane.

## Retained failures and corrections

Candidate01's public test incorrectly demanded INFO0 for local empty calls,
causing 336 assertions per ABI. The preserved original-binary debugger log
shows N0, called_provider=false and absent native INFO as the frozen contract
requires. Candidate02 checks absence locally and exact INFO0 after native
entry. Its byte-preservation check now explicitly compares byte spans, using
the established test helper pattern, and removes one unused include. Both
original strict failures remain. Every runtime assertion and call remains;
production has not changed since the first candidate. Both Debug 3/3 pass.

Candidate03 accidentally includes the external failure target twice, so both
configurations fail before builds; its new fault test also has two strict
style failures for an unnamed unused parameter and nested conditional.
Candidate04 fixes the duplicate target and style, but its copied six-file
harness omits the dense-test include directory, so both builds and fault strict
checks fail. Candidate05 fixes the fault include and adds protected-memory
source; both builds/strict checks reveal one pointer argument missed during
the alias-fixture refactor. Candidate06 dereferences that argument and passes
both current fault strict checks; its builds reach the new memory TU and expose
its missing dense-test include directory. Candidate07 adds that external target
include, with all ten source files byte-identical to candidate06. All six final
5/5 runs pass. No failed configuration/build receives numerical execution credit.
The audit binds every historical object, log, exact source delta and command.

The first seal script retained a stale expected record count90 and fails;
its original record is preserved. Seal02 corrects only the expected count136,
rehashes every source/artifact and the failed seal's own manifests/logs, and
passes. The audit itself passed on its first execution. These 136 original
records plus both seals add 138 to the ledger, now 3,832. The admission14
records were already normalized and are not counted again.

The mapping remains 338 Reference in_progress/1,775 not_started, native20
implemented_unverified and zero fully verified. PPEQU4 remains unregistered.
Finish PPSV v24's same-limit isolated timeout rechecks, root audit/documents
and commit; then import these ten files using completion-record-02 and run
fresh root v25. Full routine/mode/concurrency/platform acceptance, all P00-P11,
70 ordinary mathematical failures and 130 missing required XBLAS remain open.


## Root registration

The ten audited candidate07 files are imported byte-for-byte after PPSV commit
324ff44. The successful completion-record-02 seal is bound; the failed first
seal remains preserved. Atomic optional source/header, profile v25,
installed-family and contract registrations add four in_progress routes:
342 Reference partial/1,771 not_started, native20 implemented_unverified and
zero fully verified. All 138 candidate records were already normalized.
Fresh 70-primary-TU, 124-header and 29-family verification is recorded below.
All routine/mode/concurrency/platform acceptance, P00-P11, 70 ordinary
mathematical failures and 130 missing required XBLAS remain required.


## Executed v25 integration

Audit `3ccd3c7db9a03613842c0445b59303e3123382a2191e1f83f1fe019ed0838487`
and its seal bind the 1105-file frozen product and 30 raw records. Both GNU
Release suites execute 917 tests: 847 pass and 70 required math tests fail.
Affected Debug passes 97/105 per ABI and ASC-only Clang19 sanitizer passes
38/46, each retaining eight ordinary required math failures. No tests skip
or time out, and no sanitizer diagnostics occur. All five PPEQU tests pass
in each configuration, including 5296 independent public profiles/112176
checks, 640 failure profiles/3345 checks, 12501 source-count checks, four
complete signatures and 72 protected-memory profiles.

All 70 Core/Dense/provider translation units compile freshly per lane,
producing 420 primary objects. Four relocated packages pass all 29 family
consumers. Six static checks pass; Doxygen covers 124 headers and 2188 public
members without warnings. The ten candidate07 files are unchanged; their
exact-input strict and isolated-header evidence remains bound, including the
successful second candidate seal and preserved failed first seal. Foreign
archives remain unsanitized; this is not a full mode/platform acceptance claim.

Provider-free audit
`24e26e253a9210c964f14d0bb2ddcf4f610e5a3b2df5c35fee4cb42b7e644a37`
and its seal bind scoped GNU static/shared 16/16 checks each, 60 fresh primary
objects and 12 test/support objects. All five optional packed Cholesky headers
are absent, with no Fortran discovery or foreign linkage. This is scoped
baseline evidence, not full baseline or shared-provider verification. These
40 records bring the ledger to 4092. PPCON04 remains audited but unregistered,
with its four required mathematical failures retained. PPRFS admission and
compile-only candidate02 are recorded separately; numerical execution is open.
All P00-P11, complete routine/mode/concurrency/platform acceptance and 130
missing required XBLAS entries remain required. No fully verified credit is added.


The three final documentation changes pass six fresh checks. Audit
`641c3f8bd3d17e970f1428499d0903d94c78ff26ac83b530a0889665d3f1efba`
rehashes 10570 unchanged numerical/default compiler inputs and artifacts,
including installed consumers. Its frozen source archive is
`7ba1493f04f804fbfd2db5bc5c94f8293a213937886d9bb018d44514fc54a991`. Numerical executions retain their original v25 identities;
no numerical rerun on changed prose is claimed. The ledger now has 4098
normalized records. The exact eleven-file PPCON04 import is next.
