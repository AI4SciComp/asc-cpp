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
